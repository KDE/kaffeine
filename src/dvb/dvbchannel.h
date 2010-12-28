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
#include "dvbtransponder.h"

class QAbstractProxyModel;
class KAction;
class SqlTableModelInterface;

class DvbChannelBase
{
public:
	DvbChannelBase() : number(-1), networkId(-1), transportStreamId(-1), pmtPid(-1),
		audioPid(-1), hasVideo(false), isScrambled(false) { }
	~DvbChannelBase() { }

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
			return;
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

class DvbChannel : public DvbChannelBase, public QSharedData
{
public:
	DvbChannel() { }
	explicit DvbChannel(const DvbChannelBase &channel) : DvbChannelBase(channel) { }
	~DvbChannel() { }

	DvbChannel &operator=(const DvbChannel &other)
	{
		DvbChannelBase *base = this;
		*base = other;
		return (*this);
	}
};

Q_DECLARE_TYPEINFO(QSharedDataPointer<DvbChannel>, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QExplicitlySharedDataPointer<const DvbChannel>, Q_MOVABLE_TYPE);

class DvbChannelModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit DvbChannelModel(QObject *parent);
	~DvbChannelModel();

	/*
	 * channel names and numbers are guaranteed to be unique within this model
	 * they are automatically adjusted if necessary
	 */

	QModelIndex findChannelByName(const QString &name) const;
	QModelIndex findChannelByNumber(int number) const;
	void cloneFrom(const DvbChannelModel *other);

	enum ItemDataRole
	{
		DvbChannelRole = Qt::UserRole
	};

	QAbstractProxyModel *createProxyModel(QObject *parent);
	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool removeRows(int row, int count, const QModelIndex &parent);
	bool setData(const QModelIndex &modelIndex, const QVariant &value,
		int role = Qt::EditRole);

	Qt::ItemFlags flags(const QModelIndex &index) const;
	QMimeData *mimeData(const QModelIndexList &indexes) const;
	QStringList mimeTypes() const;
	Qt::DropActions supportedDropActions() const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
		const QModelIndex &parent);

signals:
	void checkInternalMove(bool *ok);

protected:
	QString findNextFreeName(const QString &name) const;
	int findNextFreeNumber(int number) const;

	QList<QSharedDataPointer<DvbChannel> > channels;
	QSet<QString> names;
	QSet<int> numbers;

signals:
	void queueInternalMove(const QList<QPersistentModelIndex> &indexes, int newNumber);

private slots:
	void internalMove(const QList<QPersistentModelIndex> &indexes, int newNumber);
};

Q_DECLARE_METATYPE(const DvbChannel *)

class DvbSqlChannelModel : public DvbChannelModel
{
public:
	explicit DvbSqlChannelModel(QObject *parent);
	~DvbSqlChannelModel();

private:
	SqlTableModelInterface *sqlInterface;
};

class DvbChannelView : public QTreeView
{
	Q_OBJECT
public:
	explicit DvbChannelView(QWidget *parent);
	~DvbChannelView();

	KAction *addEditAction();
	KAction *addRemoveAction();

public slots:
	void checkInternalMove(bool *ok);
	void editChannel();
	void removeChannel();
	void removeAllChannels();
};

#endif /* DVBCHANNEL_H */
