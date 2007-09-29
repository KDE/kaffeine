/*
 * dvbdevice.cpp
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * workaround buggy kernel includes
 * asm/types.h doesn't define __u64 in ansi mode, but linux/dvb/dmx.h needs it
 */
#include <QtGlobal>
typedef quint64 __u64;

#include <fcntl.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <QFile>
#include <QSocketNotifier>
#include <Solid/DvbInterface>
#include <KDebug>

#include "dvbchannel.h"
#include "dvbconfig.h"
#include "dvbdevice.h"

class DvbFilterInternal
{
public:
	DvbFilterInternal(int dmxFd_, int pid_) : dmxFd(dmxFd_), pid(pid_) { }

	~DvbFilterInternal()
	{
		close(dmxFd);
	}

	int getPid() const
	{
		return pid;
	}

	void addFilter(DvbFilter *filter)
	{
		filters.append(filter);
	}

	void processData(const QByteArray &data)
	{
		foreach (DvbFilter *filter, filters) {
			filter->processData(data);
		}
	}

private:
	int dmxFd;
	int pid;
	QList<DvbFilter *> filters;
};

DvbDevice::DvbDevice(int adapter_, int index_) : adapter(adapter_), index(index_), internalState(0),
	deviceState(DeviceNotReady), frontendFd(-1), dvrFd(-1), dvrNotifier(NULL)
{
	connect(&frontendTimer, SIGNAL(timeout()), this, SLOT(frontendEvent()));
}

DvbDevice::~DvbDevice()
{
	stopDevice();
}

void DvbDevice::componentAdded(const Solid::Device &component)
{
	const Solid::DvbInterface *dvbInterface = component.as<Solid::DvbInterface>();

	if (dvbInterface == NULL) {
		return;
	}

	switch (dvbInterface->deviceType()) {
	case Solid::DvbInterface::DvbCa:
		caComponent = component;
		caPath = dvbInterface->device();
		setState(internalState | CaPresent);
		break;
	case Solid::DvbInterface::DvbDemux:
		demuxComponent = component;
		demuxPath = dvbInterface->device();
		setState(internalState | DemuxPresent);
		break;
	case Solid::DvbInterface::DvbDvr:
		dvrComponent = component;
		dvrPath = dvbInterface->device();
		setState(internalState | DvrPresent);
		break;
	case Solid::DvbInterface::DvbFrontend:
		frontendComponent = component;
		frontendPath = dvbInterface->device();
		setState(internalState | FrontendPresent);
		break;
	default:
		break;
	}
}

bool DvbDevice::componentRemoved(const QString &udi)
{
	if (caComponent.udi() == udi) {
		setState(internalState & (~CaPresent));
		return true;
	} else if (demuxComponent.udi() == udi) {
		setState(internalState & (~DemuxPresent));
		return true;
	} else if (dvrComponent.udi() == udi) {
		setState(internalState & (~DvrPresent));
		return true;
	} else if (frontendComponent.udi() == udi) {
		setState(internalState & (~FrontendPresent));
		return true;
	}

	return false;
}

