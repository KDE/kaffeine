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

#include <QFile>
#include <QVariant>
#include <KDebug>
#include <KStandardDirs>
#include "../ensurenopendingoperation.h"
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
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
}

DvbChannelModel::~DvbChannelModel()
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (isSqlModel) {
		sqlFlush();
	}
}

DvbChannelModel *DvbChannelModel::createSqlModel(QObject *parent)
{
	DvbChannelModel *channelModel = new DvbChannelModel(parent);
	channelModel->sqlInit("Channels",
		QStringList() << "Name" << "Number" << "Source" << "Transponder" << "NetworkId" <<
		"TransportStreamId" << "PmtPid" << "PmtSection" << "AudioPid" << "Flags");
	channelModel->isSqlModel = true;

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", "channels.dtv"));

	if (!file.exists()) {
		return channelModel;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "cannot open" << file.fileName();
		return channelModel;
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

		channelModel->addChannel(channel);
	}

	if (!file.remove()) {
		kWarning() << "cannot remove" << file.fileName();
	}

	return channelModel;
}

QMap<int, DvbSharedChannel> DvbChannelModel::getChannels() const
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	return channelNumberMapping;
}

DvbSharedChannel DvbChannelModel::findChannelByName(const QString &channelName) const
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	return channelNameMapping.value(channelName);
}

DvbSharedChannel DvbChannelModel::findChannelByNumber(int channelNumber) const
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	return channelNumberMapping.value(channelNumber);
}

DvbSharedChannel DvbChannelModel::findChannelByContent(const DvbChannel &channel) const
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	return channelContentMapping.value(DvbComparableChannel(&channel));
}

void DvbChannelModel::cloneFrom(DvbChannelModel *other)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

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
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
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
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

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
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
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
	DvbChannel *channel = new DvbChannel();
	DvbSharedChannel sharedChannel(channel);
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
		const_cast<DvbChannel *>(channel.constData())->setSqlKey(sqlFindFreeKey(channels));
		sqlInsert(*channel);
	}

	emit channelAdded(channel);
}

void DvbChannelModel::internalUpdateChannel(DvbSharedChannel channel,
	const DvbChannel &modifiedChannel)
{
	if (!isSqlModel && channel->isSqlKeyValid()) {
		channel.detach();
	}

	DvbChannel oldChannel = *channel;
	*const_cast<DvbChannel *>(channel.constData()) = modifiedChannel;

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
