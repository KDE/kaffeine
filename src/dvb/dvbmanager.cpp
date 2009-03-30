/*
 * dvbmanager.cpp
 *
 * Copyright (C) 2008 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dvbmanager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <KDebug>
#include <KGlobal>
#include <KLocale>
#include <KStandardDirs>
#include "dvbchannel.h"
#include "dvbchannelview.h"
#include "dvbconfig.h"
#include "dvbdevice.h"
#include "dvbrecording.h"

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
		DvbLineReader reader(QString::fromAscii(pos));
		DvbTransponderBase *transponder = NULL;

		switch (type) {
		case DvbManager::DvbC:
			transponder = reader.readDvbCTransponder();
			break;
		case DvbManager::DvbS:
			transponder = reader.readDvbSTransponder();
			break;
		case DvbManager::DvbT:
			transponder = reader.readDvbTTransponder();
			break;
		case DvbManager::Atsc:
			transponder = reader.readAtscTransponder();
			break;
		}

		if (transponder != NULL) {
			list.append(DvbTransponder(transponder));
		} else {
			parseError = true;
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

DvbManager::DvbManager(QObject *parent) : QObject(parent), scanData(NULL)
{
	channelModel = new DvbChannelModel(this);
	readChannelList();

	recordingModel = new DvbRecordingModel(this);

	readDeviceConfigs();
	updateSourceMapping();

	deviceManager = new DvbDeviceManager(this);
	connect(deviceManager, SIGNAL(deviceAdded(DvbDevice*)),
		this, SLOT(deviceAdded(DvbDevice*)));
	connect(deviceManager, SIGNAL(deviceRemoved(DvbDevice*)),
		this, SLOT(deviceRemoved(DvbDevice*)));

	// coldplug; we can't do it earlier because now the devices are sorted (by index)!
	foreach (DvbDevice *device, getDevices()) {
		if (device->getDeviceState() != DvbDevice::DeviceNotReady) {
			deviceAdded(device);
		}
	}
}

DvbManager::~DvbManager()
{
	writeDeviceConfigs();
	writeChannelList();
	delete scanData;
}

QList<DvbDevice *> DvbManager::getDevices() const
{
	return deviceManager->getDevices();
}

DvbDevice *DvbManager::requestDevice(const DvbTransponder &transponder)
{
	// first try to find a device that is already tuned to the selected transponder

	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.useCount < 1)) {
			continue;
		}

		if (it.transponder->corresponds(transponder)) {
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
			if (config->name == transponder->source) {
				if (!it.device->checkUsable()) {
					break;
				}

				++deviceConfigs[i].useCount;
				deviceConfigs[i].transponder = transponder;

				DvbDevice *device = it.device;
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
				if (!it.device->checkUsable()) {
					break;
				}

				deviceConfigs[i].useCount = -1;

				DvbDevice *device = it.device;
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
				deviceConfigs[i].device->stop();
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

QString DvbManager::getScanFileDate()
{
	if (scanFileDate.isNull()) {
		readScanFile();
	}

	return KGlobal::locale()->formatDate(scanFileDate, KLocale::ShortDate);
}

QStringList DvbManager::getScanSources(TransmissionType type)
{
	Q_ASSERT((type >= 0) && (type <= TransmissionTypeMax));

	if (scanFileDate.isNull()) {
		readScanFile();
	}

	return scanSources[type];
}

QList<DvbTransponder> DvbManager::getTransponders(const QString &source)
{
	if (scanFileDate.isNull()) {
		readScanFile();
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

bool DvbManager::updateScanFile(const QByteArray &data)
{
	if (data.size() < 41) {
		kWarning() << "too small";
		return false;
	}

	if (!DvbScanData(data).readDate().isValid()) {
		kWarning() << "invalid format";
		return false;
	}

	QCryptographicHash hash(QCryptographicHash::Sha1);
	hash.addData(data.constData(), data.size() - 41);

	if (hash.result().toHex() != data.mid(data.size() - 41, 40)) {
		kWarning() << "invalid hash";
		return false;
	}

	QFile file(KStandardDirs::locateLocal("appdata", "scanfile.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		return false;
	}

	file.write(data);

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

void DvbManager::deviceAdded(DvbDevice *device)
{
	QString deviceId = device->getDeviceId();
	QString frontendName = device->getFrontendName();

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

void DvbManager::deviceRemoved(DvbDevice *device)
{
	for (int i = 0; i < deviceConfigs.size(); ++i) {
		if (deviceConfigs.at(i).device == device) {
			deviceConfigs[i].device = NULL;
			break;
		}
	}
}

void DvbManager::readChannelList()
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	QTextStream stream(&file);
	stream.setCodec("UTF-8");
	bool fileOk = true;
	QList<QSharedDataPointer<DvbChannel> > list;

	while (!stream.atEnd()) {
		DvbChannel *channel = DvbLineReader(stream.readLine()).readChannel();

		if (channel == NULL) {
			fileOk = false;
			continue;
		}

		list.append(QSharedDataPointer<DvbChannel>(channel));
	}

	if (!fileOk) {
		kWarning() << "invalid lines in file" << file.fileName();
	}

	kDebug() << "successfully read" << list.size() << "entries";

	channelModel->setList(list);
}

void DvbManager::writeChannelList()
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		return;
	}

	QTextStream stream(&file);
	stream.setCodec("UTF-8");

	foreach (const QSharedDataPointer<DvbChannel> &channel, channelModel->getList()) {
		DvbLineWriter writer;
		writer.writeChannel(channel);
		stream << writer.getLine();
	}
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

void DvbManager::readScanFile()
{
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

	QFile globalFile(KStandardDirs::locate("appdata", "scanfile_g.dvb"));
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
		scanFileDate = localDate;

		if (localFile.seek(0)) {
			scanData = new DvbScanData(localFile.readAll());
		} else {
			kWarning() << "can't seek" << localFile.fileName();
			return;
		}
	} else if (!globalDate.isNull()) {
		// use global file
		scanFileDate = globalDate;

		if (globalFile.seek(0)) {
			scanData = new DvbScanData(globalFile.readAll());
		} else {
			kWarning() << "can't seek" << globalFile.fileName();
			return;
		}
	} else {
		scanFileDate = QDate(1900, 1, 1);
		return;
	}

	if (scanData->readDate() != scanFileDate) {
		kWarning() << "consistency error";
		return;
	}

	if (!scanData->readSources(scanSources[DvbC], scanOffsets[DvbC], "[dvb-c/") ||
	    !scanData->readSources(scanSources[DvbS], scanOffsets[DvbS], "[dvb-s/") ||
	    !scanData->readSources(scanSources[DvbT], scanOffsets[DvbT], "[dvb-t/") ||
	    !scanData->readSources(scanSources[Atsc], scanOffsets[Atsc], "[atsc/") ||
	    !scanData->checkEnd()) {
		kWarning() << "parsing error occurred";
	}
}
