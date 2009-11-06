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
#include "dvbconfig.h"
#include "dvbepg.h"
#include "dvbliveview.h"
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
		return (pos == end);
	}

	const char *getLine() const
	{
		return pos;
	}

	const char *readLine();
	QDate readDate();

private:
	QByteArray data;
	char *begin;
	char *pos;
	const char *end;
};

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
	useCount(0), prioritizedUseCount(0)
{
}

DvbDeviceConfig::~DvbDeviceConfig()
{
}

DvbManager::DvbManager(MediaWidget *mediaWidget_, QWidget *parent_) : QObject(parent_),
	parent(parent_), mediaWidget(mediaWidget_), dvbDumpEnabled(false)
{
	channelModel = new DvbChannelModel(this);
	channelModel->loadChannels();

	epgModel = new DvbEpgModel(this);

	liveView = new DvbLiveView(this);

	recordingModel = new DvbRecordingModel(this);

	readDeviceConfigs();
	updateSourceMapping();

	loadDeviceManager();

	scanDataDate = KGlobal::config()->group("DVB").readEntry("ScanDataDate", QDate(1900, 1, 1));
}

DvbManager::~DvbManager()
{
	KGlobal::config()->group("DVB").writeEntry("ScanDataDate", scanDataDate);
	writeDeviceConfigs();
	channelModel->saveChannels();

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		delete deviceConfig.device;
	}
}

DvbDevice *DvbManager::requestDevice(const DvbTransponder &transponder,
	DvbManager::RequestType requestType)
{
	Q_ASSERT(requestType != Exclusive);

	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.useCount < 1)) {
			continue;
		}

		if (it.transponder->corresponds(transponder)) {
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
			if (config->name == transponder->source) {
				DvbDevice *device = it.device;

				if (!device->acquire(config)) {
					continue;
				}

				deviceConfigs[i].useCount = 1;

				if (requestType == Prioritized) {
					deviceConfigs[i].prioritizedUseCount = 1;
				}

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
			if (config->name == transponder->source) {
				deviceConfigs[i].useCount = 1;
				deviceConfigs[i].prioritizedUseCount = 1;
				deviceConfigs[i].transponder = transponder;

				DvbDevice *device = it.device;
				device->reacquire(config);
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

				if (!device->acquire(config)) {
					continue;
				}

				deviceConfigs[i].useCount = -1;
				deviceConfigs[i].transponder = NULL;
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
	if (scanData.isEmpty()) {
		readScanData();
	}

	return scanSources.value(type);
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

QList<DvbTransponder> DvbManager::getTransponders(DvbDevice *device, const QString &source)
{
	if (scanData.isEmpty()) {
		readScanData();
	}

	QPair<TransmissionType, QString> scanSource = sourceMapping.value(source);

	if (scanSource.second.isEmpty()) {
		kWarning() << "invalid source";
		return QList<DvbTransponder>();
	}

	if ((scanSource.first == DvbS) &&
	    ((device->getTransmissionTypes() & DvbBackendDevice::DvbS2) != 0)) {
		scanSource.first = DvbS2;
	}

	return scanData.value(scanSource);
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
	QString path = KGlobal::config()->group("DVB").readEntry("RecordingFolder");

	if (path.isEmpty() || !QDir(path).exists()) {
		path = QDir::homePath();
		setRecordingFolder(path);
	}

	return path;
}

QString DvbManager::getTimeShiftFolder()
{
	QString path = KGlobal::config()->group("DVB").readEntry("TimeShiftFolder");

	if (path.isEmpty() || !QDir(path).exists()) {
		path = QDir::homePath();
		setTimeShiftFolder(path);
	}

	return path;
}

void DvbManager::setRecordingFolder(const QString &path)
{
	KGlobal::config()->group("DVB").writeEntry("RecordingFolder", path);
}

void DvbManager::setTimeShiftFolder(const QString &path)
{
	KGlobal::config()->group("DVB").writeEntry("TimeShiftFolder", path);
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

void DvbManager::deviceAdded(DvbBackendDevice *backendDevice)
{
	DvbDevice *device = new DvbDevice(backendDevice, this);
	QString deviceId = backendDevice->getDeviceId();
	QString frontendName = backendDevice->getFrontendName();

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
		if (deviceConfigs.at(i).device->getBackendDevice() == backendDevice) {
			if (deviceConfigs[i].useCount != 0) {
				deviceConfigs[i].useCount = 0;
				deviceConfigs[i].prioritizedUseCount = 0;
				deviceConfigs[i].device->release();
			}

			delete deviceConfigs[i].device;
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
			kError() << "couldn't load dvb module" << path;
			return;
		}

		QObject *deviceManager = func();

		if (deviceManager->property("backendMagic") != dvbBackendMagic) {
			kError() << "invalid magic number for dvb module" << path;
			return;
		}

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
	scanSources.clear();
	scanData.clear();

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

	QFile *file = NULL;

	if (!localDate.isNull() && (globalDate.isNull() || (localDate >= globalDate))) {
		file = &localFile;
	} else if (!globalDate.isNull()) {
		file = &globalFile;
	} else {
		scanDataDate = QDate(1900, 1, 1);
		return;
	}

	if (!file->seek(0)) {
		kWarning() << "can't seek" << file->fileName();
		return;
	}

	DvbScanData data(file->readAll());

	QDate date = data.readDate();

	if (scanDataDate < date) {
		scanDataDate = date;
	}

	if (!readScanSources(data, "[dvb-c/", DvbC) ||
	    !readScanSources(data, "[dvb-s/", DvbS) ||
	    !readScanSources(data, "[dvb-t/", DvbT) ||
	    !readScanSources(data, "[atsc/", Atsc) ||
	    !data.checkEnd()) {
		kWarning() << "can't parse" << file->fileName();
	}
}

bool DvbManager::readScanSources(DvbScanData &data, const char *tag, TransmissionType type)
{
	int tagLen = strlen(tag);
	bool parseError = false;

	while (strncmp(data.getLine(), tag, tagLen) == 0) {
		const char *line = data.readLine();

		QString name = QString(line + tagLen);

		if ((name.size() < 2) || (name.at(name.size() - 1) != ']')) {
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
			DvbTransponderBase *transponder = NULL;

			switch (type) {
			case DvbC:
				transponder = new DvbCTransponder;
				break;
			case DvbS:
			case DvbS2:
				if (line[1] == '2') {
					transponder = new DvbS2Transponder;
				} else {
					transponder = new DvbSTransponder;
					containsDvbS1 = true;
				}
				break;
			case DvbT:
				transponder = new DvbTTransponder;
				break;
			case Atsc:
				transponder = new AtscTransponder;
				break;
			}

			if (!transponder->fromString(QString::fromAscii(line))) {
				parseError = true;
				delete transponder;
			} else {
				transponders.append(DvbTransponder(transponder));
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
					if (transponders.at(i)->getTransmissionType() ==
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
		kWarning() << "can't parse complete scan data";
	}

	return true;
}
