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

#include <KLocalizedString>
#include <QDebug>
#if QT_VERSION < 0x050500
# define qInfo qDebug
#endif

#include <KConfigGroup>
#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include "../datetimeedit.h"
#include "dvbchanneldialog.h"
#include "dvbmanager.h"
#include "dvbrecordingdialog.h"
#include "dvbrecordingdialog_p.h"

DvbRecordingDialog::DvbRecordingDialog(DvbManager *manager_, QWidget *parent) : QDialog(parent),
	manager(manager_)
{
	setWindowTitle(i18nc("@title:window", "Recording Schedule"));

	QWidget *mainWidget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(mainWidget);

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	QWidget *widget = new QWidget(this);

	model = new DvbRecordingTableModel(this);
	treeView = new QTreeView(widget);
	treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
	treeView->setModel(model);
	treeView->setRootIsDecorated(false);
	treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	treeView->sortByColumn(2, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);
	model->setRecordingModel(manager->getRecordingModel());

	QBoxLayout *boxLayout = new QHBoxLayout();
	QAction *action = new QAction(QIcon::fromTheme(QLatin1String("list-add"), QIcon(":list-add")), i18nc("@action", "New"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(newRecording()));
	treeView->addAction(action);
	QPushButton *pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(newRecording()));
	boxLayout->addWidget(pushButton);

	action = new QAction(QIcon::fromTheme(QLatin1String("configure"), QIcon(":configure")), i18nc("@action", "Edit"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(editRecording()));
	treeView->addAction(action);
	pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(editRecording()));
	boxLayout->addWidget(pushButton);

	action = new QAction(QIcon::fromTheme(QLatin1String("edit-delete"), QIcon(":edit-delete")), i18nc("@action", "Remove"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(removeRecording()));
	treeView->addAction(action);
	pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(removeRecording()));
	boxLayout->addWidget(pushButton);
	boxLayout->addStretch();

	mainLayout->addLayout(boxLayout);
	mainLayout->addWidget(treeView);
	mainLayout->addWidget(buttonBox);

	resize(100 * fontMetrics().averageCharWidth(), 20 * fontMetrics().height());
}

DvbRecordingDialog::~DvbRecordingDialog()
{
}