void DvbDevice::tuneDevice(const DvbTransponder &transponder, const DvbConfig &config)
{
	if (deviceState == DeviceNotReady) {
		return;
	}

	if (deviceState != DeviceIdle) {
		// retuning
		stopDevice();
	}

	frontendFd = open(QFile::encodeName(frontendPath), O_RDWR | O_NONBLOCK);

	if (frontendFd < 0) {
		kWarning() << k_funcinfo << "couldn't open" << frontendPath;
		return;
	}

	switch (transponder.transmissionType) {
	case DvbC:
		// FIXME
		break;

	case DvbS: {
		const DvbSTransponder *dvbSTransponder =
			dynamic_cast<const DvbSTransponder *>(&transponder);
		const DvbSConfig *dvbSConfig = dynamic_cast<const DvbSConfig *>(&config);

		Q_ASSERT(dvbSTransponder != NULL);
		Q_ASSERT(dvbSConfig != NULL);

		// parameters

		QPair<int, bool> lnbParameters = dvbSConfig->getParameters(dvbSTransponder);

		int  switchPos = dvbSConfig->getSwitchPos();
		bool horPolar  = (dvbSTransponder->polarization == DvbSTransponder::Horizontal);
		bool highBand  = lnbParameters.second;
		int  intFreq   = lnbParameters.first;

		// tone off

		if (ioctl(frontendFd, FE_SET_TONE, SEC_TONE_OFF) != 0) {
			kWarning() << k_funcinfo << "ioctl FE_SET_TONE failed for" << frontendPath;
		}

		// horizontal --> 18V ; vertical --> 13V

		if (ioctl(frontendFd, FE_SET_VOLTAGE, horPolar ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13) != 0) {
			kWarning() << k_funcinfo << "ioctl FE_SET_VOLTAGE failed for" << frontendPath;
		}

		// diseqc

		usleep(15000);

		struct dvb_diseqc_master_cmd cmd = { { 0xe0, 0x10, 0x38, 0x00, 0x00, 0x00 }, 4 };

		cmd.msg[3] = 0xf0 | (switchPos << 2) | (horPolar ? 2 : 0) | (highBand ? 1 : 0);

		if (ioctl(frontendFd, FE_DISEQC_SEND_MASTER_CMD, &cmd) != 0) {
			kWarning() << k_funcinfo << "ioctl FE_DISEQC_SEND_MASTER_CMD failed for" << frontendPath;
		}

		usleep(15000);

		if (ioctl(frontendFd, FE_DISEQC_SEND_BURST, (switchPos % 2) ? SEC_MINI_B : SEC_MINI_A) != 0) {
			kWarning() << k_funcinfo << "ioctl FE_DISEQC_SEND_BURST failed for" << frontendPath;
		}

		usleep(15000);

		// low band --> tone off ; high band --> tone on

		if (ioctl(frontendFd, FE_SET_TONE, highBand ? SEC_TONE_ON : SEC_TONE_OFF) != 0) {
			kWarning() << k_funcinfo << "ioctl FE_SET_TONE failed for" << frontendPath;
		}

		// tune

		struct dvb_frontend_parameters params;
		memset(&params, 0, sizeof(params));

		params.frequency = intFreq;
		params.inversion = INVERSION_AUTO;
		params.u.qpsk.symbol_rate = dvbSTransponder->symbolRate;
		params.u.qpsk.fec_inner = FEC_AUTO;

		if (ioctl(frontendFd, FE_SET_FRONTEND, &params) != 0) {
			kWarning() << k_funcinfo << "ioctl FE_SET_FRONTEND failed for" << frontendPath;
			stopDevice();
			break;
		}

		setDeviceState(DeviceTuning);

		// wait for tuning
		frontendTimeout = dvbSConfig->getTimeout();
		frontendTimer.start(100);

		break;
	    }
	case DvbT:
		// FIXME
		break;
	case Atsc:
		// FIXME
		break;
	}
}

void DvbDevice::stopDevice()
{
	if ((deviceState == DeviceNotReady) || (deviceState == DeviceIdle)) {
		return;
	}

	// stop waiting for tuning
	frontendTimer.stop();

	// stop all filters
	qDeleteAll(internalFilters);
	internalFilters.clear();

	// remove socket notifier
	if (dvrNotifier != NULL) {
		delete dvrNotifier;
		dvrNotifier = NULL;
	}

	// close dvr
	if (dvrFd >= 0) {
		close(dvrFd);
		dvrFd = -1;
	}

	// close frontend
	if (frontendFd >= 0) {
		close(frontendFd);
		frontendFd = -1;
	}

	setDeviceState(DeviceIdle);
}

