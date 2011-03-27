/*
 * dvbchanneldialog.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbchanneldialog.h"
#include "dvbchanneldialog_p.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QSpinBox>
#include <KAction>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include "dvbsi.h"

bool DvbChannelLessThan::operator()(const DvbSharedChannel &x, const DvbSharedChannel &y) const
{
	switch (sortOrder) {
	case ChannelNameAscending:
		if (x->name != y->name) {
			return (x->name.localeAwareCompare(y->name) < 0);
		}

		break;
	case ChannelNameDescending:
		if (x->name != y->name) {
			return (x->name.localeAwareCompare(y->name) > 0);
		}

		break;
	case ChannelNumberAscending:
		if (x->number != y->number) {
			return (x->number < y->number);
		}

		break;
	case ChannelNumberDescending:
		if (x->number != y->number) {
			return (x->number > y->number);
		}

		break;
	}

	return (x < y);
}

DvbChannelTableModel::DvbChannelTableModel(QObject *parent) :
	TableModel<DvbChannelTableModelHelper>(parent), channelModel(NULL),
	dndInsertBeforeNumber(-1), dndEventPosted(false)
{
}

DvbChannelTableModel::~DvbChannelTableModel()
{
}

void DvbChannelTableModel::setChannelModel(DvbChannelModel *channelModel_)
{
	if (channelModel != NULL) {
		kWarning() << "channel model already set";
		return;
	}

	channelModel = channelModel_;
	connect(channelModel, SIGNAL(channelAdded(DvbSharedChannel)),
		this, SLOT(channelAdded(DvbSharedChannel)));
	connect(channelModel, SIGNAL(channelAboutToBeUpdated(DvbSharedChannel)),
		this, SLOT(channelAboutToBeUpdated(DvbSharedChannel)));
	connect(channelModel, SIGNAL(channelUpdated(DvbSharedChannel)),
		this, SLOT(channelUpdated(DvbSharedChannel)));
	connect(channelModel, SIGNAL(channelRemoved(DvbSharedChannel)),
		this, SLOT(channelRemoved(DvbSharedChannel)));
	reset(channelModel->getChannels());
}

QVariant DvbChannelTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		switch (section) {
		case 0:
			return i18nc("@title:column tv show", "Channel");
		case 1:
			return i18nc("@title:column tv channel", "Number");
		}
	}

	return QVariant();
}

QVariant DvbChannelTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedChannel &channel = value(index);

	if (channel.isValid()) {
		switch (role) {
		case Qt::DecorationRole:
			if (index.column() == 0) {
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

			break;
		case Qt::DisplayRole:
			switch (index.column()) {
			case 0:
				return channel->name;
			case 1:
				return channel->number;
			}

			break;
		}
	}

	return QVariant();
}

void DvbChannelTableModel::sort(int column, Qt::SortOrder order)
{
	DvbChannelLessThan::SortOrder sortOrder;

	if (order == Qt::AscendingOrder) {
		if (column == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameAscending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberAscending;
		}
	} else {
		if (column == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameDescending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberDescending;
		}
	}

	internalSort(sortOrder);
}

Qt::ItemFlags DvbChannelTableModel::flags(const QModelIndex &index) const
{
	if (index.isValid()) {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDragEnabled);
	} else {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDropEnabled);
	}
}

QMimeData *DvbChannelTableModel::mimeData(const QModelIndexList &indexes) const
{
	QList<DvbSharedChannel> selectedChannels;

	foreach (const QModelIndex &index, indexes) {
		if (index.column() == 0) {
			selectedChannels.append(value(index));
		}
	}

	QMimeData *mimeData = new QMimeData();
	mimeData->setData("application/x-org.kde.kaffeine-selectedchannels", QByteArray());
	// this way the list will be properly deleted once drag and drop ends
	mimeData->setProperty("SelectedChannels", QVariant::fromValue(selectedChannels));
	return mimeData;
}

QStringList DvbChannelTableModel::mimeTypes() const
{
	return QStringList("application/x-org.kde.kaffeine-selectedchannels");
}

Qt::DropActions DvbChannelTableModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

bool DvbChannelTableModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
	int column, const QModelIndex &parent)
{
	Q_UNUSED(column)
	Q_UNUSED(parent)
	dndSelectedChannels.clear();
	dndInsertBeforeNumber = -1;

	if (action == Qt::MoveAction) {
		dndSelectedChannels =
			data->property("SelectedChannels").value<QList<DvbSharedChannel> >();
		const DvbSharedChannel &dndInsertBeforeChannel = value(row);

		if (dndInsertBeforeChannel.isValid()) {
			dndInsertBeforeNumber = dndInsertBeforeChannel->number;
		}

		if (!dndSelectedChannels.isEmpty() && !dndEventPosted) {
			dndEventPosted = true;
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		}
	}

	return false;
}

void DvbChannelTableModel::setFilter(const QString &filter)
{
	// Qt::CaseSensitive == no filtering
	helper.filter.setCaseSensitivity(
		filter.isEmpty() ? Qt::CaseSensitive : Qt::CaseInsensitive);
	helper.filter.setPattern(filter);
	reset(channelModel->getChannels());
}

void DvbChannelTableModel::channelAdded(const DvbSharedChannel &channel)
{
	insert(channel);
}

void DvbChannelTableModel::channelAboutToBeUpdated(const DvbSharedChannel &channel)
{
	aboutToUpdate(channel);
}

void DvbChannelTableModel::channelUpdated(const DvbSharedChannel &channel)
{
	update(channel);
}

void DvbChannelTableModel::channelRemoved(const DvbSharedChannel &channel)
{
	remove(channel);
}

void DvbChannelTableModel::customEvent(QEvent *event)
{
	Q_UNUSED(event)
	bool ok = true;
	emit checkChannelDragAndDrop(&ok);

	if (ok && !dndSelectedChannels.isEmpty()) {
		channelModel->dndMoveChannels(dndSelectedChannels, dndInsertBeforeNumber);
	}

	dndSelectedChannels.clear();
	dndEventPosted = false;
}

DvbChannelView::DvbChannelView(QWidget *parent) : QTreeView(parent), tableModel(NULL)
{
}

DvbChannelView::~DvbChannelView()
{
}

KAction *DvbChannelView::addEditAction()
{
	KAction *action = new KAction(KIcon("configure"), i18nc("@action", "Edit"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(editChannel()));
	addAction(action);
	return action;
}

KAction *DvbChannelView::addRemoveAction()
{
	KAction *action = new KAction(KIcon("edit-delete"), i18nc("@action", "Remove"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(removeChannel()));
	addAction(action);
	return action;
}

void DvbChannelView::setModel(DvbChannelTableModel *tableModel_)
{
	tableModel = tableModel_;
	QTreeView::setModel(tableModel);
}

void DvbChannelView::checkChannelDragAndDrop(bool *ok)
{
	if ((*ok) && ((header()->sortIndicatorSection() != 1) ||
	    (header()->sortIndicatorOrder() != Qt::AscendingOrder))) {
		if (KMessageBox::warningContinueCancel(this, i18nc("@info",
			"The channels will be sorted by number to allow drag and drop.\n"
			"Do you want to continue?")) == KMessageBox::Continue) {
			sortByColumn(1, Qt::AscendingOrder);
		} else {
			*ok = false;
		}
	}
}

void DvbChannelView::editChannel()
{
	QModelIndex index = currentIndex();

	if (index.isValid()) {
		KDialog *dialog = new DvbChannelEditor(tableModel, tableModel->value(index), this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
	}
}

void DvbChannelView::removeChannel()
{
	QList<DvbSharedChannel> selectedChannels;

	foreach (const QModelIndex &index, selectionModel()->selectedIndexes()) {
		if (index.column() == 0) {
			selectedChannels.append(tableModel->value(index));
		}
	}

	DvbChannelModel *channelModel = tableModel->getChannelModel();

	foreach (const DvbSharedChannel &channel, selectedChannels) {
		channelModel->removeChannel(channel);
	}
}

void DvbChannelView::removeAllChannels()
{
	DvbChannelModel *channelModel = tableModel->getChannelModel();

	foreach (const DvbSharedChannel &channel, channelModel->getChannels().values()) {
		channelModel->removeChannel(channel);
	}
}

static QString enumToString(DvbTransponderBase::FecRate fecRate)
{
	switch (fecRate) {
	case DvbTransponderBase::FecNone: return "NONE";
	case DvbTransponderBase::Fec1_2: return "1/2";
	case DvbTransponderBase::Fec1_3: return "1/3";
	case DvbTransponderBase::Fec1_4: return "1/4";
	case DvbTransponderBase::Fec2_3: return "2/3";
	case DvbTransponderBase::Fec2_5: return "2/5";
	case DvbTransponderBase::Fec3_4: return "3/4";
	case DvbTransponderBase::Fec3_5: return "3/5";
	case DvbTransponderBase::Fec4_5: return "4/5";
	case DvbTransponderBase::Fec5_6: return "5/6";
	case DvbTransponderBase::Fec6_7: return "6/7";
	case DvbTransponderBase::Fec7_8: return "7/8";
	case DvbTransponderBase::Fec8_9: return "8/9";
	case DvbTransponderBase::Fec9_10: return "9/10";
	case DvbTransponderBase::FecAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbCTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbCTransponder::Qam16: return "16-QAM";
	case DvbCTransponder::Qam32: return "32-QAM";
	case DvbCTransponder::Qam64: return "64-QAM";
	case DvbCTransponder::Qam128: return "128-QAM";
	case DvbCTransponder::Qam256: return "256-QAM";
	case DvbCTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbSTransponder::Polarization polarization)
{
	switch (polarization) {
	case DvbSTransponder::Horizontal: return i18n("Horizontal");
	case DvbSTransponder::Vertical: return i18n("Vertical");
	case DvbSTransponder::CircularLeft: return i18n("Circular left");
	case DvbSTransponder::CircularRight: return i18n("Circular right");
	}

	return QString();
}

static QString enumToString(DvbS2Transponder::Modulation modulation)
{
	switch (modulation) {
	case DvbS2Transponder::Qpsk: return "QPSK";
	case DvbS2Transponder::Psk8: return "8-PSK";
	case DvbS2Transponder::Apsk16: return "16-APSK";
	case DvbS2Transponder::Apsk32: return "32-APSK";
	case DvbS2Transponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbS2Transponder::RollOff rollOff)
{
	switch (rollOff) {
	case DvbS2Transponder::RollOff20: return "0.20";
	case DvbS2Transponder::RollOff25: return "0.25";
	case DvbS2Transponder::RollOff35: return "0.35";
	case DvbS2Transponder::RollOffAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbTTransponder::Bandwidth6MHz: return "6MHz";
	case DvbTTransponder::Bandwidth7MHz: return "7MHz";
	case DvbTTransponder::Bandwidth8MHz: return "8MHz";
	case DvbTTransponder::BandwidthAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbTTransponder::Qpsk: return "QPSK";
	case DvbTTransponder::Qam16: return "16-QAM";
	case DvbTTransponder::Qam64: return "64-QAM";
	case DvbTTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::TransmissionMode transmissionMode)
{
	switch (transmissionMode) {
	case DvbTTransponder::TransmissionMode2k: return "2k";
	case DvbTTransponder::TransmissionMode4k: return "4k";
	case DvbTTransponder::TransmissionMode8k: return "8k";
	case DvbTTransponder::TransmissionModeAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case DvbTTransponder::GuardInterval1_4: return "1/4";
	case DvbTTransponder::GuardInterval1_8: return "1/8";
	case DvbTTransponder::GuardInterval1_16: return "1/16";
	case DvbTTransponder::GuardInterval1_32: return "1/32";
	case DvbTTransponder::GuardIntervalAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Hierarchy hierarchy)
{
	switch (hierarchy) {
	case DvbTTransponder::HierarchyNone: return "NONE";
	case DvbTTransponder::Hierarchy1: return "1";
	case DvbTTransponder::Hierarchy2: return "2";
	case DvbTTransponder::Hierarchy4: return "4";
	case DvbTTransponder::HierarchyAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(AtscTransponder::Modulation modulation)
{
	switch (modulation) {
	case AtscTransponder::Qam64: return "64-QAM";
	case AtscTransponder::Qam256: return "256-QAM";
	case AtscTransponder::Vsb8: return "8-VSB";
	case AtscTransponder::Vsb16: return "16-VSB";
	case AtscTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

DvbChannelEditor::DvbChannelEditor(DvbChannelTableModel *model_, const DvbSharedChannel &channel_,
	QWidget *parent) : KDialog(parent), model(model_), channel(channel_)
{
	setCaption(i18nc("@title:window", "Edit Channel"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);
	QGridLayout *gridLayout = new QGridLayout();

	nameEdit = new KLineEdit(widget);
	nameEdit->setText(channel->name);
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18nc("@label tv channel", "Name:"), widget);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	numberBox = new QSpinBox(widget);
	numberBox->setRange(1, 99999);
	numberBox->setValue(channel->number);
	gridLayout->addWidget(numberBox, 0, 3);

	label = new QLabel(i18nc("@label tv channel", "Number:"), widget);
	label->setBuddy(numberBox);
	gridLayout->addWidget(label, 0, 2);
	mainLayout->addLayout(gridLayout);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QGroupBox *groupBox = new QGroupBox(widget);
	gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18nc("@label tv channel", "Source:")), 0, 0);
	gridLayout->addWidget(new QLabel(channel->source), 0, 1);

	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *tp = channel->transponder.as<DvbCTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
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
		const DvbS2Transponder *tp2 = channel->transponder.as<DvbS2Transponder>();
		const DvbSTransponder *tp = channel->transponder.as<DvbSTransponder>();

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
		const DvbTTransponder *tp = channel->transponder.as<DvbTTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
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
		const AtscTransponder *tp = channel->transponder.as<AtscTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 2, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 2, 1);
		break;
	    }
	}

	gridLayout->addWidget(new QLabel(), 10, 0, 1, 2);

	gridLayout->addWidget(new QLabel(i18n("PMT PID:")), 11, 0);
	gridLayout->addWidget(new QLabel(QString::number(channel->pmtPid)), 11, 1);

	DvbPmtParser pmtParser(DvbPmtSection(channel->pmtSectionData));
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
		gridLayout->addWidget(
			new QLabel(QString::number(pmtParser.teletextPid)), row++, 1);
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
	DvbChannel updatedChannel = *channel;
	updatedChannel.name = nameEdit->text();
	updatedChannel.number = numberBox->value();
	updatedChannel.networkId = networkIdBox->value();
	updatedChannel.transportStreamId = transportStreamIdBox->value();
	updatedChannel.serviceId = serviceIdBox->value();

	if (audioChannelBox->currentIndex() != -1) {
		updatedChannel.audioPid = audioPids.at(audioChannelBox->currentIndex());
	}

	updatedChannel.isScrambled = scrambledBox->isChecked();
	model->getChannelModel()->updateChannel(channel, updatedChannel);
	KDialog::accept();
}
