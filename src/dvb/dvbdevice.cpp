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

#include <KDebug>

#include <Solid/DvbInterface>
using namespace Solid;

#include "dvbdevice.h"

DvbDevice::DvbDevice(int adapter_, int index_) : adapter(adapter_), index(index_), failure(false)
{
}

void DvbDevice::componentAdded(const DvbInterface *component)
{
	if (failure) {
		return;
	}

	QString *targetPath;

	switch (component->deviceType()) {
	case DvbInterface::DvbCa:
		targetPath = &caPath;
		break;
	case DvbInterface::DvbDemux:
		targetPath = &demuxPath;
		break;
	case DvbInterface::DvbDvr:
		targetPath = &dvrPath;
		break;
	case DvbInterface::DvbFrontend:
		targetPath = &frontendPath;
		break;
	default:
		return;
	}

	QString path = component->device();
	if (path.isEmpty()) {
		// FIXME how to get udi?
		kWarning() << k_lineinfo << "invalid path for \n";
		failure = true;
		return;
	}

	if (!targetPath->isEmpty()) {
		// FIXME how to get udi?
		kWarning() << k_lineinfo << "component is present twice for \n";
		failure = true;
		return;
	}

	*targetPath = path;
}

void DvbDevice::componentRemoved(const DvbInterface * /*component*/ )
{
	// FIXME
}
