/*
 * dvbdevice.cpp
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dvbdevice.h"

/*
 * workaround buggy kernel includes
 * asm/types.h doesn't define __u64 in ansi mode, but linux/dvb/dmx.h needs it
 */
typedef quint64 __u64;

#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cmath>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QThread>
#include <Solid/DeviceNotifier>
#include <Solid/DvbInterface>
#include <KDebug>
#include "dvbchannel.h"
#include "dvbconfig.h"
#include "dvbmanager.h"

class DvbFilterInternal
{
public:
	DvbFilterInternal(int pid_, int dmxFd_) : pid(pid_), dmxFd(dmxFd_) { }
	~DvbFilterInternal() { }

	bool operator<(const DvbFilterInternal &x) const
	{
		return pid < x.pid;
	}

	friend bool operator<(const DvbFilterInternal &x, int y)
	{
		return x.pid < y;
	}

	friend bool operator<(int x, const DvbFilterInternal &y)
	{
		return x < y.pid;
	}

	int pid;
	int dmxFd;
	QList<DvbPidFilter *> filters;
};

struct DvbFilterData
{
	char packets[21][188];
	int count;
	DvbFilterData *next;
};

class DvbDeviceThread : private QThread
{
public:
	explicit DvbDeviceThread(QObject *parent) : QThread(parent), dvrFd(-1)
	{
		currentUnused = new DvbFilterData;
		currentUnused->next = currentUnused;
		totalBuffers = 1;

		usedBuffers = 0;
		currentUsed = currentUnused;

		if (pipe(pipes) != 0) {
			kError() << "pipe() failed";
			pipes[0] = -1;
			pipes[1] = -1;
		}
	}

	~DvbDeviceThread()
	{
		if (isRunning()) {
			stop();
		}

		close(pipes[0]);
		close(pipes[1]);

		// delete all buffers
		for (int i = 0; i < totalBuffers; ++i) {
			DvbFilterData *temp = currentUnused->next;
			delete currentUnused;
			currentUnused = temp;
		}
	}

	void start(int dvrFd_);
	void stop();

	bool isRunning()
	{
		return dvrFd != -1;
	}

	bool addPidFilter(int pid, DvbPidFilter *filter, const QString &demuxPath);
	void removePidFilter(int pid, DvbPidFilter *filter);

private:
	void customEvent(QEvent *);
	void run();

	int dvrFd;
	QList<DvbFilterInternal> filters;

	QMutex mutex;
	DvbFilterData *currentUnused;
	DvbFilterData *currentUsed;
	int usedBuffers;
	int totalBuffers;

	int pipes[2];
};

void DvbDeviceThread::start(int dvrFd_)
{
	Q_ASSERT(dvrFd == -1);

	dvrFd = dvrFd_;
	QThread::start();
}

void DvbDeviceThread::stop()
{
	Q_ASSERT(dvrFd != -1);

	if (write(pipes[1], " ", 1) != 1) {
		kError() << "write() failed";
	}

	wait();

	char temp;
	read(pipes[0], &temp, 1);

	// consistent state (there may still be a pending event!)
	usedBuffers = 0;
	currentUsed = currentUnused;

	// close all filters
	for (QList<DvbFilterInternal>::iterator it = filters.begin(); it != filters.end(); ++it) {
		close(it->dmxFd);
	}

	filters.clear();

	// close dvr
	close(dvrFd);
	dvrFd = -1;
}

bool DvbDeviceThread::addPidFilter(int pid, DvbPidFilter *filter, const QString &demuxPath)
{
	QList<DvbFilterInternal>::iterator it = qBinaryFind(filters.begin(), filters.end(), pid);

	// FIXME don't allow a filter to be registered twice for the same pid

	if (it != filters.end()) {
		it->filters.append(filter);
	} else {
		int dmxFd = open(QFile::encodeName(demuxPath), O_RDONLY | O_NONBLOCK);

		if (dmxFd < 0) {
			kWarning() << "couldn't open" << demuxPath;
			return false;
		}

		dmx_pes_filter_params pes_filter;
		memset(&pes_filter, 0, sizeof(pes_filter));

		pes_filter.pid = pid;
		pes_filter.input = DMX_IN_FRONTEND;
		pes_filter.output = DMX_OUT_TS_TAP;
		pes_filter.pes_type = DMX_PES_OTHER;
		pes_filter.flags = DMX_IMMEDIATE_START;

		if (ioctl(dmxFd, DMX_SET_PES_FILTER, &pes_filter) != 0) {
			kWarning() << "couldn't set up filter for" << demuxPath;
			close(dmxFd);
			return false;
		}

		DvbFilterInternal filterInternal(pid, dmxFd);
		filterInternal.filters.append(filter);

		filters.append(filterInternal);
		qSort(filters);
	}

	return true;
}

