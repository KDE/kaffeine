/*
 * dvbchannelui.h
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

#ifndef DVBCHANNELUI_H
#define DVBCHANNELUI_H

#include <QTreeView>

class QAbstractProxyModel;
class KAction;
class DvbChannel;
class SqlTableModelInterface;

class DvbChannelModel : public QAbstractTableModel
{
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
	void addUpdateChannels(const QList<const DvbChannel *> &channelList);

	enum ItemDataRole
	{
		DvbChannelRole = Qt::UserRole
	};

	QAbstractProxyModel *createProxyModel(QObject *parent);
	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool removeRows(int row, int count, const QModelIndex &parent);
	bool setData(const QModelIndex &modelIndex, const QVariant &value, int role);

protected:
	bool adjustNameNumber(DvbChannel *channel) const;

	QList<QSharedDataPointer<DvbChannel> > channels;
	QSet<QString> names;
	QSet<int> numbers;
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
	void editChannel();
	void removeChannel();
	void removeAllChannels();
};

#endif /* DVBCHANNELUI_H */
