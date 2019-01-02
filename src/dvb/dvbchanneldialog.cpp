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

#include "../log.h"

#include <KConfigGroup>
#include <KMessageBox>
#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QSpinBox>

#include "dvbchanneldialog.h"
#include "dvbchanneldialog_p.h"
#include "dvbsi.h"
#include "../iso-codes.h"

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
		qCWarning(logDvb, "Channel model already set");
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
						return QIcon::fromTheme(QLatin1String("video-television"), QIcon(":video-television"));
					} else {
						return QIcon::fromTheme(QLatin1String("video-television-encrypted"), QIcon(":video-television-encrypted"));
					}
				} else {
					if (!channel->isScrambled) {
						return QIcon::fromTheme(QLatin1String("text-speak"), QIcon(":text-speak"));
					} else {
						return QIcon::fromTheme(QLatin1String("audio-radio-encrypted"), QIcon(":audio-radio-encrypted"));
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

QAction *DvbChannelView::addEditAction()
{
	QAction *action = new QAction(QIcon::fromTheme(QLatin1String("configure"), QIcon(":configure")), i18nc("@action", "Edit"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(editChannel()));
	addAction(action);
	return action;
}

QAction *DvbChannelView::addRemoveAction()
{
	QAction *action = new QAction(QIcon::fromTheme(QLatin1String("edit-delete"), QIcon(":edit-delete")), i18nc("@action", "Remove"), this);
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
		QDialog *dialog = new DvbChannelEditor(tableModel, tableModel->value(index), this);
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
	case DvbSTransponder::Off: return i18n("Off");
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
	case DvbTTransponder::Bandwidth5MHz: text = "5MHz"; break;
	case DvbTTransponder::Bandwidth6MHz: text = "6MHz"; break;
	case DvbTTransponder::Bandwidth7MHz: text = "7MHz"; break;
	case DvbTTransponder::Bandwidth8MHz: text = "8MHz"; break;
	case DvbTTransponder::BandwidthAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(DvbT2Transponder::Bandwidth bandwidth)
{
	const char *text = "";

	switch (bandwidth) {
	case DvbT2Transponder::Bandwidth1_7MHz:	text = "1.7MHz"; break;
	case DvbT2Transponder::Bandwidth5MHz:	text = "5MHz"; break;
	case DvbT2Transponder::Bandwidth6MHz:	text = "6MHz"; break;
	case DvbT2Transponder::Bandwidth7MHz:	text = "7MHz"; break;
	case DvbT2Transponder::Bandwidth8MHz:	text = "8MHz"; break;
	case DvbT2Transponder::Bandwidth10MHz:	text = "10MHz"; break;
	case DvbT2Transponder::BandwidthAuto:	text = "AUTO"; break;
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

static QLatin1String enumToString(DvbT2Transponder::Modulation modulation)
{
	const char *text = "";

	switch (modulation) {
	case DvbT2Transponder::Qpsk: text = "QPSK"; break;
	case DvbT2Transponder::Qam16: text = "16-QAM"; break;
	case DvbT2Transponder::Qam64: text = "64-QAM"; break;
	case DvbT2Transponder::Qam256: text = "256-QAM"; break;
	case DvbT2Transponder::ModulationAuto: text = "AUTO"; break;
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

static QLatin1String enumToString(DvbT2Transponder::TransmissionMode transmissionMode)
{
	const char *text = "";

	switch (transmissionMode) {
	case DvbT2Transponder::TransmissionMode1k: text = "1k"; break;
	case DvbT2Transponder::TransmissionMode2k: text = "2k"; break;
	case DvbT2Transponder::TransmissionMode4k: text = "4k"; break;
	case DvbT2Transponder::TransmissionMode8k: text = "8k"; break;
	case DvbT2Transponder::TransmissionMode16k: text = "16k"; break;
	case DvbT2Transponder::TransmissionMode32k: text = "32k"; break;
	case DvbT2Transponder::TransmissionModeAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(IsdbTTransponder::TransmissionMode transmissionMode)
{
	const char *text = "";

	switch (transmissionMode) {
	case IsdbTTransponder::TransmissionMode2k: text = "2k"; break;
	case IsdbTTransponder::TransmissionMode4k: text = "4k"; break;
	case IsdbTTransponder::TransmissionMode8k: text = "8k"; break;
	case IsdbTTransponder::TransmissionModeAuto: text = "AUTO"; break;
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

static QLatin1String enumToString(DvbT2Transponder::GuardInterval guardInterval)
{
	const char *text = "";

	switch (guardInterval) {
	case DvbT2Transponder::GuardInterval1_4:	text = "1/4"; break;
	case DvbT2Transponder::GuardInterval19_128:	text = "19/128"; break;
	case DvbT2Transponder::GuardInterval1_8:	text = "1/8"; break;
	case DvbT2Transponder::GuardInterval19_256:	text = "19/256"; break;
	case DvbT2Transponder::GuardInterval1_16:	text = "1/16"; break;
	case DvbT2Transponder::GuardInterval1_32:	text = "1/32"; break;
	case DvbT2Transponder::GuardInterval1_128:	text = "1/128"; break;
	case DvbT2Transponder::GuardIntervalAuto:	text = "AUTO"; break;
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

static QLatin1String enumToString(DvbT2Transponder::Hierarchy hierarchy)
{
	const char *text = "";

	switch (hierarchy) {
	case DvbT2Transponder::HierarchyNone: text = "NONE"; break;
	case DvbT2Transponder::Hierarchy1: text = "1"; break;
	case DvbT2Transponder::Hierarchy2: text = "2"; break;
	case DvbT2Transponder::Hierarchy4: text = "4"; break;
	case DvbT2Transponder::HierarchyAuto: text = "AUTO"; break;
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

static QLatin1String enumToString(IsdbTTransponder::Bandwidth bandwidth)
{
	const char *text = "";

	switch (bandwidth) {
	case IsdbTTransponder::Bandwidth6MHz: text = "6MHz"; break;
	case IsdbTTransponder::Bandwidth7MHz: text = "7MHz"; break;
	case IsdbTTransponder::Bandwidth8MHz: text = "8MHz"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(IsdbTTransponder::Modulation modulation)
{
	const char *text = "";

	switch (modulation) {
	case IsdbTTransponder::Dqpsk: text = "DQPSK"; break;
	case IsdbTTransponder::Qpsk: text = "QPSK"; break;
	case IsdbTTransponder::Qam16: text = "16-QAM"; break;
	case IsdbTTransponder::Qam64: text = "64-QAM"; break;
	case IsdbTTransponder::ModulationAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(IsdbTTransponder::GuardInterval guardInterval)
{
	const char *text = "";

	switch (guardInterval) {
	case IsdbTTransponder::GuardInterval1_4: text = "1/4"; break;
	case IsdbTTransponder::GuardInterval1_8: text = "1/8"; break;
	case IsdbTTransponder::GuardInterval1_16: text = "1/16"; break;
	case IsdbTTransponder::GuardInterval1_32: text = "1/32"; break;
	case IsdbTTransponder::GuardIntervalAuto: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(IsdbTTransponder::PartialReception partialReception)
{
	const char *text = "";

	switch (partialReception) {
	case IsdbTTransponder::PR_disabled: text = "No"; break;
	case IsdbTTransponder::PR_enabled: text = "Yes"; break;
	case IsdbTTransponder::PR_AUTO: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

static QLatin1String enumToString(IsdbTTransponder::SoundBroadcasting soundBroadcasting)
{
	const char *text = "";

	switch (soundBroadcasting) {
	case IsdbTTransponder::SB_disabled: text = "No"; break;
	case IsdbTTransponder::SB_enabled: text = "Yes"; break;
	case IsdbTTransponder::SB_AUTO: text = "AUTO"; break;
	}

	return QLatin1String(text);
}

DvbChannelEditor::DvbChannelEditor(DvbChannelTableModel *model_, const DvbSharedChannel &channel_,
	QWidget *parent) : QDialog(parent), model(model_), channel(channel_)
{
	setWindowTitle(i18nc("@title:window", "Edit Channel"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);
	QGridLayout *gridLayout = new QGridLayout();

	nameEdit = new QLineEdit(widget);
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

	int row = 11;

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
	case DvbTransponderBase::DvbT2: {
		const DvbT2Transponder *tp = channel->transponder.as<DvbT2Transponder>();
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
		gridLayout->addWidget(new QLabel(i18n("PLP (stream ID):")), 9, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->streamId)), 9, 1);
		break;
	    }
	case DvbTransponderBase::IsdbT: {
		const IsdbTTransponder *tp = channel->transponder.as<IsdbTTransponder>();
		row= 1;
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), row, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), row++, 1);
		gridLayout->addWidget(new QLabel(i18n("Bandwidth:")), row, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->bandwidth)), row++, 1);
		gridLayout->addWidget(new QLabel(i18n("Transmission mode:")), row, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->transmissionMode)), row++, 1);
		gridLayout->addWidget(new QLabel(i18n("Guard interval:")), row, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->guardInterval)), row++, 1);

		gridLayout->addWidget(new QLabel(i18n("Partial reception:")), row, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->partialReception)), row++, 1);
		gridLayout->addWidget(new QLabel(i18n("Sound broadcasting:")), row, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->soundBroadcasting)), row++, 1);

		if (tp->soundBroadcasting == 1) {
			gridLayout->addWidget(new QLabel(i18n("SB channel ID:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->subChannelId)), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("SB index:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->subChannelId)), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("SB count:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->sbSegmentCount)), row++, 1);
		}

		if (tp->layerEnabled[0]) {
			gridLayout->addWidget(new QLabel(i18n("Layer A Modulation:")), row, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp->modulation[0])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer A FEC rate:")), row, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp->fecRate[0])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer A segments:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->segmentCount[0])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer A interleaving:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->interleaving[0])), row++, 1);
		}
		if (tp->layerEnabled[1]) {
			gridLayout->addWidget(new QLabel(i18n("Layer B Modulation:")), row, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp->modulation[1])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer B FEC rate:")), row, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp->fecRate[1])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer B segments:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->segmentCount[1])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer B interleaving:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->interleaving[1])), row++, 1);
		}
		if (tp->layerEnabled[2]) {
			gridLayout->addWidget(new QLabel(i18n("Layer C Modulation:")), row, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp->modulation[2])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer C FEC rate:")), row, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp->fecRate[2])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer C segments:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->segmentCount[2])), row++, 1);
			gridLayout->addWidget(new QLabel(i18n("Layer C interleaving:")), row, 0);
			gridLayout->addWidget(new QLabel(QString::number(tp->interleaving[2])), row++, 1);
		}

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

	gridLayout->addItem(new QSpacerItem(0, 0), row, 0, 1, 2);
	gridLayout->setRowStretch(row, 1);
	boxLayout->addWidget(groupBox);

	groupBox = new QGroupBox(widget);
	gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18n("Network ID:")), 0, 0);

	row = 1;

	networkIdBox = new QSpinBox(groupBox);
	networkIdBox->setRange(-1, (1 << 16) - 1);
	networkIdBox->setValue(channel->networkId);
	gridLayout->addWidget(networkIdBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("Transport stream ID:")), row, 0);

	transportStreamIdBox = new QSpinBox(groupBox);
	transportStreamIdBox->setRange(0, (1 << 16) - 1);
	transportStreamIdBox->setValue(channel->transportStreamId);
	gridLayout->addWidget(transportStreamIdBox, row++, 1);

	gridLayout->addWidget(new QLabel(i18n("Service ID:")), row, 0);

	serviceIdBox = new QSpinBox(groupBox);
	serviceIdBox->setRange(0, (1 << 16) - 1);
	serviceIdBox->setValue(channel->serviceId);
	gridLayout->addWidget(serviceIdBox, row++, 1);

	gridLayout->addWidget(new QLabel(i18n("Audio channel:")), row, 0);
	audioStreamBox = new QComboBox(groupBox);
	audioStreamBox->setCurrentIndex(audioPids.indexOf(channel->audioPid));
	if (audioPids.size() <= 1) {
		audioStreamBox->setEnabled(false);
	}
	gridLayout->addWidget(audioStreamBox, row++, 1);

	gridLayout->addWidget(new QLabel(i18n("Scrambled:")), row, 0);
	scrambledBox = new QCheckBox(groupBox);
	scrambledBox->setChecked(channel->isScrambled);
	gridLayout->addWidget(scrambledBox, row++, 1);

	row++;

	gridLayout->addWidget(new QLabel(i18n("PMT PID:")), row, 0);
	gridLayout->addWidget(new QLabel(QString::number(channel->pmtPid)), row++, 1);

	DvbPmtSection pmtSection(channel->pmtSectionData);
	DvbPmtParser pmtParser(pmtSection);

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.audioPids.at(i);
		QString text = QString::number(it.first);

		if (!it.second.isEmpty()) {
			QString languageString;

			if (!IsoCodes::getLanguage(it.second, &languageString))
				languageString = it.second;

			text = text + QLatin1String(" (") + languageString + QLatin1Char(')');
		}

		audioStreamBox->addItem(text);
		audioPids.append(it.first);
	}

	if (pmtParser.videoPid >= 0) {
		gridLayout->addWidget(new QLabel(i18n("Video PID:")), row, 0);
		gridLayout->addWidget(new QLabel(QString::number(pmtParser.videoPid)), row++, 1);
	}

	if (!pmtParser.subtitlePids.isEmpty()) {
		gridLayout->addWidget(new QLabel(i18n("Subtitle PID:")), row, 0);
	}

	for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.subtitlePids.at(i);
		QString languageString;

		if (!IsoCodes::getLanguage(it.second, &languageString))
			languageString = it.second;


		gridLayout->addWidget(new QLabel(QString(QLatin1String("%1 (%2)")).arg(it.first).arg(languageString)),
			row++, 1);
	}

	if (pmtParser.teletextPid != -1) {
		gridLayout->addWidget(new QLabel(i18n("Teletext PID:")), row, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(pmtParser.teletextPid)), row++, 1);
	}

	gridLayout->addItem(new QSpacerItem(0, 0), 5, 0, 1, 2);
	gridLayout->setRowStretch(5, 1);
	boxLayout->addWidget(groupBox);
	mainLayout->addLayout(boxLayout);

	mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(widget);

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	mainLayout->addWidget(buttonBox);

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
	QDialog::accept();
}
