/*
 * dvbdevice_debug.h
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBDEVICE_DEBUG_H
#define DVBDEVICE_DEBUG_H

#include <QObject>
#include "dvbbackenddevice.h"

class DvbDebugDevice : public DvbBackendDevice
{
public:
	DvbDebugDevice();
	~DvbDebugDevice();

	void setBuffer(DvbAbstractDeviceBuffer *buffer);
	QString getDeviceId();
	QString getFrontendName();
	TransmissionTypes getTransmissionTypes();
	Capabilities getCapabilities();
	bool acquire();
	bool setTone(SecTone tone);
	bool setVoltage(SecVoltage voltage);
	bool sendMessage(const char *message, int length);
	bool sendBurst(SecBurst burst);
	bool tune(const DvbTransponder &transponder);
	int getSignal();
	int getSnr();
	bool isTuned();
	bool addPidFilter(int pid);
	void removePidFilter(int pid);
	void startDescrambling(const DvbPmtSection &pmtSection);
	void stopDescrambling(int serviceId);
	void release();

private:
	void execute(Command command, ReturnData returnData, Data data);
};

class DvbDeviceManager : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int backendMagic READ backendMagic)
public:
	DvbDeviceManager();
	~DvbDeviceManager();

	int backendMagic();

public slots:
	void doColdPlug();

signals:
	void deviceAdded(DvbBackendDevice *device);
	void deviceRemoved(DvbBackendDevice *device);

private:
	DvbDebugDevice *device;
};

#endif /* DVBDEVICE_DEBUG_H */
