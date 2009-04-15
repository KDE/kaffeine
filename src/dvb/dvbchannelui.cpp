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
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <KAction>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox> // temporary
#include <KStandardDirs>
#include "../proxytreeview.h"
#include "dvbchannel.h"
#include "dvbsi.h"

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

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	while (!stream.atEnd()) {
		DvbChannel *channel = new DvbChannel;
		channel->readChannel(stream);

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid channels in file" << file.fileName();
			// no i18n: temporary, pre-release only
			KMessageBox::information(NULL, "Please rescan your channels.");
			delete channel;
			break;
		}

		channels.append(QSharedDataPointer<DvbChannel>(channel));
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

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	foreach (const QSharedDataPointer<DvbChannel> &channel, channels) {
		channel->writeChannel(stream);
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

template<class T> static QString enumToString(T value)
{
	return DvbTransponderBase::displayStrings<T>().at(value);
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

	boxLayout = new QHBoxLayout();

	QGroupBox *groupBox = new QGroupBox(widget);
	QGridLayout *gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18n("Source:")), 0, 0);
	gridLayout->addWidget(new QLabel(channel->source), 0, 1);

	switch (channel->transponder->getTransmissionType()) {
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *tp = channel->transponder->getDvbCTransponder();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Symbol rate (kS/s):")), 2, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->symbolRate / 1000.0)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 3, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRate)), 4, 1);
		break;
	    }
	case DvbTransponderBase::DvbS: {
		const DvbSTransponder *tp = channel->transponder->getDvbSTransponder();
		gridLayout->addWidget(new QLabel(i18n("Polarization:")), 1, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->polarization)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 2, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->frequency / 1000.0)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Symbol rate (kS/s):")), 3, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->symbolRate / 1000.0)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRate)), 4, 1);
		break;
	    }
	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *tp = channel->transponder->getDvbTTransponder();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Bandwidth:")), 2, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->bandwidth)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 3, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRateHigh)), 4, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate LP:")), 5, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRateLow)), 5, 1);
		gridLayout->addWidget(new QLabel(i18n("Transmission mode:")), 6, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->transmissionMode)), 6, 1);
		gridLayout->addWidget(new QLabel(i18n("Guard interval:")), 7, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->guardInterval)), 7, 1);
		gridLayout->addWidget(new QLabel(i18n("Hierarchy:")), 8, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->hierarchy)), 8, 1);
		break;
	    }
	case DvbTransponderBase::Atsc: {
		const AtscTransponder *tp = channel->transponder->getAtscTransponder();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 2, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 2, 1);
		break;
	    }
	}

	gridLayout->addWidget(new QLabel(), 10, 0, 1, 2);

	gridLayout->addWidget(new QLabel(i18n("PMT PID:")), 11, 0);
	gridLayout->addWidget(new QLabel(QString::number(channel->pmtPid)), 11, 1);

	gridLayout->addWidget(new QLabel(i18n("Video PID:")), 12, 0);
	gridLayout->addWidget(new QLabel(QString::number(channel->videoPid)), 12, 1);

	int row = 13;
	DvbPmtParser pmtParser(DvbPmtSection(DvbSection(channel->pmtSection)));

	for (QMap<int, QString>::const_iterator it = pmtParser.subtitlePids.constBegin();
	     it != pmtParser.subtitlePids.constEnd(); ++it) {
		if (it == pmtParser.subtitlePids.constBegin()) {
			gridLayout->addWidget(new QLabel(i18n("Subtitle PID:")), row, 0);
		}

		gridLayout->addWidget(new QLabel(QString("%1 (%2)").arg(it.key()).arg(it.value())),
				      row++, 1);
	}

	if (pmtParser.teletextPid != -1) {
		gridLayout->addWidget(new QLabel(i18n("Teletext PID:")), row, 0);
		gridLayout->addWidget(new QLabel(QString::number(pmtParser.teletextPid)), row++, 1);
	}

	gridLayout->addItem(new QSpacerItem(0, 0), row, 0, 1, 2);
	gridLayout->setRowStretch(row, 1);
	boxLayout->addWidget(groupBox);

	groupBox = new QGroupBox(widget);
	gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18n("Network ID:")), 0, 0);

	networkIdBox = new QSpinBox(groupBox);
	networkIdBox->setRange(-1, (1 << 16) - 1);
	networkIdBox->setValue(channel->networkId);
	gridLayout->addWidget(networkIdBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("Transport stream ID:")), 1, 0);

	transportStreamIdBox = new QSpinBox(groupBox);
	transportStreamIdBox->setRange(0, (1 << 16) - 1);
	transportStreamIdBox->setValue(channel->transportStreamId);
	gridLayout->addWidget(transportStreamIdBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Service ID:")), 2, 0);

	serviceIdBox = new QSpinBox(groupBox);
	serviceIdBox->setRange(0, (1 << 16) - 1);
	serviceIdBox->setValue(channel->serviceId);
	gridLayout->addWidget(serviceIdBox, 2, 1);

	gridLayout->addWidget(new QLabel(i18n("Audio channel:")), 3, 0);

	audioChannelBox = new KComboBox(groupBox);

	for (QMap<int, QString>::const_iterator it = pmtParser.audioPids.constBegin();
	     it != pmtParser.audioPids.constEnd(); ++it) {
		QString text = QString::number(it.key());

		if (!it.value().isEmpty()) {
			text = text + " (" + it.value() + ')';
		}

		audioChannelBox->addItem(text);
		audioPids.append(it.key());
	}

	audioChannelBox->setCurrentIndex(audioPids.indexOf(channel->audioPid));

	if (audioPids.size() <= 1) {
		audioChannelBox->setEnabled(false);
	}

	gridLayout->addWidget(audioChannelBox, 3, 1);

	gridLayout->addWidget(new QLabel(i18n("Scrambled:")), 4, 0);

	scrambledBox = new QCheckBox(groupBox);
	scrambledBox->setChecked(channel->scrambled);
	gridLayout->addWidget(scrambledBox, 4, 1);

	gridLayout->addItem(new QSpacerItem(0, 0), 5, 0, 1, 2);
	gridLayout->setRowStretch(5, 1);
	boxLayout->addWidget(groupBox);
	mainLayout->addLayout(boxLayout);

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

	if (audioChannelBox->currentIndex() != -1) {
		channel->audioPid = audioPids.at(audioChannelBox->currentIndex());
	}

	channel->scrambled = scrambledBox->isChecked();

	return channel;
}
