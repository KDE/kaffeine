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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include <KStandardDirs>
#include "dvbsi.h"

template<class T> static QStringList displayStrings();

template<> QStringList displayStrings<DvbTransponderBase::FecRate>()
{
	QStringList strings;
	strings.append(/* FecNone = 0  */ "NONE");
	strings.append(/* Fec1_2 = 1   */ "1/2");
	strings.append(/* Fec2_3 = 2   */ "2/3");
	strings.append(/* Fec3_4 = 3   */ "3/4");
	strings.append(/* Fec4_5 = 4   */ "4/5");
	strings.append(/* Fec5_6 = 5   */ "5/6");
	strings.append(/* Fec6_7 = 6   */ "6/7");
	strings.append(/* Fec7_8 = 7   */ "7/8");
	strings.append(/* Fec8_9 = 8   */ "8/9");
	strings.append(/* FecAuto = 9  */ "AUTO");
	strings.append(/* Fec1_3 = 10  */ "1/3");
	strings.append(/* Fec1_4 = 11  */ "1/4");
	strings.append(/* Fec2_5 = 12  */ "2/5");
	strings.append(/* Fec3_5 = 13  */ "3/5");
	strings.append(/* Fec9_10 = 14 */ "9/10");
	return strings;
}

template<> QStringList displayStrings<DvbCTransponder::Modulation>()
{
	QStringList strings;
	strings.append(/* Qam16 = 0          */ "16-QAM");
	strings.append(/* Qam32 = 1          */ "32-QAM");
	strings.append(/* Qam64 = 2          */ "64-QAM");
	strings.append(/* Qam128 = 3         */ "128-QAM");
	strings.append(/* Qam256 = 4         */ "256-QAM");
	strings.append(/* ModulationAuto = 5 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<DvbSTransponder::Polarization>()
{
	QStringList strings;
	strings.append(/* Horizontal = 0    */ i18n("Horizontal"));
	strings.append(/* Vertical = 1      */ i18n("Vertical"));
	strings.append(/* CircularLeft = 2  */ i18n("Circular left"));
	strings.append(/* CircularRight = 3 */ i18n("Circular right"));
	return strings;
}

template<> QStringList displayStrings<DvbS2Transponder::Modulation>()
{
	QStringList strings;
	strings.append(/* Qpsk = 0           */ "QPSK");
	strings.append(/* Psk8 = 1           */ "8-PSK");
	strings.append(/* Apsk16 = 2         */ "16-APSK");
	strings.append(/* Apsk32 = 3         */ "32-APSK");
	strings.append(/* ModulationAuto = 4 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<DvbS2Transponder::RollOff>()
{
	QStringList strings;
	strings.append(/* RollOff20 = 0   */ "0.20");
	strings.append(/* RollOff25 = 1   */ "0.25");
	strings.append(/* RollOff35 = 2   */ "0.35");
	strings.append(/* RollOffAuto = 3 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<DvbTTransponder::Bandwidth>()
{
	QStringList strings;
	strings.append(/* Bandwidth6MHz = 0 */ "6MHz");
	strings.append(/* Bandwidth7MHz = 1 */ "7MHz");
	strings.append(/* Bandwidth8MHz = 2 */ "8MHz");
	strings.append(/* BandwidthAuto = 3 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<DvbTTransponder::Modulation>()
{
	QStringList strings;
	strings.append(/* Qpsk = 0           */ "QPSK");
	strings.append(/* Qam16 = 1          */ "16-QAM");
	strings.append(/* Qam64 = 2          */ "64-QAM");
	strings.append(/* ModulationAuto = 3 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<DvbTTransponder::TransmissionMode>()
{
	QStringList strings;
	strings.append(/* TransmissionMode2k = 0   */ "2k");
	strings.append(/* TransmissionMode8k = 1   */ "8k");
	strings.append(/* TransmissionModeAuto = 2 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<DvbTTransponder::GuardInterval>()
{
	QStringList strings;
	strings.append(/* GuardInterval1_4 = 0  */ "1/4");
	strings.append(/* GuardInterval1_8 = 1  */ "1/8");
	strings.append(/* GuardInterval1_16 = 2 */ "1/16");
	strings.append(/* GuardInterval1_32 = 3 */ "1/32");
	strings.append(/* GuardIntervalAuto = 4 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<DvbTTransponder::Hierarchy>()
{
	QStringList strings;
	strings.append(/* HierarchyNone = 0 */ "NONE");
	strings.append(/* Hierarchy1 = 1    */ "1");
	strings.append(/* Hierarchy2 = 2    */ "2");
	strings.append(/* Hierarchy4 = 3    */ "4");
	strings.append(/* HierarchyAuto = 4 */ "AUTO");
	return strings;
}