void DvbDeviceThread::removePidFilter(int pid, DvbPidFilter *filter)
{
	QList<DvbFilterInternal>::iterator it = qBinaryFind(filters.begin(), filters.end(), pid);

	if (it == filters.end()) {
		return;
	}

	int index = it->filters.indexOf(filter);

	if (index == -1) {
		return;
	}

	it->filters.removeAt(index);

	if (it->filters.isEmpty()) {
		close(it->dmxFd);
		filters.erase(it);
	}
}

void DvbDeviceThread::customEvent(QEvent *)
{
	while (true) {
		int i;

		for (i = 0; i < usedBuffers; ++i) {
			for (int j = 0; j < currentUsed->count; ++j) {
				char *packet = currentUsed->packets[j];

				if ((packet[1] & 0x80) != 0) {
					// transport error indicator
					kDebug() << "discarding packet due to TEI being set";
					continue;
				}

				int pid = ((static_cast<unsigned char> (packet[1]) << 8) |
					static_cast<unsigned char> (packet[2])) & ((1 << 13) - 1);

				QList<DvbFilterInternal>::iterator it =
					qBinaryFind(filters.begin(), filters.end(), pid);

				if (it != filters.end()) {
					/*
					 * it->filters has to be copied (foreach does that for us),
					 * because processData() may modify it
					 */
					foreach (DvbPidFilter *filter, it->filters) {
						filter->processData(packet);

						// FIXME
						if (usedBuffers == 0) {
							return;
						}
					}
				}
			}

			currentUsed = currentUsed->next;
		}

		mutex.lock();
		usedBuffers -= i;
		if (usedBuffers <= 0) {
			mutex.unlock();
			break;
		}
		mutex.unlock();
	}
}

void DvbDeviceThread::run()
{
	pollfd pfds[2];
	memset(&pfds, 0, sizeof(pfds));

	pfds[0].fd = pipes[0];
	pfds[0].events = POLLIN;

	pfds[1].fd = dvrFd;
	pfds[1].events = POLLIN;

	while (true) {
		if (poll(pfds, 2, -1) < 0) {
			if (errno == EINTR) {
				continue;
			}

			kWarning() << "poll() failed";
			break;
		}

		if ((pfds[0].revents & POLLIN) != 0) {
			break;
		}

		bool needNotify = false;

		while (true) {
			int size = read(dvrFd, currentUnused->packets, 21*188);

			if (size < 0) {
				if (errno == EAGAIN) {
					break;
				}

				size = read(dvrFd, currentUnused->packets, 21*188);

				if (size < 0) {
					if (errno == EAGAIN) {
						break;
					}

					kWarning() << "read() failed";
					return;
				}
			}

			currentUnused->count = size / 188;

			if ((usedBuffers + 1) == totalBuffers) {
				// we ran out of buffers
				DvbFilterData *newBuffer = new DvbFilterData;

				newBuffer->next = currentUnused->next;
				currentUnused->next = newBuffer;

				++totalBuffers;
			}

			mutex.lock();
			if (usedBuffers == 0) {
				needNotify = true;
			}
			++usedBuffers;
			mutex.unlock();

			currentUnused = currentUnused->next;

			if (size != 21*188) {
				break;
			}
		}

		if (needNotify) {
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		}

		msleep(1);
	}
}

DvbDevice::DvbDevice(DvbManager *manager_, int deviceIndex_) : manager(manager_),
	deviceIndex(deviceIndex_), internalState(0), deviceState(DeviceNotReady), frontendFd(-1),
	isAuto(false)
{
	thread = new DvbDeviceThread(this);
	connect(&frontendTimer, SIGNAL(timeout()), this, SLOT(frontendEvent()));
}

DvbDevice::~DvbDevice()
{
	stop();
}

void DvbDevice::componentAdded(const Solid::Device &component)
{
	const Solid::DvbInterface *dvbInterface = component.as<Solid::DvbInterface>();

	Q_ASSERT(dvbInterface != 0);

	switch (dvbInterface->deviceType()) {
	case Solid::DvbInterface::DvbCa:
		caComponent = component;
		caPath = dvbInterface->device();
		setInternalState(internalState | CaPresent);
		break;
	case Solid::DvbInterface::DvbDemux:
		demuxComponent = component;
		demuxPath = dvbInterface->device();
		setInternalState(internalState | DemuxPresent);
		break;
	case Solid::DvbInterface::DvbDvr:
		dvrComponent = component;
		dvrPath = dvbInterface->device();
		setInternalState(internalState | DvrPresent);
		break;
	case Solid::DvbInterface::DvbFrontend:
		frontendComponent = component;
		frontendPath = dvbInterface->device();
		setInternalState(internalState | FrontendPresent);
		break;
	default:
		break;
	}
}

