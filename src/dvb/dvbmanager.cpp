/*
 * dvbmanager.cpp
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

#include "dvbmanager.h"

#include <QDir>
#include <QLibrary>
#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include <config-kaffeine.h>
#include "dvbchannelui.h"
#include "dvbepg.h"
#include "dvbrecording.h"

static QString installPath(const char *component)
{
	if (strcmp(component, "data") == 0) {
		return QString::fromUtf8(KAFFEINE_DATA_INSTALL_DIR "/");
	}

	if (strcmp(component, "module") == 0) {
		return QString::fromUtf8(KAFFEINE_PLUGIN_INSTALL_DIR "/");
	}

	return QString();
}

class DvbScanData
{
public:
	explicit DvbScanData(const QByteArray &data_) : data(data_)
	{
		begin = data.begin();
		pos = data.begin();
		end = data.constEnd();
	}

	~DvbScanData() { }

	bool checkEnd() const
	{
		return pos == end;
	}

	QDate readDate();
	bool readSources(QStringList &sources, QList<int> &offsets, const char *tag);
	QList<DvbTransponder> readTransponders(int offset, DvbManager::TransmissionType type);

private:
	const char *readLine();

	QByteArray data;
	char *begin;
	char *pos;
	const char *end;

};

QDate DvbScanData::readDate()
{
	if (strcmp(readLine(), "[date]") != 0) {
		return QDate();
	}

	return QDate::fromString(readLine(), Qt::ISODate);
}

bool DvbScanData::readSources(QStringList &sources, QList<int> &offsets, const char *tag)
{
	int tagLen = strlen(tag);

	while (strncmp(pos, tag, tagLen) == 0) {
		const char *line = readLine();

		QString name = QString(line + tagLen);

		if ((name.size() < 2) || (name[name.size() - 1] != ']')) {
			return false;
		}

		name.resize(name.size() - 1);

		sources.append(name);
		offsets.append(pos - begin);

		while ((*pos != '[') && (*pos != 0)) {
			readLine();
		}
	}

	return true;
}

QList<DvbTransponder> DvbScanData::readTransponders(int offset, DvbManager::TransmissionType type)
{
	pos = begin + offset;

	QList<DvbTransponder> list;
	bool parseError = false;

	while ((*pos != '[') && (*pos != 0)) {
		DvbTransponderBase *transponder = NULL;

		switch (type) {
		case DvbManager::DvbC:
			transponder = new DvbCTransponder;
			break;
		case DvbManager::DvbS:
			transponder = new DvbSTransponder;
			break;
		case DvbManager::DvbT:
			transponder = new DvbTTransponder;
			break;
		case DvbManager::Atsc:
			transponder = new AtscTransponder;
			break;
		}

		if ((transponder == NULL) || !transponder->fromString(QString::fromAscii(pos))) {
			parseError = true;
			delete transponder;
		} else {
			list.append(DvbTransponder(transponder));
		}

		while (pos != end) {
			if (*pos == 0) {
				++pos;
				break;
			}

			++pos;
		}
	}

	if (list.isEmpty() || parseError) {
		kWarning() << "parse error";
	}

	return list;
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

class DvbDeviceConfigReader : public QTextStream
{
public:
	explicit DvbDeviceConfigReader(QIODevice *device) : QTextStream(device), valid(true)
	{
		setCodec("UTF-8");
	}

	~DvbDeviceConfigReader() { }

	bool isValid() const
	{
		return valid;
	}

	template<typename T> T readEnum(const QString &entry, T maxValue)
	{
		int value = readInt(entry);

		if (value > maxValue) {
			valid = false;
		}

		return static_cast<T>(value);
	}

	int readInt(const QString &entry)
	{
		QString string = readString(entry);

		if (string.isEmpty()) {
			valid = false;
			return -1;
		}

		bool ok;
		int value = string.toInt(&ok);

		if (!ok || (value < 0)) {
			valid = false;
		}

		return value;
	}

	QString readString(const QString &entry)
	{
		QString line = readLine();

		if (!line.startsWith(entry + '=')) {
			valid = false;
			return QString();
		}

		return line.remove(0, entry.size() + 1);
	}

private:
	bool valid;
};

class DvbDeviceConfigWriter : public QTextStream
{
public:
	explicit DvbDeviceConfigWriter(QIODevice *device) : QTextStream(device)
	{
		setCodec("UTF-8");
	}

	~DvbDeviceConfigWriter() { }

	void write(const QString &string)
	{
		*this << string << '\n';
	}

	void write(const QString &entry, int value)
	{
		*this << entry << '=' << value << '\n';
	}

	void write(const QString &entry, const QString &string)
	{
		*this << entry << '=' << string << '\n';
	}
};

DvbDeviceConfig::DvbDeviceConfig(const QString &deviceId_, const QString &frontendName_,
	DvbDevice *device_) : deviceId(deviceId_), frontendName(frontendName_), device(device_),
	useCount(0)
{
}

DvbDeviceConfig::~DvbDeviceConfig()
{
}

DvbManager::DvbManager(QObject *parent) : QObject(parent), scanData(NULL)
{
	channelModel = new DvbChannelModel(this);
	channelModel->loadChannels();

	epgModel = new DvbEpgModel(this);

	recordingModel = new DvbRecordingModel(this);

	readDeviceConfigs();
	updateSourceMapping();

	loadDeviceManager();

	scanDataDate = KGlobal::config()->group("DVB").readEntry("ScanDataDate", QDate(1900, 1, 1));
}

DvbManager::~DvbManager()
{
	KConfigGroup(KGlobal::config(), "DVB").writeEntry("ScanDataDate", scanDataDate);
	writeDeviceConfigs();
	channelModel->saveChannels();
	delete scanData;
}

DvbDevice *DvbManager::requestDevice(const QString &source, const DvbTransponder &transponder)
{
	// first try to find a device that is already tuned to the selected transponder

	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.useCount < 1)) {
			continue;
		}

		if ((it.source == source) && it.transponder->corresponds(transponder)) {
			++deviceConfigs[i].useCount;
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

				if (!device->acquire()) {
					continue;
				}

				++deviceConfigs[i].useCount;
				deviceConfigs[i].source = source;
				deviceConfigs[i].transponder = transponder;

				device->config = config;
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

				if (!device->acquire()) {
					continue;
				}

				deviceConfigs[i].useCount = -1;

				device->config = config;
				return device;
			}
		}
	}

	return NULL;
}

void DvbManager::releaseDevice(DvbDevice *device)
{
	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if (it.device == device) {
			if ((--deviceConfigs[i].useCount) <= 0) {
				deviceConfigs[i].device->release();
				deviceConfigs[i].useCount = 0;
			}

			break;
		}
	}
}

QList<DvbDeviceConfig> DvbManager::getDeviceConfigs() const
{
	return deviceConfigs;
}

void DvbManager::setDeviceConfigs(const QList<QList<DvbConfig> > &configs)
{
	Q_ASSERT(configs.size() <= deviceConfigs.size());

	for (int i = 0; i < configs.size(); ++i) {
		deviceConfigs[i].configs = configs.at(i);
	}

	for (int i = configs.size(); i < deviceConfigs.size(); ++i) {
		if (deviceConfigs.at(i).device == NULL) {
			deviceConfigs.removeAt(i);
			--i;
		}
	}

	updateSourceMapping();
}

QString DvbManager::getScanDataDate()
{
	if (scanDataDate.isNull()) {
		readScanData();
	}

	return KGlobal::locale()->formatDate(scanDataDate, KLocale::ShortDate);
}

QStringList DvbManager::getScanSources(TransmissionType type)
{
	Q_ASSERT((type >= 0) && (type <= TransmissionTypeMax));

	if (scanData == NULL) {
		readScanData();
	}

	return scanSources[type];
}

QString DvbManager::getAutoScanSource(const QString &source) const
{
	QPair<TransmissionType, QString> scanSource = sourceMapping.value(source);

	if (scanSource.second.isEmpty()) {
		kWarning() << "invalid source";
		return QString();
	}

	if ((scanSource.first == DvbT) && (scanSource.second.startsWith(QLatin1String("AUTO")))) {
		return scanSource.second;
	}

	return QString();
}

QList<DvbTransponder> DvbManager::getTransponders(const QString &source)
{
	if (scanData == NULL) {
		readScanData();
	}

	QPair<TransmissionType, QString> scanSource = sourceMapping.value(source);

	if (scanSource.second.isEmpty()) {
		kWarning() << "invalid source";
		return QList<DvbTransponder>();
	}

	TransmissionType type = scanSource.first;
	int index = scanSources[type].indexOf(scanSource.second);

	if (index == -1) {
		kWarning() << "invalid source";
		return QList<DvbTransponder>();
	}

	return scanData->readTransponders(scanOffsets[type].at(index), type);
}

bool DvbManager::updateScanData(const QByteArray &data)
{
	QByteArray uncompressed = qUncompress(data);

	if (uncompressed.isEmpty()) {
		kWarning() << "qUncompress failed";
		return false;
	}

	if (!DvbScanData(uncompressed).readDate().isValid()) {
		kWarning() << "invalid format";
		return false;
	}

	QFile file(KStandardDirs::locateLocal("appdata", "scanfile.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		return false;
	}

	file.write(uncompressed);
	file.close();

	scanDataDate = QDate::currentDate();
	readScanData();

	return true;
}

QString DvbManager::getRecordingFolder()
{
	QString path = KConfigGroup(KGlobal::config(), "DVB").readEntry("RecordingFolder");

	if (path.isEmpty() || !QDir(path).exists()) {
		path = QDir::homePath();
		setRecordingFolder(path);
	}

	return path;
}

QString DvbManager::getTimeShiftFolder()
{
	QString path = KConfigGroup(KGlobal::config(), "DVB").readEntry("TimeShiftFolder");

	if (path.isEmpty() || !QDir(path).exists()) {
		path = QDir::homePath();
		setTimeShiftFolder(path);
	}

	return path;
}

void DvbManager::setRecordingFolder(const QString &path)
{
	KConfigGroup(KGlobal::config(), "DVB").writeEntry("RecordingFolder", path);
}

void DvbManager::setTimeShiftFolder(const QString &path)
{
	KConfigGroup(KGlobal::config(), "DVB").writeEntry("TimeShiftFolder", path);
}

double DvbManager::getLatitude()
{
	return KConfigGroup(KGlobal::config(), "DVB").readEntry("Latitude", 0.0);
}

double DvbManager::getLongitude()
{
	return KConfigGroup(KGlobal::config(), "DVB").readEntry("Longitude", 0.0);
}

void DvbManager::setLatitude(double value)
{
	KConfigGroup(KGlobal::config(), "DVB").writeEntry("Latitude", value);
}

void DvbManager::setLongitude(double value)
{
	KConfigGroup(KGlobal::config(), "DVB").writeEntry("Longitude", value);
}

void DvbManager::deviceAdded(DvbBackendDevice *backendDevice)
{
	DvbDevice *device = new DvbDevice(backendDevice, this);
	QString deviceId = backendDevice->getDeviceId();
	QString frontendName = backendDevice->getFrontendName();

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
		if (deviceConfigs.at(i).device->getBackendDevice() == backendDevice) {
			deviceConfigs[i].device = NULL;
			break;
		}
	}
}

void DvbManager::loadDeviceManager()
{
	QDir dir(installPath("module"));
	QStringList entries = dir.entryList(QStringList("kaffeinedvb*"));
	qSort(entries.begin(), entries.end(), qGreater<QString>());

	foreach (const QString &entry, entries) {
		QString path = dir.filePath(entry);

		if (!QLibrary::isLibrary(path)) {
			continue;
		}

		typedef QObject *(*funcPointer)();

		funcPointer func = (funcPointer) QLibrary::resolve(path, "create_device_manager");

		if (func == NULL) {
			kWarning() << "couldn't load dvb module" << path;
			return;
		}

		QObject *deviceManager = func();
		deviceManager->setParent(this);
		connect(deviceManager, SIGNAL(deviceAdded(DvbBackendDevice*)),
			this, SLOT(deviceAdded(DvbBackendDevice*)));
		connect(deviceManager, SIGNAL(deviceRemoved(DvbBackendDevice*)),
			this, SLOT(deviceRemoved(DvbBackendDevice*)));
		QMetaObject::invokeMethod(deviceManager, "doColdPlug");

		kDebug() << "successfully loaded" << path;
		return;
	}

	kError() << "no dvb module found";
	return;
}

void DvbManager::readDeviceConfigs()
{
	QFile file(KStandardDirs::locateLocal("appdata", "config.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "can't open" << file.fileName();
		return;
	}

	DvbDeviceConfigReader reader(&file);

	while (!reader.atEnd()) {
		if (reader.readLine() != "[device]") {
			continue;
		}

		QString deviceId = reader.readString("deviceId");
		QString frontendName = reader.readString("frontendName");
		int configCount = reader.readInt("configCount");

		if (!reader.isValid()) {
			break;
		}

		DvbDeviceConfig deviceConfig(deviceId, frontendName, NULL);

		for (int i = 0; i < configCount; ++i) {
			while (!reader.atEnd()) {
				if (reader.readLine() == "[config]") {
					break;
				}
			}

			DvbConfigBase::TransmissionType type =
				reader.readEnum("type", DvbConfigBase::TransmissionTypeMax);

			if (!reader.isValid()) {
				break;
			}

			DvbConfigBase *config = new DvbConfigBase(type);
			config->name = reader.readString("name");
			config->scanSource = reader.readString("scanSource");
			config->timeout = reader.readInt("timeout");

			if (type == DvbConfigBase::DvbS) {
				config->configuration = reader.readEnum("configuration",
								   DvbConfigBase::ConfigurationMax);
				config->lnbNumber = reader.readInt("lnbNumber");
				config->lowBandFrequency = reader.readInt("lowBandFrequency");
				config->switchFrequency = reader.readInt("switchFrequency");
				config->highBandFrequency = reader.readInt("highBandFrequency");
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
		kWarning() << "can't read" << file.fileName();
	}
}

void DvbManager::writeDeviceConfigs()
{
	QFile file(KStandardDirs::locateLocal("appdata", "config.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		return;
	}

	DvbDeviceConfigWriter writer(&file);

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		writer.write("[device]");
		writer.write("deviceId", deviceConfig.deviceId);
		writer.write("frontendName", deviceConfig.frontendName);
		writer.write("configCount", deviceConfig.configs.size());

		foreach (const DvbConfig &config, deviceConfig.configs) {
			writer.write("[config]");
			writer.write("type", config->getTransmissionType());
			writer.write("name", config->name);
			writer.write("scanSource", config->scanSource);
			writer.write("timeout", config->timeout);

			if (config->getTransmissionType() == DvbConfigBase::DvbS) {
				writer.write("configuration", config->configuration);
				writer.write("lnbNumber", config->lnbNumber);
				writer.write("lowBandFrequency", config->lowBandFrequency);
				writer.write("switchFrequency", config->switchFrequency);
				writer.write("highBandFrequency", config->highBandFrequency);
			}
		}
	}
}

void DvbManager::updateSourceMapping()
{
	sourceMapping.clear();

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		foreach (const DvbConfig &config, deviceConfig.configs) {
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
	delete scanData;
	scanData = NULL;

	for (int i = 0; i <= TransmissionTypeMax; ++i) {
		scanSources[i].clear();
		scanOffsets[i].clear();
	}

	QFile localFile(KStandardDirs::locateLocal("appdata", "scanfile.dvb"));
	QDate localDate;

	if (localFile.open(QIODevice::ReadOnly)) {
		localDate = DvbScanData(localFile.read(1024)).readDate();

		if (localDate.isNull()) {
			kWarning() << "can't read" << localFile.fileName();
		}
	} else {
		kDebug() << "can't open" << localFile.fileName();
	}

	QFile globalFile(installPath("data") + "kaffeine/scanfile.dvb");
	QDate globalDate;

	if (globalFile.open(QIODevice::ReadOnly)) {
		globalDate = DvbScanData(globalFile.read(1024)).readDate();

		if (globalDate.isNull()) {
			kWarning() << "can't read" << globalFile.fileName();
		}
	} else {
		kWarning() << "can't open" << globalFile.fileName();
	}

	if (!localDate.isNull() && (globalDate.isNull() || (localDate >= globalDate))) {
		// use local file
		if (localFile.seek(0)) {
			scanData = new DvbScanData(localFile.readAll());
		} else {
			kWarning() << "can't seek" << localFile.fileName();
			return;
		}
	} else if (!globalDate.isNull()) {
		// use global file
		if (globalFile.seek(0)) {
			scanData = new DvbScanData(globalFile.readAll());
		} else {
			kWarning() << "can't seek" << globalFile.fileName();
			return;
		}
	} else {
		scanDataDate = QDate(1900, 1, 1);
		return;
	}

	QDate date = scanData->readDate();

	if (scanDataDate < date) {
		scanDataDate = date;
	}

	if (!scanData->readSources(scanSources[DvbC], scanOffsets[DvbC], "[dvb-c/") ||
	    !scanData->readSources(scanSources[DvbS], scanOffsets[DvbS], "[dvb-s/") ||
	    !scanData->readSources(scanSources[DvbT], scanOffsets[DvbT], "[dvb-t/") ||
	    !scanData->readSources(scanSources[Atsc], scanOffsets[Atsc], "[atsc/") ||
	    !scanData->checkEnd()) {
		kWarning() << "parsing error occurred";
	}
}
