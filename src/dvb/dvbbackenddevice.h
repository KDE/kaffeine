/*
 * dvbbackenddevice.h
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

#ifndef DVBBACKENDDEVICE_H
#define DVBBACKENDDEVICE_H

#include <QtGlobal>

class DvbTransponder;

class DvbDeviceBase
{
public:
	enum TransmissionType {
		DvbC = (1 << 0),
		DvbS = (1 << 1),
		DvbS2 = (1 << 4),
		DvbT = (1 << 2),
		Atsc = (1 << 3)
	};

	Q_DECLARE_FLAGS(TransmissionTypes, TransmissionType)

	enum Capability {
		DvbTModulationAuto = (1 << 0),
		DvbTFecAuto = (1 << 1),
		DvbTTransmissionModeAuto = (1 << 2),
		DvbTGuardIntervalAuto = (1 << 3)
	};

	Q_DECLARE_FLAGS(Capabilities, Capability)

	enum SecTone {
		ToneOff = 0,
		ToneOn = 1
	};

	enum SecVoltage {
		Voltage13V = 0,
		Voltage18V = 1
	};

	enum SecBurst {
		BurstMiniA = 0,
		BurstMiniB = 1
	};

protected:
	DvbDeviceBase() { }
	virtual ~DvbDeviceBase() { }
};

class DvbDataBuffer
{
public:
	DvbDataBuffer(char *data_, int bufferSize_) : data(data_), bufferSize(bufferSize_) { }
	~DvbDataBuffer() { }

	char *data;
	int dataSize; // must be a multiple of 188
	int bufferSize; // must be a multiple of 188
};

class DvbPidFilter
{
public:
	virtual void processData(const char data[188]) = 0;

protected:
	DvbPidFilter() { }
	virtual ~DvbPidFilter() { }
};

class DvbSectionFilter
{
public:
	// the crc is either valid or has appeared at least twice
	virtual void processSection(const char *data, int size) = 0;

protected:
	DvbSectionFilter() { }
	virtual ~DvbSectionFilter() { }
};

class DvbFrontendDevice : public DvbDeviceBase
{
public:
	virtual bool addPidFilter(int pid, DvbPidFilter *filter) = 0;
	virtual bool addSectionFilter(int pid, DvbSectionFilter *filter) = 0;
	virtual void removePidFilter(int pid, DvbPidFilter *filter) = 0;
	virtual void removeSectionFilter(int pid, DvbSectionFilter *filter) = 0;

	// these two functions are thread-safe
	virtual DvbDataBuffer getBuffer() = 0;
	virtual void writeBuffer(const DvbDataBuffer &dataBuffer) = 0;

protected:
	DvbFrontendDevice() { }
	virtual ~DvbFrontendDevice() { }
};

class DvbBackendDevice : public DvbDeviceBase
{
public:
	virtual QString getDeviceId() = 0;
	virtual QString getFrontendName() = 0;
	virtual TransmissionTypes getTransmissionTypes() = 0;
	virtual Capabilities getCapabilities() = 0;
	virtual void setFrontendDevice(DvbFrontendDevice *frontend) = 0;
	virtual void setDeviceEnabled(bool enabled) = 0;
	virtual bool acquire() = 0;
	virtual bool setTone(SecTone tone) = 0;
	virtual bool setVoltage(SecVoltage voltage) = 0;
	virtual bool sendMessage(const char *message, int length) = 0;
	virtual bool sendBurst(SecBurst burst) = 0;
	virtual bool tune(const DvbTransponder &transponder) = 0; // discards obsolete data
	virtual bool isTuned() = 0;
	virtual int getSignal() = 0; // 0 - 100 [%] or -1 = not supported
	virtual int getSnr() = 0; // 0 - 100 [%] or -1 = not supported
	virtual bool addPidFilter(int pid) = 0;
	virtual void removePidFilter(int pid) = 0;
	virtual void startDescrambling(const QByteArray &pmtSectionData) = 0;
	virtual void stopDescrambling(int serviceId) = 0;
	virtual void release() = 0;

protected:
	DvbBackendDevice() { }
	virtual ~DvbBackendDevice() { }
};

/*

class DvbXXXDeviceManager : public QObject
{
	Q_OBJECT
public:
	DvbXXXDeviceManager();
	~DvbXXXDeviceManager();

public slots:
	void doColdPlug();

signals:
	void requestBuiltinDeviceManager(QObject *&builtinDeviceManager);
	void deviceAdded(DvbBackendDevice *device);
	void deviceRemoved(DvbBackendDevice *device);
};

*/

#endif /* DVBBACKENDDEVICE_H */
