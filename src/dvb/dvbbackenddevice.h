/*
 * dvbbackenddevice.h
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

#ifndef DVBBACKENDDEVICE_H
#define DVBBACKENDDEVICE_H

#include <QtGlobal>

class DvbDataBuffer
{
public:
	DvbDataBuffer(char *data_, int bufferSize_) : data(data_), bufferSize(bufferSize_) { }
	~DvbDataBuffer() { }

	char *data;
	int dataSize; // must be a multiple of 188
	int bufferSize; // must be a multiple of 188
};

class DvbAbstractDataChannel
{
public:
	// these functions are thread-safe
	virtual DvbDataBuffer getBuffer() = 0;
	virtual void writeBuffer(const DvbDataBuffer &dataBuffer) = 0;

protected:
	DvbAbstractDataChannel() { }
	~DvbAbstractDataChannel() { }
};

class DvbBackendDeviceBase
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
};

/*

class DvbXXXDevice : public QObject, public DvbBackendDeviceBase
{
	Q_OBJECT
public:
	DvbXXXDevice();
	~DvbXXXDevice();

public slots:
	void getDeviceId(QString &result) const;
	void getFrontendName(QString &result) const;
	void getTransmissionTypes(TransmissionTypes &result) const;
	void getCapabilities(Capabilities &result) const;
	void setDataChannel(DvbAbstractDataChannel *dataChannel);
	void setDeviceEnabled(bool enabled);
	void acquire(bool &ok);
	void setTone(SecTone tone, bool &ok);
	void setVoltage(SecVoltage voltage, bool &ok);
	void sendMessage(const char *message, int length, bool &ok);
	void sendBurst(SecBurst burst, bool &ok);
	void tune(const DvbTransponder &transponder, bool &ok); // discards obsolete data
	void isTuned(bool &result) const;
	void getSignal(int &result) const; // 0 - 100 [%] or -1 = not supported
	void getSnr(int &result) const; // 0 - 100 [%] or -1 = not supported
	void addPidFilter(int pid, bool &ok);
	void removePidFilter(int pid);
	void startDescrambling(const DvbPmtSection &pmtSection);
	void stopDescrambling(int serviceId);
	void release();
};

class DvbXXXDeviceManager : public QObject
{
	Q_OBJECT
public:
	DvbXXXDeviceManager();
	~DvbXXXDeviceManager();

public slots:
	void doColdPlug();

signals:
	void requestBuiltinDeviceManager(QObject *&bultinDeviceManager);
	void deviceAdded(QObject *device);
	void deviceRemoved(QObject *device);
};

*/

#endif /* DVBBACKENDDEVICE_H */
