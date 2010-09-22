/*
 * dvbdevice.h
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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
#include <QMutex>
#include <QTimer>
#include "dvbbackenddevice.h"
#include "dvbchannel.h"

class DvbConfigBase;
class DvbDeviceDataBuffer;
class DvbFilterInternal;
class DvbPmtSection;

class DvbPidFilter
{
public:
	DvbPidFilter() { }
	virtual ~DvbPidFilter() { }

	virtual void processData(const char data[188]) = 0;
};

class DvbDevice : public QObject, private DvbAbstractDataChannel, public DvbBackendDeviceBase
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

	DvbDevice(QObject *backendDevice, QObject *parent);
	~DvbDevice();

	const QObject *getBackendDevice() const
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
	void removePidFilter(int pid, DvbPidFilter *filter);
	void startDescrambling(const DvbPmtSection &pmtSection, QObject *user);
	void stopDescrambling(int serviceId, QObject *user);
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

signals:
	void backendGetDeviceId(QString &result) const;
	void backendGetFrontendName(QString &result) const;
	void backendGetTransmissionTypes(TransmissionTypes &result) const;
	void backendGetCapabilities(Capabilities &result) const;
	void backendSetDataChannel(DvbAbstractDataChannel *dataChannel);
	void backendSetDeviceEnabled(bool enabled);
	void backendAcquire(bool &ok);
	void backendSetTone(SecTone tone, bool &ok);
	void backendSetVoltage(SecVoltage voltage, bool &ok);
	void backendSendMessage(const char *message, int length, bool &ok);
	void backendSendBurst(SecBurst burst, bool &ok);
	void backendTune(const DvbTransponder &transponder, bool &ok); // discards obsolete data
	void backendIsTuned(bool &result) const;
	void backendGetSignal(int &result) const; // 0 - 100 [%] or -1 = not supported
	void backendGetSnr(int &result) const; // 0 - 100 [%] or -1 = not supported
	void backendAddPidFilter(int pid, bool &ok);
	void backendRemovePidFilter(int pid);
	void backendStartDescrambling(const DvbPmtSection &pmtSection);
	void backendStopDescrambling(int serviceId);
	void backendRelease();

private:
	void setDeviceState(DeviceState newState);
	void discardBuffers();
	void stop();

	DvbDataBuffer getBuffer();
	void writeBuffer(const DvbDataBuffer &dataBuffer);
	void customEvent(QEvent *);

	QObject *backend;
	DeviceState deviceState;
	QExplicitlySharedDataPointer<const DvbConfigBase> config;

	int frontendTimeout;
	QTimer frontendTimer;
	QMap<int, DvbFilterInternal> filters;
	DvbPidFilter *dummyFilter;
	DvbPidFilter *dataDumper;
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
