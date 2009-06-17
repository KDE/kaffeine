/*
 * dvbbackenddevice.h
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

#ifndef DVBBACKENDDEVICE_H
#define DVBBACKENDDEVICE_H

class DvbBackendDevice;
class DvbFilterData;
class DvbFilterInternal;
class DvbManager;
class DvbTransponder;

static const int dvbBackendMagic = 0x58d9ca73;

class DvbAbstractDeviceBuffer
{
public:
	DvbAbstractDeviceBuffer() { }
	virtual ~DvbAbstractDeviceBuffer() { }

	// all those functions must be callable from a QThread

	virtual int size() = 0; // must be a multiple of 188
	virtual char *getCurrent() = 0;
	virtual void submitCurrent(int packets) = 0;
};

class DvbBackendDevice
{
public:
	enum TransmissionType {
		DvbC	= (1 << 0),
		DvbS	= (1 << 1),
		DvbS2	= (1 << 4),
		DvbT	= (1 << 2),
		Atsc	= (1 << 3)
	};

	Q_DECLARE_FLAGS(TransmissionTypes, TransmissionType)

	enum Capability {
		DvbTModulationAuto		= (1 << 0),
		DvbTFecAuto			= (1 << 1),
		DvbTTransmissionModeAuto	= (1 << 2),
		DvbTGuardIntervalAuto		= (1 << 3)
	};

	Q_DECLARE_FLAGS(Capabilities, Capability)

	enum SecTone {
		ToneOff = 0,
		ToneOn  = 1
	};

	enum SecVoltage {
		Voltage13V = 0,
		Voltage18V = 1
	};

	enum SecBurst {
		BurstMiniA = 0,
		BurstMiniB = 1
	};

	DvbBackendDevice() : buffer(NULL) { }
	virtual ~DvbBackendDevice() { }

	virtual QString getDeviceId() = 0;
	virtual QString getFrontendName() = 0;
	virtual TransmissionTypes getTransmissionTypes() = 0;
	virtual Capabilities getCapabilities() = 0;
	virtual bool acquire() = 0;
	virtual bool setTone(SecTone tone) = 0;
	virtual bool setVoltage(SecVoltage voltage) = 0;
	virtual bool sendMessage(const char *message, int length) = 0;
	virtual bool sendBurst(SecBurst burst) = 0;
	virtual bool tune(const DvbTransponder &transponder) = 0;
	virtual int getSignal() = 0;
	virtual int getSnr() = 0;
	virtual bool isTuned() = 0;
	virtual bool addPidFilter(int pid) = 0;
	virtual void removePidFilter(int pid) = 0;
	virtual void release() = 0;

	DvbAbstractDeviceBuffer *buffer;
};

#endif /* DVBBACKENDDEVICE_H */
