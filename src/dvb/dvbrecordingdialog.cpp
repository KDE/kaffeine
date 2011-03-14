/*
 * dvbrecordingdialog.cpp
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbrecordingdialog.h"
#include "dvbrecordingdialog_p.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <KAction>
#include <KCalendarSystem>
#include <KComboBox>
#include <KLineEdit>
#include "../datetimeedit.h"
#include "dvbmanager.h"

DvbRecordingDialog::DvbRecordingDialog(DvbManager *manager_, QWidget *parent) : KDialog(parent),
	manager(manager_)
{
	setButtons(KDialog::Close);
	setCaption(i18nc("@title:window", "Recording Schedule"));
	QWidget *widget = new QWidget(this);

	model = new DvbRecordingTableModel(manager, this);
	treeView = new QTreeView(widget);
	treeView->header()->setResizeMode(QHeaderView::ResizeToContents);
	treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
	treeView->setModel(model);
	treeView->setRootIsDecorated(false);
	treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	treeView->sortByColumn(2, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);

	QBoxLayout *boxLayout = new QHBoxLayout();
	KAction *action = new KAction(KIcon("list-add"), i18nc("@action", "New"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(newRecording()));
	treeView->addAction(action);
	QPushButton *pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(newRecording()));
	boxLayout->addWidget(pushButton);

	action = new KAction(KIcon("configure"), i18nc("@action", "Edit"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(editRecording()));
	treeView->addAction(action);
	pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(editRecording()));
	boxLayout->addWidget(pushButton);

	action = new KAction(KIcon("edit-delete"), i18nc("@action", "Remove"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(removeRecording()));
	treeView->addAction(action);
	pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(removeRecording()));
	boxLayout->addWidget(pushButton);
	boxLayout->addStretch();

	QBoxLayout *mainLayout = new QVBoxLayout(widget);
	mainLayout->addLayout(boxLayout);
	mainLayout->addWidget(treeView);
	setMainWidget(widget);
	resize(100 * fontMetrics().averageCharWidth(), 20 * fontMetrics().height());
}

DvbRecordingDialog::~DvbRecordingDialog()
{
}

void DvbRecordingDialog::showDialog(DvbManager *manager_, QWidget *parent)
{
	KDialog *dialog = new DvbRecordingDialog(manager_, parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbRecordingDialog::newRecording()
{
	KDialog *dialog = new DvbRecordingEditor(manager, DvbSharedRecording(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbRecordingDialog::editRecording()
{
	QModelIndex index = treeView->currentIndex();

	if (index.isValid()) {
		KDialog *dialog =
			new DvbRecordingEditor(manager, model->getRecording(index), this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
	}
}

void DvbRecordingDialog::removeRecording()
{
	QSet<DvbSharedRecording> recordings;

	foreach (const QModelIndex &modelIndex, treeView->selectionModel()->selectedIndexes()) {
		recordings.insert(model->getRecording(modelIndex));
	}

	DvbRecordingModel *recordingModel = manager->getRecordingModel();

	foreach (const DvbSharedRecording &recording, recordings) {
		recordingModel->removeRecording(recording);
	}
}

DvbRecordingTableModel::DvbRecordingTableModel(DvbManager *manager_, QObject *parent) :
	QAbstractTableModel(parent), manager(manager_)
{
	DvbRecordingModel *recordingModel = manager->getRecordingModel();
	connect(recordingModel, SIGNAL(recordingAdded(DvbSharedRecording)),
		this, SLOT(recordingAdded(DvbSharedRecording)));
	connect(recordingModel, SIGNAL(recordingUpdated(DvbSharedRecording,DvbRecording)),
		this, SLOT(recordingUpdated(DvbSharedRecording,DvbRecording)));
	connect(recordingModel, SIGNAL(recordingRemoved(DvbSharedRecording)),
		this, SLOT(recordingRemoved(DvbSharedRecording)));

	foreach (const DvbSharedRecording &recording, recordingModel->getRecordings()) {
		recordings.append(recording);
	}

	qSort(recordings.begin(), recordings.end(), DvbRecordingLessThan());
}

DvbRecordingTableModel::~DvbRecordingTableModel()
{
}

DvbSharedRecording DvbRecordingTableModel::getRecording(const QModelIndex &index) const
{
	if ((index.row() >= 0) && (index.row() < recordings.size())) {
		return recordings.at(index.row());
	}

	return DvbSharedRecording();
}

int DvbRecordingTableModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 4;
	}

	return 0;
}

int DvbRecordingTableModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return recordings.size();
	}

	return 0;
}

QVariant DvbRecordingTableModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		switch (section) {
		case 0:
			return i18nc("@title:column recording", "Name");
		case 1:
			return i18nc("@title:column tv show", "Channel");
		case 2:
			return i18nc("@title:column tv show", "Begin");
		case 3:
			return i18nc("@title:column tv show", "Duration");
		}
	}

	return QVariant();
}

QVariant DvbRecordingTableModel::data(const QModelIndex &index, int role) const
{
	if ((index.row() < 0) || (index.row() >= recordings.size())) {
		return QVariant();
	}

	const DvbSharedRecording &recording = recordings.at(index.row());

	switch (role) {
	case Qt::DecorationRole:
		if (index.column() == 0) {
			switch (recording->status) {
			case DvbRecording::Inactive:
				break;
			case DvbRecording::Recording:
				return KIcon("media-record");
			case DvbRecording::Error:
				return KIcon("dialog-error");
			}

			if (recording->repeat != 0) {
				return KIcon("view-refresh");
			}
		}

		break;
	case Qt::DisplayRole:
		switch (index.column()) {
		case 0:
			return recording->name;
		case 1:
			return recording->channel->name;
		case 2:
			return KGlobal::locale()->formatDateTime(recording->begin.toLocalTime());
		case 3:
			return KGlobal::locale()->formatTime(recording->duration, false, true);
		}

		break;
	}

	return QVariant();
}

void DvbRecordingTableModel::recordingAdded(const DvbSharedRecording &recording)
{
	int row = (qLowerBound(recordings.constBegin(), recordings.constEnd(), recording,
		DvbRecordingLessThan()) - recordings.constBegin());

	beginInsertRows(QModelIndex(), row, row);
	recordings.insert(row, recording);
	endInsertRows();
}

void DvbRecordingTableModel::recordingUpdated(const DvbSharedRecording &recording,
	const DvbRecording &oldRecording)
{
	Q_UNUSED(recording)
	int row = (qBinaryFind(recordings.constBegin(), recordings.constEnd(), oldRecording,
		DvbRecordingLessThan()) - recordings.constBegin());

	if (row < recordings.size()) {
		emit dataChanged(index(row, 0), index(row, 3));
	}
}

void DvbRecordingTableModel::recordingRemoved(const DvbSharedRecording &recording)
{
	int row = (qBinaryFind(recordings.constBegin(), recordings.constEnd(), recording,
		DvbRecordingLessThan()) - recordings.constBegin());

	if (row < recordings.size()) {
		beginRemoveRows(QModelIndex(), row, row);
		recordings.removeAt(row);
		endRemoveRows();
	}
}

DvbRecordingEditor::DvbRecordingEditor(DvbManager *manager_, const DvbSharedRecording &recording_,
	QWidget *parent) : KDialog(parent), manager(manager_), recording(recording_)
{
	setCaption(i18nc("@title:window recording", "Edit Schedule Entry"));
	QWidget *widget = new QWidget(this);
	QGridLayout *gridLayout = new QGridLayout(widget);

	nameEdit = new KLineEdit(widget);
	connect(nameEdit, SIGNAL(textChanged(QString)), this, SLOT(checkValidity()));
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18nc("@label recording", "Name:"), widget);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	channelBox = new KComboBox(widget);
	DvbChannelTableModel *channelModel =
		new DvbChannelTableModel(manager->getChannelModel(), widget);
	QHeaderView *header = manager->getChannelView()->header();
	channelModel->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
	channelBox->setModel(channelModel);
	connect(channelBox, SIGNAL(currentIndexChanged(int)), this, SLOT(checkValidity()));
	gridLayout->addWidget(channelBox, 1, 1);

	label = new QLabel(i18nc("@label tv show", "Channel:"), widget);
	label->setBuddy(channelBox);
	gridLayout->addWidget(label, 1, 0);

	beginEdit = new DateTimeEdit(widget);
	beginEdit->setCurrentSection(DateTimeEdit::HourSection);
	connect(beginEdit, SIGNAL(dateTimeChanged(QDateTime)),
		this, SLOT(beginChanged(QDateTime)));
	gridLayout->addWidget(beginEdit, 2, 1);

	label = new QLabel(i18nc("@label tv show", "Begin:"), widget);
	label->setBuddy(beginEdit);
	gridLayout->addWidget(label, 2, 0);

	durationEdit = new DurationEdit(widget);
	connect(durationEdit, SIGNAL(timeChanged(QTime)), this, SLOT(durationChanged(QTime)));
	gridLayout->addWidget(durationEdit, 3, 1);

	label = new QLabel(i18nc("@label tv show", "Duration:"), widget);
	label->setBuddy(durationEdit);
	gridLayout->addWidget(label, 3, 0);

	endEdit = new DateTimeEdit(widget);
	endEdit->setCurrentSection(DateTimeEdit::HourSection);
	connect(endEdit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(endChanged(QDateTime)));
	gridLayout->addWidget(endEdit, 4, 1);

	label = new QLabel(i18nc("@label tv show", "End:"), widget);
	label->setBuddy(endEdit);
	gridLayout->addWidget(label, 4, 0);

	gridLayout->addWidget(new QLabel(i18nc("@label recording", "Repeat:"), widget), 5, 0);

	QBoxLayout *boxLayout = new QHBoxLayout();
	QPushButton *pushButton =
		new QPushButton(i18nc("@action next to 'Repeat:'", "Never"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(repeatNever()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(i18nc("@action next to 'Repeat:'", "Daily"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(repeatDaily()));
	boxLayout->addWidget(pushButton);
	gridLayout->addLayout(boxLayout, 5, 1);

	QGridLayout *dayLayout = new QGridLayout();
	const KCalendarSystem *calendar = KGlobal::locale()->calendar();

	for (int i = 0; i < 7; ++i) {
		dayCheckBoxes[i] = new QCheckBox(
			calendar->weekDayName(i + 1, KCalendarSystem::ShortDayName), widget);
		dayLayout->addWidget(dayCheckBoxes[i], (i >> 2), (i & 0x03));
	}

	gridLayout->addLayout(dayLayout, 6, 1);
	setMainWidget(widget);

	if (recording.isValid()) {
		nameEdit->setText(recording->name);
		channelBox->setCurrentIndex(
			channelModel->indexForChannel(recording->channel).row());
		beginEdit->setDateTime(recording->begin.toLocalTime());
		durationEdit->setTime(recording->duration);

		for (int i = 0; i < 7; ++i) {
			if ((recording->repeat & (1 << i)) != 0) {
				dayCheckBoxes[i]->setChecked(true);
			}
		}

		switch (recording->status) {
		case DvbRecording::Inactive:
			break;
		case DvbRecording::Recording:
		case DvbRecording::Error:
			nameEdit->setEnabled(false);
			channelBox->setEnabled(false);
			beginEdit->setEnabled(false);
			break;
		}
	} else {
		channelBox->setCurrentIndex(-1);
		beginEdit->setDateTime(QDateTime::currentDateTime());
		durationEdit->setTime(QTime(2, 0));
	}

	checkValidity();

	if (nameEdit->text().isEmpty()) {
		nameEdit->setFocus();
	}
}

DvbRecordingEditor::~DvbRecordingEditor()
{
}

void DvbRecordingEditor::beginChanged(const QDateTime &begin)
{
	// attention: setDateTimeRange and setDateTime influence each other!
	QTime duration = durationEdit->time();
	endEdit->setDateTimeRange(begin, begin.addSecs((23 * 60 + 59) * 60));
	endEdit->setDateTime(begin.addSecs(QTime().secsTo(duration)));
}

void DvbRecordingEditor::durationChanged(const QTime &duration)
{
	endEdit->setDateTime(beginEdit->dateTime().addSecs(QTime().secsTo(duration)));
}

void DvbRecordingEditor::endChanged(const QDateTime &end)
{
	durationEdit->setTime(QTime().addSecs(beginEdit->dateTime().secsTo(end)));
}

void DvbRecordingEditor::repeatNever()
{
	for (int i = 0; i < 7; ++i) {
		dayCheckBoxes[i]->setChecked(false);
	}
}

void DvbRecordingEditor::repeatDaily()
{
	for (int i = 0; i < 7; ++i) {
		dayCheckBoxes[i]->setChecked(true);
	}
}

void DvbRecordingEditor::checkValidity()
{
	enableButtonOk(!nameEdit->text().isEmpty() && (channelBox->currentIndex() != -1));
}

void DvbRecordingEditor::accept()
{
	DvbRecording newRecording;
	newRecording.name = nameEdit->text();
	newRecording.channel =
		manager->getChannelModel()->findChannelByName(channelBox->currentText());
	newRecording.begin = beginEdit->dateTime().toUTC();
	newRecording.duration = durationEdit->time();

	for (int i = 0; i < 7; ++i) {
		if (dayCheckBoxes[i]->isChecked()) {
			newRecording.repeat |= (1 << i);
		}
	}

	if (!recording.isValid()) {
		manager->getRecordingModel()->addRecording(newRecording);
	} else {
		manager->getRecordingModel()->updateRecording(recording, newRecording);
	}

	KDialog::accept();
}
