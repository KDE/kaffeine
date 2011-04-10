/*
 * dvbchannel.h
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

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <QMultiHash>
#include "../shareddata.h"
#include "../sqlinterface.h"
#include "dvbtransponder.h"

class DvbChannel : public SharedData, public SqlKey
{
public:
	DvbChannel() : number(-1), networkId(-1), transportStreamId(-1), pmtPid(-1), serviceId(-1),
		audioPid(-1), hasVideo(false), isScrambled(false) { }
	~DvbChannel() { }

	// checks that all variables are ok
	// updates 'pmtSectionData' if 'serviceId' is valid
	// (else updates 'serviceId' if 'pmtSectionData' is valid)
	// 'sqlKey' is ignored
	bool validate();

	QString name;
	int number;

	QString source;
	DvbTransponder transponder;
	int networkId; // may be -1 (not present), atsc meaning: source id
	int transportStreamId;
	int pmtPid;

	QByteArray pmtSectionData;
	int serviceId;
	int audioPid; // may be -1 (not present), doesn't have to be present in the pmt section
	bool hasVideo;
	bool isScrambled;
};

typedef ExplicitlySharedDataPointer<const DvbChannel> DvbSharedChannel;
Q_DECLARE_TYPEINFO(DvbSharedChannel, Q_MOVABLE_TYPE);

class DvbChannelId
{
public:
	explicit DvbChannelId(const DvbChannel *channel_) : channel(channel_) { }
	explicit DvbChannelId(const DvbSharedChannel &channel_) : channel(channel_.constData()) { }
	~DvbChannelId() { }

	// compares channels by transmission type and
	// ( dvb) 'source', 'networkId', 'transportStreamId', 'serviceId'
	// (atsc) 'source', 'transponder', 'networkId'

	bool operator==(const DvbChannelId &other) const;

	bool operator!=(const DvbChannelId &other) const
	{
		return !(*this == other);
	}

	friend uint qHash(const DvbChannelId &channel);

private:
	const DvbChannel *channel;
};

class DvbChannelModel : public QObject, private SqlInterface
{
	Q_OBJECT
public:
	explicit DvbChannelModel(QObject *parent);
	~DvbChannelModel();

	static DvbChannelModel *createSqlModel(QObject *parent);

	// channel names and numbers are guaranteed to be unique within this model

	QMap<int, DvbSharedChannel> getChannels() const;
	DvbSharedChannel findChannelByName(const QString &channelName) const;
	DvbSharedChannel findChannelByNumber(int channelNumber) const;
	DvbSharedChannel findChannelById(const DvbChannel &channel) const;

	void cloneFrom(DvbChannelModel *other);
	void addChannel(DvbChannel &channel);
	void updateChannel(DvbSharedChannel channel, DvbChannel &modifiedChannel);
	void removeChannel(DvbSharedChannel channel);
	void dndMoveChannels(const QList<DvbSharedChannel> &selectedChannels,
		int insertBeforeNumber);

signals:
	void channelAdded(const DvbSharedChannel &channel);
	// if this is the main model, updating doesn't change the channel pointer
	// (modifies existing content); otherwise the channel pointer may be updated
	void channelAboutToBeUpdated(const DvbSharedChannel &channel);
	void channelUpdated(const DvbSharedChannel &channel);
	void channelRemoved(const DvbSharedChannel &channel);

private:
	void bindToSqlQuery(SqlKey sqlKey, QSqlQuery &query, int index) const;
	bool insertFromSqlQuery(SqlKey sqlKey, const QSqlQuery &query, int index);

	QString extractBaseName(const QString &name) const;
	QString findNextFreeChannelName(const QString &name) const;
	int findNextFreeChannelNumber(int number) const;

	QMap<QString, DvbSharedChannel> channelNames;
	QMap<int, DvbSharedChannel> channelNumbers;
	QMultiHash<DvbChannelId, DvbSharedChannel> channelIds;
	QMap<SqlKey, DvbSharedChannel> channels; // only used for the sql model
	bool hasPendingOperation;
	bool isSqlModel;
};

#endif /* DVBCHANNEL_H */
