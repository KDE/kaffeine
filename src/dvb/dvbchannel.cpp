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

bool DvbChannel::validate()
{
	if (!name.isEmpty() && (number >= 1) && !source.isEmpty() && transponder.isValid() &&
	    (networkId >= -1) && (networkId <= 0xffff) && (transportStreamId >= 0) &&
	    (transportStreamId <= 0xffff) && (pmtPid >= 0) && (pmtPid <= 0x1fff) &&
	    (audioPid >= -1) && (audioPid <= 0x1fff)) {
		if ((serviceId >= 0) && (serviceId <= 0xffff)) {
			if (pmtSectionData.size() < 5) {
				pmtSectionData = QByteArray(5, 0);
			}

			pmtSectionData[3] = (serviceId >> 8);
			pmtSectionData[4] = (serviceId & 0xff);
			return true;
		} else if (pmtSectionData.size() >= 5) {
			serviceId = ((static_cast<unsigned char>(pmtSectionData.at(3)) << 8) |
				static_cast<unsigned char>(pmtSectionData.at(4)));
			return true;
		}
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
			(channel->serviceId == other.channel->serviceId));
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
		hash ^= (qHash(channel.channel->serviceId) << 16);
		break;
	case DvbTransponderBase::Atsc:
		break;
	}

	return hash;
}

DvbChannelModel::DvbChannelModel(QObject *parent) : QObject(parent), hasPendingOperation(false),
	isSqlModel(false)
{
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
		int type;
		stream >> type;
		stream >> channel.name;
		stream >> channel.number;
		stream >> channel.source;

		switch (type) {
		case DvbTransponderBase::DvbC:
			channel.transponder = DvbTransponder(DvbTransponderBase::DvbC);
			channel.transponder.as<DvbCTransponder>()->readTransponder(stream);
			break;
		case DvbTransponderBase::DvbS:
			channel.transponder = DvbTransponder(DvbTransponderBase::DvbS);
			channel.transponder.as<DvbSTransponder>()->readTransponder(stream);
			break;
		case DvbTransponderBase::DvbS2:
			channel.transponder = DvbTransponder(DvbTransponderBase::DvbS2);
			channel.transponder.as<DvbS2Transponder>()->readTransponder(stream);
			break;
		case DvbTransponderBase::DvbT:
			channel.transponder = DvbTransponder(DvbTransponderBase::DvbT);
			channel.transponder.as<DvbTTransponder>()->readTransponder(stream);
			break;
		case DvbTransponderBase::Atsc:
			channel.transponder = DvbTransponder(DvbTransponderBase::Atsc);
			channel.transponder.as<AtscTransponder>()->readTransponder(stream);
			break;
		default:
			stream.setStatus(QDataStream::ReadCorruptData);
			break;
		}

		stream >> channel.networkId;
		stream >> channel.transportStreamId;
		int serviceId;
		stream >> serviceId;
		stream >> channel.pmtPid;

		stream >> channel.pmtSectionData;
		int videoPid;
		stream >> videoPid;
		stream >> channel.audioPid;

		int flags;
		stream >> flags;
		channel.hasVideo = (videoPid >= 0);
		channel.isScrambled = (flags & 0x1) != 0;

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
	return channelNumbers;
}

DvbSharedChannel DvbChannelModel::findChannelByName(const QString &channelName) const
{
	return channelNames.value(channelName);
}

DvbSharedChannel DvbChannelModel::findChannelByNumber(int channelNumber) const
{
	return channelNumbers.value(channelNumber);
}

DvbSharedChannel DvbChannelModel::findChannelByContent(const DvbChannel &channel) const
{
	return channelContents.value(DvbComparableChannel(&channel));
}

