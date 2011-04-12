/*
 * dvbdevice.h
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include <QExplicitlySharedDataPointer>
#include <QMap>
#include <QMutex>
#include <QTimer>
#include "dvbbackenddevice.h"
#include "dvbtransponder.h"

class DvbConfigBase;
class DvbDataDumper;
class DvbDeviceDataBuffer;
class DvbFilterInternal;
class DvbSectionFilterInternal;

class DvbDummyPidFilter : public DvbPidFilter
{
public:
	DvbDummyPidFilter() { }
	~DvbDummyPidFilter() { }

	void processData(const char [188]) { }
};

class DvbDummySectionFilter : public DvbSectionFilter
{
public:
	DvbDummySectionFilter() { }
	~DvbDummySectionFilter() { }

	void processSection(const char *, int) { }
};

// FIXME make DvbDevice shared ...
class DvbDevice : public QObject, public DvbFrontendDevice
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
		// FIXME introduce a TuningFailed state
	};

	DvbDevice(DvbBackendDevice *backend_, QObject *parent);
	~DvbDevice();

	const DvbBackendDevice *getBackendDevice() const
	{
		return backend;
	}

	DeviceState getDeviceState() const
	{
		return deviceState;
	}

	TransmissionTypes getTransmissionTypes() const;
	QString getDeviceId() const;
	QString getFrontendName() const;

	void tune(const DvbTransponder &transponder);
	void autoTune(const DvbTransponder &transponder);
	bool addPidFilter(int pid, DvbPidFilter *filter);
	bool addSectionFilter(int pid, DvbSectionFilter *filter);
	void removePidFilter(int pid, DvbPidFilter *filter);
	void removeSectionFilter(int pid, DvbSectionFilter *filter);
	void startDescrambling(const QByteArray &pmtSectionData, QObject *user);
	void stopDescrambling(const QByteArray &pmtSectionData, QObject *user);
	bool isTuned() const;
	int getSignal() const; // 0 - 100 [%] or -1 = not supported
	int getSnr() const; // 0 - 100 [%] or -1 = not supported
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

	void processData(const char data[188]);
	DvbDataBuffer getBuffer();
	void writeBuffer(const DvbDataBuffer &dataBuffer);
	void customEvent(QEvent *);

	DvbBackendDevice *backend;
	DeviceState deviceState;
	QExplicitlySharedDataPointer<const DvbConfigBase> config;

	int frontendTimeout;
	QTimer frontendTimer;
	QMap<int, DvbFilterInternal> filters;
	QMap<int, DvbSectionFilterInternal> sectionFilters;
	DvbDummyPidFilter dummyPidFilter;
	DvbDummySectionFilter dummySectionFilter;
	DvbDataDumper *dataDumper;
	bool cleanUpFilters;
	QMultiMap<int, QObject *> descramblingServices;

	bool isAuto;
	DvbTransponder autoTransponder;
	Capabilities capabilities;

	DvbDeviceDataBuffer *unusedBuffersHead;
	DvbDeviceDataBuffer *usedBuffersHead;
	DvbDeviceDataBuffer *usedBuffersTail;
	QMutex dataChannelMutex;
};

#endif /* DVBDEVICE_H */
