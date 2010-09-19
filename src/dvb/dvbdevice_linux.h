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

#include <QThread>
#include <QMap>
#include "dvbbackenddevice.h"
#include "dvbcam_linux.h"

class DvbPmtSection;
class DvbTransponder;

class DvbLinuxDevice : public QThread, public DvbBackendDeviceBase
{
	Q_OBJECT
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

public slots:
	void getDeviceId(QString &result) const;
	void getFrontendName(QString &result) const;
	void getTransmissionTypes(TransmissionTypes &result) const;
	void getCapabilities(Capabilities &result) const;
	void setDataChannel(DvbAbstractDataChannel *dataChannel_);
	void setDeviceEnabled(bool enabled_);
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

private:
	void startDvr();
	void stopDvr();
	void run();

	bool ready;
	QString deviceId;
	QString frontendName;
	TransmissionTypes transmissionTypes;
	Capabilities capabilities;
	DvbAbstractDataChannel *dataChannel;
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
	void deviceAdded(QObject *device);
	void deviceRemoved(QObject *device);

private slots:
	void componentAdded(const QString &udi);
	void componentRemoved(const QString &udi);

private:
	int readSysAttr(const QString &path);

	QMap<int, DvbLinuxDevice *> devices;
	QMap<QString, DvbLinuxDevice *> udis;
};

#endif /* DVBDEVICE_LINUX_H */
