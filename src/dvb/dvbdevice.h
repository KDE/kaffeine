/*
 * dvbdevice.h
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

#ifndef DVBDEVICE_H
#define DVBDEVICE_H

#include <QString>
#include <Solid/Device>

class DvbDevice
{
public:
	enum TransmissionType
	{
		DvbC	= (1 << 0),
		DvbS	= (1 << 1),
		DvbT	= (1 << 2),
		Atsc	= (1 << 3)
	};

	Q_DECLARE_FLAGS(TransmissionTypes, TransmissionType)

	DvbDevice(int adapter_, int index_);
	~DvbDevice() { }

	int getAdapter() const
	{
		return adapter;
	}

	int getIndex() const
	{
		return index;
	}

	void componentAdded(const Solid::Device &component);
	bool componentRemoved(const QString &udi);

	bool isReady()
	{
		return ((currentState & DeviceReady) != 0);
	}

	TransmissionTypes getTransmissionTypes()
	{
		return transmissionTypes;
	}

	QString getFrontendName()
	{
		return frontendName;
	}

private:
	Q_DISABLE_COPY(DvbDevice)

	enum stateFlag {
		CaPresent	= (1 << 0),
		DemuxPresent	= (1 << 1),
		DvrPresent	= (1 << 2),
		FrontendPresent	= (1 << 3),

		DevicePresent	= (DemuxPresent | DvrPresent | FrontendPresent),

		DeviceReady	= (1 << 4)
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	void setState(stateFlags newState);

	bool identifyDevice();

	int adapter;
	int index;

	stateFlags currentState;

	Solid::Device caComponent;
	Solid::Device demuxComponent;
	Solid::Device dvrComponent;
	Solid::Device frontendComponent;

	QString caPath;
	QString demuxPath;
	QString dvrPath;
	QString frontendPath;

	TransmissionTypes transmissionTypes;
	QString frontendName;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DvbDevice::TransmissionTypes)

#endif /* DVBDEVICE_H */
