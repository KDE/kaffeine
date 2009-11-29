/*
 * dvbchannelui.h
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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

#include <KDialog>
#include "../proxytreeview.h"

class QCheckBox;
class QSpinBox;
class KComboBox;
class KLineEdit;
class DvbChannel;
class DvbSqlChannelModelAdaptor;

class DvbChannelModel : public QAbstractTableModel
{
public:
	explicit DvbChannelModel(QObject *parent);
	~DvbChannelModel();

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	bool removeRows(int row, int count, const QModelIndex &parent);

	QList<QSharedDataPointer<DvbChannel> > getChannels() const;
	void cloneFrom(const DvbChannelModel *other);
	void clear();

	QSharedDataPointer<DvbChannel> getChannel(int pos) const;
	int indexOfName(const QString &name) const;
	int indexOfNumber(int number) const;

	/*
	 * these three functions automatically adjust the channel numbers
	 */

	void appendChannel(const QSharedDataPointer<DvbChannel> &channel);
	void appendChannels(const QList<QSharedDataPointer<DvbChannel> > &list);
	void updateChannel(int pos, const QSharedDataPointer<DvbChannel> &channel);

private:
	QString findUniqueName(const QString &name) const;
	int findUniqueNumber(int number) const;

	QList<QSharedDataPointer<DvbChannel> > channels;
	QSet<QString> names;
	QSet<int> numbers;
};

class DvbSqlChannelModel : public DvbChannelModel
{
public:
	explicit DvbSqlChannelModel(QObject *parent);
	~DvbSqlChannelModel();

private:
	DvbSqlChannelModelAdaptor *sqlAdaptor;
};

class DvbChannelView : public ProxyTreeView
{
	Q_OBJECT
public:
	DvbChannelView(DvbChannelModel *channelModel_, QWidget *parent);
	~DvbChannelView();

	void addDeleteAction(); // should only be used in the scan dialog

private slots:
	void editChannel();
	void deleteChannel();

private:
	DvbChannelModel *channelModel;
};

class DvbChannelEditor : public KDialog
{
public:
	DvbChannelEditor(const QSharedDataPointer<DvbChannel> &channel_, QWidget *parent);
	~DvbChannelEditor();

	QSharedDataPointer<DvbChannel> getChannel();

private:
	QSharedDataPointer<DvbChannel> channel;
	KLineEdit *nameEdit;
	QSpinBox *numberBox;
	QSpinBox *networkIdBox;
	QSpinBox *transportStreamIdBox;
	QSpinBox *serviceIdBox;
	KComboBox *audioChannelBox;
	QList<int> audioPids;
	QCheckBox *scrambledBox;
};

#endif /* DVBCHANNELUI_H */