void DvbChannelModel::cloneFrom(DvbChannelModel *other)
{
	if (isSqlModel == other->isSqlModel) {
		kError() << "cloning is only supported between sql model and auxiliary model";
		return;
	}

	QMultiMap<SqlKey, DvbSharedChannel> otherChannelKeys;

	foreach (const DvbSharedChannel &channel, other->getChannels()) {
		otherChannelKeys.insert(*channel, channel);
	}

	foreach (const DvbSharedChannel &channel, getChannels()) {
		DvbSharedChannel otherChannel = otherChannelKeys.take(*channel);

		if (otherChannel.isValid()) {
			if (otherChannel != channel) {
				DvbChannel modifiedChannel(*otherChannel);
				updateChannel(channel, modifiedChannel);
			}
		} else {
			removeChannel(channel);
		}
	}

	foreach (const DvbSharedChannel &channel, otherChannelKeys) {
		DvbChannel newChannel(*channel);
		addChannel(newChannel);
	}
}

void DvbChannelModel::addChannel(DvbChannel &channel)
{
	bool forceAdd;

	if (channel.number < 1) {
		channel.number = 1;
		forceAdd = false;
	} else {
		forceAdd = true;
	}

	if (!channel.validate()) {
		kWarning() << "invalid channel";
		return;
	}

	if (forceAdd) {
		DvbSharedChannel existingChannel = channelNames.value(channel.name);

		if (existingChannel.isValid()) {
			DvbChannel updatedChannel = *existingChannel;
			updatedChannel.name = findNextFreeChannelName(updatedChannel.name);
			updateChannel(existingChannel, updatedChannel);
		}

		existingChannel = channelNumbers.value(channel.number);

		if (existingChannel.isValid()) {
			DvbChannel updatedChannel = *existingChannel;
			updatedChannel.number = findNextFreeChannelNumber(updatedChannel.number);
			updateChannel(existingChannel, updatedChannel);
		}
	} else {
		DvbSharedChannel existingChannel =
			channelContents.value(DvbComparableChannel(&channel));

		if (existingChannel.isValid()) {
			if (channel.name == extractBaseName(existingChannel->name)) {
				channel.name = existingChannel->name;
			} else {
				channel.name = findNextFreeChannelName(channel.name);
			}

			channel.number = existingChannel->number;
			DvbPmtParser pmtParser(DvbPmtSection(channel.pmtSectionData));

			for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
				if (pmtParser.audioPids.at(i).first == existingChannel->audioPid) {
					channel.audioPid = existingChannel->audioPid;
					break;
				}
			}

			updateChannel(existingChannel, channel);
			return;
		}

		channel.name = findNextFreeChannelName(channel.name);
		channel.number = findNextFreeChannelNumber(channel.number);
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (isSqlModel) {
		channel.setSqlKey(sqlFindFreeKey(channels));
	} else {
		channel.setSqlKey(SqlKey());
	}

	DvbSharedChannel newChannel(new DvbChannel(channel));
	channelNames.insert(newChannel->name, newChannel);
	channelNumbers.insert(newChannel->number, newChannel);
	channelContents.insert(DvbComparableChannel(newChannel), newChannel);

	if (isSqlModel) {
		channels.insert(*newChannel, newChannel);
		sqlInsert(*newChannel);
	}

	emit channelAdded(newChannel);
}