bool DvbDevice::componentRemoved(const QString &udi)
{
	if (caComponent.udi() == udi) {
		setInternalState(internalState & (~CaPresent));
		return true;
	} else if (demuxComponent.udi() == udi) {
		setInternalState(internalState & (~DemuxPresent));
		return true;
	} else if (dvrComponent.udi() == udi) {
		setInternalState(internalState & (~DvrPresent));
		return true;
	} else if (frontendComponent.udi() == udi) {
		setInternalState(internalState & (~FrontendPresent));
		return true;
	}

	return false;
}

bool DvbDevice::checkUsable()
{
	Q_ASSERT(deviceState == DeviceIdle);

	int fd = open(QFile::encodeName(frontendPath), O_RDWR | O_NONBLOCK);

	if (fd < 0) {
		return false;
	}

	close(fd);
	return true;
}

static fe_modulation_t convertDvbModulation(DvbCTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbCTransponder::Qam16: return QAM_16;
	case DvbCTransponder::Qam32: return QAM_32;
	case DvbCTransponder::Qam64: return QAM_64;
	case DvbCTransponder::Qam128: return QAM_128;
	case DvbCTransponder::Qam256: return QAM_256;
	case DvbCTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_modulation_t convertDvbModulation(DvbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbTTransponder::Qpsk: return QPSK;
	case DvbTTransponder::Qam16: return QAM_16;
	case DvbTTransponder::Qam64: return QAM_64;
	case DvbTTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_modulation_t convertDvbModulation(AtscTransponder::Modulation modulation)
{
	switch (modulation) {
	case AtscTransponder::Qam64: return QAM_64;
	case AtscTransponder::Qam256: return QAM_256;
	case AtscTransponder::Vsb8: return VSB_8;
	case AtscTransponder::Vsb16: return VSB_16;
	case AtscTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_code_rate convertDvbFecRate(DvbTransponderBase::FecRate fecRate)
{
	switch (fecRate) {
	case DvbTransponderBase::FecNone: return FEC_NONE;
	case DvbTransponderBase::Fec1_2: return FEC_1_2;
	case DvbTransponderBase::Fec2_3: return FEC_2_3;
	case DvbTransponderBase::Fec3_4: return FEC_3_4;
	case DvbTransponderBase::Fec4_5: return FEC_4_5;
	case DvbTransponderBase::Fec5_6: return FEC_5_6;
	case DvbTransponderBase::Fec6_7: return FEC_6_7;
	case DvbTransponderBase::Fec7_8: return FEC_7_8;
	case DvbTransponderBase::Fec8_9: return FEC_8_9;
	case DvbTransponderBase::FecAuto: return FEC_AUTO;
	}

	return FEC_AUTO;
}

static fe_bandwidth convertDvbBandwidth(DvbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbTTransponder::Bandwidth6MHz: return BANDWIDTH_6_MHZ;
	case DvbTTransponder::Bandwidth7MHz: return BANDWIDTH_7_MHZ;
	case DvbTTransponder::Bandwidth8MHz: return BANDWIDTH_8_MHZ;
	case DvbTTransponder::BandwidthAuto: return BANDWIDTH_AUTO;
	}

	return BANDWIDTH_AUTO;
}

static fe_transmit_mode convertDvbTransmissionMode(DvbTTransponder::TransmissionMode mode)
{
	switch (mode) {
	case DvbTTransponder::TransmissionMode2k: return TRANSMISSION_MODE_2K;
	case DvbTTransponder::TransmissionMode8k: return TRANSMISSION_MODE_8K;
	case DvbTTransponder::TransmissionModeAuto: return TRANSMISSION_MODE_AUTO;
	}

	return TRANSMISSION_MODE_AUTO;
}

static fe_guard_interval convertDvbGuardInterval(DvbTTransponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case DvbTTransponder::GuardInterval1_4: return GUARD_INTERVAL_1_4;
	case DvbTTransponder::GuardInterval1_8: return GUARD_INTERVAL_1_8;
	case DvbTTransponder::GuardInterval1_16: return GUARD_INTERVAL_1_16;
	case DvbTTransponder::GuardInterval1_32: return GUARD_INTERVAL_1_32;
	case DvbTTransponder::GuardIntervalAuto: return GUARD_INTERVAL_AUTO;
	}

	return GUARD_INTERVAL_AUTO;
}

static fe_hierarchy convertDvbHierarchy(DvbTTransponder::Hierarchy hierarchy)
{
	switch (hierarchy) {
	case DvbTTransponder::HierarchyNone: return HIERARCHY_NONE;
	case DvbTTransponder::Hierarchy1: return HIERARCHY_1;
	case DvbTTransponder::Hierarchy2: return HIERARCHY_2;
	case DvbTTransponder::Hierarchy4: return HIERARCHY_4;
	case DvbTTransponder::HierarchyAuto: return HIERARCHY_AUTO;
	}

	return HIERARCHY_AUTO;
}

void DvbDevice::tune(const DvbTransponder &transponder)
{
	Q_ASSERT((deviceState == DeviceIdle) || ((deviceState == DeviceTuningFailed)));

	if (frontendFd < 0) {
		frontendFd = open(QFile::encodeName(frontendPath), O_RDWR | O_NONBLOCK);

		if (frontendFd < 0) {
			kWarning() << "couldn't open" << frontendPath;
			return;
		}

		int dvrFd = open(QFile::encodeName(dvrPath), O_RDONLY | O_NONBLOCK);

		if (dvrFd < 0) {
			kWarning() << "couldn't open" << dvrPath;
			close(frontendFd);
			frontendFd = -1;
			return;
		}

		// start thread
		thread->start(dvrFd);
	}

	bool moveRotor = false;

	switch (transponder->getTransmissionType()) {
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *dvbCTransponder = transponder->getDvbCTransponder();
		Q_ASSERT(dvbCTransponder != NULL);

		// tune

		dvb_frontend_parameters params;
		memset(&params, 0, sizeof(params));

		params.frequency = dvbCTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.qam.symbol_rate = dvbCTransponder->symbolRate;
		params.u.qam.fec_inner = convertDvbFecRate(dvbCTransponder->fecRate);
		params.u.qam.modulation = convertDvbModulation(dvbCTransponder->modulation);

		if (ioctl(frontendFd, FE_SET_FRONTEND, &params) != 0) {
			kWarning() << "ioctl FE_SET_FRONTEND failed for" << frontendPath;
			setDeviceState(DeviceTuningFailed);
			return;
		}

		break;
	    }

	case DvbTransponderBase::DvbS: {
		const DvbSTransponder *dvbSTransponder = transponder->getDvbSTransponder();
		Q_ASSERT(dvbSTransponder != NULL);

		// parameters

		bool horPolar = (dvbSTransponder->polarization == DvbSTransponder::Horizontal) ||
				(dvbSTransponder->polarization == DvbSTransponder::CircularLeft);

		int frequency = dvbSTransponder->frequency;
		bool highBand = false;

		if (config->switchFrequency != 0) {
			// dual LO (low / high)
			if (frequency < config->switchFrequency) {
				frequency = abs(frequency - config->lowBandFrequency);
			} else {
				frequency = abs(frequency - config->highBandFrequency);
				highBand = true;
			}
		} else if (config->highBandFrequency != 0) {
			// single LO (horizontal / vertical)
			if (horPolar) {
				frequency = abs(frequency - config->lowBandFrequency);
			} else {
				frequency = abs(frequency - config->highBandFrequency);
			}
		} else {
			// single LO
			frequency = abs(frequency - config->lowBandFrequency);
		}

		// tone off

		if (ioctl(frontendFd, FE_SET_TONE, SEC_TONE_OFF) != 0) {
			kWarning() << "ioctl FE_SET_TONE failed for" << frontendPath;
		}

		// horizontal / circular left --> 18V ; vertical / circular right --> 13V

		if (ioctl(frontendFd, FE_SET_VOLTAGE,
			  horPolar ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13) != 0) {
			kWarning() << "ioctl FE_SET_VOLTAGE failed for" << frontendPath;
		}

		// diseqc / rotor

		usleep(15000);

		dvb_diseqc_master_cmd mCmd;

		switch (config->configuration) {
		case DvbConfigBase::DiseqcSwitch: {
			dvb_diseqc_master_cmd cmd = { { 0xe0, 0x10, 0x38, 0x00, 0x00, 0x00 }, 4 };
			cmd.msg[3] = 0xf0 | (config->lnbNumber << 2) | (horPolar ? 2 : 0) |
				            (highBand ? 1 : 0);
			mCmd = cmd;
			break;
		    }

		case DvbConfigBase::UsalsRotor: {
			QString source = config->scanSource;
			source.remove(0, source.lastIndexOf('-') + 1);

			bool ok = false;
			double position = 0;

			if (source.endsWith('E')) {
				source.chop(1);
				position = source.toDouble(&ok);
			} else if (source.endsWith('W')) {
				source.chop(1);
				position = -source.toDouble(&ok);
			}

			if (!ok) {
				kWarning() << "couldn't extract orbital position";
			}

			double longitudeDiff = manager->getLongitude() - position;

			double latRad = manager->getLatitude() * M_PI / 180;
			double longDiffRad = longitudeDiff * M_PI / 180;
			double temp = cos(latRad) * cos(longDiffRad);
			double temp2 = -sin(latRad) * cos(longDiffRad) / sqrt(1 - temp * temp);

			// deal with corner cases
			if (temp2 < -1) {
				temp2 = -1;
			} else if (temp2 > 1) {
				temp2 = 1;
			} else if (temp2 != temp2) {
				temp2 = 1;
			}

			double azimuth = acos(temp2) * 180 / M_PI;

			if (((longitudeDiff > 0) && (longitudeDiff < 180)) ||
			    (longitudeDiff < -180)) {
				azimuth = 360 - azimuth;
			}

			int value = (azimuth * 16) + 0.5;

			if (value == (360 * 16)) {
				value = 0;
			}

			dvb_diseqc_master_cmd cmd = { { 0xe0, 0x31, 0x6e, 0x00, 0x00, 0x00 }, 5 };
			cmd.msg[3] = value / 256;
			cmd.msg[4] = value % 256;
			mCmd = cmd;
			moveRotor = true;
			break;
		    }

		case DvbConfigBase::PositionsRotor: {
			dvb_diseqc_master_cmd cmd = { { 0xe0, 0x31, 0x6b, 0x00, 0x00, 0x00 }, 4 };
			cmd.msg[3] = config->lnbNumber;
			mCmd = cmd;
			moveRotor = true;
			break;
		    }
		}

		if (ioctl(frontendFd, FE_DISEQC_SEND_MASTER_CMD, &mCmd) != 0) {
			kWarning() << "ioctl FE_DISEQC_SEND_MASTER_CMD failed for" << frontendPath;
		}

		usleep(15000);

		if (!moveRotor) {
			int burst = ((config->lnbNumber % 2) == 0) ? SEC_MINI_A : SEC_MINI_B;

			if (ioctl(frontendFd, FE_DISEQC_SEND_BURST, burst) != 0) {
				kWarning() << "ioctl FE_DISEQC_SEND_BURST failed for" <<
					      frontendPath;
			}

			usleep(15000);
		}

		// low band --> tone off ; high band --> tone on

		if (ioctl(frontendFd, FE_SET_TONE, highBand ? SEC_TONE_ON : SEC_TONE_OFF) != 0) {
			kWarning() << "ioctl FE_SET_TONE failed for" << frontendPath;
		}

		// tune

		dvb_frontend_parameters params;
		memset(&params, 0, sizeof(params));

		params.frequency = frequency;
		params.inversion = INVERSION_AUTO;
		params.u.qpsk.symbol_rate = dvbSTransponder->symbolRate;
		params.u.qpsk.fec_inner = convertDvbFecRate(dvbSTransponder->fecRate);

		if (ioctl(frontendFd, FE_SET_FRONTEND, &params) != 0) {
			kWarning() << "ioctl FE_SET_FRONTEND failed for" << frontendPath;
			setDeviceState(DeviceTuningFailed);
			return;
		}

		break;
	    }

	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *dvbTTransponder = transponder->getDvbTTransponder();
		Q_ASSERT(dvbTTransponder != NULL);

		// tune

		dvb_frontend_parameters params;
		memset(&params, 0, sizeof(params));

		params.frequency = dvbTTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.ofdm.bandwidth = convertDvbBandwidth(dvbTTransponder->bandwidth);
		params.u.ofdm.code_rate_HP = convertDvbFecRate(dvbTTransponder->fecRateHigh);
		params.u.ofdm.code_rate_LP = convertDvbFecRate(dvbTTransponder->fecRateLow);
		params.u.ofdm.constellation = convertDvbModulation(dvbTTransponder->modulation);
		params.u.ofdm.transmission_mode =
			convertDvbTransmissionMode(dvbTTransponder->transmissionMode);
		params.u.ofdm.guard_interval =
			convertDvbGuardInterval(dvbTTransponder->guardInterval);
		params.u.ofdm.hierarchy_information =
			convertDvbHierarchy(dvbTTransponder->hierarchy);

		if (ioctl(frontendFd, FE_SET_FRONTEND, &params) != 0) {
			kWarning() << "ioctl FE_SET_FRONTEND failed for" << frontendPath;
			setDeviceState(DeviceTuningFailed);
			return;
		}

		break;
	    }

	case DvbTransponderBase::Atsc: {
		const AtscTransponder *atscTransponder = transponder->getAtscTransponder();
		Q_ASSERT(atscTransponder != NULL);

		// tune

		dvb_frontend_parameters params;
		memset(&params, 0, sizeof(params));

		params.frequency = atscTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.vsb.modulation = convertDvbModulation(atscTransponder->modulation);

		if (ioctl(frontendFd, FE_SET_FRONTEND, &params) != 0) {
			kWarning() << "ioctl FE_SET_FRONTEND failed for" << frontendPath;
			setDeviceState(DeviceTuningFailed);
			return;
		}

		break;
	    }
	}

	if (!moveRotor) {
		setDeviceState(DeviceTuning);

		// wait for tuning
		frontendTimeout = config->timeout;
		frontendTimer.start(100);
	} else {
		setDeviceState(DeviceRotorMoving);

		// wait for tuning
		frontendTimeout = config->timeout;
		frontendTimer.start(15000);
	}
}

