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
#include "../tablemodel.h"
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
			const DvbChannel *channel = channels.at(index.row()).constData();

			if (channel->hasVideo) {
				if (!channel->isScrambled) {
					return KIcon("video-television");
				} else {
					return KIcon("video-television-encrypted");
				}
			} else {
				if (!channel->isScrambled) {
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

	for (int i = row; i < (row + count); ++i) {
		names.remove(channels.at(i)->name);
		numbers.remove(channels.at(i)->number);
	}

	channels.erase(channels.begin() + row, channels.begin() + row + count);
	endRemoveRows();

	return true;
}

const DvbChannel *DvbChannelModel::getChannel(int row) const
{
	return channels.at(row).constData();
}

int DvbChannelModel::indexOfName(const QString &name) const
{
	for (int row = 0; row < channels.size(); ++row) {
		if (channels.at(row)->name == name) {
			return row;
		}
	}

	return -1;
}

int DvbChannelModel::indexOfNumber(int number) const
{
	for (int row = 0; row < channels.size(); ++row) {
		if (channels.at(row)->number == number) {
			return row;
		}
	}

	return -1;
}

QList<QSharedDataPointer<DvbChannel> > DvbChannelModel::getChannels() const
{
	return channels;
}

void DvbChannelModel::cloneFrom(const DvbChannelModel *other)
{
	channels = other->channels;
	names = other->names;
	numbers = other->numbers;
	reset();
}

void DvbChannelModel::clear()
{
	int size = channels.size();

	if (size > 0) {
		beginRemoveRows(QModelIndex(), 0, size - 1);
		channels.clear();
		names.clear();
		numbers.clear();
		endRemoveRows();
	}
}

void DvbChannelModel::appendChannels(const QList<DvbChannel *> &list)
{
	if (!list.isEmpty()) {
		beginInsertRows(QModelIndex(), channels.size(), channels.size() + list.size() - 1);

		foreach (DvbChannel *channel, list) {
			adjustNameNumber(channel);
			channels.append(QSharedDataPointer<DvbChannel>(channel));
			names.insert(channel->name);
			numbers.insert(channel->number);
		}

		endInsertRows();
	}
}

void DvbChannelModel::updateChannel(int pos, DvbChannel *channel)
{
	QString oldName = channels.at(pos)->name;
	QString newName = channel->name;

	if (oldName != newName) {
		names.remove(oldName);

		for (int i = 0;; ++i) {
			if (i == channels.size()) {
				names.insert(newName);
				break;
			}

			if ((i != pos) && (channels.at(i)->name == newName)) {
				adjustNameNumber(channels[i].data());
				names.insert(channels.at(i)->name);
				QModelIndex modelIndex = index(i, 0);
				emit dataChanged(modelIndex, modelIndex);
			}
		}
	}

	int oldNumber = channels.at(pos)->number;
	int newNumber = channel->number;

	if (oldNumber != newNumber) {
		numbers.remove(oldNumber);

		for (int i = 0;; ++i) {
			if (i == channels.size()) {
				numbers.insert(newNumber);
				break;
			}

			if ((i != pos) && (channels.at(i)->number == newNumber)) {
				adjustNameNumber(channels[i].data());
				numbers.insert(channels.at(i)->number);
				QModelIndex modelIndex = index(i, 1);
				emit dataChanged(modelIndex, modelIndex);
			}
		}
	}

	channels.replace(pos, QSharedDataPointer<DvbChannel>(channel));
	emit dataChanged(index(pos, 0), index(pos, columnCount(QModelIndex()) - 1));
}

bool DvbChannelModel::adjustNameNumber(DvbChannel *channel) const
{
	bool dataModified = false;

	if (names.contains(channel->name)) {
		QString baseName = channel->name;
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

		channel->name = newName;
		dataModified = true;
	}

	if (numbers.contains(channel->number)) {
		int newNumber = channel->number + 1;

		while (numbers.contains(newNumber)) {
			++newNumber;
		}

		channel->number = newNumber;
		dataModified = true;
	}

	return dataModified;
}

class DvbSqlChannelModelAdaptor : public SqlModelAdaptor, public SqlTableModelBase
{
public:
	DvbSqlChannelModelAdaptor(DvbChannelModel *model_) : SqlModelAdaptor(model_, this),
		model(model_)
	{
		init("Channels", QStringList() << "Name" << "Number" << "Source" << "Transponder" <<
			"NetworkId" << "TransportStreamId" << "PmtPid" << "PmtSection" <<
			"AudioPid" << "Flags");

		for (int i = 0; i < model->channels.size(); ++i) {
			if (model->adjustNameNumber(model->channels[i].data())) {
				emit model->dataChanged(model->index(i, 0), model->index(i, 1));
			}

			const DvbChannel *channel = model->channels.at(i).constData();
			model->names.insert(channel->name);
			model->numbers.insert(channel->number);
		}
	}

	~DvbSqlChannelModelAdaptor()
	{
	}

private:
	int insertFromSqlQuery(const QSqlQuery &query, int index)
	{
		DvbChannel *channel = new DvbChannel();
		channel->name = query.value(index++).toString();
		channel->number = query.value(index++).toInt();
		channel->source = query.value(index++).toString();
		QString transponder = query.value(index++).toString();

		DvbTransponderBase *transponderBase = NULL;

		if (transponder.size() >= 2) {
			if (transponder.at(0) == 'C') {
				transponderBase = new DvbCTransponder();
			} else if (transponder.at(0) == 'S') {
				if (transponder.at(1) != '2') {
					transponderBase = new DvbSTransponder();
				} else {
					transponderBase = new DvbS2Transponder();
				}
			} else if (transponder.at(0) == 'T') {
				transponderBase = new DvbTTransponder();
			} else if (transponder.at(0) == 'A') {
				transponderBase = new AtscTransponder();
			}
		}

		if ((transponderBase == NULL) || !transponderBase->fromString(transponder)) {
			delete transponderBase;
			delete channel;
			return -1;
		}

		channel->transponder = DvbTransponder(transponderBase);
		channel->networkId = query.value(index++).toInt();
		channel->transportStreamId = query.value(index++).toInt();
		channel->pmtPid = query.value(index++).toInt();
		channel->pmtSection = query.value(index++).toByteArray();
		channel->audioPid = query.value(index++).toInt();
		int flags = query.value(index++).toInt();

		channel->hasVideo = ((flags & 0x01) != 0);
		channel->isScrambled = ((flags & 0x02) != 0);

		if (channel->name.isEmpty() || (channel->number < 1) || channel->source.isEmpty() ||
		    (channel->networkId < -1) || (channel->networkId > 0xffff) ||
		    (channel->transportStreamId < 0) || (channel->transportStreamId > 0xffff) ||
		    (channel->pmtPid < 0) || (channel->pmtPid > 0x1fff) ||
		    channel->pmtSection.isEmpty() ||
		    (channel->audioPid < -1) || (channel->audioPid > 0x1fff)) {
			delete channel;
			return -1;
		}

		model->channels.append(QSharedDataPointer<DvbChannel>(channel));
		return (model->channels.size() - 1);
	}

	void bindToSqlQuery(int row, QSqlQuery &query, int index) const
	{
		const DvbChannel *channel = model->getChannel(row);
		query.bindValue(index++, channel->name);
		query.bindValue(index++, channel->number);
		query.bindValue(index++, channel->source);
		query.bindValue(index++, channel->transponder->toString());
		query.bindValue(index++, channel->networkId);
		query.bindValue(index++, channel->transportStreamId);
		query.bindValue(index++, channel->pmtPid);
		query.bindValue(index++, channel->pmtSection);
		query.bindValue(index++, channel->audioPid);
		query.bindValue(index++, (channel->hasVideo ? 0x01 : 0) |
					 (channel->isScrambled ? 0x02 : 0));
	}

	DvbChannelModel *model;
};

DvbSqlChannelModel::DvbSqlChannelModel(QObject *parent) : DvbChannelModel(parent)
{
	sqlAdaptor = new DvbSqlChannelModelAdaptor(this);

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", "channels.dtv"));

	if (!file.exists()) {
		return;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	QList<DvbChannel *> channels;

	while (!stream.atEnd()) {
		DvbChannel *channel = new DvbChannel;
		channel->readChannel(stream);

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid channels in file" << file.fileName();
			delete channel;
			break;
		}

		channels.append(channel);
	}

	appendChannels(channels);

	if (!file.remove()) {
		kWarning() << "can't remove" << file.fileName();
	}
}

DvbSqlChannelModel::~DvbSqlChannelModel()
{
	sqlAdaptor->flush();
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
	int row = selectedRow();

	if (row >= 0) {
		KDialog *dialog = new DvbChannelEditor(channelModel, row, this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
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

DvbChannelEditor::DvbChannelEditor(DvbChannelModel *model_, int row_, QWidget *parent) :
	KDialog(parent), model(model_), row(row_)
{
	setCaption(i18n("Channel Settings"));
	const DvbChannel *channel = model->getChannel(row);

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

	DvbPmtParser pmtParser(DvbPmtSection(channel->pmtSection));
	int row = 12;

	if (pmtParser.videoPid >= 0) {
		gridLayout->addWidget(new QLabel(i18n("Video PID:")), row, 0);
		gridLayout->addWidget(new QLabel(QString::number(pmtParser.videoPid)), row++, 1);
	}

	if (!pmtParser.subtitlePids.isEmpty()) {
		gridLayout->addWidget(new QLabel(i18n("Subtitle PID:")), row, 0);
	}

	for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.subtitlePids.at(i);
		gridLayout->addWidget(new QLabel(QString("%1 (%2)").arg(it.first).arg(it.second)),
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
	serviceIdBox->setValue(channel->getServiceId());
	gridLayout->addWidget(serviceIdBox, 2, 1);

	gridLayout->addWidget(new QLabel(i18n("Audio channel:")), 3, 0);

	audioChannelBox = new KComboBox(groupBox);

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.audioPids.at(i);
		QString text = QString::number(it.first);

		if (!it.second.isEmpty()) {
			text = text + " (" + it.second + ')';
		}

		audioChannelBox->addItem(text);
		audioPids.append(it.first);
	}

	audioChannelBox->setCurrentIndex(audioPids.indexOf(channel->audioPid));

	if (audioPids.size() <= 1) {
		audioChannelBox->setEnabled(false);
	}

	gridLayout->addWidget(audioChannelBox, 3, 1);

	gridLayout->addWidget(new QLabel(i18n("Scrambled:")), 4, 0);

	scrambledBox = new QCheckBox(groupBox);
	scrambledBox->setChecked(channel->isScrambled);
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

void DvbChannelEditor::accept()
{
	DvbChannel *channel = new DvbChannel(*model->getChannel(row));
	channel->name = nameEdit->text();
	channel->number = numberBox->value();
	channel->networkId = networkIdBox->value();
	channel->transportStreamId = transportStreamIdBox->value();
	channel->setServiceId(serviceIdBox->value());

	if (audioChannelBox->currentIndex() != -1) {
		channel->audioPid = audioPids.at(audioChannelBox->currentIndex());
	}

	channel->isScrambled = scrambledBox->isChecked();
	model->updateChannel(row, channel);

	KDialog::accept();
}
