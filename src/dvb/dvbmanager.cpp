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

#include <QFile>
#include <KDebug>
#include <KGlobal>
#include <KLocale>
#include <KStandardDirs>
#include "dvbchannel.h"
#include "dvbchannelview.h"
#include "dvbconfig.h"
#include "dvbdevice.h"

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

	readDeviceConfigs();

	sourceMapping.clear();

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		foreach (const DvbConfig &config, deviceConfig.configs) {
			if (!config->name.isEmpty()) {
				sourceMapping.insert(config->name, config->scanSource);
			}
		}
	}

	sources = sourceMapping.keys();

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

DvbDevice *DvbManager::requestDevice(const QString &source)
{
	for (int i = 0; i < deviceConfigs.size(); ++i) {
		const DvbDeviceConfig &it = deviceConfigs.at(i);

		if ((it.device == NULL) || (it.used)) {
			continue;
		}

		foreach (DvbConfig config, it.configs) {
			if (config->name == source) {
				if (!it.device->checkUsable()) {
					break;
				}

				DvbDevice *device = it.device;
				device->config = config;
				deviceConfigs[i].used = true;
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
			deviceConfigs[i].used = false;
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

	sourceMapping.clear();

	foreach (const DvbDeviceConfig &deviceConfig, deviceConfigs) {
		foreach (const DvbConfig &config, deviceConfig.configs) {
			if (!config->name.isEmpty()) {
				sourceMapping.insert(config->name, config->scanSource);
			}
		}
	}

	sources = sourceMapping.keys();
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

	// FIXME
	int index = scanSources[DvbS].indexOf(sourceMapping.value(source));

	if (index == -1) {
		kWarning() << "invalid source";
		return QList<DvbTransponder>();
	}

	return scanData->readTransponders(scanOffsets[DvbS].at(index), DvbS);
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
	QList<DvbSharedChannel> list;

	while (!stream.atEnd()) {
		DvbChannel *channel = DvbLineReader(stream.readLine()).readChannel();

		if (channel == NULL) {
			fileOk = false;
			continue;
		}

		list.append(DvbSharedChannel(channel));
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
	QList<DvbSharedChannel> list = channelModel->getList();

	for (QList<DvbSharedChannel>::const_iterator it = list.begin(); it != list.end(); ++it) {
		DvbLineWriter writer;
		writer.writeChannel(*it);
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

			QString name = reader.readString("name");
			QString scanSource = reader.readString("scanSource");
			int timeout = reader.readInt("timeout");
			QString type = reader.readString("type");

			if (!reader.isValid()) {
				break;
			}

			if (type == "DvbC") {
				DvbCConfig config;

				config.name = name;
				config.scanSource = scanSource;
				config.timeout = timeout;

				deviceConfig.configs.append(DvbConfig(new DvbCConfig(config)));
			} else if (type == "DvbS") {
				DvbSConfig config(reader.readInt("lnbNumber"));

				config.name = name;
				config.scanSource = scanSource;
				config.timeout = timeout;
				config.lowBandFrequency = reader.readInt("lowBandFrequency");
				config.switchFrequency = reader.readInt("switchFrequency");
				config.highBandFrequency = reader.readInt("highBandFrequency");

				if (!reader.isValid()) {
					break;
				}

				deviceConfig.configs.append(DvbConfig(new DvbSConfig(config)));
			} else if (type == "DvbT") {
				DvbTConfig config;

				config.name = name;
				config.scanSource = scanSource;
				config.timeout = timeout;

				deviceConfig.configs.append(DvbConfig(new DvbTConfig(config)));
			} else if (type == "Atsc") {
				AtscConfig config;

				config.name = name;
				config.scanSource = scanSource;
				config.timeout = timeout;

				deviceConfig.configs.append(DvbConfig(new AtscConfig(config)));
			}
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
			writer.write("name", config->name);
			writer.write("scanSource", config->scanSource);
			writer.write("timeout", config->timeout);

			switch (config->getTransmissionType()) {
			case DvbTransponderBase::DvbC: {
				writer.write("type", "DvbC");
				break;
			    }

			case DvbTransponderBase::DvbS: {
				const DvbSConfig *dvbSConfig = config->getDvbSConfig();

				writer.write("type", "DvbS");
				writer.write("lnbNumber", dvbSConfig->lnbNumber);
				writer.write("lowBandFrequency", dvbSConfig->lowBandFrequency);
				writer.write("switchFrequency", dvbSConfig->switchFrequency);
				writer.write("highBandFrequency", dvbSConfig->highBandFrequency);

				break;
			    }

			case DvbTransponderBase::DvbT: {
				writer.write("type", "DvbT");
				break;
			    }

			case DvbTransponderBase::Atsc: {
				writer.write("type", "Atsc");
				break;
			    }
			}
		}
	}
}

void DvbManager::readScanFile()
{
	QFile localFile(KStandardDirs::locateLocal("appdata", "scanfile.dvb"));
	QDate localDate;

	if (localFile.open(QIODevice::ReadOnly)) {
		localDate = DvbScanData((localFile.read(1024))).readDate();

		if (localDate.isNull()) {
			kWarning() << "can't read" << localFile.fileName();
		}
	} else {
		kDebug() << "can't open" << localFile.fileName();
	}

	QFile globalFile(KStandardDirs::locate("appdata", "scanfile_g.dvb"));
	QDate globalDate;

	if (globalFile.open(QIODevice::ReadOnly)) {
		globalDate = DvbScanData((globalFile.read(1024))).readDate();

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
		kWarning() << "parsing error occured";
	}
}
