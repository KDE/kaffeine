/*
 * dvbchannelui.cpp
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

#include "dvbchannelui.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QSpinBox>
#include <KAction>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KLocalizedString>
#include <KStandardDirs>
#include "dvbchannel.h"
#include "../proxytreeview.h"

int DvbChannelModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 2;
}

int DvbChannelModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return channels.size();
}

QVariant DvbChannelModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole || index.row() >= channels.size()) {
		return QVariant();
	}

	switch (index.column()) {
	case 0:
		return channels.at(index.row())->name;
	case 1:
		return channels.at(index.row())->number;
	}

	return QVariant();
}

QVariant DvbChannelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18n("Name");
	case 1:
		return i18n("Number");
	}

	return QVariant();
}

QSharedDataPointer<DvbChannel> DvbChannelModel::getChannel(int pos) const
{
	return channels.at(pos);
}

QSharedDataPointer<DvbChannel> DvbChannelModel::channelForName(const QString &name) const
{
	foreach (const QSharedDataPointer<DvbChannel> &channel, channels) {
		if (channel->name == name) {
			return channel;
		}
	}

	return QSharedDataPointer<DvbChannel>();
}

QList<QSharedDataPointer<DvbChannel> > DvbChannelModel::getChannels() const
{
	return channels;
}

void DvbChannelModel::setChannels(const QList<QSharedDataPointer<DvbChannel> > &channels_)
{
	channels = channels_;
	reset();
}

void DvbChannelModel::appendChannels(const QList<QSharedDataPointer<DvbChannel> > &list)
{
	beginInsertRows(QModelIndex(), channels.size(), channels.size() + list.size() - 1);
	channels += list;
	endInsertRows();
}

void DvbChannelModel::updateChannel(int pos, const QSharedDataPointer<DvbChannel> &channel)
{
	channels.replace(pos, channel);
	emit dataChanged(index(pos, 0), index(pos, columnCount(QModelIndex()) - 1));
}

void DvbChannelModel::removeChannel(int pos)
{
	beginRemoveRows(QModelIndex(), pos, pos);
	channels.removeAt(pos);
	endRemoveRows();
}

void DvbChannelModel::loadChannels()
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	QTextStream stream(&file);
	stream.setCodec("UTF-8");
	bool fileOk = true;

	while (!stream.atEnd()) {
		DvbChannel *channel = DvbLineReader(stream.readLine()).readChannel();

		if (channel == NULL) {
			fileOk = false;
			continue;
		}

		channels.append(QSharedDataPointer<DvbChannel>(channel));
	}

	if (!fileOk) {
		kWarning() << "invalid lines in file" << file.fileName();
	}

	kDebug() << "successfully loaded" << channels.size() << "channels";
}

void DvbChannelModel::saveChannels()
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		return;
	}

	QTextStream stream(&file);
	stream.setCodec("UTF-8");

	foreach (const QSharedDataPointer<DvbChannel> &channel, channels) {
		DvbLineWriter writer;
		writer.writeChannel(channel);
		stream << writer.getLine();
	}
}

DvbChannelContextMenu::DvbChannelContextMenu(DvbChannelModel *model_, ProxyTreeView *view_) :
	KMenu(view_), model(model_), view(view_)
{
	KAction *action = new KAction(i18n("Edit channel"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(editChannel()));
	addAction(action);

	action = new KAction(i18n("Change icon"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(changeIcon()));
	// addAction(action); // FIXME wait till it's implemented
}

DvbChannelContextMenu::~DvbChannelContextMenu()
{
}

void DvbChannelContextMenu::addDeleteAction()
{
	KAction *action = new KAction(i18n("Delete channel"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(deleteChannel()));
	addAction(action);
}

void DvbChannelContextMenu::editChannel()
{
	int pos = view->selectedRow();

	if (pos == -1) {
		return;
	}

	DvbChannelEditor editor(model->getChannel(pos), view);

	if (editor.exec() == QDialog::Accepted) {
		model->updateChannel(pos, editor.getChannel());
	}
}

void DvbChannelContextMenu::changeIcon()
{
	// FIXME
}

void DvbChannelContextMenu::deleteChannel()
{
	int pos = view->selectedRow();

	if (pos != -1) {
		model->removeChannel(pos);
	}
}

DvbChannelEditor::DvbChannelEditor(const QSharedDataPointer<DvbChannel> &channel_, QWidget *parent)
	: KDialog(parent), channel(channel_)
{
	setCaption(i18n("Channel Settings"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);

	QBoxLayout *boxLayout = new QHBoxLayout();
	boxLayout->addWidget(new QLabel(i18n("Name:")));

	nameEdit = new KLineEdit(widget);
	nameEdit->setText(channel->name);
	boxLayout->addWidget(nameEdit);

	boxLayout->addWidget(new QLabel(i18n("Number:")));

	numberBox = new QSpinBox(widget);
	numberBox->setMaximum(99999);
	numberBox->setValue(channel->number);
	boxLayout->addWidget(numberBox);
	mainLayout->addLayout(boxLayout);

	QFrame *frame = new QFrame(widget);
	frame->setFrameShape(QFrame::HLine);
	mainLayout->addWidget(frame);

	QGridLayout *gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Network id:")), 0, 0);

	networkIdBox = new QSpinBox(widget);
	networkIdBox->setRange(-1, (1 << 16) - 1);
	networkIdBox->setValue(channel->networkId);
	gridLayout->addWidget(networkIdBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("Transport stream id:")), 1, 0);

	transportStreamIdBox = new QSpinBox(widget);
	transportStreamIdBox->setRange(0, (1 << 16) - 1);
	transportStreamIdBox->setValue(channel->transportStreamId);
	gridLayout->addWidget(transportStreamIdBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Service id:")), 2, 0);

	serviceIdBox = new QSpinBox(widget);
	serviceIdBox->setRange(0, (1 << 16) - 1);
	serviceIdBox->setValue(channel->serviceId);
	gridLayout->addWidget(serviceIdBox, 2, 1);

	gridLayout->addWidget(new QLabel(i18n("Audio channel:")), 3, 0);

 	// FIXME
	audioChannelBox = new KComboBox(widget);
	audioChannelBox->addItem(QString::number(channel->audioPid));
	audioChannelBox->setCurrentIndex(0);
	gridLayout->addWidget(audioChannelBox, 3, 1);

	gridLayout->addWidget(new QLabel(i18n("Scrambled:")), 4, 0);

	scrambledBox = new QCheckBox(widget);
	scrambledBox->setChecked(channel->scrambled);
	gridLayout->addWidget(scrambledBox, 4, 1);
	mainLayout->addLayout(gridLayout);

	setMainWidget(widget);
}

DvbChannelEditor::~DvbChannelEditor()
{
}

QSharedDataPointer<DvbChannel> DvbChannelEditor::getChannel()
{
	channel->name = nameEdit->text();
	channel->number = numberBox->value(); // FIXME adjust the other channel numbers
	channel->networkId = networkIdBox->value();
	channel->transportStreamId = transportStreamIdBox->value();
	channel->serviceId = serviceIdBox->value();
	// FIXME audio pid
	channel->scrambled = scrambledBox->isChecked();

	return channel;
}
