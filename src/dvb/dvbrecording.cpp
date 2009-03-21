/*
 * dvbrecording.cpp
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbrecording.h"

#include <QBoxLayout>
#include <QDateTimeEdit>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTimerEvent>
#include <KComboBox>
#include <KLineEdit>
#include <KLocalizedString>
#include "../proxytreeview.h"
#include "dvbchannelview.h"
#include "dvbmanager.h"
#include "dvbsi.h"

void DvbFileWriter::write(const QByteArray &data)
{
	file->write(data);
}

class DvbRecording
{
public:
	explicit DvbRecording(DvbManager *manager_);
	~DvbRecording();

	bool isRunning() const;
	void start();

	void releaseDevice();

	QString name;
	QString channelName;
	QDateTime begin;
	QTime duration;
	QDateTime end;

private:
	DvbManager *manager;
	DvbDevice *device;
	QSharedDataPointer<DvbChannel> channel;

	QFile file;
	DvbFileWriter fileWriter;
	DvbPatPmtInjector *injector;
};

DvbRecording::DvbRecording(DvbManager *manager_) : manager(manager_), device(NULL),
	fileWriter(&file), injector(NULL)
{
}

DvbRecording::~DvbRecording()
{
	delete injector;
}

bool DvbRecording::isRunning() const
{
	return (device != NULL) && (device->getDeviceState() != DvbDevice::DeviceIdle) &&
		(device->getDeviceState() != DvbDevice::DeviceTuningFailed);
}

void DvbRecording::start()
{
	if (!file.isOpen()) {
		QString basePath = manager->getRecordingFolder() + '/' + name;

		for (int attempt = 0; attempt < 100; ++attempt) {
			QString path = basePath;

			if (attempt > 0) {
				path += '-' + QString::number(attempt);
			}

			// FIXME .ts or .m2t(s) ?

			file.setFileName(path + ".ts");

			if (file.exists()) {
				continue;
			}

			// FIXME racy - but shouldn't hurt?

			if (!file.open(QIODevice::WriteOnly)) {
				continue;
			}

			break;
		}

		if (!file.isOpen()) {
			// FIXME error message
			return;
		}
	}

	if (channel == NULL) {
		channel = manager->getChannelModel()->channelForName(channelName);

		if (channel == NULL) {
			// FIXME error message
			return;
		}
	}

	if (device == NULL) {
		// assign device
		device = manager->requestDevice(channel->transponder);

		if (device == NULL) {
			// FIXME error message
			return;
		}
	}

	if (injector == NULL) {
		QList<int> pids;

		if (channel->videoPid != -1) {
			pids.append(channel->videoPid);
		}

		if (channel->audioPid != -1) {
			pids.append(channel->audioPid);
		}

		injector = new DvbPatPmtInjector(device, channel->transportStreamId,
			channel->serviceId, channel->pmtPid, pids);

		QObject::connect(injector, SIGNAL(dataReady(QByteArray)),
			&fileWriter, SLOT(write(QByteArray)));
	}

	if ((device->getDeviceState() != DvbDevice::DeviceIdle) &&
	    (device->getDeviceState() != DvbDevice::DeviceTuningFailed)) {
		return;
	}

	// FIXME retune
}

void DvbRecording::releaseDevice()
{
	if (device != NULL) {
		injector->removePidFilters();
		manager->releaseDevice(device);
	}
}

DvbRecordingModel::DvbRecordingModel(DvbManager *manager_) : QAbstractTableModel(manager_),
	manager(manager_)
{
	// FIXME read recordings from file

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed

	timerId = startTimer(5000);
}

DvbRecordingModel::~DvbRecordingModel()
{
	// FIXME write recordings to file
	qDeleteAll(recordings);
}

DvbRecording *DvbRecordingModel::at(int i)
{
	return recordings.at(i);
}

void DvbRecordingModel::append(DvbRecording *recording)
{
	beginInsertRows(QModelIndex(), recordings.size(), recordings.size());
	recordings.append(recording);
	endInsertRows();
	checkStatus();
}

void DvbRecordingModel::remove(int i)
{
	beginRemoveRows(QModelIndex(), i, i);
	DvbRecording *recording = recordings.takeAt(i);
	recording->releaseDevice();
	delete recording;
	endRemoveRows();
}

void DvbRecordingModel::updated(int i)
{
	emit dataChanged(index(i, 0), index(i, columnCount(QModelIndex()) - 1));
	checkStatus();
}

int DvbRecordingModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 4;
}

QVariant DvbRecordingModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() >= recordings.size()) {
		return QVariant();
	}

	if (role != Qt::DisplayRole) {
		if ((role == Qt::DecorationRole) && (index.column() == 0) &&
		    recordings.at(index.row())->isRunning()) {
			return KIcon("media-record");
		}

		return QVariant();
	}

	switch (index.column()) {
	case 0:
		return recordings.at(index.row())->name;
	case 1:
		return recordings.at(index.row())->channelName;
	case 2:
		return recordings.at(index.row())->begin;
	case 3:
		return recordings.at(index.row())->duration;
	}

	return QVariant();
}

QVariant DvbRecordingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18n("Name");
	case 1:
		return i18n("Channel");
	case 2:
		return i18n("Begin");
	case 3:
		return i18n("Duration");
	}

	return QVariant();
}

int DvbRecordingModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return recordings.size();
}

void DvbRecordingModel::timerEvent(QTimerEvent *event)
{
	if (event->timerId() != timerId) {
		QAbstractTableModel::timerEvent(event);
		return;
	}

	checkStatus();
}

void DvbRecordingModel::checkStatus()
{
	QDateTime current = QDateTime::currentDateTime();

	for (int i = 0; i < recordings.size(); ++i) {
		DvbRecording *recording = recordings.at(i);

		if (recording->end <= current) {
			// stop recording
			remove(i);
			--i;
		}
	}

	for (int i = 0; i < recordings.size(); ++i) {
		DvbRecording *recording = recordings.at(i);

		if (recording->begin <= current) {
			if (recording->isRunning()) {
				continue;
			}

			// start recording
			recording->start();

			if (recording->isRunning()) {
				QModelIndex modelIndex = index(i, 0);
				emit dataChanged(modelIndex, modelIndex);
			}
		}
	}
}

DvbRecordingDialog::DvbRecordingDialog(DvbManager *manager_, QWidget *parent) : KDialog(parent),
	manager(manager_)
{
	setCaption(i18n("Recordings"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);

	QBoxLayout *layout = new QHBoxLayout();
	mainLayout->addLayout(layout);

	QPushButton *pushButton = new QPushButton(i18n("New"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(newRecording()));
	layout->addWidget(pushButton);

	pushButton = new QPushButton(i18n("Edit"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(editRecording()));
	layout->addWidget(pushButton);

	pushButton = new QPushButton(i18n("Remove"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(removeRecording()));
	layout->addWidget(pushButton);

	layout->addStretch(1);

	model = manager->getRecordingModel();

	treeView = new ProxyTreeView(this);
	treeView->setIndentation(0);
	treeView->setModel(model);
	treeView->sortByColumn(2, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);
	mainLayout->addWidget(treeView);

	setMainWidget(widget);
}

DvbRecordingDialog::~DvbRecordingDialog()
{
}

void DvbRecordingDialog::newRecording()
{
	DvbRecording *recording = new DvbRecording(manager);
	recording->begin = QDateTime::currentDateTime();
	// the seconds aren't visible --> set them to zero
	recording->begin = recording->begin.addSecs(-recording->begin.time().second());
	recording->duration = QTime(2, 0);

	DvbRecordingEditor editor(manager->getChannelModel(), recording, this);

	if (editor.exec() == QDialog::Accepted) {
		// FIXME enforce validation
		editor.updateRecording(recording);
		model->append(recording);
	} else {
		delete recording;
	}
}

void DvbRecordingDialog::editRecording()
{
	int row = treeView->selectedRow();

	if (row < 0) {
		return;
	}

	DvbRecording *recording = model->at(row);

	DvbRecordingEditor editor(manager->getChannelModel(), recording, this);

	if (editor.exec() == QDialog::Accepted) {
		editor.updateRecording(recording);
		model->updated(row);
	}
}

void DvbRecordingDialog::removeRecording()
{
	int row = treeView->selectedRow();

	if (row < 0) {
		return;
	}

	model->remove(row);
}

DvbRecordingEditor::DvbRecordingEditor(QAbstractItemModel *channels, const DvbRecording *recording,
	QWidget *parent) : KDialog(parent)
{
	setCaption(i18n("Edit Recording"));

	QWidget *widget = new QWidget(this);
	QGridLayout *layout = new QGridLayout(widget);

	layout->addWidget(new QLabel(i18n("Name:")), 0, 0);

	nameEdit = new KLineEdit(widget);
	nameEdit->setText(recording->name);
	layout->addWidget(nameEdit, 0, 1);

	layout->addWidget(new QLabel(i18n("Channel:")), 1, 0);

	channelBox = new KComboBox(widget);
	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
	proxyModel->setSortLocaleAware(true);
	proxyModel->setSourceModel(channels);
	proxyModel->sort(0);
	channelBox->setModel(proxyModel);
	// findText doesn't work with qt < 4.5 (uses Qt::EditRole)
	channelBox->setCurrentIndex(channelBox->findData(recording->channelName, Qt::DisplayRole));
	layout->addWidget(channelBox, 1, 1);

	layout->addWidget(new QLabel(i18n("Begin:")), 2, 0);

	beginEdit = new QDateTimeEdit(recording->begin, widget);
	beginEdit->setCurrentSection(QDateTimeEdit::HourSection);
	connect(beginEdit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(beginChanged(QDateTime)));
	layout->addWidget(beginEdit, 2, 1);

	layout->addWidget(new QLabel(i18n("Duration:")), 3, 0);

	durationEdit = new QTimeEdit(recording->duration, widget);
	connect(durationEdit, SIGNAL(timeChanged(QTime)), this, SLOT(durationChanged(QTime)));
	layout->addWidget(durationEdit, 3, 1);

	layout->addWidget(new QLabel(i18n("End:")), 4, 0);

	endEdit = new QDateTimeEdit(widget);
	endEdit->setCurrentSection(QDateTimeEdit::HourSection);
	connect(endEdit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(endChanged(QDateTime)));
	layout->addWidget(endEdit, 4, 1);

	beginChanged(beginEdit->dateTime());

	// FIXME disallow to edit active recordings

	setMainWidget(widget);
}

void DvbRecordingEditor::updateRecording(DvbRecording *recording) const
{
	recording->name = nameEdit->text();
	recording->channelName = channelBox->currentText();
	recording->begin = beginEdit->dateTime();
	recording->duration = durationEdit->time();
	recording->end = endEdit->dateTime();
}

void DvbRecordingEditor::beginChanged(const QDateTime &dateTime)
{
	// attention: setDateTimeRange and setDateTime influence each other!
	QTime duration = durationEdit->time();
	endEdit->setDateTimeRange(dateTime, dateTime.addSecs((23 * 60 + 59) * 60));
	endEdit->setDateTime(dateTime.addSecs(QTime().secsTo(duration)));
}

void DvbRecordingEditor::durationChanged(const QTime &time)
{
	endEdit->setDateTime(beginEdit->dateTime().addSecs(QTime().secsTo(time)));
}

void DvbRecordingEditor::endChanged(const QDateTime &dateTime)
{
	durationEdit->setTime(QTime().addSecs(beginEdit->dateTime().secsTo(dateTime)));
}
