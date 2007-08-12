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
#include <KDebug>

#include "dvbdevice.h"

DvbDevice::DvbDevice(int adapter_, int index_) : adapter(adapter_), index(index_), currentState(0)
{
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
		setState(currentState | CaPresent);
		break;
	case Solid::DvbInterface::DvbDemux:
		demuxComponent = component;
		demuxPath = dvbInterface->device();
		setState(currentState | DemuxPresent);
		break;
	case Solid::DvbInterface::DvbDvr:
		dvrComponent = component;
		dvrPath = dvbInterface->device();
		setState(currentState | DvrPresent);
		break;
	case Solid::DvbInterface::DvbFrontend:
		frontendComponent = component;
		frontendPath = dvbInterface->device();
		setState(currentState | FrontendPresent);
		break;
	default:
		break;
	}
}

bool DvbDevice::componentRemoved(const QString &udi)
{
	if (caComponent.udi() == udi) {
		setState(currentState & (~CaPresent));
		return true;
	} else if (demuxComponent.udi() == udi) {
		setState(currentState & (~DemuxPresent));
		return true;
	} else if (dvrComponent.udi() == udi) {
		setState(currentState & (~DvrPresent));
		return true;
	} else if (frontendComponent.udi() == udi) {
		setState(currentState & (~FrontendPresent));
		return true;
	}

	return false;
}

void DvbDevice::setState(stateFlags newState)
{
	if (currentState == newState) {
		return;
	}

	bool oldPresent = ((currentState & DevicePresent) == DevicePresent);
	bool newPresent = ((newState & DevicePresent) == DevicePresent);

	if (oldPresent != newPresent) {
		if (newPresent) {
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
