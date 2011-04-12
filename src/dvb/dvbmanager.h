/*
 * dvbmanager.h
 *
 * Copyright (C) 2008-2011 Christoph Pfister <christophpfister@gmail.com>
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
#include "dvbtransponder.h"

class QTreeView;
class DvbBackendDevice;
class DvbChannelModel;
class DvbConfig;
class DvbDevice;
class DvbDeviceConfig;
class DvbDeviceConfigUpdate;
class DvbEpgModel;
class DvbLiveView;
class DvbRecordingModel;
class DvbScanData;
class MediaWidget;

class DvbManager : public QObject
{
	Q_OBJECT
public:
	enum RequestType {
		Shared,
		Exclusive, // you can freely tune() and stop(), because the device isn't shared
		Prioritized // takes precedence over 'Shared' and 'Exclusive'
	};

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

	QTreeView *getChannelView() const
	{
		return channelView;
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

	void setChannelView(QTreeView *channelView_)
	{
		channelView = channelView_;
	}

	DvbDevice *requestDevice(const QString &source, const DvbTransponder &transponder,
		RequestType requestType);
	DvbDevice *requestExclusiveDevice(const QString &source);
	void releaseDevice(DvbDevice *device, RequestType requestType);

	QList<DvbDeviceConfig> getDeviceConfigs() const;
	void updateDeviceConfigs(const QList<DvbDeviceConfigUpdate> &configUpdates);

	QDate getScanDataDate();
	QStringList getScanSources(TransmissionType type);
	QString getAutoScanSource(const QString &source) const;
	QList<DvbTransponder> getTransponders(DvbDevice *device, const QString &source);
	bool updateScanData(const QByteArray &data);

	QString getRecordingFolder() const;
	QString getTimeShiftFolder() const;
	int getBeginMargin() const; // seconds
	int getEndMargin() const; // seconds
	bool override6937Charset() const;
	void setRecordingFolder(const QString &path);
	void setTimeShiftFolder(const QString &path);
	void setBeginMargin(int beginMargin); // seconds
	void setEndMargin(int endMargin); // seconds
	void setOverride6937Charset(bool override);

	static double getLatitude();
	static double getLongitude();
	void setLatitude(double value);
	void setLongitude(double value);

	void enableDvbDump();

private slots:
	void requestBuiltinDeviceManager(QObject *&builtinDeviceManager);
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
	QTreeView *channelView;
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
	DvbDeviceConfig(const QString &deviceId_, const QString &frontendName_,
		DvbDevice *device_);
	~DvbDeviceConfig();

	QString deviceId;
	QString frontendName;
	DvbDevice *device;
	QList<DvbConfig> configs;
	int useCount; // -1 means exclusive use
	int prioritizedUseCount;
	QString source;
	DvbTransponder transponder;
};

class DvbDeviceConfigUpdate
{
public:
	explicit DvbDeviceConfigUpdate(const DvbDeviceConfig *deviceConfig_);
	~DvbDeviceConfigUpdate();

	const DvbDeviceConfig *deviceConfig;
	QList<DvbConfig> configs;
};

#endif /* DVBMANAGER_H */