template<> QStringList displayStrings<AtscTransponder::Modulation>()
{
	QStringList strings;
	strings.append(/* Qam64 = 0          */ "64-QAM");
	strings.append(/* Qam256 = 1         */ "256-QAM");
	strings.append(/* Vsb8 = 2           */ "8-VSB");
	strings.append(/* Vsb16 = 3          */ "16-VSB");
	strings.append(/* ModulationAuto = 4 */ "AUTO");
	return strings;
}

DvbChannelModel::DvbChannelModel(QObject *parent) : QAbstractTableModel(parent)
{
}

DvbChannelModel::~DvbChannelModel()
{
}

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
	if (!index.isValid() || index.row() >= channels.size()) {
		return QVariant();
	}

	if (role != Qt::DisplayRole) {
		if ((role == Qt::DecorationRole) && (index.column() == 0)) {
			const QSharedDataPointer<DvbChannel> &channel = channels.at(index.row());

			if (channel->videoPid >= 0) {
				if (!channel->scrambled) {
					return KIcon("video-television");
				} else {
					return KIcon("video-television-encrypted");
				}
			} else {
				if (!channel->scrambled) {
					return KIcon("text-speak");
				} else {
					return KIcon("audio-radio-encrypted");
				}
			}
		}

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

bool DvbChannelModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (parent.isValid()) {
		return false;
	}

	beginRemoveRows(QModelIndex(), row, row + count - 1);
	channels.erase(channels.begin() + row, channels.begin() + row + count);
	endRemoveRows();

	return true;
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

QSharedDataPointer<DvbChannel> DvbChannelModel::channelForNumber(int number) const
{
	foreach (const QSharedDataPointer<DvbChannel> &channel, channels) {
		if (channel->number == number) {
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
	QSet<QString> names;
	QSet<int> numbers;

	foreach (const QSharedDataPointer<DvbChannel> &channel, channels) {
		names.insert(channel->name);
		numbers.insert(channel->number);
	}

	beginInsertRows(QModelIndex(), channels.size(), channels.size() + list.size() - 1);

	int number = 1;

	foreach (const QSharedDataPointer<DvbChannel> &channel, list) {
		QString name = channel->name;

		if (names.contains(name)) {
			name = findUniqueName(names, name);
		}

		names.insert(name);

		while (numbers.contains(number)) {
			++number;
		}

		channels.append(channel);
		channels.last()->name = name;
		channels.last()->number = number;

		++number;
	}

	endInsertRows();
}

void DvbChannelModel::updateChannel(int pos, const QSharedDataPointer<DvbChannel> &channel)
{
	QString oldName = channels.at(pos)->name;
	QString newName = channel->name;
	int oldNumber = channels.at(pos)->number;
	int newNumber = channel->number;

	channels.replace(pos, channel);
	emit dataChanged(index(pos, 0), index(pos, columnCount(QModelIndex()) - 1));

	if (oldName != newName) {
		QSet<QString> names;
		int affectedPos = -1;

		for (int i = 0; i < channels.size(); ++i) {
			QString name = channels.at(i)->name;

			if ((i != pos) && (name == newName)) {
				affectedPos = i;
			}

			names.insert(name);
		}

		if (affectedPos != -1) {
			channels[affectedPos]->name = findUniqueName(names, newName);
			QModelIndex modelIndex = index(affectedPos, 0);
			emit dataChanged(modelIndex, modelIndex);
		}
	}

	if (oldNumber != newNumber) {
		QSet<int> numbers;
		int affectedPos = -1;

		for (int i = 0; i < channels.size(); ++i) {
			int number = channels.at(i)->number;

			if ((i != pos) && (number == newNumber)) {
				affectedPos = i;
			}

			numbers.insert(number);
		}

		if (affectedPos != -1) {
			int number = newNumber + 1;

			while (numbers.contains(number)) {
				++number;
			}

			channels[affectedPos]->number = number;
			QModelIndex modelIndex = index(affectedPos, 1);
			emit dataChanged(modelIndex, modelIndex);
		}
	}
}

void DvbChannelModel::loadChannels()
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dtv"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	QSet<QString> names;
	QSet<int> numbers;

	while (!stream.atEnd()) {
		DvbChannel *channel = new DvbChannel;
		channel->readChannel(stream);

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid channels in file" << file.fileName();
			delete channel;
			break;
		}

		if (names.contains(channel->name)) {
			channel->name = findUniqueName(names, channel->name);
		}

		names.insert(channel->name);

		while (numbers.contains(channel->number)) {
			++channel->number;
		}

		numbers.insert(channel->number);

		channels.append(QSharedDataPointer<DvbChannel>(channel));
	}
}

void DvbChannelModel::saveChannels() const
{
	QFile file(KStandardDirs::locateLocal("appdata", "channels.dtv"));

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

QString DvbChannelModel::findUniqueName(const QSet<QString> &names, const QString &name)
{
	QString baseName = name;
	int index = baseName.lastIndexOf('-');

	if (index > 0) {
		QString suffixString = baseName.mid(index + 1);

		if (suffixString == QString::number(suffixString.toInt())) {
			baseName.truncate(index);
		}
	}

	QString newName = baseName;
	int suffix = 0;

	while (names.contains(newName)) {
		newName = baseName + '-' + QString::number(++suffix);
	}

	return newName;
}

DvbChannelView::DvbChannelView(DvbChannelModel *channelModel_, QWidget *parent) :
	ProxyTreeView(parent), channelModel(channelModel_)
{
	KAction *action = new KAction(i18n("Edit"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(editChannel()));
	addAction(action);

	// FIXME Change Icon action
}

DvbChannelView::~DvbChannelView()
{
}

void DvbChannelView::addDeleteAction()
{
	KAction *action = new KAction(i18nc("remove an item from a list", "Remove"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(deleteChannel()));
	addAction(action);
}

void DvbChannelView::editChannel()
{
	int pos = selectedRow();

	if (pos == -1) {
		return;
	}

	DvbChannelEditor editor(channelModel->getChannel(pos), this);

	if (editor.exec() == QDialog::Accepted) {
		channelModel->updateChannel(pos, editor.getChannel());
	}
}

void DvbChannelView::deleteChannel()
{
	QList<int> rows = selectedRows();
	qSort(rows);

	for (int i = rows.size() - 1; i >= 0; --i) {
		// FIXME compress
		channelModel->removeRow(rows.at(i));
	}
}

template<class T> static QString enumToString(T value)
{
	// FIXME efficiency
	return displayStrings<T>().at(value);
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
	numberBox->setRange(1, 99999);
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
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2: {
		const DvbS2Transponder *tp2 = channel->transponder->getDvbS2Transponder();
		const DvbSTransponder *tp = channel->transponder->getDvbSTransponder();

		if (tp == NULL) {
			tp = tp2;
		}

		gridLayout->addWidget(new QLabel(i18n("Polarization:")), 1, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->polarization)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 2, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->frequency / 1000.0)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Symbol rate (kS/s):")), 3, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->symbolRate / 1000.0)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRate)), 4, 1);

		if (tp2 != NULL) {
			gridLayout->addWidget(new QLabel(i18n("Modulation:")), 5, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp2->modulation)), 5, 1);
			gridLayout->addWidget(new QLabel(i18n("Roll-off:")), 6, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp2->rollOff)), 6, 1);
		}

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
	channel->number = numberBox->value();
	channel->networkId = networkIdBox->value();
	channel->transportStreamId = transportStreamIdBox->value();
	channel->serviceId = serviceIdBox->value();

	if (audioChannelBox->currentIndex() != -1) {
		channel->audioPid = audioPids.at(audioChannelBox->currentIndex());
	}

	channel->scrambled = scrambledBox->isChecked();

	return channel;
}
