/*
 * dvbchannel.cpp
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#include <QFile>
#include <KDebug>
#include <KLocalizedString>
#include <KStandardDirs>

#include "dvbchannel.h"

class DvbChannelReader
{
public:
	DvbChannelReader(QIODevice *device) : begin(0), end(-1), valid(true)
	{
		line = QString::fromUtf8(device->readLine());
	}

	~DvbChannelReader() { }

	bool isValid() const
	{
		return valid;
	}

	template<class T> T readEnum(T max)
	{
		int value = readInt();

		if (value > max) {
			valid = false;
		}

		return static_cast<T> (value);
	}

	int readInt(bool disallowEmpty = true)
	{
		QString string = readString();

		if (string.isEmpty()) {
			if (disallowEmpty) {
				valid = false;
			}

			return -1;
		}

		bool ok;
		int value = string.toInt(&ok);

		if (!ok || (value < 0)) {
			valid = false;
		}

		return value;
	}

	QString readString()
	{
		begin = end + 1;
		end = line.indexOf('|', begin);

		if (end == -1) {
			end = line.size();
		}

		if (begin >= end) {
			return QString();
		}

		return line.mid(begin, end - begin);
	}

private:
	QString line;
	int begin;
	int end;
	bool valid;
};

class DvbChannelWriter
{
public:
	DvbChannelWriter() { }
	~DvbChannelWriter() { }

	void writeFirstInt(int value)
	{
		line.append(QString::number(value));
	}

	void writeInt(int value)
	{
		line.append('|');

		if (value >= 0) {
			line.append(QString::number(value));
		}
	}

	void writeString(const QString &value)
	{
		line.append('|');
		line.append(value);
	}

	void outputLine(QIODevice *device)
	{
		line.append('\n');
		device->write(line.toUtf8());
	}

private:
	QString line;
};

DvbCTransponder *DvbCTransponder::readTransponder(DvbChannelReader &reader)
{
	int frequency = reader.readInt();
	ModulationType modulationType = reader.readEnum(ModulationTypeMax);
	int symbolRate = reader.readInt();
	FecRate fecRate = reader.readEnum(FecRateMax);

	if (!reader.isValid()) {
		return NULL;
	}

	return new DvbCTransponder(frequency, modulationType, symbolRate, fecRate);
}

void DvbCTransponder::writeTransponder(DvbChannelWriter &writer) const
{
	writer.writeInt(frequency);
	writer.writeInt(modulationType);
	writer.writeInt(symbolRate);
	writer.writeInt(fecRate);
}

DvbSTransponder *DvbSTransponder::readTransponder(DvbChannelReader &reader)
{
	Polarization polarization = reader.readEnum(PolarizationMax);
	int frequency = reader.readInt();
	int symbolRate = reader.readInt();
	FecRate fecRate = reader.readEnum(FecRateMax);

	if (!reader.isValid()) {
		return NULL;
	}

	return new DvbSTransponder(polarization, frequency, symbolRate, fecRate);
}

void DvbSTransponder::writeTransponder(DvbChannelWriter &writer) const
{
	writer.writeInt(polarization);
	writer.writeInt(frequency);
	writer.writeInt(symbolRate);
	writer.writeInt(fecRate);
}

DvbTTransponder *DvbTTransponder::readTransponder(DvbChannelReader &reader)
{
	int frequency = reader.readInt();
	ModulationType modulationType = reader.readEnum(ModulationTypeMax);
	Bandwidth bandwidth = reader.readEnum(BandwidthMax);
	FecRate fecRateHigh = reader.readEnum(FecRateMax);
	FecRate fecRateLow = reader.readEnum(FecRateMax);
	TransmissionMode transmissionMode = reader.readEnum(TransmissionModeMax);
	GuardInterval guardInterval = reader.readEnum(GuardIntervalMax);
	Hierarchy hierarchy = reader.readEnum(HierarchyMax);

	if (!reader.isValid()) {
		return NULL;
	}

	return new DvbTTransponder(frequency, modulationType, bandwidth, fecRateHigh, fecRateLow,
		transmissionMode, guardInterval, hierarchy);
}

void DvbTTransponder::writeTransponder(DvbChannelWriter &writer) const
{
	writer.writeInt(frequency);
	writer.writeInt(modulationType);
	writer.writeInt(bandwidth);
	writer.writeInt(fecRateHigh);
	writer.writeInt(fecRateLow);
	writer.writeInt(transmissionMode);
	writer.writeInt(guardInterval);
	writer.writeInt(hierarchy);
}

AtscTransponder *AtscTransponder::readTransponder(DvbChannelReader &reader)
{
	int frequency = reader.readInt();
	ModulationType modulationType = reader.readEnum(ModulationTypeMax);

	if (!reader.isValid()) {
		return NULL;
	}

	return new AtscTransponder(frequency, modulationType);
}

void AtscTransponder::writeTransponder(DvbChannelWriter &writer) const
{
	writer.writeInt(frequency);
	writer.writeInt(modulationType);
}

DvbChannel *DvbChannel::readChannel(DvbChannelReader &reader)
{
	int type = reader.readInt();
	QString name = reader.readString();
	int number = reader.readInt();
	QString source = reader.readString();
	int serviceId = reader.readInt(false);
	int videoPid = reader.readInt(false);

	if (!reader.isValid()) {
		return NULL;
	}

	DvbTransponder *transponder = NULL;

	switch (type) {
	case 0: // DVB-C
		transponder = DvbCTransponder::readTransponder(reader);
		break;
	case 1: // DVB-S
		transponder = DvbSTransponder::readTransponder(reader);
		break;
	case 2: // DVB-T
		transponder = DvbTTransponder::readTransponder(reader);
		break;
	case 3: // ATSC
		transponder = AtscTransponder::readTransponder(reader);
		break;
	}

	if (transponder == NULL) {
		return NULL;
	}

	return new DvbChannel(name, number, source, transponder, serviceId, videoPid);
}

void DvbChannel::writeChannel(DvbChannelWriter &writer) const
{
	int type;

	switch (transponder->transmissionType) {
	case DvbTransponder::DvbC:
		type = 0;
		break;
	case DvbTransponder::DvbS:
		type = 1;
		break;
	case DvbTransponder::DvbT:
		type = 2;
		break;
	case DvbTransponder::Atsc:
		type = 3;
		break;
	default:
		Q_ASSERT(false);
	}

	writer.writeFirstInt(type);
	writer.writeString(name);
	writer.writeInt(number);
	writer.writeString(source);
	writer.writeInt(serviceId);
	writer.writeInt(videoPid);

	transponder->writeTransponder(writer);
}

DvbChannelModel::DvbChannelModel(const QList<DvbSharedChannel> &list_, QObject *parent)
	: QAbstractTableModel(parent), list(list_)
{
}

DvbChannelModel::~DvbChannelModel()
{
}

int DvbChannelModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return list.size();
}

int DvbChannelModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 2;
}

QVariant DvbChannelModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole || index.column() >= 2
	    || index.row() >= list.size()) {
		return QVariant();
	}

	if (index.column() == 0) {
		return list.at(index.row())->name;
	} else {
		return list.at(index.row())->number;
	}
}

QVariant DvbChannelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}

	if (section == 0) {
		return i18n("Name");
	} else {
		return i18n("Number");
	}
}

void DvbChannelModel::setList(const QList<DvbSharedChannel> &list_)
{
	reset();
	list = list_;
}

QList<DvbSharedChannel> DvbChannelModel::readList()
{
	QList<DvbSharedChannel> list;

	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return list;
	}

	bool fileOk = true;

	while (!file.atEnd()) {
		DvbChannelReader line(&file);

		DvbChannel *channel = DvbChannel::readChannel(line);

		if (channel == NULL) {
			fileOk = false;
			continue;
		}

		list.append(DvbSharedChannel(channel));
	}

	if (!fileOk) {
		kWarning() << "file contains invalid lines" << file.fileName();
	}

	kDebug() << "successfully read" << list.size() << "entries";

	return list;
}

void DvbChannelModel::writeList(QList<DvbSharedChannel> list)
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	for (QList<DvbSharedChannel>::const_iterator it = list.begin(); it != list.end(); ++it) {
		DvbChannelWriter writer;

		(*it)->writeChannel(writer);

		writer.outputLine(&file);
	}
}
