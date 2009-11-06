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
#include <QSharedData>
#include <QStringList>

class DvbBackendDevice;
class DvbChannelModel;
class DvbConfig;
class DvbDevice;
class DvbDeviceConfig;
class DvbEpgModel;
class DvbLiveView;
class DvbRecordingModel;
class DvbScanData;
class DvbTransponder;
class DvbTransponderBase;
class MediaWidget;

class DvbManager : public QObject
{
	Q_OBJECT
public:
	enum TransmissionType {
		DvbC,
		DvbS,
		DvbS2, // includes DvbS
		DvbT,
		Atsc
	};

	DvbManager(MediaWidget *mediaWidget_, QWidget *parent_);
	~DvbManager();

	QWidget *getParentWidget() const
	{
		return parent;
	}

	MediaWidget *getMediaWidget() const
	{
		return mediaWidget;
	}

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

	DvbLiveView *getLiveView() const
	{
		return liveView;
	}

	DvbRecordingModel *getRecordingModel() const
	{
		return recordingModel;
	}

	DvbDevice *requestDevice(const DvbTransponder &transponder, bool highPriority = false);
	// exclusive = you can freely tune() and stop(), because the device isn't shared
	DvbDevice *requestExclusiveDevice(const QString &source);
	void releaseDevice(DvbDevice *device);

	QList<DvbDeviceConfig> getDeviceConfigs() const;
	void setDeviceConfigs(const QList<QList<DvbConfig> > &configs);

	QString getScanDataDate(); // returns the formatted short date of the last scan file update
	QStringList getScanSources(TransmissionType type);
	QString getAutoScanSource(const QString &source) const;
	QList<DvbTransponder> getTransponders(DvbDevice *device, const QString &source);
	bool updateScanData(const QByteArray &data);

	QString getRecordingFolder();
	QString getTimeShiftFolder();
	void setRecordingFolder(const QString &path);
	void setTimeShiftFolder(const QString &path);

	static double getLatitude();
	static double getLongitude();
	void setLatitude(double value);
	void setLongitude(double value);

	void enableDvbDump();

private slots:
	void deviceAdded(DvbBackendDevice *backendDevice);
	void deviceRemoved(DvbBackendDevice *backendDevice);

private:
	void loadDeviceManager();

	void readDeviceConfigs();
	void writeDeviceConfigs();

	void updateSourceMapping();

	void readScanData();
	bool readScanSources(DvbScanData &data, const char *tag, TransmissionType type);

	QWidget *parent;
	MediaWidget *mediaWidget;
	DvbChannelModel *channelModel;
	DvbEpgModel *epgModel;
	DvbLiveView *liveView;
	DvbRecordingModel *recordingModel;

	QList<DvbDeviceConfig> deviceConfigs;
	bool dvbDumpEnabled;
	QMap<QString, QPair<TransmissionType, QString> > sourceMapping;
	QStringList sources;

	QDate scanDataDate;
	QMap<TransmissionType, QStringList> scanSources;
	QMap<QPair<TransmissionType, QString>, QList<DvbTransponder> > scanData;
};

class DvbDeviceConfig
{
public:
	DvbDeviceConfig(const QString &deviceId_, const QString &frontendName_, DvbDevice *device_);
	~DvbDeviceConfig();

	QString deviceId;
	QString frontendName;
	DvbDevice *device;
	QList<DvbConfig> configs;
	int useCount; // -1 means exclusive use
	bool highPriorityUse;
	QExplicitlySharedDataPointer<const DvbTransponderBase> transponder;
};

#endif /* DVBMANAGER_H */
