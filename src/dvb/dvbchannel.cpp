/*
 * dvbchannel.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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
#include <KStandardDirs>
#include "../ensurenopendingoperation.h"
#include "../log.h"
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

			pmtSectionData[3] = ((serviceId >> 8) & 0xff);
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

bool DvbChannelId::operator==(const DvbChannelId &other) const
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
		// source id has to be unique only within a transport stream
		// --> we need to check transponder as well
		return channel->transponder.corresponds(other.channel->transponder);
	}

	return false;
}

uint qHash(const DvbChannelId &channel)
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
	if (hasPendingOperation) {
		Log("DvbChannelModel::~DvbChannelModel: illegal recursive call");
	}

	if (isSqlModel) {
		sqlFlush();
	}
}

DvbChannelModel *DvbChannelModel::createSqlModel(QObject *parent)
{
	DvbChannelModel *channelModel = new DvbChannelModel(parent);
	channelModel->isSqlModel = true;
	channelModel->sqlInit(QLatin1String("Channels"),
		QStringList() << QLatin1String("Name") << QLatin1String("Number") << QLatin1String("Source") <<
		QLatin1String("Transponder") << QLatin1String("NetworkId") << QLatin1String("TransportStreamId") <<
		QLatin1String("PmtPid") << QLatin1String("PmtSection") << QLatin1String("AudioPid") <<
		QLatin1String("Flags"));

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("channels.dtv")));

	if (!file.exists()) {
		return channelModel;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		Log("DvbChannelModel::createSqlModel: cannot open") << file.fileName();
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
			Log("DvbChannelModel::createSqlModel: invalid channels in file") <<
				file.fileName();
			break;
		}

		channelModel->addChannel(channel);
	}

	if (!file.remove()) {
		Log("DvbChannelModel::createSqlModel: cannot remove") << file.fileName();
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

DvbSharedChannel DvbChannelModel::findChannelById(const DvbChannel &channel) const
{
	return channelIds.value(DvbChannelId(&channel));
}

void DvbChannelModel::cloneFrom(DvbChannelModel *other)
{
	if (!isSqlModel && other->isSqlModel && channelNumbers.isEmpty()) {
		if (hasPendingOperation) {
			Log("DvbChannelModel::cloneFrom: illegal recursive call");
			return;
		}

		EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
		channelNames = other->channelNames;
		channelNumbers = other->channelNumbers;
		channelIds = other->channelIds;

		foreach (const DvbSharedChannel &channel, channelNumbers) {
			emit channelAdded(channel);
		}
	} else if (isSqlModel && !other->isSqlModel) {
		QMultiMap<SqlKey, DvbSharedChannel> otherChannelKeys;

		foreach (const DvbSharedChannel &channel, other->getChannels()) {
			otherChannelKeys.insert(*channel, channel);
		}

		for (QMap<SqlKey, DvbSharedChannel>::ConstIterator it = channels.constBegin();
		     it != channels.constEnd();) {
			const DvbSharedChannel &channel = *it;
			++it;
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
	} else {
		Log("DvbChannelModel::cloneFrom: illegal type of clone");
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
		Log("DvbChannelModel::addChannel: invalid channel");
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
		DvbSharedChannel existingChannel = channelIds.value(DvbChannelId(&channel));

		if (existingChannel.isValid()) {
			if (channel.name == extractBaseName(existingChannel->name)) {
				channel.name = existingChannel->name;
			} else {
				channel.name = findNextFreeChannelName(channel.name);
			}

			channel.number = existingChannel->number;
			DvbPmtSection pmtSection(channel.pmtSectionData);
			DvbPmtParser pmtParser(pmtSection);

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

	if (hasPendingOperation) {
		Log("DvbChannelModel::addChannel: illegal recursive call");
		return;
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
	channelIds.insert(DvbChannelId(newChannel), newChannel);

	if (isSqlModel) {
		channels.insert(*newChannel, newChannel);
		sqlInsert(*newChannel);
	}

	emit channelAdded(newChannel);
}

void DvbChannelModel::updateChannel(DvbSharedChannel channel, DvbChannel &modifiedChannel)
{
	if (!channel.isValid() || (channelNumbers.value(channel->number) != channel) ||
	    !modifiedChannel.validate()) {
		Log("DvbChannelModel::updateChannel: invalid channel");
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

	if (hasPendingOperation) {
		Log("DvbChannelModel::updateChannel: illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	modifiedChannel.setSqlKey(*channel);
	bool channelNameChanged = (channel->name != modifiedChannel.name);
	bool channelNumberChanged = (channel->number != modifiedChannel.number);
	bool channelIdChanged = (DvbChannelId(channel) != DvbChannelId(&modifiedChannel));
	emit channelAboutToBeUpdated(channel);

	if (channelNameChanged) {
		channelNames.remove(channel->name);
	}

	if (channelNumberChanged) {
		channelNumbers.remove(channel->number);
	}

	if (channelIdChanged) {
		channelIds.remove(DvbChannelId(channel), channel);
	}

	if (!isSqlModel && channel->isSqlKeyValid()) {
		DvbSharedChannel detachedChannel(new DvbChannel(modifiedChannel));
		channelNames.insert(detachedChannel->name, detachedChannel);
		channelNumbers.insert(detachedChannel->number, detachedChannel);
		channelIds.insert(DvbChannelId(detachedChannel), detachedChannel);
		emit channelUpdated(detachedChannel);
	} else {
		*const_cast<DvbChannel *>(channel.constData()) = modifiedChannel;

		if (channelNameChanged) {
			channelNames.insert(channel->name, channel);
		}

		if (channelNumberChanged) {
			channelNumbers.insert(channel->number, channel);
		}

		if (channelIdChanged) {
			channelIds.insert(DvbChannelId(channel), channel);
		}

		if (isSqlModel) {
			sqlUpdate(*channel);
		}

		emit channelUpdated(channel);
	}
}

void DvbChannelModel::removeChannel(DvbSharedChannel channel)
{
	if (!channel.isValid() || (channelNumbers.value(channel->number) != channel)) {
		Log("DvbChannelModel::removeChannel: invalid channel");
		return;
	}

	if (hasPendingOperation) {
		Log("DvbChannelModel::removeChannel: illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	channelNames.remove(channel->name);
	channelNumbers.remove(channel->number);
	channelIds.remove(DvbChannelId(channel), channel);

	if (isSqlModel) {
		channels.remove(*channel);
		sqlRemove(*channel);
	}

	emit channelRemoved(channel);
}

void DvbChannelModel::dndMoveChannels(const QList<DvbSharedChannel> &selectedChannels,
	int insertBeforeNumber)
{
	if (hasPendingOperation) {
		Log("DvbChannelModel::dndMoveChannels: illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	typedef QMap<int, DvbSharedChannel>::ConstIterator ConstIterator;
	QList<DvbSharedChannel> channelQueue;

	foreach (const DvbSharedChannel &channel, selectedChannels) {
		if (channel.isValid()) {
			ConstIterator it = channelNumbers.constFind(channel->number);

			if ((it != channelNumbers.constEnd()) && (*it == channel)) {
				channelNumbers.remove(channel->number);
				channelQueue.append(channel);
			}
		}
	}

	ConstIterator it = channelNumbers.constFind(insertBeforeNumber);
	int currentNumber = 1;

	if (it != channelNumbers.constBegin()) {
		currentNumber = ((it - 1).key() + 1);
	}

	while (!channelQueue.isEmpty()) {
		DvbSharedChannel channel = channelQueue.takeFirst();

		if (channel->number != currentNumber) {
			emit channelAboutToBeUpdated(channel);
			DvbSharedChannel existingChannel = channelNumbers.take(currentNumber);

			if (existingChannel.isValid()) {
				channelQueue.append(existingChannel);
			}

			if (!isSqlModel && channel->isSqlKeyValid()) {
				DvbChannel *newChannel = new DvbChannel(*channel);
				newChannel->number = currentNumber;
				DvbSharedChannel detachedChannel(newChannel);
				channelNames.insert(detachedChannel->name, detachedChannel);
				channelNumbers.insert(detachedChannel->number, detachedChannel);
				channelIds.insert(DvbChannelId(detachedChannel), detachedChannel);
				emit channelUpdated(detachedChannel);
			} else {
				const_cast<DvbChannel *>(channel.constData())->number =
					currentNumber;
				channelNumbers.insert(channel->number, channel);

				if (isSqlModel) {
					sqlUpdate(*channel);
				}

				emit channelUpdated(channel);
			}
		} else {
			channelNumbers.insert(channel->number, channel);
		}

		++currentNumber;
	}
}

void DvbChannelModel::bindToSqlQuery(SqlKey sqlKey, QSqlQuery &query, int index) const
{
	DvbSharedChannel channel = channels.value(sqlKey);

	if (!channel.isValid()) {
		Log("DvbChannelModel::bindToSqlQuery: invalid channel");
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
		channelIds.insert(DvbChannelId(sharedChannel), sharedChannel);
		channels.insert(*sharedChannel, sharedChannel);
		return true;
	}

	return false;
}

QString DvbChannelModel::extractBaseName(const QString &name) const
{
	QString baseName = name;
	int position = baseName.lastIndexOf(QLatin1Char('-'));

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
		newName = baseName + QLatin1Char('-') + QString::number(suffix);
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
