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

#include <QContextMenuEvent>
#include <QFile>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <KAction>
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

	bool readChannel(DvbChannel *channel);

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

	void writeChannel(const DvbChannel &channel);

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

bool DvbChannelReader::readChannel(DvbChannel *channel)
{
	int type = readInt();

	channel->name = readString();
	channel->number = readInt();
	channel->source = readString();
	channel->networkId = readInt(false);
	channel->tsId = readInt(false);
	channel->serviceId = readInt(false);
	channel->videoPid = readInt(false);
	channel->videoType = readInt(false);

	if (!isValid()) {
		return false;
	}

	DvbTransponder *transponder = NULL;

	switch (type) {
	case 0: // DVB-C
		transponder = DvbCTransponder::readTransponder(*this);
		break;
	case 1: // DVB-S
		transponder = DvbSTransponder::readTransponder(*this);
		break;
	case 2: // DVB-T
		transponder = DvbTTransponder::readTransponder(*this);
		break;
	case 3: // ATSC
		transponder = AtscTransponder::readTransponder(*this);
		break;
	}

	if (transponder == NULL) {
		return false;
	}

	channel->setTransponder(transponder);

	return true;
}

void DvbChannelWriter::writeChannel(const DvbChannel &channel)
{
	int type;

	switch (channel.getTransponder()->transmissionType) {
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

	writeFirstInt(type);

	writeString(channel.name);
	writeInt(channel.number);
	writeString(channel.source);
	writeInt(channel.networkId);
	writeInt(channel.tsId);
	writeInt(channel.serviceId);
	writeInt(channel.videoPid);
	writeInt(channel.videoType);

	channel.getTransponder()->writeTransponder(*this);
}

DvbChannelModel::DvbChannelModel(QObject *parent) : QAbstractTableModel(parent)
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
		return list.at(index.row()).name;
	} else {
		return list.at(index.row()).number;
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

const DvbChannel *DvbChannelModel::getChannel(const QModelIndex &index) const
{
	if (!index.isValid() || index.row() >= list.size()) {
		return NULL;
	}

	return &list.at(index.row());
}

void DvbChannelModel::setList(const QList<DvbChannel> &list_)
{
	reset();
	list = list_;
}

QList<DvbChannel> DvbChannelModel::readList()
{
	QList<DvbChannel> list;

	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return list;
	}

	bool fileOk = true;

	while (!file.atEnd()) {
		DvbChannelReader channelReader(&file);

		DvbChannel channel;

		if (!channelReader.readChannel(&channel)) {
			fileOk = false;
			continue;
		}

		list.append(channel);
	}

	if (!fileOk) {
		kWarning() << "file contains invalid lines" << file.fileName();
	}

	kDebug() << "successfully read" << list.size() << "entries";

	return list;
}

void DvbChannelModel::writeList(QList<DvbChannel> list)
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	for (QList<DvbChannel>::const_iterator it = list.begin(); it != list.end(); ++it) {
		DvbChannelWriter writer;
		writer.writeChannel(*it);
		writer.outputLine(&file);
	}
}

DvbChannelView::DvbChannelView(QWidget *parent) : QTreeView(parent)
{
	proxyModel = new QSortFilterProxyModel(this);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);

	setIndentation(0);
	QTreeView::setModel(proxyModel);
	setSortingEnabled(true);
	sortByColumn(0, Qt::AscendingOrder);

	menu = new QMenu(this);

	KAction *action = new KAction(i18n("Edit channel"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(actionEdit()));
	menu->addAction(action);

	action = new KAction(i18n("Change icon"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(actionChangeIcon()));
	menu->addAction(action);
}

DvbChannelView::~DvbChannelView()
{
}

void DvbChannelView::setModel(QAbstractItemModel *model)
{
	proxyModel->setSourceModel(model);
}

void DvbChannelView::enableDeleteAction()
{
	KAction *action = new KAction(i18n("Delete channel"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(actionDelete()));
	menu->addAction(action);
}

void DvbChannelView::setFilterRegExp(const QString& string)
{
	proxyModel->setFilterRegExp(string);
}

void DvbChannelView::contextMenuEvent(QContextMenuEvent *event)
{
	QModelIndex index = indexAt(event->pos());

	if (!index.isValid()) {
		return;
	}

	menuIndex = index;
	menu->popup(event->globalPos());
}

void DvbChannelView::actionEdit()
{
	if (!menuIndex.isValid()) {
		return;
	}

	// FIXME
}

void DvbChannelView::actionChangeIcon()
{
	if (!menuIndex.isValid()) {
		return;
	}

	// FIXME
}

void DvbChannelView::actionDelete()
{
	if (!menuIndex.isValid()) {
		return;
	}

	// FIXME
}