void DvbDevice::autoTune(const DvbTransponder &transponder)
{
	if (transponder->getTransmissionType() != DvbTransponderBase::DvbT) {
		kWarning() << "can't handle != DVB-T";
		return;
	}

	isAuto = true;
	autoTTransponder = new DvbTTransponder(*transponder->getDvbTTransponder());
	autoTransponder = DvbTransponder(autoTTransponder);

	// we have to iterate over unsupported AUTO values

	if ((frontendCapabilities & FE_CAN_FEC_AUTO) == 0) {
		autoTTransponder->fecRateHigh = DvbTTransponder::Fec2_3;
	}

	if ((frontendCapabilities & FE_CAN_GUARD_INTERVAL_AUTO) == 0) {
		autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_8;
	}

	if ((frontendCapabilities & FE_CAN_QAM_AUTO) == 0) {
		autoTTransponder->modulation = DvbTTransponder::Qam64;
	}

	if ((frontendCapabilities & FE_CAN_TRANSMISSION_MODE_AUTO) == 0) {
		autoTTransponder->transmissionMode = DvbTTransponder::TransmissionMode8k;
	}

	tune(autoTransponder);
}

DvbTransponder DvbDevice::getAutoTransponder() const
{
	// FIXME query back information like frequency - tuning parameters - ...
	return autoTransponder;
}