void DvbDevice::addPidFilter(int pid, DvbFilter *filter)
{
	foreach (DvbFilterInternal *internalFilter, internalFilters) {
		if (internalFilter->getPid() == pid) {
			internalFilter->addFilter(filter);
			return;
		}
	}

	if (dvrFd < 0) {
		dvrFd = open(QFile::encodeName(dvrPath), O_RDONLY | O_NONBLOCK);
		if (dvrFd < 0) {
			kWarning() << k_funcinfo << "couldn't open" << dvrPath;
			return;
		}
	}

	if (dvrNotifier == NULL) {
		dvrNotifier = new QSocketNotifier(dvrFd, QSocketNotifier::Read);
		connect(dvrNotifier, SIGNAL(activated(int)), this, SLOT(dvrEvent()));
	}

	int dmxFd = open(QFile::encodeName(demuxPath), O_RDONLY | O_NONBLOCK);

	if (dmxFd < 0) {
		kWarning() << k_funcinfo << "couldn't open" << demuxPath;
		return;
	}

	struct dmx_pes_filter_params pes_filter;
	memset(&pes_filter, 0, sizeof(pes_filter));
	pes_filter.pid = pid;
	pes_filter.input = DMX_IN_FRONTEND;
	pes_filter.output = DMX_OUT_TS_TAP;
	pes_filter.pes_type = DMX_PES_OTHER;
	pes_filter.flags = DMX_IMMEDIATE_START;

	if (ioctl(dmxFd, DMX_SET_PES_FILTER, &pes_filter) != 0) {
		close(dmxFd);
		kWarning() << k_funcinfo << "couldn't set up filter for" << demuxPath;
		return;
	}

	DvbFilterInternal *internalFilter = new DvbFilterInternal(dmxFd, pid);
	internalFilter->addFilter(filter);
	internalFilters.append(internalFilter);
}

void DvbDevice::frontendEvent()
{
	fe_status_t status;
	memset(&status, 0, sizeof(status));

	if (ioctl(frontendFd, FE_READ_STATUS, &status) != 0) {
		kWarning() << k_funcinfo << "ioctl FE_READ_STATUS failed for" << frontendPath;
	} else {
		if ((status & FE_HAS_LOCK) != 0) {
			kDebug() << k_funcinfo << "tuning succeeded for" << frontendPath;
			setDeviceState(DeviceTuned);
			frontendTimer.stop();
		}
	}

	frontendTimeout -= 100;

	if (frontendTimeout <= 0) {
		kDebug() << k_funcinfo << "tuning failed for" << frontendPath;
		stopDevice();
	}
}

void DvbDevice::dvrEvent()
{
	QByteArray data;
	data.resize(188);

	while(1) {
		if (read(dvrFd, data.data(), 188) != 188) {
			break;
		}

		int pid = ((data[1] << 8) + data[2]) & 0x1fff;

		foreach (DvbFilterInternal *internalFilter, internalFilters) {
			if (internalFilter->getPid() == pid) {
				internalFilter->processData(data);
				break;
			}
		}
	}
}

void DvbDevice::setState(stateFlags newState)
{
	if (internalState == newState) {
		return;
	}

	bool oldPresent = ((internalState & DevicePresent) == DevicePresent);
	bool newPresent = ((newState & DevicePresent) == DevicePresent);

	internalState = newState;

	// have this block after the statement above so that setState could be called recursively

	if (oldPresent != newPresent) {
		if (newPresent) {
			if (identifyDevice()) {
				setDeviceState(DeviceIdle);
			}
		} else {
			stopDevice();
			setDeviceState(DeviceNotReady);
		}
	}
}

bool DvbDevice::identifyDevice()
{
	int fd = open(QFile::encodeName(frontendPath), O_RDONLY | O_NONBLOCK);

	if (fd < 0) {
		kWarning() << k_funcinfo << "couldn't open" << frontendPath;
		return false;
	}

	dvb_frontend_info frontend_info;

	if (ioctl(fd, FE_GET_INFO, &frontend_info) != 0) {
		close(fd);
		kWarning() << k_funcinfo << "couldn't execute FE_GET_INFO ioctl on" << frontendPath;
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
		kWarning() << k_funcinfo << "unknown frontend type" << frontend_info.type << "for"
			<< frontendPath;
		return false;
	}

	frontendName = QString::fromAscii(frontend_info.name);

	kDebug() << k_funcinfo << "found dvb card" << frontendName;

	return true;
}
