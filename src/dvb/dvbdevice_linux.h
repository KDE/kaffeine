/*
 * dvbdevice_linux.h
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

#ifndef DVBDEVICE_LINUX_H
#define DVBDEVICE_LINUX_H

#include <Solid/Device>
#include "dvbbackenddevice.h"
#include "dvbcam_linux.h"

class DvbDeviceThread;

class DvbLinuxDevice : public DvbBackendDevice
{
public:
	DvbLinuxDevice(int adapter_, int index_);
	~DvbLinuxDevice();

	bool componentAdded(const Solid::Device &component);
	bool componentRemoved(const QString &udi);

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
	int getSignal(); // 0 - 100 ; -1 = unsupported
	int getSnr(); // 0 - 100 ; -1 = unsupported
	bool isTuned();
	bool addPidFilter(int pid);
	void removePidFilter(int pid);
	void startDescrambling(const DvbPmtSection &pmtSection);
	void stopDescrambling(int serviceId);
	void release();

	int adapter;
	int index;

private:
	Q_DISABLE_COPY(DvbLinuxDevice)

	enum stateFlag {
		CaPresent	= (1 << 0),
		DemuxPresent	= (1 << 1),
		DvrPresent	= (1 << 2),
		FrontendPresent	= (1 << 3),

		DevicePresent	= (DemuxPresent | DvrPresent | FrontendPresent)
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	void execute(Command command, ReturnData returnData, Data data);
	bool identifyDevice();

	Solid::Device caComponent;
	Solid::Device demuxComponent;
	Solid::Device dvrComponent;
	Solid::Device frontendComponent;

	QString caPath;
	QString demuxPath;
	QString dvrPath;
	QString frontendPath;

	DvbAbstractDeviceBuffer *buffer;
	stateFlags internalState;
	TransmissionTypes transmissionTypes;
	Capabilities capabilities;
	QString deviceId;
	QString frontendName;
	bool ready;
	DvbDeviceThread *thread;
	int frontendFd;
	int dvrFd;
	QMap<int, int> dmxFds;
	DvbLinuxCam cam;
};

class DvbDeviceManager : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int backendMagic READ backendMagic)
public:
	DvbDeviceManager();
	~DvbDeviceManager();

	int backendMagic()
	{
		return dvbBackendMagic;
	}

public slots:
	void doColdPlug();

signals:
	void deviceAdded(DvbBackendDevice *device);
	void deviceRemoved(DvbBackendDevice *device);

private slots:
	void componentAdded(const QString &udi);
	void componentRemoved(const QString &udi);

private:
	void componentAdded(const Solid::Device &component);

	QMap<int, DvbLinuxDevice *> devices;
	QMap<QString, DvbLinuxDevice *> udis;
};

#endif /* DVBDEVICE_LINUX_H */
