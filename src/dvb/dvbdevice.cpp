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

#include <fcntl.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <QFile>
#include <Solid/DvbInterface>
#include <Solid/Device>
#include <KDebug>

#include "dvbdevice.h"

DvbDevice::DvbDevice(int adapter_, int index_) : adapter(adapter_), index(index_), currentState(0)
{
}

void DvbDevice::componentAdded(const Solid::Device &component)
{
	const Solid::DvbInterface *dvbInterface = component.as<Solid::DvbInterface>();

	QString *targetPath;
	stateFlag targetPresent;
	stateFlag targetInvalid;

	switch (dvbInterface->deviceType()) {
	case Solid::DvbInterface::DvbCa:
		targetPath = &caPath;
		targetPresent = CaPresent;
		targetInvalid = CaInvalid;
		break;
	case Solid::DvbInterface::DvbDemux:
		targetPath = &demuxPath;
		targetPresent = DemuxPresent;
		targetInvalid = DemuxInvalid;
		break;
	case Solid::DvbInterface::DvbDvr:
		targetPath = &dvrPath;
		targetPresent = DvrPresent;
		targetInvalid = DvrInvalid;
		break;
	case Solid::DvbInterface::DvbFrontend:
		targetPath = &frontendPath;
		targetPresent = FrontendPresent;
		targetInvalid = FrontendInvalid;
		break;
	default:
		return;
	}

	QString path = dvbInterface->device();

	if ((currentState & targetPresent) != 0) {
		if (*targetPath != path) {
			kWarning() << k_funcinfo << "different paths for" << component.udi();
			setState(currentState | targetInvalid);
		}
		return;
	}

	*targetPath = path;
	setState(currentState | targetPresent);
}

void DvbDevice::setState(stateFlags newState)
{
	if (currentState == newState) {
		return;
	}

	bool oldComplete = ((currentState & MaskComplete) == ValueComplete);
	bool newComplete = ((newState & MaskComplete) == ValueComplete);

	if (oldComplete != newComplete) {
		if (newComplete) {
			if (identifyDevice()) {
				newState |= DeviceReady;
			}
		} else {
			newState &= ~DeviceReady;
		}
	}

	currentState = newState;
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
