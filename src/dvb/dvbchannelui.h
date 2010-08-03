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

#include <KDialog>
#include "../proxytreeview.h"

class QCheckBox;
class QSpinBox;
class KComboBox;
class KLineEdit;
class DvbChannel;
class SqlTableModelInterface;

class DvbChannelModel : public QAbstractTableModel
{
public:
	explicit DvbChannelModel(QObject *parent);
	~DvbChannelModel();

	/*
	 * channel names and numbers are guaranteed to be unique within this model
	 */

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	bool removeRows(int row, int count, const QModelIndex &parent);

	QList<QSharedDataPointer<DvbChannel> > getChannels() const;
	void cloneFrom(const DvbChannelModel *other);
	void clear();

	const DvbChannel *getChannel(int row) const;
	int indexOfName(const QString &name) const;
	int indexOfNumber(int number) const;

	/*
	 * these two functions automatically adjust the channel numbers
	 */

	void appendChannels(const QList<DvbChannel *> &list);
	void updateChannel(int pos, DvbChannel *channel);

protected:
	bool adjustNameNumber(DvbChannel *channel) const;

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
	SqlTableModelInterface *sqlInterface;
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
	DvbChannelEditor(DvbChannelModel *model_, int row_, QWidget *parent);
	~DvbChannelEditor();

private:
	void accept();

	DvbChannelModel *model;
	int row;
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
