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
class DvbTransponder;

class DvbLinuxDevice : public QObject, public DvbBackendDeviceBase
{
	Q_OBJECT
public:
	DvbLinuxDevice(int adapter_, int index_);
	~DvbLinuxDevice();

	bool componentAdded(const Solid::Device &component);
	bool componentRemoved(const QString &udi);

	int adapter;
	int index;

public slots:
	void setDataChannel(DvbAbstractDataChannel *dataChannel_);
	void getDeviceId(QString &result) const;
	void getFrontendName(QString &result) const;
	void getTransmissionTypes(TransmissionTypes &result) const;
	void getCapabilities(Capabilities &result) const;
	void acquire(bool &ok);
	void setTone(SecTone tone, bool &ok);
	void setVoltage(SecVoltage voltage, bool &ok);
	void sendMessage(const char *message, int length, bool &ok);
	void sendBurst(SecBurst burst, bool &ok);
	void tune(const DvbTransponder &transponder, bool &ok);
	void getSignal(int &result) const; // 0 - 100 ; -1 = unsupported
	void getSnr(int &result) const; // 0 - 100 ; -1 = unsupported
	void isTuned(bool &result) const;
	void addPidFilter(int pid, bool &ok);
	void removePidFilter(int pid);
	void startDescrambling(const DvbPmtSection &pmtSection);
	void stopDescrambling(int serviceId);
	void release();

private:
	enum stateFlag {
		CaPresent	= (1 << 0),
		DemuxPresent	= (1 << 1),
		DvrPresent	= (1 << 2),
		FrontendPresent	= (1 << 3),

		DevicePresent	= (DemuxPresent | DvrPresent | FrontendPresent)
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	bool identifyDevice();

	Solid::Device caComponent;
	Solid::Device demuxComponent;
	Solid::Device dvrComponent;
	Solid::Device frontendComponent;

	QString caPath;
	QString demuxPath;
	QString dvrPath;
	QString frontendPath;

	DvbAbstractDataChannel *dataChannel;
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
public:
	DvbDeviceManager();
	~DvbDeviceManager();

public slots:
	void doColdPlug();

signals:
	void requestBuiltinDeviceManager(QObject *&bultinDeviceManager);
	void deviceAdded(QObject *device);
	void deviceRemoved(QObject *device);

private slots:
	void componentAdded(const QString &udi);
	void componentRemoved(const QString &udi);

private:
	void componentAdded(const Solid::Device &component);

	QMap<int, DvbLinuxDevice *> devices;
	QMap<QString, DvbLinuxDevice *> udis;
};

#endif /* DVBDEVICE_LINUX_H */
