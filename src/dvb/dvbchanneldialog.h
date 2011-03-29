/*
 * dvbchanneldialog.h
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

#ifndef DVBCHANNELDIALOG_H
#define DVBCHANNELDIALOG_H

#include <QTreeView>
#include "../tablemodel.h"
#include "dvbchannel.h"

class KAction;

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

	SortOrder getSortOrder() const
	{
		return sortOrder;
	}

	void setSortOrder(SortOrder sortOrder_)
	{
		sortOrder = sortOrder_;
	}

	bool operator()(const DvbSharedChannel &x, const DvbSharedChannel &y) const;

private:
	SortOrder sortOrder;
};

class DvbChannelTableModelHelper
{
public:
	DvbChannelTableModelHelper() { }
	~DvbChannelTableModelHelper() { }

	typedef DvbSharedChannel ItemType;
	typedef DvbChannelLessThan LessThanType;

	int columnCount() const
	{
		return 2;
	}

	bool filterAcceptsItem(const DvbSharedChannel &channel) const
	{
		// Qt::CaseSensitive == no filtering
		return ((filter.caseSensitivity() == Qt::CaseSensitive) ||
			(filter.indexIn(channel->name) >= 0));
	}

	QStringMatcher filter;

private:
	Q_DISABLE_COPY(DvbChannelTableModelHelper)
};

class DvbChannelTableModel : public TableModel<DvbChannelTableModelHelper>
{
	Q_OBJECT
public:
	explicit DvbChannelTableModel(QObject *parent);
	~DvbChannelTableModel();

	void setChannelModel(DvbChannelModel *channelModel_);

	DvbChannelModel *getChannelModel() const
	{
		return channelModel;
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	void sort(int column, Qt::SortOrder order);

	Qt::ItemFlags flags(const QModelIndex &index) const;
	QMimeData *mimeData(const QModelIndexList &indexes) const;
	QStringList mimeTypes() const;
	Qt::DropActions supportedDropActions() const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
		const QModelIndex &parent);

public slots:
	void setFilter(const QString &filter);

signals:
	void checkChannelDragAndDrop(bool *ok);

private slots:
	void channelAdded(const DvbSharedChannel &channel);
	void channelAboutToBeUpdated(const DvbSharedChannel &channel);
	void channelUpdated(const DvbSharedChannel &channel);
	void channelRemoved(const DvbSharedChannel &channel);

private:
	void customEvent(QEvent *event);

	DvbChannelModel *channelModel;
	QList<DvbSharedChannel> dndSelectedChannels;
	int dndInsertBeforeNumber;
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

#endif /* DVBCHANNELDIALOG_H */
