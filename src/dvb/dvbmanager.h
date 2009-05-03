/*
 * dvbmanager.h
 *
 * Copyright (C) 2008-2009 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBMANAGER_H
#define DVBMANAGER_H

#include <QDate>
#include <QMap>
#include <QPair>
#include <QStringList>
#include "dvbchannel.h"

class DvbChannelModel;
class DvbConfig;
class DvbDevice;
class DvbDeviceConfig;
class DvbDeviceManager;
class DvbEpgModel;
class DvbRecordingModel;
class DvbScanData;
class DvbTransponder;

class DvbDeviceConfig
{
public:
	DvbDeviceConfig(const QString &deviceId_, const QString &frontendName_, DvbDevice *device_);
	~DvbDeviceConfig();

	QString deviceId;
	QString frontendName;
	DvbDevice *device;
	QList<DvbConfig> configs;
	int useCount; // -1 means exlusive use
	QString source;
	DvbTransponder transponder;
};

class DvbManager : public QObject
{
	Q_OBJECT
public:
	enum TransmissionType
	{
		DvbC = 0,
		DvbS = 1,
		DvbT = 2,
		Atsc = 3,
		TransmissionTypeMax = Atsc
	};

	explicit DvbManager(QObject *parent);
	~DvbManager();

	QStringList getSources() const
	{
		return sources;
	}

	DvbChannelModel *getChannelModel() const
	{
		return channelModel;
	}

	DvbEpgModel *getEpgModel() const
	{
		return epgModel;
	}

	DvbRecordingModel *getRecordingModel() const
	{
		return recordingModel;
	}

	QList<DvbDevice *> getDevices() const;
	DvbDevice *requestDevice(const QString &source, const DvbTransponder &transponder);
	// exclusive = you can freely tune() and stop(), because the device isn't shared
	DvbDevice *requestExclusiveDevice(const QString &source);
	void releaseDevice(DvbDevice *device);

	QList<DvbDeviceConfig> getDeviceConfigs() const;
	void setDeviceConfigs(const QList<QList<DvbConfig> > &configs);

	QString getScanDataDate(); // returns the formatted short date of the last scan file update
	QStringList getScanSources(TransmissionType type);
	QString getAutoScanSource(const QString &source) const;
	QList<DvbTransponder> getTransponders(const QString &source);
	bool updateScanData(const QByteArray &data);

	QString getRecordingFolder();
	QString getTimeShiftFolder();
	void setRecordingFolder(const QString &path);
	void setTimeShiftFolder(const QString &path);

	double getLatitude();
	double getLongitude();
	void setLatitude(double value);
	void setLongitude(double value);

private slots:
	void deviceAdded(DvbDevice *device);
	void deviceRemoved(DvbDevice *device);

private:
	void readDeviceConfigs();
	void writeDeviceConfigs();

	void updateSourceMapping();

	void readScanData();

	DvbChannelModel *channelModel;
	DvbEpgModel *epgModel;
	DvbRecordingModel *recordingModel;

	QList<DvbDeviceConfig> deviceConfigs;
	QMap<QString, QPair<TransmissionType, QString> > sourceMapping;
	QStringList sources;

	DvbDeviceManager *deviceManager;

	QDate scanDataDate;
	DvbScanData *scanData;
	QStringList scanSources[TransmissionTypeMax + 1];
	QList<int> scanOffsets[TransmissionTypeMax + 1];
};

#endif /* DVBMANAGER_H */
