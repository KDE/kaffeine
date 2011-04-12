/*
 * dvbdevice_linux.h
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

#ifndef DVBDEVICE_LINUX_H
#define DVBDEVICE_LINUX_H

#include <QThread>
#include "dvbbackenddevice.h"
#include "dvbcam_linux.h"

class DvbLinuxDevice : public QThread, public DvbBackendDevice
{
public:
	explicit DvbLinuxDevice(QObject *parent);
	~DvbLinuxDevice();

	bool isReady() const;
	void startDevice(const QString &deviceId_);
	void startCa();
	void stopCa();
	void stopDevice();

	QString caPath;
	QString caUdi;
	QString demuxPath;
	QString demuxUdi;
	QString dvrPath;
	QString dvrUdi;
	QString frontendPath;
	QString frontendUdi;

protected:
	QString getDeviceId();
	QString getFrontendName();
	TransmissionTypes getTransmissionTypes();
	Capabilities getCapabilities();
	void setFrontendDevice(DvbFrontendDevice *frontend_);
	void setDeviceEnabled(bool enabled_);
	bool acquire();
	bool setTone(SecTone tone);
	bool setVoltage(SecVoltage voltage);
	bool sendMessage(const char *message, int length);
	bool sendBurst(SecBurst burst);
	bool tune(const DvbTransponder &transponder); // discards obsolete data
	bool isTuned();
	int getSignal(); // 0 - 100 [%] or -1 = not supported
	int getSnr(); // 0 - 100 [%] or -1 = not supported
	bool addPidFilter(int pid);
	void removePidFilter(int pid);
	void startDescrambling(const QByteArray &pmtSectionData);
	void stopDescrambling(int serviceId);
	void release();

private:
	void startDvr();
	void stopDvr();
	void run();

	bool ready;
	QString deviceId;
	QString frontendName;
	TransmissionTypes transmissionTypes;
	Capabilities capabilities;
	DvbFrontendDevice *frontend;
	bool enabled;
	int frontendFd;
	QMap<int, int> dmxFds;

	int dvrFd;
	int dvrPipe[2];
	DvbDataBuffer dvrBuffer;

	DvbLinuxCam cam;
};

class DvbLinuxDeviceManager : public QObject
{
	Q_OBJECT
public:
	explicit DvbLinuxDeviceManager(QObject *parent);
	~DvbLinuxDeviceManager();

public slots:
	void doColdPlug();

signals:
	void requestBuiltinDeviceManager(QObject *&bultinDeviceManager);
	void deviceAdded(DvbBackendDevice *device);
	void deviceRemoved(DvbBackendDevice *device);

private slots:
	void componentAdded(const QString &udi);
	void componentRemoved(const QString &udi);

private:
	int readSysAttr(const QString &path);

	QMap<int, DvbLinuxDevice *> devices;
	QMap<QString, DvbLinuxDevice *> udis;
};

#endif /* DVBDEVICE_LINUX_H */
