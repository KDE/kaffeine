/*
 * dvbdevice.h
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

#ifndef DVBDEVICE_H
#define DVBDEVICE_H

#include <QMap>
#include <QTimer>
#include "dvbbackenddevice.h"
#include "dvbchannel.h"
#include "dvbconfig.h"

class DvbFilterInternal;

class DvbPidFilter
{
public:
	DvbPidFilter() { }
	virtual ~DvbPidFilter() { }

	virtual void processData(const char data[188]) = 0;
};

class DvbDevice : public QObject, private DvbAbstractDeviceBuffer
{
	Q_OBJECT
public:
	enum DeviceState
	{
		DeviceNotReady,
		DeviceIdle,
		DeviceRotorMoving,
		DeviceTuning,
		DeviceTuningFailed,
		DeviceTuned
	};

	DvbDevice(DvbBackendDevice *backendDevice_, QObject *parent);
	DvbDevice(int deviceIndex_, QObject *parent);
	~DvbDevice();

	DvbBackendDevice *getBackendDevice()
	{
		return backendDevice;
	}

	DeviceState getDeviceState() const
	{
		return deviceState;
	}

	DvbBackendDevice::TransmissionTypes getTransmissionTypes() const
	{
		Q_ASSERT(deviceState != DeviceNotReady);
		return backendDevice->getTransmissionTypes();
	}

	QString getDeviceId() const
	{
		Q_ASSERT(deviceState != DeviceNotReady);
		return backendDevice->getDeviceId();
	}

	QString getFrontendName() const
	{
		Q_ASSERT(deviceState != DeviceNotReady);
		return backendDevice->getFrontendName();
	}

	bool acquire()
	{
		return backendDevice->acquire();
	}

	void tune(const DvbTransponder &transponder);
	void autoTune(const DvbTransponder &transponder);
	DvbTransponder getAutoTransponder() const;
	void release();

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

	void enableDvbDump();

	/*
	 * assigned by DvbManager::requestDevice()
	 */

	DvbConfig config;

signals:
	void stateChanged();

private slots:
	void frontendEvent();

private:
	void setDeviceState(DeviceState newState);

	int size();
	char *getCurrent();
	void submitCurrent(int packets);
	void customEvent(QEvent *);

	DvbBackendDevice *backendDevice;
	DeviceState deviceState;

	int frontendTimeout;
	QTimer frontendTimer;
	QMap<int, DvbFilterInternal> filters;
	DvbPidFilter *dummyFilter;
	DvbPidFilter *dataDumper;
	bool cleanUpFilters;

	bool isAuto;
	DvbTTransponder *autoTTransponder;
	DvbTransponder autoTransponder;
	DvbBackendDevice::Capabilities capabilities;

	DvbFilterData *currentUnused;
	DvbFilterData *currentUsed;
	QAtomicInt usedBuffers;
	int totalBuffers;
};

#endif /* DVBDEVICE_H */