void DvbDevice::stop()
{
	if ((deviceState == DeviceNotReady) || (deviceState == DeviceIdle)) {
		return;
	}

	// stop thread
	if (thread->isRunning()) {
		thread->stop();
	}

	// stop waiting for tuning
	frontendTimer.stop();

	// close frontend
	if (frontendFd >= 0) {
		close(frontendFd);
		frontendFd = -1;
	}

	isAuto = false;
	setDeviceState(DeviceIdle);
}

int DvbDevice::getSignal()
{
	Q_ASSERT(frontendFd >= 0);

	quint16 signal;

	if (ioctl(frontendFd, FE_READ_SIGNAL_STRENGTH, &signal) != 0) {
		kWarning() << "ioctl FE_READ_SIGNAL_STRENGTH failed for" << frontendPath;
		return 0;
	}

	return (signal * 100) / 0xffff;
}

int DvbDevice::getSnr()
{
	Q_ASSERT(frontendFd >= 0);

	quint16 snr;

	if (ioctl(frontendFd, FE_READ_SNR, &snr) != 0) {
		kWarning() << "ioctl FE_READ_SNR failed for" << frontendPath;
		return 0;
	}

	return (snr * 100) / 0xffff;
}

bool DvbDevice::isTuned()
{
	Q_ASSERT(frontendFd >= 0);

	fe_status_t status;
	memset(&status, 0, sizeof(status));

	if (ioctl(frontendFd, FE_READ_STATUS, &status) != 0) {
		kWarning() << "ioctl FE_READ_STATUS failed for" << frontendPath;
		return false;
	}

	return (status & FE_HAS_LOCK) != 0;
}

