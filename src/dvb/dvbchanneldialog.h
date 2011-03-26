/*
 * dvbchanneldialog.h
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

#ifndef DVBCHANNELDIALOG_H
#define DVBCHANNELDIALOG_H

#include <QSharedData> // qt 4.5 compatibility
#include <QTreeView>
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

	bool operator()(const DvbSharedChannel &x, const DvbSharedChannel &y) const;

	SortOrder sortOrder;
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
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
		const QModelIndex &parent);
	void sort(int column, Qt::SortOrder order);

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
	QList<DvbSharedChannel> channels;
	DvbChannelLessThan lessThan;
	QList<DvbSharedChannel> dndSelectedChannels;
	DvbSharedChannel dndInsertBeforeChannel;
	int updatingRow;
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
