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
#include <Solid/DvbInterface>
#include <KDebug>

#include "dvbchannel.h"
#include "dvbdevice.h"

DvbDevice::DvbDevice(int adapter_, int index_) : adapter(adapter_), index(index_), internalState(0),
	deviceState(DeviceNotReady), frontendFd(-1)
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

void DvbDevice::tuneDevice(const DvbTransponder &transponder)
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
		return;

	case DvbS: {
		const DvbSTransponder *dvbSTransponder =
			dynamic_cast<const DvbSTransponder *> (&transponder);

		if (dvbSTransponder == NULL) {
			return;
		}

		// FIXME - everything just a hack

		// diseqc
		ioctl(frontendFd, FE_SET_TONE, SEC_TONE_OFF);
		ioctl(frontendFd, FE_SET_VOLTAGE, SEC_VOLTAGE_18);
		usleep(15000);
		struct dvb_diseqc_master_cmd cmd = { { 0xe0, 0x10, 0x38, 0xf3, 0x00, 0x00 }, 4 };
		ioctl(frontendFd, FE_DISEQC_SEND_MASTER_CMD, &cmd);
		usleep(15000);
		ioctl(frontendFd, FE_DISEQC_SEND_BURST, SEC_MINI_A);
		usleep(15000);
		ioctl(frontendFd, FE_SET_TONE, SEC_TONE_ON);

		// tune
		struct dvb_frontend_parameters params;
		params.frequency = dvbSTransponder->frequency - 10600000;
		params.inversion = INVERSION_AUTO;
		params.u.qpsk.symbol_rate = dvbSTransponder->symbolRate;
		params.u.qpsk.fec_inner = FEC_AUTO;
		ioctl(frontendFd, FE_SET_FRONTEND, &params);

		setDeviceState(DeviceTuning);

		// wait for tuning
		frontendTimer.start(100);
	    }
	case DvbT:
	case Atsc:
		return;
	}
}

void DvbDevice::stopDevice()
{
	if ((deviceState == DeviceNotReady) || (deviceState == DeviceIdle)) {
		return;
	}

	// stop waiting for tuning
	frontendTimer.stop();

	// close frontend
	if (frontendFd >= 0) {
		close(frontendFd);
		frontendFd = -1;
	}

	setDeviceState(DeviceIdle);
}

// FIXME demo hack
void DvbDevice::setupFilter()
{
	int admx_fd = open("/dev/dvb/adapter0/demux0", O_RDWR | O_NONBLOCK);
	int vdmx_fd = open("/dev/dvb/adapter0/demux0", O_RDWR | O_NONBLOCK);

	struct dmx_pes_filter_params filter;
	filter.pid = 110;
	filter.input = DMX_IN_FRONTEND;
	filter.output = DMX_OUT_TS_TAP;
	filter.pes_type = DMX_PES_OTHER;
	filter.flags = DMX_IMMEDIATE_START;
	ioctl(vdmx_fd, DMX_SET_PES_FILTER, &filter);

	filter.pid = 120;
	filter.input = DMX_IN_FRONTEND;
	filter.output = DMX_OUT_TS_TAP;
	filter.pes_type = DMX_PES_OTHER;
	filter.flags = DMX_IMMEDIATE_START;
	ioctl(admx_fd, DMX_SET_PES_FILTER, &filter);
}

void DvbDevice::frontendEvent()
{
	fe_status_t status;
	ioctl(frontendFd, FE_READ_STATUS, &status);
	if ((status & FE_HAS_LOCK) != 0) {
		setDeviceState(DeviceTuned);
		frontendTimer.stop();
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
