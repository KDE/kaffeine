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

#include <QTimer>
#include <Solid/Device>

class QSocketNotifier;
class DvbConfig;
class DvbFilterInternal;
class DvbTransponder;

class DvbFilter
{
public:
	DvbFilter() { }
	virtual ~DvbFilter() { }

	virtual void processData(const QByteArray &data) = 0;
};

class DvbDevice : public QObject
{
	Q_OBJECT
public:
	enum TransmissionType
	{
		DvbC	= (1 << 0),
		DvbS	= (1 << 1),
		DvbT	= (1 << 2),
		Atsc	= (1 << 3)
	};

	Q_DECLARE_FLAGS(TransmissionTypes, TransmissionType)

	enum DeviceState
	{
		DeviceNotReady,
		DeviceIdle,
		DeviceRotorMoving,
		DeviceTuning,
		DeviceTuned
	};

	DvbDevice(int adapter_, int index_);
	~DvbDevice();

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

	DeviceState getDeviceState()
	{
		return deviceState;
	}

	TransmissionTypes getTransmissionTypes()
	{
		return transmissionTypes;
	}

	QString getFrontendName()
	{
		return frontendName;
	}

	void tuneDevice(const DvbTransponder &transponder, const DvbConfig &config);
	void stopDevice();

	/*
	 * you can use the same filter object for different pids
	 * filtering will be stopped when the device becomes idle
	 */

	void addPidFilter(int pid, DvbFilter *filter);

signals:
	void stateChanged();

private slots:
	void frontendEvent();
	void dvrEvent();

private:
	Q_DISABLE_COPY(DvbDevice)

	enum stateFlag {
		CaPresent	= (1 << 0),
		DemuxPresent	= (1 << 1),
		DvrPresent	= (1 << 2),
		FrontendPresent	= (1 << 3),

		DevicePresent	= (DemuxPresent | DvrPresent | FrontendPresent)
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	void setState(stateFlags newState);

	void setDeviceState(DeviceState newState)
	{
		deviceState = newState;
		emit stateChanged();
	}

	bool identifyDevice();

	int adapter;
	int index;

	stateFlags internalState;

	Solid::Device caComponent;
	Solid::Device demuxComponent;
	Solid::Device dvrComponent;
	Solid::Device frontendComponent;

	QString caPath;
	QString demuxPath;
	QString dvrPath;
	QString frontendPath;

	DeviceState deviceState;
	TransmissionTypes transmissionTypes;
	QString frontendName;

	int frontendFd;
	int frontendTimeout;
	QTimer frontendTimer;

	QList<DvbFilterInternal *> internalFilters;

	int dvrFd;
	QSocketNotifier *dvrNotifier;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DvbDevice::TransmissionTypes)

#endif /* DVBDEVICE_H */