bool DvbDevice::addPidFilter(int pid, DvbPidFilter *filter)
{
	return thread->addPidFilter(pid, filter, demuxPath);
}

void DvbDevice::removePidFilter(int pid, DvbPidFilter *filter)
{
	thread->removePidFilter(pid, filter);
}

void DvbDevice::frontendEvent()
{
	fe_status_t status;
	memset(&status, 0, sizeof(status));

	if (ioctl(frontendFd, FE_READ_STATUS, &status) != 0) {
		kWarning() << "ioctl FE_READ_STATUS failed for" << frontendPath;
	} else {
		if ((status & FE_HAS_LOCK) != 0) {
			kDebug() << "tuning succeeded for" << frontendPath;
			frontendTimer.stop();
			setDeviceState(DeviceTuned);
			return;
		}
	}

	// FIXME progress bar when moving rotor

	frontendTimeout -= 100;

	if (frontendTimeout <= 0) {
		frontendTimer.stop();

		if (!isAuto) {
			kWarning() << "tuning failed for" << frontendPath;
			setDeviceState(DeviceTuningFailed);
			return;
		}

		quint16 signal;

		if (ioctl(frontendFd, FE_READ_SIGNAL_STRENGTH, &signal) != 0) {
			kWarning() << "ioctl FE_READ_SIGNAL_STRENGTH failed for" << frontendPath;
			signal = 0;
		}

		if ((signal != 0) && (signal < 0x2000)) {
			// signal too weak
			kWarning() << "tuning failed for" << frontendPath;
			setDeviceState(DeviceTuningFailed);
			return;
		}

		bool carry = true;

		if (carry && ((frontendCapabilities & FE_CAN_FEC_AUTO) == 0)) {
			switch (autoTTransponder->fecRateHigh) {
			case DvbTTransponder::Fec2_3:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec3_4;
				carry = false;
				break;
			case DvbTTransponder::Fec3_4:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec1_2;
				carry = false;
				break;
			case DvbTTransponder::Fec1_2:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec5_6;
				carry = false;
				break;
			case DvbTTransponder::Fec5_6:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec7_8;
				carry = false;
				break;
			case DvbTTransponder::Fec4_5:
			case DvbTTransponder::Fec6_7:
			case DvbTTransponder::Fec7_8:
			case DvbTTransponder::Fec8_9:
			case DvbTTransponder::FecNone:
			case DvbTTransponder::FecAuto:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec2_3;
				break;
			}
		}

		if (carry && ((frontendCapabilities & FE_CAN_GUARD_INTERVAL_AUTO) == 0)) {
			switch (autoTTransponder->guardInterval) {
			case DvbTTransponder::GuardInterval1_8:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_32;
				carry = false;
				break;
			case DvbTTransponder::GuardInterval1_32:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_4;
				carry = false;
				break;
			case DvbTTransponder::GuardInterval1_4:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_16;
				carry = false;
				break;
			case DvbTTransponder::GuardInterval1_16:
			case DvbTTransponder::GuardIntervalAuto:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_8;
				break;
			}
		}

		if (carry && ((frontendCapabilities & FE_CAN_QAM_AUTO) == 0)) {
			switch (autoTTransponder->modulation) {
			case DvbTTransponder::Qam64:
				autoTTransponder->modulation = DvbTTransponder::Qam16;
				carry = false;
				break;
			case DvbTTransponder::Qam16:
				autoTTransponder->modulation = DvbTTransponder::Qpsk;
				carry = false;
				break;
			case DvbTTransponder::Qpsk:
			case DvbTTransponder::ModulationAuto:
				autoTTransponder->modulation = DvbTTransponder::Qam64;
				break;
			}
		}

		if (carry && ((frontendCapabilities & FE_CAN_TRANSMISSION_MODE_AUTO) == 0)) {
			switch (autoTTransponder->transmissionMode) {
			case DvbTTransponder::TransmissionMode8k:
				autoTTransponder->transmissionMode = DvbTTransponder::TransmissionMode2k;
				carry = false;
				break;
			case DvbTTransponder::TransmissionMode2k:
			case DvbTTransponder::TransmissionModeAuto:
				autoTTransponder->transmissionMode = DvbTTransponder::TransmissionMode8k;
				break;
			}
		}

		if (!carry) {
			deviceState = DeviceTuningFailed;
			tune(autoTransponder);
		} else {
			kWarning() << "tuning failed for" << frontendPath;
			setDeviceState(DeviceTuningFailed);
		}
	}
}

