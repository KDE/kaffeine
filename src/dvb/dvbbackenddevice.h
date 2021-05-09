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
		Nothing = 0,
		DvbC  = (1 << 0),
		DvbS  = (1 << 1),
		DvbS2 = (1 << 2),
		DvbT  = (1 << 3),
		DvbT2 = (1 << 4),
		Atsc  = (1 << 5),
		IsdbT = (1 << 6),
	};

	Q_DECLARE_FLAGS(TransmissionTypes, TransmissionType)

	enum Capability {
		None = 0,
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

// Those definitions are pretty much identical to what's there
// at libdvbv5 dvb-sat.h header. However, due to the abstract
// model, we should re-define it here.

struct lnbFreqRange {
	unsigned int low, high;
};

struct lnbSat {
	QString name, alias;
	unsigned int lowFreq, highFreq, rangeSwitch;
	struct lnbFreqRange freqRange[2];
};

class DvbBackendDevice : public DvbDeviceBase
{
public:
	enum Scale {
		NotSupported = 0,
		Percentage = 1,
		Decibel = 2,
		dBuV = 3,
	};
	virtual QString getDeviceId() = 0;
	virtual QString getFrontendName() = 0;
	virtual TransmissionTypes getTransmissionTypes() = 0;
	virtual Capabilities getCapabilities() = 0;
	virtual void setFrontendDevice(DvbFrontendDevice *frontend) = 0;
	virtual void setDeviceEnabled(bool enabled) = 0;
	virtual bool acquire() = 0;
	virtual bool setHighVoltage(int higherVoltage) = 0;
	virtual bool sendMessage(const char *message, int length) = 0;
	virtual bool sendBurst(SecBurst burst) = 0;
	virtual bool satSetup(QString lnbModel, int satNumber, int bpf) = 0;
	virtual bool tune(const DvbTransponder &transponder) = 0; // discards obsolete data
	virtual bool getProps(DvbTransponder &transponder) = 0;
	virtual bool isTuned() = 0;
	virtual float getSignal(Scale &scale) = 0;
	virtual float getSnr(Scale &scale) = 0;
	virtual float getFrqMHz() = 0;
	virtual bool addPidFilter(int pid) = 0;
	virtual void removePidFilter(int pid) = 0;
	virtual void startDescrambling(const QByteArray &pmtSectionData) = 0;
	virtual void stopDescrambling(int serviceId) = 0;
	virtual void release() = 0;
	virtual void enableDvbDump() = 0;
	QList<lnbSat> getLnbSatModels() const { return lnbSatModels; };


protected:
	DvbBackendDevice() { }
	virtual ~DvbBackendDevice() { }

	QList<lnbSat> lnbSatModels;
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
