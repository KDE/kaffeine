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
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include "../log.h"
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
		Log("DvbChannelTableModel::setChannelModel: channel model already set");
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
						return KIcon(QLatin1String("video-television"));
					} else {
						return KIcon(QLatin1String("video-television-encrypted"));
					}
				} else {
					if (!channel->isScrambled) {
						return KIcon(QLatin1String("text-speak"));
					} else {
						return KIcon(QLatin1String("audio-radio-encrypted"));
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
	mimeData->setData(QLatin1String("application/x-org.kde.kaffeine-selectedchannels"), QByteArray());
	// this way the list will be properly deleted once drag and drop ends
	mimeData->setProperty("SelectedChannels", QVariant::fromValue(selectedChannels));
	return mimeData;
}

QStringList DvbChannelTableModel::mimeTypes() const
{
	return QStringList(QLatin1String("application/x-org.kde.kaffeine-selectedchannels"));
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
	KAction *action = new KAction(KIcon(QLatin1String("configure")), i18nc("@action", "Edit"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(editChannel()));
	addAction(action);
	return action;
}

KAction *DvbChannelView::addRemoveAction()
{
	KAction *action = new KAction(KIcon(QLatin1String("edit-delete")), i18nc("@action", "Remove"), this);
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

static QLatin1String enumToString(DvbTransponderBase::FecRate fecRate)
{
	const char *text = "";

	switch (fecRate) {
	case DvbTransponderBase::FecNone: text = "NONE"; break;
	case DvbTransponderBase::Fec1_2: text = "1/2"; break;
	case DvbTransponderBase::Fec1_3: text = "1/3"; break;
	case DvbTransponderBase::Fec1_4: text = "1/4"; break;
	case DvbTransponderBase::Fec2_3: text = "2/3"; break;
	case DvbTransponderBase::Fec2_5: text = "2/5"; break;
	case DvbTransponderBase::Fec3_4: text = "3/4"; break;
	case DvbTransponderBase::Fec3_5: text = "3/5"; break;
	case DvbTransponderBase::Fec4_5: text = "4/5"; break;
	case DvbTransponderBase::Fec5_6: text = "5/6"; break;
	case DvbTransponderBase::Fec6_7: text = "6/7"; break;
	case DvbTransponderBase::Fec7_8: text = "7/8"; break;
	case DvbTransponderBase::Fec8_9: text = "8/9"; break;
	case DvbTransponderBase::Fec9_10: text = "9/10"; break;
	case DvbTransponderBase::FecAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbCTransponder::Modulation modulation)
{
	const char *text = "";

	switch (modulation) {
	case DvbCTransponder::Qam16: text = "16-QAM"; break;
	case DvbCTransponder::Qam32: text = "32-QAM"; break;
	case DvbCTransponder::Qam64: text = "64-QAM"; break;
	case DvbCTransponder::Qam128: text = "128-QAM"; break;
	case DvbCTransponder::Qam256: text = "256-QAM"; break;
	case DvbCTransponder::ModulationAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
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

static QLatin1String enumToString(DvbS2Transponder::Modulation modulation)
{
	const char *text = "";

	switch (modulation) {
	case DvbS2Transponder::Qpsk: text = "QPSK"; break;
	case DvbS2Transponder::Psk8: text = "8-PSK"; break;
	case DvbS2Transponder::Apsk16: text = "16-APSK"; break;
	case DvbS2Transponder::Apsk32: text = "32-APSK"; break;
	case DvbS2Transponder::ModulationAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbS2Transponder::RollOff rollOff)
{
	const char *text = "";

	switch (rollOff) {
	case DvbS2Transponder::RollOff20: text = "0.20"; break;
	case DvbS2Transponder::RollOff25: text = "0.25"; break;
	case DvbS2Transponder::RollOff35: text = "0.35"; break;
	case DvbS2Transponder::RollOffAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbTTransponder::Bandwidth bandwidth)
{
	const char *text = "";

	switch (bandwidth) {
	case DvbTTransponder::Bandwidth6MHz: text = "6MHz"; break;
	case DvbTTransponder::Bandwidth7MHz: text = "7MHz"; break;
	case DvbTTransponder::Bandwidth8MHz: text = "8MHz"; break;
	case DvbTTransponder::BandwidthAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbTTransponder::Modulation modulation)
{
	const char *text = "";

	switch (modulation) {
	case DvbTTransponder::Qpsk: text = "QPSK"; break;
	case DvbTTransponder::Qam16: text = "16-QAM"; break;
	case DvbTTransponder::Qam64: text = "64-QAM"; break;
	case DvbTTransponder::ModulationAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbTTransponder::TransmissionMode transmissionMode)
{
	const char *text = "";

	switch (transmissionMode) {
	case DvbTTransponder::TransmissionMode2k: text = "2k"; break;
	case DvbTTransponder::TransmissionMode4k: text = "4k"; break;
	case DvbTTransponder::TransmissionMode8k: text = "8k"; break;
	case DvbTTransponder::TransmissionModeAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbTTransponder::GuardInterval guardInterval)
{
	const char *text = "";

	switch (guardInterval) {
	case DvbTTransponder::GuardInterval1_4: text = "1/4"; break;
	case DvbTTransponder::GuardInterval1_8: text = "1/8"; break;
	case DvbTTransponder::GuardInterval1_16: text = "1/16"; break;
	case DvbTTransponder::GuardInterval1_32: text = "1/32"; break;
	case DvbTTransponder::GuardIntervalAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbTTransponder::Hierarchy hierarchy)
{
	const char *text = "";

	switch (hierarchy) {
	case DvbTTransponder::HierarchyNone: text = "NONE"; break;
	case DvbTTransponder::Hierarchy1: text = "1"; break;
	case DvbTTransponder::Hierarchy2: text = "2"; break;
	case DvbTTransponder::Hierarchy4: text = "4"; break;
	case DvbTTransponder::HierarchyAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(AtscTransponder::Modulation modulation)
{
	const char *text = "";

	switch (modulation) {
	case AtscTransponder::Qam64: text = "64-QAM"; break;
	case AtscTransponder::Qam256: text = "256-QAM"; break;
	case AtscTransponder::Vsb8: text = "8-VSB"; break;
	case AtscTransponder::Vsb16: text = "16-VSB"; break;
	case AtscTransponder::ModulationAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
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

	DvbPmtSection pmtSection(channel->pmtSectionData);
	DvbPmtParser pmtParser(pmtSection);
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
		gridLayout->addWidget(new QLabel(QString(QLatin1String("%1 (%2)")).arg(it.first).arg(it.second)),
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

	audioStreamBox = new KComboBox(groupBox);

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.audioPids.at(i);
		QString text = QString::number(it.first);

		if (!it.second.isEmpty()) {
			text = text + QLatin1String(" (") + it.second + QLatin1Char(')');
		}

		audioStreamBox->addItem(text);
		audioPids.append(it.first);
	}

	audioStreamBox->setCurrentIndex(audioPids.indexOf(channel->audioPid));

	if (audioPids.size() <= 1) {
		audioStreamBox->setEnabled(false);
	}

	gridLayout->addWidget(audioStreamBox, 3, 1);

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

	if (audioStreamBox->currentIndex() != -1) {
		updatedChannel.audioPid = audioPids.at(audioStreamBox->currentIndex());
	}

	updatedChannel.isScrambled = scrambledBox->isChecked();
	model->getChannelModel()->updateChannel(channel, updatedChannel);
	KDialog::accept();
}
