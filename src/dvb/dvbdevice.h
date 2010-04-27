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

class DvbConfigBase;
class DvbFilterData;
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
		DeviceReleased,
		DeviceIdle,
		DeviceRotorMoving,
		DeviceTuning,
		DeviceTuned
	};

	DvbDevice(DvbBackendDevice *backendDevice, QObject *parent);
	~DvbDevice();

	const DvbBackendDevice *getBackendDevice() const
	{
		return backend.device;
	}

	DeviceState getDeviceState() const
	{
		return deviceState;
	}

	DvbBackendDevice::TransmissionTypes getTransmissionTypes();
	QString getDeviceId();
	QString getFrontendName();

	void tune(const DvbTransponder &transponder);
	void autoTune(const DvbTransponder &transponder);
	bool addPidFilter(int pid, DvbPidFilter *filter);
	void removePidFilter(int pid, DvbPidFilter *filter);
	void startDescrambling(const DvbPmtSection &pmtSection);
	void stopDescrambling(int serviceId);
	bool isTuned();
	int getSignal(); // 0 - 100 [%]
	int getSnr(); // 0 - 100 [%]
	DvbTransponder getAutoTransponder() const;

	/*
	 * management functions (must be only called by DvbManager)
	 */

	bool acquire(const DvbConfigBase *config_);
	void reacquire(const DvbConfigBase *config_);
	void release();
	void enableDvbDump();

signals:
	void stateChanged();

private slots:
	void frontendEvent();

private:
	void setDeviceState(DeviceState newState);
	void discardBuffers();
	void stop();

	int size();
	char *getCurrent();
	void submitCurrent(int packets);
	void customEvent(QEvent *);

	DvbBackendDeviceAdapter backend;
	DeviceState deviceState;
	QExplicitlySharedDataPointer<const DvbConfigBase> config;

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
	int totalBuffers;
	QAtomicInt usedBuffers;
};

#endif /* DVBDEVICE_H */