void DvbChannelModel::updateChannel(const DvbSharedChannel &channel, DvbChannel &modifiedChannel)
{
	if (!channel.isValid() || (channelNumbers.value(channel->number) != channel) ||
	    !modifiedChannel.validate()) {
		kWarning() << "invalid channel";
		return;
	}

	if (channel->name != modifiedChannel.name) {
		DvbSharedChannel existingChannel = channelNames.value(modifiedChannel.name);

		if (existingChannel.isValid()) {
			DvbChannel updatedChannel = *existingChannel;
			updatedChannel.name = findNextFreeChannelName(updatedChannel.name);
			updateChannel(existingChannel, updatedChannel);
		}
	}

	if (channel->number != modifiedChannel.number) {
		DvbSharedChannel existingChannel = channelNumbers.value(modifiedChannel.number);

		if (existingChannel.isValid()) {
			DvbChannel updatedChannel = *existingChannel;
			updatedChannel.number = findNextFreeChannelNumber(updatedChannel.number);
			updateChannel(existingChannel, updatedChannel);
		}
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	modifiedChannel.setSqlKey(*channel);

	if (!isSqlModel && channel->isSqlKeyValid()) {
		if (channel->name != modifiedChannel.name) {
			channelNames.remove(channel->name);
		}

		if (channel->number != modifiedChannel.number) {
			channelNumbers.remove(channel->number);
		}

		if (DvbComparableChannel(channel) != DvbComparableChannel(&modifiedChannel)) {
			channelContents.remove(DvbComparableChannel(channel), channel);
		}

		DvbSharedChannel detachedChannel(new DvbChannel(modifiedChannel));
		channelNames.insert(detachedChannel->name, detachedChannel);
		channelNumbers.insert(detachedChannel->number, detachedChannel);
		channelContents.insert(DvbComparableChannel(detachedChannel),
			detachedChannel);
		emit channelUpdated(detachedChannel, *channel);
	} else {
		DvbChannel oldChannel = *channel;
		*const_cast<DvbChannel *>(channel.constData()) = modifiedChannel;

		if (channel->name != oldChannel.name) {
			channelNames.remove(oldChannel.name);
			channelNames.insert(channel->name, channel);
		}

		if (channel->number != oldChannel.number) {
			channelNumbers.remove(oldChannel.number);
			channelNumbers.insert(channel->number, channel);
		}

		if (DvbComparableChannel(channel) != DvbComparableChannel(&oldChannel)) {
			channelContents.remove(DvbComparableChannel(&oldChannel), channel);
			channelContents.insert(DvbComparableChannel(channel), channel);
		}

		if (isSqlModel) {
			sqlUpdate(*channel);
		}

		emit channelUpdated(channel, oldChannel);
	}
}

void DvbChannelModel::removeChannel(const DvbSharedChannel &channel)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	channelNames.remove(channel->name);
	channelNumbers.remove(channel->number);
	channelContents.remove(DvbComparableChannel(channel), channel);

	if (isSqlModel) {
		sqlRemove(*channel);
	}

	emit channelRemoved(channel);
}

void DvbChannelModel::dndMoveChannels(const QList<DvbSharedChannel> &selectedChannels,
	const DvbSharedChannel &insertBeforeChannel)
{
	int newNumberBegin = 1;

	if (insertBeforeChannel.isValid()) {
		QMap<int, DvbSharedChannel>::ConstIterator it =
			channelNumbers.constFind(insertBeforeChannel->number);

		if (it != channelNumbers.constBegin()) {
			newNumberBegin = ((it - 1).key() + 1);
		}
	} else {
		QMap<int, DvbSharedChannel>::ConstIterator it = channelNumbers.constEnd();

		if (it != channelNumbers.constBegin()) {
			newNumberBegin = ((it - 1).key() + 1);
		}
	}

	int newNumberEnd = (newNumberBegin + selectedChannels.size());

	for (int i = 0; i < selectedChannels.size(); ++i) {
		const DvbSharedChannel &selectedChannel = selectedChannels.at(i);
		int number = (newNumberBegin + i);
		DvbSharedChannel existingChannel = channelNumbers.value(number);

		if (existingChannel.isValid() && (existingChannel != selectedChannel)) {
			DvbChannel modifiedChannel = *existingChannel;
			modifiedChannel.number = findNextFreeChannelNumber(newNumberEnd);
			updateChannel(existingChannel, modifiedChannel);
		}

		if (selectedChannel->number != number) {
			DvbChannel modifiedChannel = *selectedChannel;
			modifiedChannel.number = number;
			updateChannel(selectedChannel, modifiedChannel);
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

	if (channel->validate() && !channelNames.contains(channel->name) &&
	    !channelNumbers.contains(channel->number)) {
		channelNames.insert(sharedChannel->name, sharedChannel);
		channelNumbers.insert(sharedChannel->number, sharedChannel);
		channelContents.insert(DvbComparableChannel(sharedChannel), sharedChannel);
		channels.insert(*sharedChannel, sharedChannel);
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
	if (!channelNames.contains(name)) {
		return name;
	}

	QString baseName = extractBaseName(name);
	int suffix = 0;
	QString newName = baseName;

	while (channelNames.contains(newName)) {
		++suffix;
		newName = baseName + '-' + QString::number(suffix);
	}

	return newName;
}

int DvbChannelModel::findNextFreeChannelNumber(int number) const
{
	while (channelNumbers.contains(number)) {
		++number;
	}

	return number;
}
