/*
 * dvbchannel.cpp
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbchannel.h"
#include "dvbchannel_p.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFile>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QSpinBox>
#include <KAction>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardDirs>
#include "dvbsi.h"

void DvbChannel::readChannel(QDataStream &stream)
{
	int type;
	stream >> type;

	stream >> name;
	stream >> number;

	stream >> source;

	switch (type) {
	case DvbTransponderBase::DvbC:
		transponder = DvbTransponder(DvbTransponderBase::DvbC);
		transponder.as<DvbCTransponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::DvbS:
		transponder = DvbTransponder(DvbTransponderBase::DvbS);
		transponder.as<DvbSTransponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::DvbS2:
		transponder = DvbTransponder(DvbTransponderBase::DvbS2);
		transponder.as<DvbS2Transponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::DvbT:
		transponder = DvbTransponder(DvbTransponderBase::DvbT);
		transponder.as<DvbTTransponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::Atsc:
		transponder = DvbTransponder(DvbTransponderBase::Atsc);
		transponder.as<AtscTransponder>()->readTransponder(stream);
		break;
	default:
		stream.setStatus(QDataStream::ReadCorruptData);
		return;
	}

	stream >> networkId;
	stream >> transportStreamId;
	int serviceId;
	stream >> serviceId;
	stream >> pmtPid;

	stream >> pmtSectionData;
	int videoPid;
	stream >> videoPid;
	stream >> audioPid;

	int flags;
	stream >> flags;
	hasVideo = (videoPid >= 0);
	isScrambled = (flags & 0x1) != 0;
}

static QString enumToString(DvbTransponderBase::FecRate fecRate)
{
	switch (fecRate) {
	case DvbTransponderBase::FecNone: return "NONE";
	case DvbTransponderBase::Fec1_2: return "1/2";
	case DvbTransponderBase::Fec1_3: return "1/3";
	case DvbTransponderBase::Fec1_4: return "1/4";
	case DvbTransponderBase::Fec2_3: return "2/3";
	case DvbTransponderBase::Fec2_5: return "2/5";
	case DvbTransponderBase::Fec3_4: return "3/4";
	case DvbTransponderBase::Fec3_5: return "3/5";
	case DvbTransponderBase::Fec4_5: return "4/5";
	case DvbTransponderBase::Fec5_6: return "5/6";
	case DvbTransponderBase::Fec6_7: return "6/7";
	case DvbTransponderBase::Fec7_8: return "7/8";
	case DvbTransponderBase::Fec8_9: return "8/9";
	case DvbTransponderBase::Fec9_10: return "9/10";
	case DvbTransponderBase::FecAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbCTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbCTransponder::Qam16: return "16-QAM";
	case DvbCTransponder::Qam32: return "32-QAM";
	case DvbCTransponder::Qam64: return "64-QAM";
	case DvbCTransponder::Qam128: return "128-QAM";
	case DvbCTransponder::Qam256: return "256-QAM";
	case DvbCTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbSTransponder::Polarization polarization)
{
	switch (polarization) {
	case DvbSTransponder::Horizontal: return i18n("Horizontal");
	case DvbSTransponder::Vertical: return i18n("Vertical");
	case DvbSTransponder::CircularLeft: return i18n("Circular left");
	case DvbSTransponder::CircularRight: return i18n("Circular right");
	}

	return QString();
}

static QString enumToString(DvbS2Transponder::Modulation modulation)
{
	switch (modulation) {
	case DvbS2Transponder::Qpsk: return "QPSK";
	case DvbS2Transponder::Psk8: return "8-PSK";
	case DvbS2Transponder::Apsk16: return "16-APSK";
	case DvbS2Transponder::Apsk32: return "32-APSK";
	case DvbS2Transponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbS2Transponder::RollOff rollOff)
{
	switch (rollOff) {
	case DvbS2Transponder::RollOff20: return "0.20";
	case DvbS2Transponder::RollOff25: return "0.25";
	case DvbS2Transponder::RollOff35: return "0.35";
	case DvbS2Transponder::RollOffAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbTTransponder::Bandwidth6MHz: return "6MHz";
	case DvbTTransponder::Bandwidth7MHz: return "7MHz";
	case DvbTTransponder::Bandwidth8MHz: return "8MHz";
	case DvbTTransponder::BandwidthAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbTTransponder::Qpsk: return "QPSK";
	case DvbTTransponder::Qam16: return "16-QAM";
	case DvbTTransponder::Qam64: return "64-QAM";
	case DvbTTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::TransmissionMode transmissionMode)
{
	switch (transmissionMode) {
	case DvbTTransponder::TransmissionMode2k: return "2k";
	case DvbTTransponder::TransmissionMode4k: return "4k";
	case DvbTTransponder::TransmissionMode8k: return "8k";
	case DvbTTransponder::TransmissionModeAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case DvbTTransponder::GuardInterval1_4: return "1/4";
	case DvbTTransponder::GuardInterval1_8: return "1/8";
	case DvbTTransponder::GuardInterval1_16: return "1/16";
	case DvbTTransponder::GuardInterval1_32: return "1/32";
	case DvbTTransponder::GuardIntervalAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Hierarchy hierarchy)
{
	switch (hierarchy) {
	case DvbTTransponder::HierarchyNone: return "NONE";
	case DvbTTransponder::Hierarchy1: return "1";
	case DvbTTransponder::Hierarchy2: return "2";
	case DvbTTransponder::Hierarchy4: return "4";
	case DvbTTransponder::HierarchyAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(AtscTransponder::Modulation modulation)
{
	switch (modulation) {
	case AtscTransponder::Qam64: return "64-QAM";
	case AtscTransponder::Qam256: return "256-QAM";
	case AtscTransponder::Vsb8: return "8-VSB";
	case AtscTransponder::Vsb16: return "16-VSB";
	case AtscTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

bool DvbChannelLessThan::operator()(const DvbChannel &x, const DvbChannel &y) const
{
	switch (sortOrder) {
	case ChannelNameAscending:
		return (x.name.localeAwareCompare(y.name) < 0);
	case ChannelNameDescending:
		return (x.name.localeAwareCompare(y.name) > 0);
	case ChannelNumberAscending:
		return (x.number < y.number);
	case ChannelNumberDescending:
		return (x.number > y.number);
	}

	return false;
}

bool DvbComparableChannel::operator==(const DvbComparableChannel &other) const
{
	if ((channel->source != other.channel->source) ||
	    (channel->transponder.getTransmissionType() !=
	     other.channel->transponder.getTransmissionType()) ||
	    (channel->networkId != other.channel->networkId)) {
		return false;
	}

	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
		return ((channel->transportStreamId == other.channel->transportStreamId) &&
			(channel->getServiceId() == other.channel->getServiceId()));
	case DvbTransponderBase::Atsc:
		return true;
	}

	return false;
}

uint qHash(const DvbComparableChannel &channel)
{
	uint hash = (qHash(channel.channel->source) ^ qHash(channel.channel->networkId));

	switch (channel.channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
		hash ^= (qHash(channel.channel->transportStreamId) << 8);
		hash ^= (qHash(channel.channel->getServiceId()) << 16);
		break;
	case DvbTransponderBase::Atsc:
		break;
	}

	return hash;
}

DvbChannelModel::DvbChannelModel(QObject *parent) : QObject(parent), hasPendingOperation(false),
	isSqlModel(false)
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
}

DvbChannelModel::~DvbChannelModel()
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
}

QMap<int, DvbSharedChannel> DvbChannelModel::getChannels() const
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	return channelNumberMapping;
}

DvbSharedChannel DvbChannelModel::findChannelByName(const QString &channelName) const
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	return channelNameMapping.value(channelName);
}

DvbSharedChannel DvbChannelModel::findChannelByNumber(int channelNumber) const
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	return channelNumberMapping.value(channelNumber);
}

DvbSharedChannel DvbChannelModel::findChannelByContent(const DvbChannel &channel) const
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	return channelContentMapping.value(DvbComparableChannel(&channel));
}

void DvbChannelModel::cloneFrom(DvbChannelModel *other)
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);

	if (isSqlModel == other->isSqlModel) {
		kError() << "cloning is only supported between sql model and auxiliary model";
		return;
	}

	QMultiMap<SqlKey, DvbSharedChannel> channelKeys;

	foreach (const DvbSharedChannel &channel, channelNameMapping) {
		channelKeys.insert(*channel, channel);
	}

	QMultiMap<SqlKey, DvbSharedChannel> otherChannelKeys;

	foreach (const DvbSharedChannel &channel, other->channelNameMapping) {
		otherChannelKeys.insert(*channel, channel);
	}

	typedef QMultiMap<SqlKey, DvbSharedChannel>::ConstIterator ConstIterator;
	typedef QMultiMap<SqlKey, DvbSharedChannel>::Iterator Iterator;

	for (ConstIterator it = channelKeys.constBegin(); it != channelKeys.constEnd(); ++it) {
		const DvbSharedChannel &channel = *it;
		Iterator otherIt = otherChannelKeys.find(it.key());

		if (ConstIterator(otherIt) != otherChannelKeys.constEnd()) {
			const DvbSharedChannel &otherChannel = *otherIt;

			if (channel != otherChannel) {
				internalUpdateChannel(channel, *otherChannel);
			}

			otherChannelKeys.erase(otherIt);
		} else {
			internalRemoveChannel(channel);
		}
	}

	foreach (const DvbSharedChannel &channel, otherChannelKeys) {
		internalAddChannel(channel);
	}
}

void DvbChannelModel::addChannel(const DvbChannel &channel)
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	QHash<DvbComparableChannel, DvbSharedChannel>::ConstIterator it =
		channelContentMapping.constFind(DvbComparableChannel(&channel));
	DvbPmtParser pmtParser(DvbPmtSection(channel.pmtSectionData));
	int audioPid = -1;

	if (!pmtParser.audioPids.isEmpty()) {
		audioPid = pmtParser.audioPids.at(0).first;
	}

	if (it != channelContentMapping.constEnd()) {
		const DvbSharedChannel &existingChannel = *it;
		DvbChannel updatedChannel = channel;

		if (updatedChannel.name == extractBaseName(existingChannel->name)) {
			updatedChannel.name = existingChannel->name;
		} else {
			updatedChannel.name = findNextFreeChannelName(updatedChannel.name);
		}

		updatedChannel.number = existingChannel->number;
		updatedChannel.audioPid = audioPid;

		for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
			if (pmtParser.audioPids.at(i).first == existingChannel->audioPid) {
				updatedChannel.audioPid = existingChannel->audioPid;
				break;
			}
		}

		internalUpdateChannel(existingChannel, updatedChannel);
	} else {
		DvbChannel *newChannel = new DvbChannel(channel);
		newChannel->name = findNextFreeChannelName(channel.name);
		newChannel->number = findNextFreeChannelNumber(channel.number);
		newChannel->audioPid = audioPid;
		internalAddChannel(DvbSharedChannel(newChannel));
	}
}

void DvbChannelModel::updateChannel(const DvbSharedChannel &channel,
	const DvbChannel &updatedChannel)
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);

	if (channel->name != updatedChannel.name) {
		DvbSharedChannel existingChannel = channelNameMapping.value(updatedChannel.name);

		if (existingChannel.isValid()) {
			DvbChannel modifiedChannel = *existingChannel;
			modifiedChannel.name = findNextFreeChannelName(modifiedChannel.name);
			internalUpdateChannel(existingChannel, modifiedChannel);
		}
	}

	if (channel->number != updatedChannel.number) {
		DvbSharedChannel existingChannel =
			channelNumberMapping.value(updatedChannel.number);

		if (existingChannel.isValid()) {
			DvbChannel modifiedChannel = *existingChannel;
			modifiedChannel.number = findNextFreeChannelNumber(modifiedChannel.number);
			internalUpdateChannel(existingChannel, modifiedChannel);
		}
	}

	internalUpdateChannel(channel, updatedChannel);
}

void DvbChannelModel::removeChannel(const DvbSharedChannel &channel)
{
	internalRemoveChannel(channel);
}

void DvbChannelModel::dndMoveChannels(const QList<DvbSharedChannel> &selectedChannels,
	const DvbSharedChannel &insertBeforeChannel)
{
	DvbChannelEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	int newNumberBegin = 1;

	if (insertBeforeChannel.isValid()) {
		QMap<int, DvbSharedChannel>::ConstIterator it =
			channelNumberMapping.constFind(insertBeforeChannel->number);

		if (it != channelNumberMapping.constBegin()) {
			newNumberBegin = ((it - 1).key() + 1);
		}
	} else {
		QMap<int, DvbSharedChannel>::ConstIterator it = channelNumberMapping.constEnd();

		if (it != channelNumberMapping.constBegin()) {
			newNumberBegin = ((it - 1).key() + 1);
		}
	}

	int newNumberEnd = (newNumberBegin + selectedChannels.size());

	for (int i = 0; i < selectedChannels.size(); ++i) {
		const DvbSharedChannel &selectedChannel = selectedChannels.at(i);
		int number = (newNumberBegin + i);
		DvbSharedChannel existingChannel = channelNumberMapping.value(number);

		if (existingChannel.isValid() && (existingChannel != selectedChannel)) {
			DvbChannel modifiedChannel = *existingChannel;
			modifiedChannel.number = findNextFreeChannelNumber(newNumberEnd);
			internalUpdateChannel(existingChannel, modifiedChannel);
		}

		if (selectedChannel->number != number) {
			DvbChannel modifiedChannel = *selectedChannel;
			modifiedChannel.number = number;
			internalUpdateChannel(selectedChannel, modifiedChannel);
		}
	}
}

void DvbChannelModel::bindToSqlQuery(SqlKey sqlKey, QSqlQuery &query, int index) const
{
	DvbSharedChannel channel = channels.value(sqlKey);

	if (!channel.isValid()) {
		kError() << "invalid channel";
		return;
	}

	query.bindValue(index++, channel->name);
	query.bindValue(index++, channel->number);
	query.bindValue(index++, channel->source);
	query.bindValue(index++, channel->transponder.toString());
	query.bindValue(index++, channel->networkId);
	query.bindValue(index++, channel->transportStreamId);
	query.bindValue(index++, channel->pmtPid);
	query.bindValue(index++, channel->pmtSectionData);
	query.bindValue(index++, channel->audioPid);
	query.bindValue(index++, (channel->hasVideo ? 0x01 : 0) |
		(channel->isScrambled ? 0x02 : 0));
}

bool DvbChannelModel::insertFromSqlQuery(SqlKey sqlKey, const QSqlQuery &query, int index)
{
	DvbSharedChannel sharedChannel(new DvbChannel());
	DvbChannel *channel = sharedChannel.data();
	channel->setSqlKey(sqlKey);
	channel->name = query.value(index++).toString();
	channel->number = query.value(index++).toInt();
	channel->source = query.value(index++).toString();
	channel->transponder = DvbTransponder::fromString(query.value(index++).toString());
	channel->networkId = query.value(index++).toInt();
	channel->transportStreamId = query.value(index++).toInt();
	channel->pmtPid = query.value(index++).toInt();
	channel->pmtSectionData = query.value(index++).toByteArray();
	channel->audioPid = query.value(index++).toInt();
	int flags = query.value(index++).toInt();
	channel->hasVideo = ((flags & 0x01) != 0);
	channel->isScrambled = ((flags & 0x02) != 0);

	if (!channel->name.isEmpty() && !channelNameMapping.contains(channel->name) &&
	    (channel->number >= 1) && !channelNumberMapping.contains(channel->number) &&
	    !channel->source.isEmpty() && channel->transponder.isValid() &&
	    (channel->networkId >= -1) && (channel->networkId <= 0xffff) &&
	    (channel->transportStreamId >= 0) && (channel->transportStreamId <= 0xffff) &&
	    (channel->pmtPid >= 0) && (channel->pmtPid <= 0x1fff) &&
	    !channel->pmtSectionData.isEmpty() &&
	    (channel->audioPid >= -1) && (channel->audioPid <= 0x1fff)) {
		channels.insert(*sharedChannel, sharedChannel);
		channelNameMapping.insert(sharedChannel->name, sharedChannel);
		channelNumberMapping.insert(sharedChannel->number, sharedChannel);
		channelContentMapping.insert(DvbComparableChannel(sharedChannel), sharedChannel);
		return true;
	}

	return false;
}

QString DvbChannelModel::extractBaseName(const QString &name) const
{
	QString baseName = name;
	int position = baseName.lastIndexOf('-');

	if (position > 0) {
		QString suffix = baseName.mid(position + 1);

		if (suffix == QString::number(suffix.toInt())) {
			baseName.truncate(position);
		}
	}

	return baseName;
}

QString DvbChannelModel::findNextFreeChannelName(const QString &name) const
{
	if (!channelNameMapping.contains(name)) {
		return name;
	}

	QString baseName = extractBaseName(name);
	int suffix = 0;
	QString newName = baseName;

	while (channelNameMapping.contains(newName)) {
		++suffix;
		newName = baseName + '-' + QString::number(suffix);
	}

	return newName;
}

int DvbChannelModel::findNextFreeChannelNumber(int number) const
{
	if (number < 1) {
		number = 1;
	}

	while (channelNumberMapping.contains(number)) {
		++number;
	}

	return number;
}

void DvbChannelModel::internalAddChannel(const DvbSharedChannel &channel)
{
	channelNameMapping.insert(channel->name, channel);
	channelNumberMapping.insert(channel->number, channel);
	channelContentMapping.insert(DvbComparableChannel(channel), channel);

	if (isSqlModel) {
		channel.data()->setSqlKey(sqlFindFreeKey(channels));
		sqlInsert(*channel);
	}

	emit channelAdded(channel);
}

void DvbChannelModel::internalUpdateChannel(const DvbSharedChannel &channel,
	const DvbChannel &modifiedChannel)
{
	if (!isSqlModel && channel->isSqlKeyValid()) {
		channel.detach();
	}

	DvbChannel oldChannel = *channel;
	*channel.data() = modifiedChannel;

	if (channel->name != oldChannel.name) {
		channelNameMapping.remove(oldChannel.name);
		channelNameMapping.insert(channel->name, channel);
	}

	if (channel->number != oldChannel.number) {
		channelNumberMapping.remove(oldChannel.number);
		channelNumberMapping.insert(channel->number, channel);
	}

	if (DvbComparableChannel(channel) != DvbComparableChannel(&oldChannel)) {
		channelContentMapping.remove(DvbComparableChannel(&oldChannel), channel);
		channelContentMapping.insert(DvbComparableChannel(channel), channel);
	}

	if (isSqlModel) {
		sqlUpdate(*channel);
	}

	emit channelUpdated(channel, oldChannel);
}

void DvbChannelModel::internalRemoveChannel(const DvbSharedChannel &channel)
{
	channelNameMapping.remove(channel->name);
	channelNumberMapping.remove(channel->number);
	channelContentMapping.remove(DvbComparableChannel(channel), channel);

	if (isSqlModel) {
		sqlRemove(*channel);
	}

	emit channelRemoved(channel);
}

void DvbChannelEnsureNoPendingOperation::printFatalErrorMessage()
{
	kFatal() << "illegal recursive call";
}

DvbChannelTableModel::DvbChannelTableModel(DvbChannelModel *channelModel_, QObject *parent) :
	QAbstractTableModel(parent), channelModel(channelModel_), dndEventPosted(false)
{
	connect(channelModel, SIGNAL(channelAdded(DvbSharedChannel)),
		this, SLOT(channelAdded(DvbSharedChannel)));
	connect(channelModel, SIGNAL(channelUpdated(DvbSharedChannel,DvbChannel)),
		this, SLOT(channelUpdated(DvbSharedChannel,DvbChannel)));
	connect(channelModel, SIGNAL(channelRemoved(DvbSharedChannel)),
		this, SLOT(channelRemoved(DvbSharedChannel)));

	foreach (const DvbSharedChannel &channel, channelModel->getChannels()) {
		channels.append(channel);
	}

	qSort(channels.begin(), channels.end(), lessThan);
}

DvbChannelTableModel::~DvbChannelTableModel()
{
}

DvbChannelModel *DvbChannelTableModel::getChannelModel() const
{
	return channelModel;
}

const DvbSharedChannel &DvbChannelTableModel::getChannel(const QModelIndex &index) const
{
	return channels.at(index.row());
}

QModelIndex DvbChannelTableModel::indexForChannel(const DvbSharedChannel &channel) const
{
	int row = (qBinaryFind(channels.constBegin(), channels.constEnd(), channel, lessThan)
		- channels.constBegin());

	if (row < channels.size()) {
		return index(row,  0);
	}

	return QModelIndex();
}

int DvbChannelTableModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 2;
	}

	return 0;
}

int DvbChannelTableModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return channels.size();
	}

	return 0;
}

QVariant DvbChannelTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		switch (section) {
		case 0:
			return i18n("Name");
		case 1:
			return i18n("Number");
		}
	}

	return QVariant();
}

QVariant DvbChannelTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedChannel &channel = channels.at(index.row());

	switch (role) {
	case Qt::DecorationRole:
		if (index.column() == 0) {
			if (channel->hasVideo) {
				if (!channel->isScrambled) {
					return KIcon("video-television");
				} else {
					return KIcon("video-television-encrypted");
				}
			} else {
				if (!channel->isScrambled) {
					return KIcon("text-speak");
				} else {
					return KIcon("audio-radio-encrypted");
				}
			}
		}

		break;
	case Qt::DisplayRole:
		switch (index.column()) {
		case 0:
			return channel->name;
		case 1:
			return channel->number;
		}

		break;
	}

	return QVariant();
}

Qt::ItemFlags DvbChannelTableModel::flags(const QModelIndex &index) const
{
	if (index.isValid()) {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDragEnabled);
	} else {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDropEnabled);
	}
}

QMimeData *DvbChannelTableModel::mimeData(const QModelIndexList &indexes) const
{
	QList<DvbSharedChannel> selectedChannels;

	foreach (const QModelIndex &index, indexes) {
		const DvbSharedChannel &channel = channels.at(index.row());

		if (selectedChannels.isEmpty() ||
		    (selectedChannels.at(selectedChannels.size() - 1) != channel)) {
			selectedChannels.append(channel);
		}
	}

	QMimeData *mimeData = new QMimeData();
	mimeData->setData("application/x-org.kde.kaffeine-selectedchannels", QByteArray());
	mimeData->setProperty("SelectedChannels", QVariant::fromValue(selectedChannels));
	return mimeData;
}

QStringList DvbChannelTableModel::mimeTypes() const
{
	return QStringList("application/x-org.kde.kaffeine-selectedchannels");
}

Qt::DropActions DvbChannelTableModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QModelIndex DvbChannelTableModel::index(int row, int column, const QModelIndex &parent) const
{
	if ((row >= 0) && (row < channels.size()) && (column >= 0) && (column <= 1) &&
	    !parent.isValid()) {
		return createIndex(row, column, const_cast<void *>(static_cast<const void *>(
			channels.at(row).constData())));
	}

	return QModelIndex();
}

bool DvbChannelTableModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
	int column, const QModelIndex &parent)
{
	Q_UNUSED(column)
	Q_UNUSED(parent)
	dndSelectedChannels.clear();
	dndInsertBeforeChannel = DvbSharedChannel();

	if (action == Qt::MoveAction) {
		dndSelectedChannels =
			data->property("SelectedChannels").value<QList<DvbSharedChannel> >();

		if (row >= 0) {
			dndInsertBeforeChannel = channels.at(row);
		}

		if (!dndSelectedChannels.isEmpty() && !dndEventPosted) {
			dndEventPosted = true;
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		}
	}

	return false;
}

void DvbChannelTableModel::sort(int column, Qt::SortOrder order)
{
	DvbChannelLessThan::SortOrder sortOrder;

	if (order == Qt::AscendingOrder) {
		if (column == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameAscending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberAscending;
		}
	} else {
		if (column == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameDescending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberDescending;
		}
	}

	if (lessThan.sortOrder != sortOrder) {
		lessThan.sortOrder = sortOrder;
		emit layoutChanged();
		qSort(channels.begin(), channels.end(), lessThan);
		QModelIndexList oldPersistentIndexes = persistentIndexList();
		QModelIndexList newPersistentIndexes;

		foreach (const QModelIndex &oldIndex, oldPersistentIndexes) {
			if (oldIndex.isValid()) {
				const DvbChannel &channel = *static_cast<const DvbChannel *>(
					oldIndex.internalPointer());
				int row = (qBinaryFind(channels.constBegin(), channels.constEnd(),
					channel, lessThan) - channels.constBegin());
				newPersistentIndexes.append(index(row, oldIndex.column()));
			} else {
				newPersistentIndexes.append(QModelIndex());
			}
		}

		changePersistentIndexList(oldPersistentIndexes, newPersistentIndexes);
		emit layoutChanged();
	}
}

void DvbChannelTableModel::setFilter(const QString &filter)
{
	QStringMatcher matcher(filter, Qt::CaseInsensitive);
	emit layoutChanged();
	channels.clear();

	foreach (const DvbSharedChannel &channel, channelModel->getChannels()) {
		if (matcher.indexIn(channel->name) >= 0) {
			channels.append(channel);
		}
	}

	qSort(channels.begin(), channels.end(), lessThan);
	QModelIndexList oldPersistentIndexes = persistentIndexList();
	QModelIndexList newPersistentIndexes;

	foreach (const QModelIndex &oldIndex, oldPersistentIndexes) {
		if (oldIndex.isValid()) {
			const DvbChannel &channel =
				*static_cast<const DvbChannel *>(oldIndex.internalPointer());
			int row = (qBinaryFind(channels.constBegin(), channels.constEnd(), channel,
				lessThan) - channels.constBegin());
			newPersistentIndexes.append(index(row, oldIndex.column()));
		} else {
			newPersistentIndexes.append(QModelIndex());
		}
	}

	changePersistentIndexList(oldPersistentIndexes, newPersistentIndexes);
	emit layoutChanged();
}

void DvbChannelTableModel::channelAdded(const DvbSharedChannel &channel)
{
	int row = (qLowerBound(channels.constBegin(), channels.constEnd(), channel, lessThan)
		- channels.constBegin());

	if ((row >= channels.size()) || (channels.at(row) != channel)) {
		beginInsertRows(QModelIndex(), row, row);
		channels.insert(row, channel);
		endInsertRows();
	}
}

void DvbChannelTableModel::channelUpdated(const DvbSharedChannel &channel,
	const DvbChannel &oldChannel)
{
	int row = (qBinaryFind(channels.constBegin(), channels.constEnd(), oldChannel, lessThan)
		- channels.constBegin());

	if (row < channels.size()) {
		channels.replace(row, channel);
		emit dataChanged(index(row, 0), index(row, 1));
	}
}

void DvbChannelTableModel::channelRemoved(const DvbSharedChannel &channel)
{
	int row = (qBinaryFind(channels.constBegin(), channels.constEnd(), channel, lessThan)
		- channels.constBegin());

	if (row < channels.size()) {
		beginRemoveRows(QModelIndex(), row, row);
		channels.removeAt(row);
		endRemoveRows();
	}
}

void DvbChannelTableModel::customEvent(QEvent *event)
{
	Q_UNUSED(event)
	dndEventPosted = false;
	bool ok = true;
	emit checkChannelDragAndDrop(&ok);

	if (ok && !dndSelectedChannels.isEmpty()) {
		channelModel->dndMoveChannels(dndSelectedChannels, dndInsertBeforeChannel);
	}
}

DvbChannelView::DvbChannelView(QWidget *parent) : QTreeView(parent), tableModel(NULL)
{
}

DvbChannelView::~DvbChannelView()
{
}

KAction *DvbChannelView::addEditAction()
{
	KAction *action = new KAction(KIcon("configure"), i18n("Edit"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(editChannel()));
	addAction(action);
	return action;
}

KAction *DvbChannelView::addRemoveAction()
{
	KAction *action = new KAction(KIcon("edit-delete"),
		i18nc("remove an item from a list", "Remove"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(removeChannel()));
	addAction(action);
	return action;
}

void DvbChannelView::setModel(DvbChannelTableModel *tableModel_)
{
	tableModel = tableModel_;
	QTreeView::setModel(tableModel);
}

void DvbChannelView::checkChannelDragAndDrop(bool *ok)
{
	if ((*ok) && ((header()->sortIndicatorSection() != 1) ||
	    (header()->sortIndicatorOrder() != Qt::AscendingOrder))) {
		if (KMessageBox::warningContinueCancel(this, i18nc("message box",
			"The channels will be sorted by number to allow drag and drop.\n"
			"Do you want to continue?")) == KMessageBox::Continue) {
			sortByColumn(1, Qt::AscendingOrder);
		} else {
			*ok = false;
		}
	}
}

void DvbChannelView::editChannel()
{
	QModelIndex index = currentIndex();

	if (index.isValid()) {
		KDialog *dialog = new DvbChannelEditor(tableModel, index, this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
	}
}

void DvbChannelView::removeChannel()
{
	QSet<DvbSharedChannel> selectedChannels;

	foreach (const QModelIndex &modelIndex, selectionModel()->selectedIndexes()) {
		selectedChannels.insert(tableModel->getChannel(modelIndex));
	}

	DvbChannelModel *channelModel = tableModel->getChannelModel();

	foreach (const DvbSharedChannel &channel, selectedChannels) {
		channelModel->removeChannel(channel);
	}
}

void DvbChannelView::removeAllChannels()
{
	DvbChannelModel *channelModel = tableModel->getChannelModel();

	foreach (const DvbSharedChannel &channel, channelModel->getChannels()) {
		channelModel->removeChannel(channel);
	}
}

DvbSqlChannelModel::DvbSqlChannelModel(QObject *parent) : DvbChannelModel(parent)
{
	sqlInit("Channels",
		QStringList() << "Name" << "Number" << "Source" << "Transponder" << "NetworkId" <<
		"TransportStreamId" << "PmtPid" << "PmtSection" << "AudioPid" << "Flags");
	isSqlModel = true;

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", "channels.dtv"));

	if (!file.exists()) {
		return;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "cannot open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	while (!stream.atEnd()) {
		DvbChannel channel;
		channel.readChannel(stream);

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid channels in file" << file.fileName();
			break;
		}

		addChannel(channel);
	}

	if (!file.remove()) {
		kWarning() << "cannot remove" << file.fileName();
	}
}

DvbSqlChannelModel::~DvbSqlChannelModel()
{
	sqlFlush();
}

DvbChannelEditor::DvbChannelEditor(DvbChannelTableModel *model_, const QModelIndex &modelIndex_,
	QWidget *parent) : KDialog(parent), model(model_), persistentIndex(modelIndex_)
{
	setCaption(i18n("Channel Settings"));
	channel = model->getChannel(persistentIndex);

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);
	QGridLayout *gridLayout = new QGridLayout();

	nameEdit = new KLineEdit(widget);
	nameEdit->setText(channel->name);
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18n("Name:"), widget);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	numberBox = new QSpinBox(widget);
	numberBox->setRange(1, 99999);
	numberBox->setValue(channel->number);
	gridLayout->addWidget(numberBox, 0, 3);

	label = new QLabel(i18n("Number:"), widget);
	label->setBuddy(numberBox);
	gridLayout->addWidget(label, 0, 2);
	mainLayout->addLayout(gridLayout);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QGroupBox *groupBox = new QGroupBox(widget);
	gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18n("Source:")), 0, 0);
	gridLayout->addWidget(new QLabel(channel->source), 0, 1);

	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *tp = channel->transponder.as<DvbCTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Symbol rate (kS/s):")), 2, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->symbolRate / 1000.0)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 3, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRate)), 4, 1);
		break;
	    }
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2: {
		const DvbS2Transponder *tp2 = channel->transponder.as<DvbS2Transponder>();
		const DvbSTransponder *tp = channel->transponder.as<DvbSTransponder>();

		if (tp == NULL) {
			tp = tp2;
		}

		gridLayout->addWidget(new QLabel(i18n("Polarization:")), 1, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->polarization)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 2, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->frequency / 1000.0)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Symbol rate (kS/s):")), 3, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->symbolRate / 1000.0)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRate)), 4, 1);

		if (tp2 != NULL) {
			gridLayout->addWidget(new QLabel(i18n("Modulation:")), 5, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp2->modulation)), 5, 1);
			gridLayout->addWidget(new QLabel(i18n("Roll-off:")), 6, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp2->rollOff)), 6, 1);
		}

		break;
	    }
	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *tp = channel->transponder.as<DvbTTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Bandwidth:")), 2, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->bandwidth)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 3, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRateHigh)), 4, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate LP:")), 5, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRateLow)), 5, 1);
		gridLayout->addWidget(new QLabel(i18n("Transmission mode:")), 6, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->transmissionMode)), 6, 1);
		gridLayout->addWidget(new QLabel(i18n("Guard interval:")), 7, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->guardInterval)), 7, 1);
		gridLayout->addWidget(new QLabel(i18n("Hierarchy:")), 8, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->hierarchy)), 8, 1);
		break;
	    }
	case DvbTransponderBase::Atsc: {
		const AtscTransponder *tp = channel->transponder.as<AtscTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 2, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 2, 1);
		break;
	    }
	}

	gridLayout->addWidget(new QLabel(), 10, 0, 1, 2);

	gridLayout->addWidget(new QLabel(i18n("PMT PID:")), 11, 0);
	gridLayout->addWidget(new QLabel(QString::number(channel->pmtPid)), 11, 1);

	DvbPmtParser pmtParser(DvbPmtSection(channel->pmtSectionData));
	int row = 12;

	if (pmtParser.videoPid >= 0) {
		gridLayout->addWidget(new QLabel(i18n("Video PID:")), row, 0);
		gridLayout->addWidget(new QLabel(QString::number(pmtParser.videoPid)), row++, 1);
	}

	if (!pmtParser.subtitlePids.isEmpty()) {
		gridLayout->addWidget(new QLabel(i18n("Subtitle PID:")), row, 0);
	}

	for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.subtitlePids.at(i);
		gridLayout->addWidget(new QLabel(QString("%1 (%2)").arg(it.first).arg(it.second)),
			row++, 1);
	}

	if (pmtParser.teletextPid != -1) {
		gridLayout->addWidget(new QLabel(i18n("Teletext PID:")), row, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(pmtParser.teletextPid)), row++, 1);
	}

	gridLayout->addItem(new QSpacerItem(0, 0), row, 0, 1, 2);
	gridLayout->setRowStretch(row, 1);
	boxLayout->addWidget(groupBox);

	groupBox = new QGroupBox(widget);
	gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18n("Network ID:")), 0, 0);

	networkIdBox = new QSpinBox(groupBox);
	networkIdBox->setRange(-1, (1 << 16) - 1);
	networkIdBox->setValue(channel->networkId);
	gridLayout->addWidget(networkIdBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("Transport stream ID:")), 1, 0);

	transportStreamIdBox = new QSpinBox(groupBox);
	transportStreamIdBox->setRange(0, (1 << 16) - 1);
	transportStreamIdBox->setValue(channel->transportStreamId);
	gridLayout->addWidget(transportStreamIdBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Service ID:")), 2, 0);

	serviceIdBox = new QSpinBox(groupBox);
	serviceIdBox->setRange(0, (1 << 16) - 1);
	serviceIdBox->setValue(channel->getServiceId());
	gridLayout->addWidget(serviceIdBox, 2, 1);

	gridLayout->addWidget(new QLabel(i18n("Audio channel:")), 3, 0);

	audioChannelBox = new KComboBox(groupBox);

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.audioPids.at(i);
		QString text = QString::number(it.first);

		if (!it.second.isEmpty()) {
			text = text + " (" + it.second + ')';
		}

		audioChannelBox->addItem(text);
		audioPids.append(it.first);
	}

	audioChannelBox->setCurrentIndex(audioPids.indexOf(channel->audioPid));

	if (audioPids.size() <= 1) {
		audioChannelBox->setEnabled(false);
	}

	gridLayout->addWidget(audioChannelBox, 3, 1);

	gridLayout->addWidget(new QLabel(i18n("Scrambled:")), 4, 0);

	scrambledBox = new QCheckBox(groupBox);
	scrambledBox->setChecked(channel->isScrambled);
	gridLayout->addWidget(scrambledBox, 4, 1);

	gridLayout->addItem(new QSpacerItem(0, 0), 5, 0, 1, 2);
	gridLayout->setRowStretch(5, 1);
	boxLayout->addWidget(groupBox);
	mainLayout->addLayout(boxLayout);

	setMainWidget(widget);
}

DvbChannelEditor::~DvbChannelEditor()
{
}

void DvbChannelEditor::accept()
{
	if (persistentIndex.isValid()) {
		DvbChannel updatedChannel = *channel;
		updatedChannel.name = nameEdit->text();
		updatedChannel.number = numberBox->value();
		updatedChannel.networkId = networkIdBox->value();
		updatedChannel.transportStreamId = transportStreamIdBox->value();
		updatedChannel.setServiceId(serviceIdBox->value());

		if (audioChannelBox->currentIndex() != -1) {
			updatedChannel.audioPid = audioPids.at(audioChannelBox->currentIndex());
		}

		updatedChannel.isScrambled = scrambledBox->isChecked();
		model->getChannelModel()->updateChannel(channel, updatedChannel);
	}

	KDialog::accept();
}
