/*
 * dvbchannel.h
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

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <QSharedData> // qt 4.5 compatibility
#include <QTreeView>
#include "../sqltablemodel.h"
#include "dvbtransponder.h"

class QSqlQuery;
class KAction;
class DvbSqlChannelModelInterface;

class DvbChannelBase : public QSharedData
{
public:
	DvbChannelBase() { }
	~DvbChannelBase() { }

	DvbChannelBase &operator=(const DvbChannelBase &)
	{
		return *this;
	}
};

class DvbChannel : public SqlKey, public DvbChannelBase
{
public:
	DvbChannel() : number(-1), networkId(-1), transportStreamId(-1), pmtPid(-1), audioPid(-1),
		hasVideo(false), isScrambled(false) { }
	~DvbChannel() { }

	void readChannel(QDataStream &stream);

	int getServiceId() const
	{
		if (pmtSectionData.size() < 5) {
			return -1;
		}

		return ((static_cast<unsigned char>(pmtSectionData.at(3)) << 8) |
			static_cast<unsigned char>(pmtSectionData.at(4)));
	}

	void setServiceId(int serviceId)
	{
		if (pmtSectionData.size() < 5) {
			pmtSectionData = QByteArray(5, 0);
		}

		pmtSectionData[3] = (serviceId >> 8);
		pmtSectionData[4] = (serviceId & 0xff);
	}

	QString name;
	int number;

	QString source;
	DvbTransponder transponder;
	int networkId; // may be -1 (not present); ATSC meaning: source id
	int transportStreamId;
	int pmtPid;

	QByteArray pmtSectionData;
	int audioPid; // may be -1 (not present)
	bool hasVideo;
	bool isScrambled;
};

class DvbSharedChannel
{
	friend class DvbChannelModel;
public:
	DvbSharedChannel() { }
	~DvbSharedChannel() { }

	bool isValid() const
	{
		return (channel.constData() != NULL);
	}

	const DvbChannel *constData() const
	{
		return channel.constData();
	}

	const DvbChannel &operator*() const
	{
		return *channel;
	}

	const DvbChannel *operator->() const
	{
		return channel.constData();
	}

	bool operator==(const DvbSharedChannel &other) const
	{
		return (channel.constData() == other.channel.constData());
	}

	bool operator!=(const DvbSharedChannel &other) const
	{
		return (channel.constData() != other.channel.constData());
	}

	bool operator<(const DvbSharedChannel &other) const
	{
		return (channel.constData() < other.channel.constData());
	}

	friend uint qHash(const DvbSharedChannel &sharedChannel) // FIXME remove??
	{
		return qHash(sharedChannel.channel.constData());
	}

private:
	explicit DvbSharedChannel(DvbChannel *channel_) : channel(channel_) { }

	DvbChannel *data() const
	{
		return channel.data();
	}

	void detach() const
	{
		const_cast<DvbSharedChannel *>(this)->channel.detach();
	}

	QExplicitlySharedDataPointer<DvbChannel> channel;
};

Q_DECLARE_TYPEINFO(DvbSharedChannel, Q_MOVABLE_TYPE);

class DvbChannelLessThan
{
public:
	DvbChannelLessThan() : sortOrder(ChannelNameAscending) { }
	~DvbChannelLessThan() { }

	enum SortOrder
	{
		ChannelNameAscending,
		ChannelNameDescending,
		ChannelNumberAscending,
		ChannelNumberDescending
	};

	bool operator()(const DvbChannel &x, const DvbChannel &y) const;

	bool operator()(const DvbChannel &x, const DvbSharedChannel &y) const
	{
		return (*this)(x, *y);
	}

	bool operator()(const DvbSharedChannel &x, const DvbChannel &y) const
	{
		return (*this)(*x, y);
	}

	bool operator()(const DvbSharedChannel &x, const DvbSharedChannel &y) const
	{
		return (*this)(*x, *y);
	}

	SortOrder sortOrder;
};

class DvbComparableChannel
{
public:
	explicit DvbComparableChannel(const DvbChannel *channel_) : channel(channel_) { }
	explicit DvbComparableChannel(const DvbSharedChannel &channel_) :
		channel(channel_.constData()) { }
	~DvbComparableChannel() { }

	bool operator==(const DvbComparableChannel &other) const;

	bool operator!=(const DvbComparableChannel &other) const
	{
		return !(*this == other);
	}

	friend uint qHash(const DvbComparableChannel &channel);

private:
	const DvbChannel *channel;
};

class DvbChannelModel : public QObject, protected SqlInterface
{
	Q_OBJECT
public:
	explicit DvbChannelModel(QObject *parent);
	~DvbChannelModel();

	/*
	 * channel names and numbers are guaranteed to be unique within this model
	 */

	QMap<int, DvbSharedChannel> getChannels() const;
	DvbSharedChannel findChannelByName(const QString &channelName) const;
	DvbSharedChannel findChannelByNumber(int channelNumber) const;
	DvbSharedChannel findChannelByContent(const DvbChannel &channel) const;

	void cloneFrom(DvbChannelModel *other);
	void addChannel(const DvbChannel &channel);
	void updateChannel(const DvbSharedChannel &channel, const DvbChannel &updatedChannel);
	void removeChannel(const DvbSharedChannel &channel);
	void dndMoveChannels(const QList<DvbSharedChannel> &selectedChannels,
		const DvbSharedChannel &insertBeforeChannel);

