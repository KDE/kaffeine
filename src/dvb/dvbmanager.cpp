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
		DvbLineReader reader(readLine());
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

DvbManager::DvbManager(QObject *parent) : QObject(parent)
{
	// FIXME
	sourceList << "Astra-19.2E";
	sourceList << "Hotbird-13.0E";

	// FIXME
	channelModel = new DvbChannelModel(this);
	QList<DvbSharedChannel> list;
	DvbChannel *channel = new DvbChannel;
	channel->name = "sample";
	channel->number = 1;
	channel->source = "Astra19.2E";
	channel->networkId = 1;
	channel->transportStreamId = 1079;
	channel->serviceId = 28006;
	channel->videoPid = 110;
	channel->audioPid = 120;
	DvbSTransponder *transponder = new DvbSTransponder;
	transponder->frequency = 11953000;
	transponder->polarization = DvbSTransponder::Horizontal;
	transponder->symbolRate = 27500000;
	transponder->fecRate = DvbSTransponder::FecAuto;
	channel->transponder = DvbTransponder(transponder);
	list.append(DvbSharedChannel(channel));
	channel = new DvbChannel(*channel);
	channel->name = "channel";
	channel->number = 2;
	list.append(DvbSharedChannel(channel));
	channel = new DvbChannel(*channel);
	channel->name = "test";
	channel->number = 3;
	list.append(DvbSharedChannel(channel));
	channelModel->setList(list);

	deviceManager = new DvbDeviceManager(this);

	scanData = NULL;
}

DvbManager::~DvbManager()
{
	delete scanData;
}

QList<DvbDevice *> DvbManager::getDeviceList() const
{
	return deviceManager->getDeviceList();
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

QList<DvbTransponder> DvbManager::getTransponderList(const QString &source)
{
	if (scanFileDate.isNull()) {
		readScanFile();
	}

	int index = scanSources[DvbS].indexOf(source);

	if (index == -1) {
		kWarning() << "invalid source";
		return QList<DvbTransponder>();
	}

	return scanData->readTransponders(scanOffsets[DvbS].at(index), DvbS);
}

void DvbManager::deviceAdded(DvbDevice *device)
{
	DvbSConfig *config = new DvbSConfig(0);
	config->source = "Astra-19.2E";
	device->configList.append(DvbConfig(config));
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
