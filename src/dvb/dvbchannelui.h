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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef DVBCHANNELUI_H
#define DVBCHANNELUI_H

#include <QAbstractTableModel>
#include <KDialog>
#include <KMenu>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class DvbChannel;
class ProxyTreeView;

class DvbChannelModel : public QAbstractTableModel
{
public:
	explicit DvbChannelModel(QObject *parent) : QAbstractTableModel(parent) { }
	~DvbChannelModel() { }

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	QSharedDataPointer<DvbChannel> getChannel(int pos) const;
	QSharedDataPointer<DvbChannel> channelForName(const QString &name) const;

	QList<QSharedDataPointer<DvbChannel> > getChannels() const;
	void setChannels(const QList<QSharedDataPointer<DvbChannel> > &channels_);
	void appendChannels(const QList<QSharedDataPointer<DvbChannel> > &list);
	void updateChannel(int pos, const QSharedDataPointer<DvbChannel> &channel);
	void removeChannel(int pos);

	void loadChannels();
	void saveChannels();

private:
	QList<QSharedDataPointer<DvbChannel> > channels;
};

class DvbChannelContextMenu : public KMenu
{
	Q_OBJECT
public:
	DvbChannelContextMenu(DvbChannelModel *model_, ProxyTreeView *view_);
	~DvbChannelContextMenu();

	void addDeleteAction(); // should only be used in the scan dialog

private slots:
	void editChannel();
	void changeIcon();
	void deleteChannel();

private:
	DvbChannelModel *model;
	ProxyTreeView *view;
};

class DvbChannelEditor : public KDialog
{
public:
	DvbChannelEditor(const QSharedDataPointer<DvbChannel> &channel_, QWidget *parent);
	~DvbChannelEditor();

	QSharedDataPointer<DvbChannel> getChannel();

private:
	QSharedDataPointer<DvbChannel> channel;
	QLineEdit *nameEdit;
	QSpinBox *numberBox;
	QSpinBox *networkIdBox;
	QSpinBox *transportStreamIdBox;
	QSpinBox *serviceIdBox;
	QComboBox *audioChannelBox;
	QCheckBox *scrambledBox;
};

#endif /* DVBCHANNELUI_H */