signals:
	void channelAdded(const DvbSharedChannel &channel);
	// oldChannel is a temporary copy (channel remains the same if this is the main model)
	void channelUpdated(const DvbSharedChannel &channel, const DvbChannel &oldChannel);
	void channelRemoved(const DvbSharedChannel &channel);

private:
	void bindToSqlQuery(SqlKey sqlKey, QSqlQuery &query, int index) const;
	bool insertFromSqlQuery(SqlKey sqlKey, const QSqlQuery &query, int index);

	QString extractBaseName(const QString &name) const;
	QString findNextFreeChannelName(const QString &name) const;
	int findNextFreeChannelNumber(int number) const;
	void internalAddChannel(const DvbSharedChannel &channel);
	void internalUpdateChannel(const DvbSharedChannel &channel,
		const DvbChannel &modifiedChannel);
	void internalRemoveChannel(const DvbSharedChannel &channel);

	QMap<SqlKey, DvbSharedChannel> channels;
	QMap<QString, DvbSharedChannel> channelNameMapping;
	QMap<int, DvbSharedChannel> channelNumberMapping;
	QMultiHash<DvbComparableChannel, DvbSharedChannel> channelContentMapping;
	mutable bool hasPendingOperation;

protected:
	bool isSqlModel;
};

class DvbSqlChannelModel : public DvbChannelModel
{
public:
	explicit DvbSqlChannelModel(QObject *parent);
	~DvbSqlChannelModel();
};

class DvbChannelTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbChannelTableModel(DvbChannelModel *channelModel_, QObject *parent);
	~DvbChannelTableModel();

	/*
	 * channel names and numbers are guaranteed to be unique within this model
	 * they are automatically adjusted if necessary
	 */

	DvbChannelModel *getChannelModel() const;
	const DvbSharedChannel &getChannel(const QModelIndex &index) const;
	QModelIndex indexForChannel(const DvbSharedChannel &channel) const;

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	Qt::ItemFlags flags(const QModelIndex &index) const;
	QMimeData *mimeData(const QModelIndexList &indexes) const;
	QStringList mimeTypes() const;
	Qt::DropActions supportedDropActions() const;
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
		const QModelIndex &parent);
	void sort(int column, Qt::SortOrder order);

public slots:
	void setFilter(const QString &filter);

signals:
	void checkChannelDragAndDrop(bool *ok);

private slots:
	void channelAdded(const DvbSharedChannel &channel);
	void channelUpdated(const DvbSharedChannel &channel, const DvbChannel &oldChannel);
	void channelRemoved(const DvbSharedChannel &channel);

private:
	void customEvent(QEvent *event);

	DvbChannelModel *channelModel;
	QList<DvbSharedChannel> channels;
	DvbChannelLessThan lessThan;
	QList<DvbSharedChannel> dndSelectedChannels;
	DvbSharedChannel dndInsertBeforeChannel;
	bool dndEventPosted;
};

class DvbChannelView : public QTreeView
{
	Q_OBJECT
public:
	explicit DvbChannelView(QWidget *parent);
	~DvbChannelView();

	KAction *addEditAction();
	KAction *addRemoveAction();

	void setModel(DvbChannelTableModel *tableModel_);

public slots:
	void checkChannelDragAndDrop(bool *ok);
	void editChannel();
	void removeChannel();
	void removeAllChannels();

private:
	void setModel(QAbstractItemModel *) { }

	DvbChannelTableModel *tableModel;
};

#endif /* DVBCHANNEL_H */
