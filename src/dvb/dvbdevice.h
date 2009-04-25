/*
 * dvbdevice.h
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
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
#include "dvbchannel.h"
#include "dvbconfig.h"

class DvbDeviceThread;
class DvbManager;
class DvbTransponder;

class DvbPidFilter
{
public:
	DvbPidFilter() { }
	virtual ~DvbPidFilter() { }

	virtual void processData(const char data[188]) = 0;
};

class DvbDevice : public QObject
{
	Q_OBJECT
public:
	enum TransmissionType
	{
		DvbC = (1 << 0),
		DvbS = (1 << 1),
		DvbT = (1 << 2),
		Atsc = (1 << 3)
	};

	Q_DECLARE_FLAGS(TransmissionTypes, TransmissionType)

	enum DeviceState
	{
		DeviceNotReady,
		DeviceIdle,
		DeviceRotorMoving,
		DeviceTuning,
		DeviceTuningFailed,
		DeviceTuned
	};

	DvbDevice(DvbManager *manager_, int deviceIndex_);
	~DvbDevice();

	int getIndex() const
	{
		return deviceIndex;
	}

	void componentAdded(const Solid::Device &component);
	bool componentRemoved(const QString &udi);

	DeviceState getDeviceState() const
	{
		return deviceState;
	}

	TransmissionTypes getTransmissionTypes() const
	{
		Q_ASSERT(deviceState != DeviceNotReady);
		return transmissionTypes;
	}

	QString getFrontendName() const
	{
		Q_ASSERT(deviceState != DeviceNotReady);
		return frontendName;
	}

	QString getDeviceId() const
	{
		Q_ASSERT(deviceState != DeviceNotReady);
		return deviceId;
	}

	bool checkUsable();
	void tune(const DvbTransponder &transponder);
	void autoTune(const DvbTransponder &transponder);
	DvbTransponder getAutoTransponder() const;
	void stop();

	/*
	 * signal and SNR are scaled from 0 to 100
	 * the device has to be tuned / tuning when you call one of these functions
	 */

	int getSignal();
	int getSnr();
	bool isTuned();

	/*
	 * you can use the same filter object for different pids
	 */

	bool addPidFilter(int pid, DvbPidFilter *filter);
	void removePidFilter(int pid, DvbPidFilter *filter);

	/*
	 * assigned by DvbManager::requestDevice()
	 */

	DvbConfig config;

signals:
	void stateChanged();

private slots:
	void frontendEvent();

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

	void setInternalState(stateFlags newState);
	void setDeviceState(DeviceState newState);
	bool identifyDevice();

	DvbManager *manager;

	Solid::Device caComponent;
	Solid::Device demuxComponent;
	Solid::Device dvrComponent;
	Solid::Device frontendComponent;

	QString caPath;
	QString demuxPath;
	QString dvrPath;
	QString frontendPath;

	int deviceIndex;
	stateFlags internalState;
	DeviceState deviceState;
	TransmissionTypes transmissionTypes;
	int frontendCapabilities;
	QString frontendName;
	QString deviceId;

	int frontendFd;
	int frontendTimeout;
	QTimer frontendTimer;

	DvbDeviceThread *thread;

	bool isAuto;
	DvbTTransponder *autoTTransponder;
	DvbTransponder autoTransponder;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DvbDevice::TransmissionTypes)

class DvbDeviceManager : public QObject
{
	Q_OBJECT
public:
	explicit DvbDeviceManager(DvbManager *manager_);
	~DvbDeviceManager();

	QList<DvbDevice *> getDevices() const
	{
		return devices;
	}

signals:
	void deviceAdded(DvbDevice *device);
	void deviceRemoved(DvbDevice *device);

private slots:
	void componentAdded(const QString &udi);
	void componentRemoved(const QString &udi);

private:
	void componentAdded(const Solid::Device &component);

	DvbManager *manager;
	QList<DvbDevice *> devices;
};

#endif /* DVBDEVICE_H */