void DvbDevice::setInternalState(stateFlags newState)
{
	if (internalState == newState) {
		return;
	}

	bool oldPresent = ((internalState & DevicePresent) == DevicePresent);
	bool newPresent = ((newState & DevicePresent) == DevicePresent);

	// have this assignment here so that setInternalState can be called recursively

	internalState = newState;

	if (oldPresent != newPresent) {
		if (newPresent) {
			if (identifyDevice()) {
				setDeviceState(DeviceIdle);
			}
		} else {
			stop();
			setDeviceState(DeviceNotReady);
		}
	}
}

void DvbDevice::setDeviceState(DeviceState newState)
{
	deviceState = newState;
	emit stateChanged();
}

static int DvbReadSysAttr(const QString &path)
{
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly)) {
		return -1;
	}

	QByteArray data = file.read(16);

	if ((data.size() == 0) || (data.size() == 16)) {
		return -1;
	}

	bool ok;
	int value = QString(data).toInt(&ok, 16);

	if (!ok || (value >= (1 << 16))) {
		return -1;
	}

	return value;
}

bool DvbDevice::identifyDevice()
{
	int fd = open(QFile::encodeName(frontendPath), O_RDONLY | O_NONBLOCK);

	if (fd < 0) {
		kWarning() << "couldn't open" << frontendPath;
		return false;
	}

	dvb_frontend_info frontend_info;
	memset(&frontend_info, 0, sizeof(frontend_info));

	if (ioctl(fd, FE_GET_INFO, &frontend_info) != 0) {
		kWarning() << "couldn't execute FE_GET_INFO ioctl on" << frontendPath;
		close(fd);
		return false;
	}

	close(fd);

	switch (frontend_info.type) {
	case FE_QPSK:
		transmissionTypes = DvbS;
		break;
	case FE_QAM:
		transmissionTypes = DvbC;
		break;
	case FE_OFDM:
		transmissionTypes = DvbT;
		break;
	case FE_ATSC:
		transmissionTypes = Atsc;
		break;
	default:
		kWarning() << "unknown frontend type" << frontend_info.type << "for" << frontendPath;
		return false;
	}

	frontendCapabilities = frontend_info.caps;

	frontendName = QString::fromAscii(frontend_info.name);
	deviceId.clear();

	int adapter = deviceIndex >> 16;
	int index = deviceIndex & ((1 << 16) - 1);
	QDir dir(QString("/sys/class/dvb/dvb%1.frontend%2").arg(adapter).arg(index));

	if (QFile::exists(dir.filePath("device/vendor"))) {
		// PCI device
		int vendor = DvbReadSysAttr(dir.filePath("device/vendor"));
		int device = DvbReadSysAttr(dir.filePath("device/device"));
		int subsystem_vendor = DvbReadSysAttr(dir.filePath("device/subsystem_vendor"));
		int subsystem_device = DvbReadSysAttr(dir.filePath("device/subsystem_device"));

		if ((vendor >= 0) && (device >= 0) && (subsystem_vendor >= 0) &&
		    (subsystem_device >= 0)) {
			deviceId = 'P';
			deviceId += QString("%1").arg(vendor, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(device, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(subsystem_vendor, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(subsystem_device, 4, 16, QChar('0'));
		}
	} else if (QFile::exists(dir.filePath("device/idVendor"))) {
		// USB device
		int vendor = DvbReadSysAttr(dir.filePath("device/idVendor"));
		int product = DvbReadSysAttr(dir.filePath("device/idProduct"));

		if ((vendor >= 0) && (product >= 0)) {
			deviceId = 'U';
			deviceId += QString("%1").arg(vendor, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(product, 4, 16, QChar('0'));
		}
	}

	if (deviceId.isEmpty()) {
		kWarning() << "couldn't identify device";
	}

	kDebug() << "found dvb device" << deviceId << "/" << frontendName;

	return true;
}

DvbDeviceManager::DvbDeviceManager(DvbManager *manager_) : QObject(manager_), manager(manager_)
{
	QObject *notifier = Solid::DeviceNotifier::instance();
	connect(notifier, SIGNAL(deviceAdded(QString)), this, SLOT(componentAdded(QString)));
	connect(notifier, SIGNAL(deviceRemoved(QString)), this, SLOT(componentRemoved(QString)));

	foreach (const Solid::Device &device,
		 Solid::Device::listFromType(Solid::DeviceInterface::DvbInterface)) {
		componentAdded(device);
	}
}

DvbDeviceManager::~DvbDeviceManager()
{
	qDeleteAll(devices);
}

void DvbDeviceManager::componentAdded(const QString &udi)
{
	componentAdded(Solid::Device(udi));
}

void DvbDeviceManager::componentRemoved(const QString &udi)
{
	foreach (DvbDevice *device, devices) {
		bool wasReady = (device->getDeviceState() != DvbDevice::DeviceNotReady);

		if (device->componentRemoved(udi)) {
			if (wasReady && (device->getDeviceState() == DvbDevice::DeviceNotReady)) {
				emit deviceRemoved(device);
			}

			break;
		}
	}
}

void DvbDeviceManager::componentAdded(const Solid::Device &component)
{
	const Solid::DvbInterface *dvbInterface = component.as<Solid::DvbInterface>();

	if (dvbInterface == NULL) {
		return;
	}

	int adapter = dvbInterface->deviceAdapter();
	int index = dvbInterface->deviceIndex();

	if ((adapter < 0) || (index < 0)) {
		kWarning() << "couldn't determine adapter and/or index for" << component.udi();
		return;
	}

	int deviceIndex = (adapter << 16) | index;
	DvbDevice *device;

	for (QList<DvbDevice *>::iterator it = devices.begin();; ++it) {
		if ((it == devices.end()) || ((*it)->getIndex() > deviceIndex)) {
			device = new DvbDevice(manager, deviceIndex);
			devices.insert(it, device);
			break;
		} else if ((*it)->getIndex() == deviceIndex) {
			device = *it;
			break;
		}
	}

	bool wasNotReady = (device->getDeviceState() == DvbDevice::DeviceNotReady);

	device->componentAdded(component);

	if (wasNotReady && (device->getDeviceState() != DvbDevice::DeviceNotReady)) {
		emit deviceAdded(device);
	}
}
