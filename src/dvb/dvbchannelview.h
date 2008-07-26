/*
 * dvbchannelview.h
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBCHANNELVIEW_H
#define DVBCHANNELVIEW_H

#include <QSortFilterProxyModel>
#include <QTreeView>

class DvbSharedChannel;
class KMenu;

template<class T> class DvbGenericChannelModel : public QAbstractTableModel
{
public:
	explicit DvbGenericChannelModel(QObject *parent) : QAbstractTableModel(parent)
	{
		proxyModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
		proxyModel.setSortLocaleAware(true);
		proxyModel.setSourceModel(this);
	}

	~DvbGenericChannelModel() { }

	QSortFilterProxyModel *getProxyModel()
	{
		return &proxyModel;
	}

	QList<T> getList() const
	{
		return list;
	}

	void setList(const QList<T> &list_)
	{
		list = list_;
		reset();
	}

	void appendList(const QList<T> &other)
	{
		beginInsertRows(QModelIndex(), list.size(), list.size() + other.size() - 1);
		list += other;
		endInsertRows();
	}

	const T *getChannel(const QModelIndex &index) const
	{
		QModelIndex sourceIndex = proxyModel.mapToSource(index);

		if (!sourceIndex.isValid() || (sourceIndex.row() >= list.size())) {
			return NULL;
		}

		return &list.at(sourceIndex.row());
	}

	void updateChannel(int pos, const T &channel)
	{
		list.replace(pos, channel);
		// FIXME check behaviour
		emit dataChanged(index(pos, 0), index(pos, columnCount(QModelIndex()) - 1));
	}

	int rowCount(const QModelIndex &parent) const
	{
		if (parent.isValid()) {
			return 0;
		}

		return list.size();
	}

protected:
	QList<T> list;

private:
	QSortFilterProxyModel proxyModel;
};

class DvbChannelModel : public DvbGenericChannelModel<DvbSharedChannel>
{
public:
	explicit DvbChannelModel(QObject *parent) :
		DvbGenericChannelModel<DvbSharedChannel>(parent) { }
	~DvbChannelModel() { }

	int columnCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
};

class DvbChannelViewBase : public QTreeView
{
public:
	explicit DvbChannelViewBase(QWidget *parent);
	~DvbChannelViewBase();
};

// this class adds a context menu
class DvbChannelView : public DvbChannelViewBase
{
	Q_OBJECT
public:
	explicit DvbChannelView(QWidget *parent);
	~DvbChannelView();

	/*
	 * should be only used in the scan dialog
	 */

	void enableDeleteAction();

protected:
	void contextMenuEvent(QContextMenuEvent *event);

private slots:
	void actionEdit();
	void actionChangeIcon();
	void actionDelete();

private:
	KMenu *menu;
	QPersistentModelIndex menuIndex;
};

#endif /* DVBCHANNELVIEW_H */
