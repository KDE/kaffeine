/*
 * dvbmanager.cpp
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

#include "dvbmanager.h"
#include "dvbmanager_p.h"

#include <QDir>
#include <QPluginLoader>
#include <KConfigGroup>
#include <KLocale>
#include <KStandardDirs>
#include <config-kaffeine.h>
#include "../log.h"
#include "dvbconfig.h"
#include "dvbdevice.h"
#include "dvbdevice_linux.h"
#include "dvbepg.h"
#include "dvbliveview.h"
#include "dvbsi.h"

DvbManager::DvbManager(MediaWidget *mediaWidget_, QWidget *parent_) : QObject(parent_),
	parent(parent_), mediaWidget(mediaWidget_), channelView(NULL), dvbDumpEnabled(false)
{
	channelModel = DvbChannelModel::createSqlModel(this);
	recordingModel = new DvbRecordingModel(this, this);
	epgModel = new DvbEpgModel(this, this);
	liveView = new DvbLiveView(this, this);

	readDeviceConfigs();
	updateSourceMapping();

	loadDeviceManager();

	DvbSiText::setOverride6937(override6937Charset());
}

DvbManager::~DvbManager()
{
	writeDeviceConfigs();

	// we need an explicit deletion order (device users ; devices ; device manager)

	delete epgModel;
	delete recordingModel;

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		delete deviceConfig.device;
	}
}

DvbDevice *DvbManager::requestDevice(const QString &source, const DvbTransponder &transponder,
	DvbManager::RequestType requestType)
{
	Q_ASSERT(requestType != Exclusive);
	// FIXME call DvbEpgModel::startEventFilter / DvbEpgModel::stopEventFilter here?

	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.useCount < 1)) {
			continue;
		}

		if ((it.source == source) && it.transponder.corresponds(transponder)) {
			++deviceConfigs[i].useCount;

			if (requestType == Prioritized) {
				++deviceConfigs[i].prioritizedUseCount;
			}

			return it.device;
		}
	}

	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.useCount != 0)) {
			continue;
		}

		foreach (const DvbConfig &config, it.configs) {
			if (config->name == source) {
				DvbDevice *device = it.device;

				if (!device->acquire(config.constData())) {
					continue;
				}

				deviceConfigs[i].useCount = 1;

				if (requestType == Prioritized) {
					deviceConfigs[i].prioritizedUseCount = 1;
				}

				deviceConfigs[i].source = source;
				deviceConfigs[i].transponder = transponder;
				device->tune(transponder);
				return device;
			}
		}
	}

	if (requestType != Prioritized) {
		return NULL;
	}

	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.useCount == 0) || (it.prioritizedUseCount != 0)) {
			continue;
		}

		foreach (const DvbConfig &config, it.configs) {
			if (config->name == source) {
				deviceConfigs[i].useCount = 1;
				deviceConfigs[i].prioritizedUseCount = 1;
				deviceConfigs[i].source = source;
				deviceConfigs[i].transponder = transponder;

				DvbDevice *device = it.device;
				device->reacquire(config.constData());
				device->tune(transponder);
				return device;
			}
		}
	}

	return NULL;
}

DvbDevice *DvbManager::requestExclusiveDevice(const QString &source)
{
	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.useCount != 0)) {
			continue;
		}

		foreach (const DvbConfig &config, it.configs) {
			if (config->name == source) {
				DvbDevice *device = it.device;

				if (!device->acquire(config.constData())) {
					continue;
				}

				deviceConfigs[i].useCount = -1;
				deviceConfigs[i].source.clear();
				return device;
			}
		}
	}

	return NULL;
}

void DvbManager::releaseDevice(DvbDevice *device, RequestType requestType)
{
	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if (it.device == device) {
			switch (requestType) {
			case Prioritized:
				--deviceConfigs[i].prioritizedUseCount;
				Q_ASSERT(it.prioritizedUseCount >= 0);
			// fall through
			case Shared:
				--deviceConfigs[i].useCount;
				Q_ASSERT(it.useCount >= 0);
				Q_ASSERT(it.useCount >= it.prioritizedUseCount);

				if (it.useCount == 0) {
					it.device->release();
				}

				break;
			case Exclusive:
				Q_ASSERT(it.useCount == -1);
				Q_ASSERT(it.prioritizedUseCount == 0);
				deviceConfigs[i].useCount = 0;
				it.device->release();
				break;
			}

			break;
		}
	}
}

QList<DvbDeviceConfig> DvbManager::getDeviceConfigs() const
{
	return deviceConfigs;
}

void DvbManager::updateDeviceConfigs(const QList<DvbDeviceConfigUpdate> &configUpdates)
{
	for (int i = 0; i < configUpdates.size(); ++i) {
		const DvbDeviceConfigUpdate &configUpdate = configUpdates.at(i);

		for (int j = i;; ++j) {
			Q_ASSERT(j < deviceConfigs.size());

			if (&deviceConfigs.at(j) == configUpdate.deviceConfig) {
				if (i != j) {
					deviceConfigs.move(j, i);
				}

				deviceConfigs[i].configs = configUpdate.configs;
				break;
			}
		}
	}

	for (int i = configUpdates.size(); i < deviceConfigs.size(); ++i) {
		if (deviceConfigs.at(i).device != NULL) {
			deviceConfigs[i].configs.clear();
		} else {
			deviceConfigs.removeAt(i);
			--i;
		}
	}

	updateSourceMapping();
}

QDate DvbManager::getScanDataDate()
{
	if (!scanDataDate.isValid()) {
		readScanData();
	}

	return scanDataDate;
}

QStringList DvbManager::getScanSources(TransmissionType type)
{
	if (scanData.isEmpty()) {
		readScanData();
	}

	return scanSources.value(type);
}

QString DvbManager::getAutoScanSource(const QString &source) const
{
	QPair<TransmissionType, QString> scanSource = sourceMapping.value(source);

	if (scanSource.second.isEmpty()) {
		Log("DvbManager::getAutoScanSource: invalid source");
		return QString();
	}

	if ((scanSource.first == DvbT) && (scanSource.second.startsWith(QLatin1String("AUTO")))) {
		return scanSource.second;
	}

	return QString();
}

QList<DvbTransponder> DvbManager::getTransponders(DvbDevice *device, const QString &source)
{
	if (scanData.isEmpty()) {
		readScanData();
	}

	QPair<TransmissionType, QString> scanSource = sourceMapping.value(source);

	if (scanSource.second.isEmpty()) {
		Log("DvbManager::getTransponders: invalid source");
		return QList<DvbTransponder>();
	}

	if ((scanSource.first == DvbS) &&
	    ((device->getTransmissionTypes() & DvbDevice::DvbS2) != 0)) {
		scanSource.first = DvbS2;
	}

	return scanData.value(scanSource);
}

bool DvbManager::updateScanData(const QByteArray &data)
{
	QByteArray uncompressed = qUncompress(data);

	if (uncompressed.isEmpty()) {
		Log("DvbManager::updateScanData: qUncompress failed");
		return false;
	}

	if (!DvbScanData(uncompressed).readDate().isValid()) {
		Log("DvbManager::updateScanData: invalid format");
		return false;
	}

	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("scanfile.dvb")));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		Log("DvbManager::updateScanData: cannot open") << file.fileName();
		return false;
	}

	file.write(uncompressed);
	file.close();

	readScanData();
	return true;
}

QString DvbManager::getRecordingFolder() const
{
	return KGlobal::config()->group("DVB").readEntry("RecordingFolder", QDir::homePath());
}

QString DvbManager::getTimeShiftFolder() const
{
	return KGlobal::config()->group("DVB").readEntry("TimeShiftFolder", QDir::homePath());
}

int DvbManager::getBeginMargin() const
{
	return KGlobal::config()->group("DVB").readEntry("BeginMargin", 300);
}

int DvbManager::getEndMargin() const
{
	return KGlobal::config()->group("DVB").readEntry("EndMargin", 600);
}

bool DvbManager::override6937Charset() const
{
	return KGlobal::config()->group("DVB").readEntry("Override6937", false);
}

void DvbManager::setRecordingFolder(const QString &path)
{
	KGlobal::config()->group("DVB").writeEntry("RecordingFolder", path);
}

void DvbManager::setTimeShiftFolder(const QString &path)
{
	KGlobal::config()->group("DVB").writeEntry("TimeShiftFolder", path);
}

void DvbManager::setBeginMargin(int beginMargin)
{
	KGlobal::config()->group("DVB").writeEntry("BeginMargin", beginMargin);
}

void DvbManager::setEndMargin(int endMargin)
{
	KGlobal::config()->group("DVB").writeEntry("EndMargin", endMargin);
}

void DvbManager::setOverride6937Charset(bool override)
{
	KGlobal::config()->group("DVB").writeEntry("Override6937", override);
	DvbSiText::setOverride6937(override);
}

double DvbManager::getLatitude()
{
	return KGlobal::config()->group("DVB").readEntry("Latitude", 0.0);
}

double DvbManager::getLongitude()
{
	return KGlobal::config()->group("DVB").readEntry("Longitude", 0.0);
}

void DvbManager::setLatitude(double value)
{
	KGlobal::config()->group("DVB").writeEntry("Latitude", value);
}

void DvbManager::setLongitude(double value)
{
	KGlobal::config()->group("DVB").writeEntry("Longitude", value);
}

void DvbManager::enableDvbDump()
{
	if (dvbDumpEnabled) {
		return;
	}

	dvbDumpEnabled = true;

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		if (deviceConfig.device != NULL) {
			deviceConfig.device->enableDvbDump();
		}
	}
}

void DvbManager::requestBuiltinDeviceManager(QObject *&builtinDeviceManager)
{
	builtinDeviceManager = new DvbLinuxDeviceManager(this);
}

void DvbManager::deviceAdded(DvbBackendDevice *backendDevice)
{
	DvbDevice *device = new DvbDevice(backendDevice, this);
	QString deviceId = device->getDeviceId();
	QString frontendName = device->getFrontendName();

	if (dvbDumpEnabled) {
		device->enableDvbDump();
	}

	for (int i = 0;; ++i) {
		if (i == deviceConfigs.size()) {
			deviceConfigs.append(DvbDeviceConfig(deviceId, frontendName, device));
			break;
		}

		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.deviceId.isEmpty() || deviceId.isEmpty() || (it.deviceId == deviceId)) &&
		    (it.frontendName == frontendName) && (it.device == NULL)) {
			deviceConfigs[i].device = device;
			break;
		}
	}
}

void DvbManager::deviceRemoved(DvbBackendDevice *backendDevice)
{
	for (int i = 0; i < deviceConfigs.size(); ++i) {
		DvbDeviceConfig &it = deviceConfigs[i];

		if (it.device && it.device->getBackendDevice() == backendDevice) {
			if (it.useCount != 0) {
				it.useCount = 0;
				it.prioritizedUseCount = 0;
				it.device->release();
			}

			delete it.device;
			it.device = NULL;
			break;
		}
	}
}

void DvbManager::loadDeviceManager()
{
	QDir dir(QString::fromUtf8(KAFFEINE_LIB_INSTALL_DIR "/"));
	QStringList entries = dir.entryList(QStringList(QLatin1String("*kaffeinedvb*")), QDir::NoFilter,
		QDir::Name | QDir::Reversed);

	foreach (const QString &entry, entries) {
		QString path = dir.filePath(entry);

		if (!QLibrary::isLibrary(path)) {
			continue;
		}

		QObject *deviceManager = QPluginLoader(path).instance();

		if (deviceManager == NULL) {
			Log("DvbManager::loadDeviceManager: cannot load dvb device manager") <<
				path;
			break;
		}

		Log("DvbManager::loadDeviceManager: using dvb device manager") << path;
		deviceManager->setParent(this);
		connect(deviceManager, SIGNAL(requestBuiltinDeviceManager(QObject*&)),
			this, SLOT(requestBuiltinDeviceManager(QObject*&)));
		connect(deviceManager, SIGNAL(deviceAdded(DvbBackendDevice*)),
			this, SLOT(deviceAdded(DvbBackendDevice*)));
		connect(deviceManager, SIGNAL(deviceRemoved(DvbBackendDevice*)),
			this, SLOT(deviceRemoved(DvbBackendDevice*)));
		QMetaObject::invokeMethod(deviceManager, "doColdPlug");
		return;
	}

	Log("DvbManager::loadDeviceManager: using built-in dvb device manager");
	DvbLinuxDeviceManager *deviceManager = new DvbLinuxDeviceManager(this);
	connect(deviceManager, SIGNAL(deviceAdded(DvbBackendDevice*)),
		this, SLOT(deviceAdded(DvbBackendDevice*)));
	connect(deviceManager, SIGNAL(deviceRemoved(DvbBackendDevice*)),
		this, SLOT(deviceRemoved(DvbBackendDevice*)));
	deviceManager->doColdPlug();
}

void DvbManager::readDeviceConfigs()
{
	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("config.dvb")));

	if (!file.open(QIODevice::ReadOnly)) {
		Log("DvbManager::readDeviceConfigs: cannot open") << file.fileName();
		return;
	}

	DvbDeviceConfigReader reader(&file);

	while (!reader.atEnd()) {
		if (reader.readLine() != QLatin1String("[device]")) {
			continue;
		}

		QString deviceId = reader.readString(QLatin1String("deviceId"));
		QString frontendName = reader.readString(QLatin1String("frontendName"));
		int configCount = reader.readInt(QLatin1String("configCount"));

		if (!reader.isValid()) {
			break;
		}

		DvbDeviceConfig deviceConfig(deviceId, frontendName, NULL);

		for (int i = 0; i < configCount; ++i) {
			while (!reader.atEnd()) {
				if (reader.readLine() == QLatin1String("[config]")) {
					break;
				}
			}

			DvbConfigBase::TransmissionType type =
				reader.readEnum(QLatin1String("type"), DvbConfigBase::TransmissionTypeMax);

			if (!reader.isValid()) {
				break;
			}

			DvbConfigBase *config = new DvbConfigBase(type);
			config->name = reader.readString(QLatin1String("name"));
			config->scanSource = reader.readString(QLatin1String("scanSource"));
			config->timeout = reader.readInt(QLatin1String("timeout"));

			if (type == DvbConfigBase::DvbS) {
				config->configuration = reader.readEnum(QLatin1String("configuration"),
					DvbConfigBase::ConfigurationMax);
				config->lnbNumber = reader.readInt(QLatin1String("lnbNumber"));
				config->lowBandFrequency = reader.readInt(QLatin1String("lowBandFrequency"));
				config->switchFrequency = reader.readInt(QLatin1String("switchFrequency"));
				config->highBandFrequency = reader.readInt(QLatin1String("highBandFrequency"));
			}

			if (!reader.isValid()) {
				delete config;
				break;
			}

			deviceConfig.configs.append(DvbConfig(config));
		}

		deviceConfigs.append(deviceConfig);
	}

	if (!reader.isValid()) {
		Log("DvbManager::readDeviceConfigs: cannot read") << file.fileName();
	}
}

void DvbManager::writeDeviceConfigs()
{
	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("config.dvb")));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		Log("DvbManager::writeDeviceConfigs: cannot open") << file.fileName();
		return;
	}

	DvbDeviceConfigWriter writer(&file);

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		writer.write(QLatin1String("[device]"));
		writer.write(QLatin1String("deviceId"), deviceConfig.deviceId);
		writer.write(QLatin1String("frontendName"), deviceConfig.frontendName);
		writer.write(QLatin1String("configCount"), deviceConfig.configs.size());

		for (int i = 0; i < deviceConfig.configs.size(); ++i) {
			const DvbConfig &config = deviceConfig.configs.at(i);
			writer.write(QLatin1String("[config]"));
			writer.write(QLatin1String("type"), config->getTransmissionType());
			writer.write(QLatin1String("name"), config->name);
			writer.write(QLatin1String("scanSource"), config->scanSource);
			writer.write(QLatin1String("timeout"), config->timeout);

			if (config->getTransmissionType() == DvbConfigBase::DvbS) {
				writer.write(QLatin1String("configuration"), config->configuration);
				writer.write(QLatin1String("lnbNumber"), config->lnbNumber);
				writer.write(QLatin1String("lowBandFrequency"), config->lowBandFrequency);
				writer.write(QLatin1String("switchFrequency"), config->switchFrequency);
				writer.write(QLatin1String("highBandFrequency"), config->highBandFrequency);
			}
		}
	}
}

void DvbManager::updateSourceMapping()
{
	sourceMapping.clear();

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		for (int i = 0; i < deviceConfig.configs.size(); ++i) {
			const DvbConfig &config = deviceConfig.configs.at(i);
			TransmissionType type;

			switch (config->getTransmissionType()) {
			case DvbConfigBase::DvbC:
				type = DvbC;
				break;
			case DvbConfigBase::DvbS:
				type = DvbS;
				break;
			case DvbConfigBase::DvbT:
				type = DvbT;
				break;
			case DvbConfigBase::Atsc:
				type = Atsc;
				break;
			default:
				Q_ASSERT(false);
				continue;
			}

			sourceMapping.insert(config->name, qMakePair(type, config->scanSource));
		}
	}

	sources = sourceMapping.keys();
}

void DvbManager::readScanData()
{
	scanSources.clear();
	scanData.clear();

	QFile globalFile(QString::fromUtf8(KAFFEINE_DATA_INSTALL_DIR "/kaffeine/scanfile.dvb"));
	QDate globalDate;

	if (globalFile.open(QIODevice::ReadOnly)) {
		globalDate = DvbScanData(globalFile.read(1024)).readDate();

		if (globalDate.isNull()) {
			Log("DvbManager::readScanData: cannot parse") << globalFile.fileName();
		}

		globalFile.close();
	} else {
		Log("DvbManager::readScanData: cannot open") << globalFile.fileName();
	}

	QFile localFile(KStandardDirs::locateLocal("appdata", QLatin1String("scanfile.dvb")));
	QByteArray localData;
	QDate localDate;

	if (localFile.open(QIODevice::ReadOnly)) {
		localData = localFile.readAll();
		localDate = DvbScanData(localData).readDate();

		if (localDate.isNull()) {
			Log("DvbManager::readScanData: cannot parse") << localFile.fileName();
		}

		localFile.close();
	}

	if (localDate < globalDate) {
		localData.clear();

		if (localFile.exists() && !localFile.remove()) {
			Log("DvbManager::readScanData: cannot remove") << localFile.fileName();
		}

		if (!globalFile.copy(localFile.fileName())) {
			Log("DvbManager::readScanData: cannot copy") << globalFile.fileName() <<
				QLatin1String("to") << localFile.fileName();
		}

		if (localFile.open(QIODevice::ReadOnly)) {
			localData = localFile.readAll();
			localFile.close();
		} else {
			Log("DvbManager::readScanData: cannot open") << localFile.fileName();
			scanDataDate = QDate(1900, 1, 1);
			return;
		}
	}

	DvbScanData data(localData);
	scanDataDate = data.readDate();

	if (!scanDataDate.isValid()) {
		Log("DvbManager::readScanData: cannot parse") << localFile.fileName();
		scanDataDate = QDate(1900, 1, 1);
		return;
	}

	if (!readScanSources(data, "[dvb-c/", DvbC) ||
	    !readScanSources(data, "[dvb-s/", DvbS) ||
	    !readScanSources(data, "[dvb-t/", DvbT) ||
	    !readScanSources(data, "[atsc/", Atsc) ||
	    !data.checkEnd()) {
		Log("DvbManager::readScanData: cannot parse") << localFile.fileName();
	}
}

bool DvbManager::readScanSources(DvbScanData &data, const char *tag, TransmissionType type)
{
	int tagLen = int(strlen(tag));
	bool parseError = false;

	while (strncmp(data.getLine(), tag, tagLen) == 0) {
		const char *line = data.readLine();

		QString name = QString(QLatin1String(line)).remove(0, tagLen);

		if ((name.size() < 2) || (name.at(name.size() - 1) != QLatin1Char(']'))) {
			return false;
		}

		name.chop(1);
		QList<DvbTransponder> transponders;
		bool containsDvbS1 = false;

		while (true) {
			line = data.getLine();

			if ((*line == '[') || (*line == 0)) {
				break;
			}

			line = data.readLine();
			DvbTransponder transponder =
				DvbTransponder::fromString(QString::fromAscii(line));

			if (!transponder.isValid()) {
				parseError = true;
			} else {
				transponders.append(transponder);

				if (transponder.getTransmissionType() ==
				    DvbTransponderBase::DvbS) {
					containsDvbS1 = true;
				}
			}
		}

		if ((type != DvbS) && (type != DvbS2)) {
			scanSources[type].append(name);
			scanData.insert(qMakePair(type, name), transponders);
		} else {
			scanSources[DvbS2].append(name);
			scanData.insert(qMakePair(DvbS2, name), transponders);

			if (containsDvbS1) {
				for (int i = 0; i < transponders.size(); ++i) {
					if (transponders.at(i).getTransmissionType() ==
					    DvbTransponderBase::DvbS2) {
						transponders.removeAt(i);
						--i;
					}
				}

				scanSources[DvbS].append(name);
				scanData.insert(qMakePair(DvbS, name), transponders);
			}
		}
	}

	if (parseError) {
		Log("DvbManager::readScanSources: cannot parse complete scan data");
	}

	return true;
}

DvbDeviceConfig::DvbDeviceConfig(const QString &deviceId_, const QString &frontendName_,
	DvbDevice *device_) : deviceId(deviceId_), frontendName(frontendName_), device(device_),
	useCount(0), prioritizedUseCount(0)
{
}

DvbDeviceConfig::~DvbDeviceConfig()
{
}

DvbDeviceConfigUpdate::DvbDeviceConfigUpdate(const DvbDeviceConfig *deviceConfig_) :
	deviceConfig(deviceConfig_)
{
}

DvbDeviceConfigUpdate::~DvbDeviceConfigUpdate()
{
}

DvbScanData::DvbScanData(const QByteArray &data_) : data(data_)
{
	begin = data.begin();
	pos = data.begin();
	end = data.constEnd();
}

DvbScanData::~DvbScanData()
{
}

bool DvbScanData::checkEnd() const
{
	return (pos == end);
}

const char *DvbScanData::getLine() const
{
	return pos;
}

const char *DvbScanData::readLine()
{
	// ignore comments

	while (*pos == '#') {
		do {
			++pos;

			if (pos == end) {
				return end;
			}
		} while (*pos != '\n');

		++pos;
	}

	char *line = pos;

	while (pos != end) {
		if (*pos == '\n') {
			*pos = 0;
			++pos;
			break;
		}

		++pos;
	}

	return line;
}

QDate DvbScanData::readDate()
{
	if (strcmp(readLine(), "[date]") != 0) {
		return QDate();
	}

	return QDate::fromString(QString::fromAscii(readLine()), Qt::ISODate);
}
