/*
 * dvbdevice_linux.cpp
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbdevice_linux.h"

#include <QFile>
#include <QThread>
#include <Solid/DeviceNotifier>
#include <Solid/DvbInterface>
#include <KDebug>
#include <dmx.h>
#include <errno.h>
#include <fcntl.h>
#include <frontend.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "dvbsi.h"

// krazy:excludeall=syscalls

class DvbDeviceThread : private QThread
{
public:
	DvbDeviceThread() : dvrFd(-1), buffer(NULL, 0)
	{
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

		if (buffer.data != NULL) {
			buffer.dataSize = 0;
			dataChannel->writeBuffer(buffer);
		}

		close(pipes[0]);
		close(pipes[1]);
	}

	void start(int dvrFd_, DvbAbstractDataChannel *dataChannel_);
	void discardBuffers();
	void stop();

	bool isRunning()
	{
		return (dvrFd != -1);
	}

private:
	void run();

	int pipes[2];
	int dvrFd;
	DvbAbstractDataChannel *dataChannel;
	DvbDataBuffer buffer;
};

void DvbDeviceThread::start(int dvrFd_, DvbAbstractDataChannel *dataChannel_)
{
	Q_ASSERT(dvrFd == -1);
	dvrFd = dvrFd_;
	dataChannel = dataChannel_;

	if (buffer.data == NULL) {
		buffer = dataChannel->getBuffer();
	}

	QThread::start();
}

void DvbDeviceThread::discardBuffers()
{
	Q_ASSERT(dvrFd != -1);

	if (write(pipes[1], " ", 1) != 1) {
		kError() << "write() failed";
	}

	wait();

	char temp;
	read(pipes[0], &temp, 1);

	while (read(dvrFd, buffer.data, buffer.bufferSize) > 0) {
	}

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
	dvrFd = -1;

	if (buffer.data != NULL) {
		buffer.dataSize = 0;
		dataChannel->writeBuffer(buffer);
		buffer = DvbDataBuffer(NULL, 0);
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

		while (true) {
			buffer.dataSize = read(dvrFd, buffer.data, buffer.bufferSize);

			if (buffer.dataSize < 0) {
				if (errno == EAGAIN) {
					break;
				}

				buffer.dataSize = read(dvrFd, buffer.data, buffer.bufferSize);

				if (buffer.dataSize < 0) {
					if (errno == EAGAIN) {
						break;
					}

					kWarning() << "read() failed";
					return;
				}
			}

			if (buffer.dataSize > 0) {
				dataChannel->writeBuffer(buffer);
				buffer = dataChannel->getBuffer();
			}

			if (buffer.dataSize != buffer.bufferSize) {
				break;
			}
		}

		msleep(1);
	}
}

DvbLinuxDevice::DvbLinuxDevice(int adapter_, int index_) : adapter(adapter_), index(index_),
	dataChannel(NULL), ready(false), frontendFd(-1), dvrFd(-1)
{
	thread = new DvbDeviceThread();
}

DvbLinuxDevice::~DvbLinuxDevice()
{
	release();
	delete thread;
}

bool DvbLinuxDevice::componentAdded(const Solid::Device &component)
{
	const Solid::DvbInterface *dvbInterface = component.as<Solid::DvbInterface>();

	bool wasPresent = ((internalState & DevicePresent) == DevicePresent);

	switch (dvbInterface->deviceType()) {
	case Solid::DvbInterface::DvbCa:
		caComponent = component;
		caPath = dvbInterface->device();
		internalState |= CaPresent;
		cam.startCa(caPath);
		break;
	case Solid::DvbInterface::DvbDemux:
		demuxComponent = component;
		demuxPath = dvbInterface->device();
		internalState |= DemuxPresent;
		break;
	case Solid::DvbInterface::DvbDvr:
		dvrComponent = component;
		dvrPath = dvbInterface->device();
		internalState |= DvrPresent;
		break;
	case Solid::DvbInterface::DvbFrontend:
		frontendComponent = component;
		frontendPath = dvbInterface->device();
		internalState |= FrontendPresent;
		break;
	default:
		return false;
	}

	if (!wasPresent && ((internalState & DevicePresent) == DevicePresent)) {
		if (identifyDevice()) {
			ready = true;
			return true;
		}
	}

	return false;
}

bool DvbLinuxDevice::componentRemoved(const QString &udi)
{
	if (caComponent.udi() == udi) {
		cam.stopCa();
		internalState &= ~CaPresent;
	} else if (demuxComponent.udi() == udi) {
		internalState &= ~DemuxPresent;
	} else if (dvrComponent.udi() == udi) {
		internalState &= ~DvrPresent;
	} else if (frontendComponent.udi() == udi) {
		internalState &= ~FrontendPresent;
	} else {
		return false;
	}

	if (ready && ((internalState & DevicePresent) != DevicePresent)) {
		release();
		ready = false;
		return true;
	}

	return false;
}

void DvbLinuxDevice::setDataChannel(DvbAbstractDataChannel *dataChannel_)
{
	dataChannel = dataChannel_;
}

void DvbLinuxDevice::setDeviceEnabled(bool enabled)
{
	Q_UNUSED(enabled) // FIXME
}

void DvbLinuxDevice::getDeviceId(QString &result) const
{
	Q_ASSERT(ready);
	result = deviceId;
}

void DvbLinuxDevice::getFrontendName(QString &result) const
{
	Q_ASSERT(ready);
	result = frontendName;
}

void DvbLinuxDevice::getTransmissionTypes(TransmissionTypes &result) const
{
	Q_ASSERT(ready);
	result = transmissionTypes;
}

void DvbLinuxDevice::getCapabilities(Capabilities &result) const
{
	Q_ASSERT(ready);
	result = capabilities;
}

void DvbLinuxDevice::acquire(bool &ok)
{
	Q_ASSERT(ready && (frontendFd < 0) && (dvrFd < 0));

	frontendFd = open(QFile::encodeName(frontendPath), O_RDWR | O_NONBLOCK);

	if (frontendFd < 0) {
		kWarning() << "cannot open frontend" << frontendPath;
		ok = false;
		return;
	}

	dvrFd = open(QFile::encodeName(dvrPath), O_RDONLY | O_NONBLOCK);

	if (dvrFd < 0) {
		kWarning() << "cannot open dvr" << dvrPath;
		close(frontendFd);
		frontendFd = -1;
		ok = false;
		return;
	}

	thread->start(dvrFd, dataChannel);
	ok = true;
	return;
}

void DvbLinuxDevice::release()
{
	foreach (int dmxFd, dmxFds) {
		close(dmxFd);
	}

	dmxFds.clear();

	if (thread->isRunning()) {
		thread->stop();
	}

	if (dvrFd >= 0) {
		close(dvrFd);
		dvrFd = -1;
	}

	if (frontendFd >= 0) {
		close(frontendFd);
		frontendFd = -1;
	}
}

void DvbLinuxDevice::setTone(SecTone tone, bool &ok)
{
	Q_ASSERT(frontendFd >= 0);

	if (ioctl(frontendFd, FE_SET_TONE, (tone == ToneOff) ? SEC_TONE_OFF : SEC_TONE_ON) != 0) {
		kWarning() << "ioctl FE_SET_TONE failed for" << frontendPath;
		ok = false;
		return;
	}

	ok = true;
	return;
}

void DvbLinuxDevice::setVoltage(SecVoltage voltage, bool &ok)
{
	Q_ASSERT(frontendFd >= 0);

	if (ioctl(frontendFd, FE_SET_VOLTAGE,
		  (voltage == Voltage13V) ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18) != 0) {
		kWarning() << "ioctl FE_SET_VOLTAGE failed for" << frontendPath;
		ok = false;
		return;
	}

	ok = true;
	return;
}

void DvbLinuxDevice::sendMessage(const char *message, int length, bool &ok)
{
	Q_ASSERT((frontendFd >= 0) && (length >= 0) && (length <= 6));

	dvb_diseqc_master_cmd cmd;
	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.msg, message, length);
	cmd.msg_len = length;

	if (ioctl(frontendFd, FE_DISEQC_SEND_MASTER_CMD, &cmd) != 0) {
		kWarning() << "ioctl FE_DISEQC_SEND_MASTER_CMD failed for" << frontendPath;
		ok = false;
		return;
	}

	ok = true;
	return;
}

void DvbLinuxDevice::sendBurst(SecBurst burst, bool &ok)
{
	Q_ASSERT(frontendFd >= 0);

	if (ioctl(frontendFd, FE_DISEQC_SEND_BURST,
		  (burst == BurstMiniA) ? SEC_MINI_A : SEC_MINI_B) != 0) {
		kWarning() << "ioctl FE_DISEQC_SEND_BURST failed for" << frontendPath;
		ok = false;
		return;
	}

	ok = true;
	return;
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

static fe_modulation_t convertDvbModulation(DvbS2Transponder::Modulation modulation)
{
	switch (modulation) {
	case DvbS2Transponder::Qpsk: return QPSK;
	case DvbS2Transponder::Psk8: return PSK_8;
	case DvbS2Transponder::Apsk16: return APSK_16;
	case DvbS2Transponder::Apsk32: return APSK_32;
	case DvbS2Transponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_rolloff convertDvbRollOff(DvbS2Transponder::RollOff rollOff)
{
	switch (rollOff) {
	case DvbS2Transponder::RollOff20: return ROLLOFF_20;
	case DvbS2Transponder::RollOff25: return ROLLOFF_25;
	case DvbS2Transponder::RollOff35: return ROLLOFF_35;
	case DvbS2Transponder::RollOffAuto: return ROLLOFF_AUTO;
	}

	return ROLLOFF_AUTO;
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
	case DvbTransponderBase::Fec1_3: return FEC_AUTO;
	case DvbTransponderBase::Fec1_4: return FEC_AUTO;
	case DvbTransponderBase::Fec2_3: return FEC_2_3;
	case DvbTransponderBase::Fec2_5: return FEC_AUTO;
	case DvbTransponderBase::Fec3_4: return FEC_3_4;
	case DvbTransponderBase::Fec3_5: return FEC_3_5;
	case DvbTransponderBase::Fec4_5: return FEC_4_5;
	case DvbTransponderBase::Fec5_6: return FEC_5_6;
	case DvbTransponderBase::Fec6_7: return FEC_6_7;
	case DvbTransponderBase::Fec7_8: return FEC_7_8;
	case DvbTransponderBase::Fec8_9: return FEC_8_9;
	case DvbTransponderBase::Fec9_10: return FEC_9_10;
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
	case DvbTTransponder::TransmissionMode4k: return TRANSMISSION_MODE_4K;
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

void DvbLinuxDevice::tune(const DvbTransponder &transponder, bool &ok)
{
	Q_ASSERT(frontendFd >= 0);

	dvb_frontend_parameters params;
	memset(&params, 0, sizeof(params));

	switch (transponder->getTransmissionType()) {
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *dvbCTransponder = transponder->getDvbCTransponder();

		params.frequency = dvbCTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.qam.symbol_rate = dvbCTransponder->symbolRate;
		params.u.qam.fec_inner = convertDvbFecRate(dvbCTransponder->fecRate);
		params.u.qam.modulation = convertDvbModulation(dvbCTransponder->modulation);
		break;
	    }

	case DvbTransponderBase::DvbS: {
		const DvbSTransponder *dvbSTransponder = transponder->getDvbSTransponder();

		params.frequency = dvbSTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.qpsk.symbol_rate = dvbSTransponder->symbolRate;
		params.u.qpsk.fec_inner = convertDvbFecRate(dvbSTransponder->fecRate);
		break;
	    }

	case DvbTransponderBase::DvbS2: {
		const DvbS2Transponder *dvbS2Transponder = transponder->getDvbS2Transponder();

		dtv_property properties[9];
		memset(properties, 0, sizeof(properties));

		properties[0].cmd = DTV_DELIVERY_SYSTEM;
		properties[0].u.data = SYS_DVBS2;
		properties[1].cmd = DTV_FREQUENCY;
		properties[1].u.data = dvbS2Transponder->frequency;
		properties[2].cmd = DTV_SYMBOL_RATE;
		properties[2].u.data = dvbS2Transponder->symbolRate;
		properties[3].cmd = DTV_MODULATION;
		properties[3].u.data = convertDvbModulation(dvbS2Transponder->modulation);
		properties[4].cmd = DTV_ROLLOFF;
		properties[4].u.data = convertDvbRollOff(dvbS2Transponder->rollOff);
		properties[5].cmd = DTV_INVERSION;
		properties[5].u.data = INVERSION_AUTO;
		properties[6].cmd = DTV_PILOT;
		properties[6].u.data = PILOT_AUTO;
		properties[7].cmd = DTV_INNER_FEC;
		properties[7].u.data = convertDvbFecRate(dvbS2Transponder->fecRate);
		properties[8].cmd = DTV_TUNE;

		dtv_properties propertyList;
		memset(&propertyList, 0, sizeof(propertyList));

		propertyList.num = sizeof(properties) / sizeof(properties[0]);
		propertyList.props = properties;

		if (ioctl(frontendFd, FE_SET_PROPERTY, &propertyList) != 0) {
			kWarning() << "ioctl FE_SET_PROPERTY failed for" << frontendPath;
			ok = false;
			return;
		}

		thread->discardBuffers();
		ok = true;
		return;
	    }

	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *dvbTTransponder = transponder->getDvbTTransponder();

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
		break;
	    }

	case DvbTransponderBase::Atsc: {
		const AtscTransponder *atscTransponder = transponder->getAtscTransponder();

		params.frequency = atscTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.vsb.modulation = convertDvbModulation(atscTransponder->modulation);
		break;
	    }

	default:
		kWarning() << "unknown transmission type" << transponder->getTransmissionType();
		ok = false;
		return;
	}

	if (ioctl(frontendFd, FE_SET_FRONTEND, &params) != 0) {
		kWarning() << "ioctl FE_SET_FRONTEND failed for" << frontendPath;
		ok = false;
		return;
	}

	thread->discardBuffers();
	ok = true;
	return;
}

void DvbLinuxDevice::getSignal(int &result) const
{
	Q_ASSERT(frontendFd >= 0);

	quint16 signal = 0;

	if (ioctl(frontendFd, FE_READ_SIGNAL_STRENGTH, &signal) != 0) {
		kWarning() << "ioctl FE_READ_SIGNAL_STRENGTH failed for" << frontendPath;
		result = -1;
		return;
	}

	if (signal == 0) {
		// assume that reading signal strength isn't supported
		result = -1;
		return;
	}

	result = ((signal * 100 + 0x8001) >> 16);
	return;
}

void DvbLinuxDevice::getSnr(int &result) const
{
	Q_ASSERT(frontendFd >= 0);

	quint16 snr = 0;

	if (ioctl(frontendFd, FE_READ_SNR, &snr) != 0) {
		kWarning() << "ioctl FE_READ_SNR failed for" << frontendPath;
		result = -1;
		return;
	}

	if (snr == 0) {
		// assume that reading snr isn't supported
		result = -1;
		return;
	}

	result = ((snr * 100 + 0x8001) >> 16);
	return;
}

void DvbLinuxDevice::isTuned(bool &result) const
{
	Q_ASSERT(frontendFd >= 0);

	fe_status_t status;
	memset(&status, 0, sizeof(status));

	if (ioctl(frontendFd, FE_READ_STATUS, &status) != 0) {
		kWarning() << "ioctl FE_READ_STATUS failed for" << frontendPath;
		result = false;
		return;
	}

	result = ((status & FE_HAS_LOCK) != 0);
	return;
}

void DvbLinuxDevice::addPidFilter(int pid, bool &ok)
{
	Q_ASSERT(frontendFd >= 0);

	if (dmxFds.contains(pid)) {
		kWarning() << "filter already set up for" << pid;
		ok = false;
		return;
	}

	int dmxFd = open(QFile::encodeName(demuxPath), O_RDONLY | O_NONBLOCK);

	if (dmxFd < 0) {
		kWarning() << "cannot open demux" << demuxPath;
		ok = false;
		return;
	}

	dmx_pes_filter_params pes_filter;
	memset(&pes_filter, 0, sizeof(pes_filter));

	pes_filter.pid = pid;
	pes_filter.input = DMX_IN_FRONTEND;
	pes_filter.output = DMX_OUT_TS_TAP;
	pes_filter.pes_type = DMX_PES_OTHER;
	pes_filter.flags = DMX_IMMEDIATE_START;

	if (ioctl(dmxFd, DMX_SET_PES_FILTER, &pes_filter) != 0) {
		kWarning() << "cannot set up filter for demux" << demuxPath;
		close(dmxFd);
		ok = false;
		return;
	}

	dmxFds.insert(pid, dmxFd);
	ok = true;
	return;
}

void DvbLinuxDevice::removePidFilter(int pid)
{
	Q_ASSERT(frontendFd >= 0);

	if (!dmxFds.contains(pid)) {
		kWarning() << "no filter set up for" << pid;
		return;
	}

	close(dmxFds.take(pid));
}

void DvbLinuxDevice::startDescrambling(const DvbPmtSection &pmtSection)
{
	cam.startDescrambling(pmtSection);
}

void DvbLinuxDevice::stopDescrambling(int serviceId)
{
	cam.stopDescrambling(serviceId);
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

bool DvbLinuxDevice::identifyDevice()
{
	int fd = open(QFile::encodeName(frontendPath), O_RDONLY | O_NONBLOCK);

	if (fd < 0) {
		kWarning() << "couldn't open" << frontendPath;
		return false;
	}

	dvb_frontend_info frontend_info;
	memset(&frontend_info, 0, sizeof(frontend_info));

	if (ioctl(fd, FE_GET_INFO, &frontend_info) != 0) {
		kWarning() << "ioctl FE_GET_INFO failed for" << frontendPath;
		close(fd);
		return false;
	}

	close(fd);

	switch (frontend_info.type) {
	case FE_QPSK:
		transmissionTypes = DvbS;

		if (((frontend_info.caps & FE_CAN_2G_MODULATION) != 0) ||
		    (strcmp(frontend_info.name, "Genpix 8psk-to-USB2 DVB-S") == 0) ||
		    (strcmp(frontend_info.name, "Conexant CX24116/CX24118") == 0) ||
		    (strcmp(frontend_info.name, "STB0899 Multistandard") == 0)) {
			transmissionTypes |= DvbS2;
		}

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
		kWarning() << "unknown frontend type" << frontend_info.type << "for" <<
			frontendPath;
		return false;
	}

	capabilities = 0;

	if ((frontend_info.caps & FE_CAN_QAM_AUTO) != 0) {
		capabilities |= DvbTModulationAuto;
	}

	if ((frontend_info.caps & FE_CAN_FEC_AUTO) != 0) {
		capabilities |= DvbTFecAuto;
	}

	if ((frontend_info.caps & FE_CAN_TRANSMISSION_MODE_AUTO) != 0) {
		capabilities |= DvbTTransmissionModeAuto;
	}

	if ((frontend_info.caps & FE_CAN_GUARD_INTERVAL_AUTO) != 0) {
		capabilities |= DvbTGuardIntervalAuto;
	}

	deviceId.clear();
	QString path = QString("/sys/class/dvb/dvb%1.frontend%2/").arg(adapter).arg(index);

	if (QFile::exists(path + "device/vendor")) {
		// PCI device
		int vendor = DvbReadSysAttr(path + "device/vendor");
		int device = DvbReadSysAttr(path + "device/device");
		int subsystem_vendor = DvbReadSysAttr(path + "device/subsystem_vendor");
		int subsystem_device = DvbReadSysAttr(path + "device/subsystem_device");

		if ((vendor >= 0) && (device >= 0) && (subsystem_vendor >= 0) &&
		    (subsystem_device >= 0)) {
			deviceId = 'P';
			deviceId += QString("%1").arg(vendor, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(device, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(subsystem_vendor, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(subsystem_device, 4, 16, QChar('0'));
		}
	} else if (QFile::exists(path + "device/idVendor")) {
		// USB device
		int vendor = DvbReadSysAttr(path + "device/idVendor");
		int product = DvbReadSysAttr(path + "device/idProduct");

		if ((vendor >= 0) && (product >= 0)) {
			deviceId = 'U';
			deviceId += QString("%1").arg(vendor, 4, 16, QChar('0'));
			deviceId += QString("%1").arg(product, 4, 16, QChar('0'));
		}
	}

	if (deviceId.isEmpty()) {
		kWarning() << "couldn't identify device";
	}

	frontendName = QString::fromAscii(frontend_info.name);
	kDebug() << "found dvb device" << deviceId << '/' << frontendName;

	return true;
}

DvbDeviceManager::DvbDeviceManager()
{
	QObject *notifier = Solid::DeviceNotifier::instance();
	connect(notifier, SIGNAL(deviceAdded(QString)), this, SLOT(componentAdded(QString)));
	connect(notifier, SIGNAL(deviceRemoved(QString)), this, SLOT(componentRemoved(QString)));
}

DvbDeviceManager::~DvbDeviceManager()
{
	qDeleteAll(devices);
}

void DvbDeviceManager::doColdPlug()
{
	foreach (const Solid::Device &device,
		 Solid::Device::listFromType(Solid::DeviceInterface::DvbInterface)) {
		componentAdded(device);
	}
}

void DvbDeviceManager::componentAdded(const QString &udi)
{
	componentAdded(Solid::Device(udi));
}

void DvbDeviceManager::componentRemoved(const QString &udi)
{
	DvbLinuxDevice *device = udis.value(udi, NULL);

	if (device != NULL) {
		if (device->componentRemoved(udi)) {
			emit deviceRemoved(device);
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

	DvbLinuxDevice *device = devices.value(deviceIndex, NULL);

	if (device == NULL) {
		device = new DvbLinuxDevice(adapter, index);
		devices.insert(deviceIndex, device);
	}

	udis.insert(component.udi(), device);

	if (device->componentAdded(component)) {
		emit deviceAdded(device);
	}
}