void DvbRecordingDialog::showDialog(DvbManager *manager_, QWidget *parent)
{
	QDialog *dialog = new DvbRecordingDialog(manager_, parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbRecordingDialog::newRecording()
{
	QDialog *dialog = new DvbRecordingEditor(manager, DvbSharedRecording(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbRecordingDialog::editRecording()
{
	QModelIndex index = treeView->currentIndex();

	if (index.isValid()) {
		QDialog *dialog = new DvbRecordingEditor(manager, model->value(index), this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
	}
}

void DvbRecordingDialog::removeRecording()
{
	QSet<DvbSharedRecording> recordings;

	foreach (const QModelIndex &modelIndex, treeView->selectionModel()->selectedIndexes()) {
		recordings.insert(model->value(modelIndex));
	}

	DvbRecordingModel *recordingModel = manager->getRecordingModel();

	foreach (const DvbSharedRecording &recording, recordings) {
		recordingModel->addToUnwantedRecordings(recording);
		recordingModel->removeRecording(recording);
	}
}

void DvbRecordingLessThan::setSortOrder(SortOrder sortOrder_)
{
	for (int index = 0; index < 4; ++index) {
		if ((sortOrder[index] >> 1) == (sortOrder_ >> 1)) {
			for (int j = index; j > 0; --j) {
				sortOrder[j] = sortOrder[j - 1];
			}

			sortOrder[0] = sortOrder_;
			return;
		}
	}
}

bool DvbRecordingLessThan::operator()(const DvbSharedRecording &x,
	const DvbSharedRecording &y) const
{
	for (int index = 0; index < 4; ++index) {
		switch (sortOrder[index]) {
		case NameAscending:
			if (x->name != y->name) {
				return (x->name.localeAwareCompare(y->name) < 0);
			}

			break;
		case NameDescending:
			if (x->name != y->name) {
				return (x->name.localeAwareCompare(y->name) > 0);
			}

			break;
		case ChannelAscending:
			if (x->channel->name != y->channel->name) {
				return (x->channel->name.localeAwareCompare(y->channel->name) < 0);
			}

			break;
		case ChannelDescending:
			if (x->channel->name != y->channel->name) {
				return (x->channel->name.localeAwareCompare(y->channel->name) > 0);
			}

			break;
		case BeginAscending:
			if (x->begin != y->begin) {
				return (x->begin < y->begin);
			}

			break;
		case BeginDescending:
			if (x->begin != y->begin) {
				return (x->begin > y->begin);
			}

			break;
		case DurationAscending:
			if (x->duration != y->duration) {
				return (x->duration < y->duration);
			}

			break;
		case DurationDescending:
			if (x->duration != y->duration) {
				return (x->duration > y->duration);
			}

			break;
		}
	}

	return (x < y);
}

DvbRecordingTableModel::DvbRecordingTableModel(QObject *parent) :
	TableModel<DvbRecordingTableModelHelper>(parent), recordingModel(NULL)
{
}

DvbRecordingTableModel::~DvbRecordingTableModel()
{
}

void DvbRecordingTableModel::setRecordingModel(DvbRecordingModel *recordingModel_)
{
	if (recordingModel != NULL) {
		qWarning("%s", qPrintable(i18n("recording model already set")));
		return;
	}

	recordingModel = recordingModel_;
	connect(recordingModel, SIGNAL(recordingAdded(DvbSharedRecording)),
		this, SLOT(recordingAdded(DvbSharedRecording)));
	connect(recordingModel, SIGNAL(recordingAboutToBeUpdated(DvbSharedRecording)),
		this, SLOT(recordingAboutToBeUpdated(DvbSharedRecording)));
	connect(recordingModel, SIGNAL(recordingUpdated(DvbSharedRecording)),
		this, SLOT(recordingUpdated(DvbSharedRecording)));
	connect(recordingModel, SIGNAL(recordingRemoved(DvbSharedRecording)),
		this, SLOT(recordingRemoved(DvbSharedRecording)));
	reset(recordingModel->getRecordings());
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
			return i18nc("@title:column tv show", "Start");
		case 3:
			return i18nc("@title:column tv show", "Duration");
		case 4:
			return i18nc("@title:column tv show", "Disabled");
		}
	}

	return QVariant();
}

QVariant DvbRecordingTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedRecording &recording = value(index);

	if (recording.isValid()) {
		switch (role) {
		case Qt::DecorationRole:
			if (index.column() == 0) {
				if (recording->disabled) {
					return QIcon::fromTheme(QLatin1String("dialog-error"), QIcon(":dialog-error"));
				}
				switch (recording->status) {
				case DvbRecording::Inactive:
					break;
				case DvbRecording::Recording:
					return QIcon::fromTheme(QLatin1String("media-record"), QIcon(":media-record"));
				case DvbRecording::Error:
					return QIcon::fromTheme(QLatin1String("dialog-error"), QIcon(":dialog-error"));
				}

				if (recording->repeat != 0) {
					return QIcon::fromTheme(QLatin1String("view-refresh"), QIcon(":view-refresh"));
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
				return QLocale().toString((recording->begin.toLocalTime()), QLocale::ShortFormat);
			case 3:
				return QLocale().toString(recording->duration);
			case 4: {
				if (recording->disabled) {
					return QString("Disabled");
				}
				return QString("Enabled");
				}
			}
			break;
		}
	}

	return QVariant();
}

void DvbRecordingTableModel::sort(int column, Qt::SortOrder order)
{
	DvbRecordingLessThan::SortOrder sortOrder;

	if (order == Qt::AscendingOrder) {
		switch (column) {
		case 0:
			sortOrder = DvbRecordingLessThan::NameAscending;
			break;
		case 1:
			sortOrder = DvbRecordingLessThan::ChannelAscending;
			break;
		case 2:
			sortOrder = DvbRecordingLessThan::BeginAscending;
			break;
		case 3:
			sortOrder = DvbRecordingLessThan::DurationAscending;
			break;
		default:
			return;
		}
	} else {
		switch (column) {
		case 0:
			sortOrder = DvbRecordingLessThan::NameDescending;
			break;
		case 1:
			sortOrder = DvbRecordingLessThan::ChannelDescending;
			break;
		case 2:
			sortOrder = DvbRecordingLessThan::BeginDescending;
			break;
		case 3:
			sortOrder = DvbRecordingLessThan::DurationDescending;
			break;
		default:
			return;
		}
	}

	internalSort(sortOrder);
}

void DvbRecordingTableModel::recordingAdded(const DvbSharedRecording &recording)
{
	insert(recording);
}

void DvbRecordingTableModel::recordingAboutToBeUpdated(const DvbSharedRecording &recording)
{
	aboutToUpdate(recording);
}

void DvbRecordingTableModel::recordingUpdated(const DvbSharedRecording &recording)
{
	update(recording);
}

void DvbRecordingTableModel::recordingRemoved(const DvbSharedRecording &recording)
{
	remove(recording);
}

DvbRecordingEditor::DvbRecordingEditor(DvbManager *manager_, const DvbSharedRecording &recording_,
	QWidget *parent) : QDialog(parent), manager(manager_), recording(recording_)
{
	setWindowTitle(i18nc("@title:window recording", "Edit Schedule Entry"));
	QWidget *widget = new QWidget(this);
	QGridLayout *gridLayout = new QGridLayout(widget);
	QBoxLayout *mainLayout = new QVBoxLayout;
	setLayout(mainLayout);

	nameEdit = new QLineEdit(widget);
	mainLayout->addWidget(nameEdit);
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18nc("@label recording", "Name:"), widget);
	mainLayout->addWidget(label);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	channelBox = new QComboBox(widget);
	mainLayout->addWidget(channelBox);
	DvbChannelTableModel *channelModel = new DvbChannelTableModel(widget);
	QHeaderView *header = manager->getChannelView()->header();
	channelModel->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
	channelModel->setChannelModel(manager->getChannelModel());
	channelBox->setModel(channelModel);
	gridLayout->addWidget(channelBox, 1, 1);

	label = new QLabel(i18nc("@label tv show", "Channel:"), widget);
	mainLayout->addWidget(label);
	label->setBuddy(channelBox);
	gridLayout->addWidget(label, 1, 0);

	beginEdit = new DateTimeEdit(widget);
	mainLayout->addWidget(beginEdit);
	beginEdit->setCurrentSection(DateTimeEdit::HourSection);
	connect(beginEdit, SIGNAL(dateTimeChanged(QDateTime)),
		this, SLOT(beginChanged(QDateTime)));
	gridLayout->addWidget(beginEdit, 2, 1);

	label = new QLabel(i18nc("@label tv show", "Start:"), widget);
	mainLayout->addWidget(label);
	label->setBuddy(beginEdit);
	gridLayout->addWidget(label, 2, 0);

	durationEdit = new DurationEdit(widget);
	mainLayout->addWidget(durationEdit);
	connect(durationEdit, SIGNAL(timeChanged(QTime)), this, SLOT(durationChanged(QTime)));
	gridLayout->addWidget(durationEdit, 3, 1);

	label = new QLabel(i18nc("@label tv show", "Duration:"), widget);
	mainLayout->addWidget(label);
	label->setBuddy(durationEdit);
	gridLayout->addWidget(label, 3, 0);

	endEdit = new DateTimeEdit(widget);
	mainLayout->addWidget(endEdit);
	endEdit->setCurrentSection(DateTimeEdit::HourSection);
	connect(endEdit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(endChanged(QDateTime)));
	gridLayout->addWidget(endEdit, 4, 1);

	label = new QLabel(i18nc("@label tv show", "End:"), widget);
	mainLayout->addWidget(label);
	label->setBuddy(endEdit);
	gridLayout->addWidget(label, 4, 0);

	gridLayout->addWidget(new QLabel(i18nc("@label recording", "Repeat:"), widget), 5, 0);

	QBoxLayout *boxLayout = new QHBoxLayout();
	QPushButton *pushButton =
		new QPushButton(i18nc("@action next to 'Repeat:'", "Never"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(repeatNever()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(i18nc("@action next to 'Repeat:'", "Daily"), widget);
	mainLayout->addWidget(pushButton);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(repeatDaily()));
	boxLayout->addWidget(pushButton);
	gridLayout->addLayout(boxLayout, 5, 1);

	QGridLayout *dayLayout = new QGridLayout();

	for (int i = 0; i < 7; ++i) {
		dayCheckBoxes[i] = new QCheckBox(
			QLocale::system().dayName(i + 1, QLocale::ShortFormat), widget);
		dayLayout->addWidget(dayCheckBoxes[i], (i >> 2), (i & 0x03));
	}

	gridLayout->addLayout(dayLayout, 6, 1);
	mainLayout->addWidget(widget);

	if (recording.isValid()) {
		nameEdit->setText(recording->name);
		channelBox->setCurrentIndex(channelModel->find(recording->channel).row());
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

	buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	mainLayout->addWidget(buttonBox);

	checkValidity();

	connect(nameEdit, SIGNAL(textChanged(QString)), this, SLOT(checkValidity()));
	connect(channelBox, SIGNAL(currentIndexChanged(int)), this, SLOT(checkValidity()));

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
	endEdit->setDateTime(begin.addSecs(QTime(0, 0, 0).secsTo(duration)));
}

void DvbRecordingEditor::durationChanged(const QTime &duration)
{
	endEdit->setDateTime(beginEdit->dateTime().addSecs(QTime(0, 0, 0).secsTo(duration)));
}

void DvbRecordingEditor::endChanged(const QDateTime &end)
{
	durationEdit->setTime(QTime(0, 0, 0).addSecs(beginEdit->dateTime().secsTo(end)));
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
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
				!nameEdit->text().isEmpty() &&
				(channelBox->currentIndex() != -1));
}

void DvbRecordingEditor::accept()
{
	DvbRecording newRecording;
	newRecording.disabled = false;
	newRecording.priority = 100;
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

	QDialog::accept();
}
