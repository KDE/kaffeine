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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dvbrecording.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <KAction>
#include <KCalendarSystem>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KLocale>
#include <KLocalizedString>
#include <KStandardDirs>
#include "datetimeedit.h"
#include "dvbchannelui.h"
#include "dvbmanager.h"
#include "dvbsi.h"

class DvbRecording : public DvbPidFilter, public QObject
{
public:
	explicit DvbRecording(DvbManager *manager_) : repeat(0), manager(manager_), device(NULL),
		timerId(0) { }
	~DvbRecording() { }

	bool isRunning() const;
	void start();
	void stop();

	QString name;
	QString channelName;
	QDateTime begin;
	QTime duration;
	QDateTime end;
	int repeat; // 1 (monday) | 2 (tuesday) | 4 (wednesday) | etc

private:
	void processData(const char data[188]);
	void timerEvent(QTimerEvent *);

	DvbManager *manager;
	DvbDevice *device;

	QSharedDataPointer<DvbChannel> channel;
	QFile file;
	QList<int> pids;
	DvbSectionGenerator patGenerator;
	DvbSectionGenerator pmtGenerator;
	QByteArray buffer;
	int timerId;
};

bool DvbRecording::isRunning() const
{
	return (device != NULL) && (device->getDeviceState() != DvbDevice::DeviceIdle) &&
	       (device->getDeviceState() != DvbDevice::DeviceTuningFailed);
}

void DvbRecording::start()
{
	if (channel == NULL) {
		channel = manager->getChannelModel()->channelForName(channelName);

		if (channel == NULL) {
			kWarning() << "couldn't find channel" << channelName;
			return;
		}
	}

	if (!file.isOpen()) {
		QString basePath = manager->getRecordingFolder() + '/' + name;

		for (int attempt = 0; attempt < 100; ++attempt) {
			QString path = basePath;

			if (attempt > 0) {
				path += '-' + QString::number(attempt);
			}

			file.setFileName(path + ".m2t");

			if (file.exists() || !file.open(QIODevice::WriteOnly)) {
				continue;
			}

			break;
		}

		if (!file.isOpen()) {
			kWarning() << "couldn't open file" << basePath;
			return;
		}
	}

	if (device == NULL) {
		device = manager->requestDevice(channel->source, channel->transponder);

		if (device == NULL) {
			kWarning() << "couldn't find a suitable device";
			return;
		}

		if (channel->videoPid != -1) {
			pids.append(channel->videoPid);
		}

		if (channel->audioPid != -1) {
			pids.append(channel->audioPid);
		}

		foreach (int pid, pids) {
			device->addPidFilter(pid, this);
		}

		patGenerator.initPat(channel->transportStreamId, channel->serviceId,
			channel->pmtPid);
		pmtGenerator.initPmt(channel->pmtPid,
			DvbPmtSection(DvbSection(channel->pmtSection)), pids);

		buffer.reserve(256 * 188);
		buffer.append(patGenerator.generatePackets());
		buffer.append(pmtGenerator.generatePackets());

		timerId = startTimer(500);
	}

	if ((device->getDeviceState() != DvbDevice::DeviceIdle) &&
	    (device->getDeviceState() != DvbDevice::DeviceTuningFailed)) {
		return;
	}

	// FIXME retune
}

void DvbRecording::stop()
{
	if (timerId != 0) {
		killTimer(timerId);
		timerId = 0;
	}

	if (device != NULL) {
		foreach (int pid, pids) {
			device->removePidFilter(pid, this);
		}

		manager->releaseDevice(device);
		device = NULL;
	}

	pids.clear();

	if (file.isOpen()) {
		file.write(buffer);
		file.close();
	}

	buffer.clear();
	channel = NULL;
}

void DvbRecording::processData(const char data[188])
{
	buffer.append(QByteArray::fromRawData(data, 188));

	if (buffer.size() < (256 * 188)) {
		return;
	}

	file.write(buffer);

	buffer.clear();
	buffer.reserve(256 * 188);
}

void DvbRecording::timerEvent(QTimerEvent *)
{
	buffer.append(patGenerator.generatePackets());
	buffer.append(pmtGenerator.generatePackets());
}

DvbRecordingModel::DvbRecordingModel(DvbManager *manager_) : QAbstractTableModel(manager_),
	manager(manager_), instantRecordingRow(-1)
{
	QFile file(KStandardDirs::locateLocal("appdata", "recordings.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	int version;
	stream >> version;

	if (version != 0x4d848730) {
		// the older version didn't store a magic number ...
		file.seek(0);
		version = 0;
	}

	while (!stream.atEnd()) {
		DvbRecording *recording = new DvbRecording(manager);
		stream >> recording->name;
		stream >> recording->channelName;
		stream >> recording->begin;
		stream >> recording->duration;
		stream >> recording->end;

		if (version != 0) {
			stream >> recording->repeat;
		}

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid recordings in file" << file.fileName();
			delete recording;
			break;
		}

		recordings.append(recording);
	}

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed

	checkStatusTimer = new QTimer(this);
	checkStatusTimer->start(5000);
	connect(checkStatusTimer, SIGNAL(timeout()), this, SLOT(checkStatus()));

	// we don't call checkStatus() here, because the devices maybe aren't set up yet
}

DvbRecordingModel::~DvbRecordingModel()
{
	QFile file(KStandardDirs::locateLocal("appdata", "recordings.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		qDeleteAll(recordings);
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	int version = 0x4d848730;
	stream << version;

	foreach (DvbRecording *recording, recordings) {
		stream << recording->name;
		stream << recording->channelName;
		stream << recording->begin;
		stream << recording->duration;
		stream << recording->end;
		stream << recording->repeat;
		delete recording;
	}
}

int DvbRecordingModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 4;
}

int DvbRecordingModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return recordings.size();
}

QVariant DvbRecordingModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || (index.row() >= recordings.size())) {
		return QVariant();
	}

	if (role == Qt::DecorationRole) {
		if (index.column() == 0) {
			if (recordings.at(index.row())->isRunning()) {
				return KIcon("media-record");
			} else if (recordings.at(index.row())->repeat != 0) {
				return KIcon("view-refresh");
			}
		}
	} else if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0:
			return recordings.at(index.row())->name;
		case 1:
			return recordings.at(index.row())->channelName;
		case 2:
			return KGlobal::locale()->formatDateTime(recordings.at(index.row())->begin.toLocalTime());
		case 3:
			return KGlobal::locale()->formatTime(recordings.at(index.row())->duration, false, true);
		}
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

const DvbRecording *DvbRecordingModel::getRecording(int row)
{
	return recordings.at(row);
}

void DvbRecordingModel::appendRecording(DvbRecording *recording)
{
	beginInsertRows(QModelIndex(), recordings.size(), recordings.size());
	recordings.append(recording);
	endInsertRows();
	checkStatus();
}

void DvbRecordingModel::removeRecording(int row)
{
	beginRemoveRows(QModelIndex(), row, row);
	DvbRecording *recording = recordings.takeAt(row);
	recording->stop();
	delete recording;
	endRemoveRows();

	if (instantRecordingRow >= row) {
		if (instantRecordingRow > row) {
			--instantRecordingRow;
		} else {
			emit instantRecordingRemoved();
			instantRecordingRow = -1;
		}
	}
}

void DvbRecordingModel::updateRecording(int row, DvbRecordingEditor *editor)
{
	editor->updateRecording(recordings.at(row));
	emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex()) - 1));
	checkStatus();
}

void DvbRecordingModel::scheduleProgram(const QString &name, const QString &channel,
	const QDateTime &begin, const QTime &duration)
{
	DvbRecording *recording = new DvbRecording(manager);
	recording->name = name;
	recording->channelName = channel;
	recording->begin = begin;
	// the seconds aren't visible --> set them to zero
	recording->begin = recording->begin.addSecs(-recording->begin.time().second());
	recording->duration = duration;
	recording->end = recording->begin.addSecs(QTime().secsTo(recording->duration));
	appendRecording(recording);
}

void DvbRecordingModel::startInstantRecording(const QString &name, const QString &channel)
{
	DvbRecording *recording = new DvbRecording(manager);
	recording->name = name;
	recording->channelName = channel;
	recording->begin = QDateTime::currentDateTime();
	// the seconds aren't visible --> set them to zero
	recording->begin = recording->begin.addSecs(-recording->begin.time().second());
	recording->duration = QTime(2, 0);
	recording->end = recording->begin.addSecs(QTime().secsTo(recording->duration));
	appendRecording(recording);

	instantRecordingRow = recordings.size() - 1;
}

void DvbRecordingModel::stopInstantRecording()
{
	Q_ASSERT(instantRecordingRow >= 0);
	int row = instantRecordingRow;
	instantRecordingRow = -1; // don't emit instantRecordRemoved()
	removeRecording(row);
}

void DvbRecordingModel::checkStatus()
{
	QDateTime current = QDateTime::currentDateTime();

	for (int i = 0; i < recordings.size(); ++i) {
		DvbRecording *recording = recordings.at(i);

		if (recording->end <= current) {
			recording->stop();

			if (recording->repeat == 0) {
				removeRecording(i);
				--i;
			} else {
				int days = recording->end.daysTo(current);

				if (recording->end.addDays(days) <= current) {
					++days;
				}

				int dayOfWeek = recording->begin.date().dayOfWeek() - 1 + days;

				for (int i = 0; i < 7; ++i) {
					if ((recording->repeat & (1 << (dayOfWeek % 7))) != 0) {
						break;
					}

					++days;
					++dayOfWeek;
				}

				recording->begin = recording->begin.addDays(days);
				recording->end = recording->end.addDays(days);
			}
		}
	}

	for (int i = 0; i < recordings.size(); ++i) {
		DvbRecording *recording = recordings.at(i);

		if (recording->begin <= current) {
			if (recording->isRunning()) {
				continue;
			}

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
	setButtons(KDialog::Close);
	setCaption(i18nc("dialog", "Recording Schedule"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QPushButton *pushButton = new QPushButton(KIcon("list-add"),
						  i18nc("add a new item to a list", "New"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(newRecording()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(KIcon("configure"), i18n("Edit"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(editRecording()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(KIcon("edit-delete"),
				     i18nc("remove an item from a list", "Remove"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(removeRecording()));
	boxLayout->addWidget(pushButton);

	boxLayout->addStretch(1);
	mainLayout->addLayout(boxLayout);

	model = manager->getRecordingModel();

	treeView = new ProxyTreeView(this);
	treeView->setModel(model);
	treeView->setRootIsDecorated(false);
	treeView->sortByColumn(2, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);
	mainLayout->addWidget(treeView);

	KAction *action = new KAction(i18n("Edit"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(editRecording()));
	treeView->addAction(action);

	action = new KAction(i18nc("remove an item from a list", "Remove"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(removeRecording()));
	treeView->addAction(action);

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

	DvbRecordingEditor editor(recording, manager->getChannelModel(), this);

	if (editor.exec() == QDialog::Accepted) {
		editor.updateRecording(recording);
		model->appendRecording(recording);
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

	DvbRecordingEditor editor(model->getRecording(row), manager->getChannelModel(), this);

	if (editor.exec() == QDialog::Accepted) {
		model->updateRecording(row, &editor);
	}
}

void DvbRecordingDialog::removeRecording()
{
	int row = treeView->selectedRow();

	if (row < 0) {
		return;
	}

	model->removeRecording(row);
}

static bool localeAwareLessThan3(const QString &x, const QString &y)
{
	return x.localeAwareCompare(y) < 0;
}

DvbRecordingEditor::DvbRecordingEditor(const DvbRecording *recording, DvbChannelModel *channelModel,
	QWidget *parent) : KDialog(parent)
{
	setCaption(i18nc("subdialog of 'Recording Schedule'", "Edit Schedule Entry"));

	QWidget *widget = new QWidget(this);
	QGridLayout *gridLayout = new QGridLayout(widget);

	nameEdit = new KLineEdit(widget);
	nameEdit->setText(recording->name);
	connect(nameEdit, SIGNAL(textChanged(QString)), this, SLOT(checkValid()));
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18n("Name:"), widget);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	QStringList channels; // FIXME use proxy model?

	foreach (const QSharedDataPointer<DvbChannel> &channel, channelModel->getChannels()) {
		channels.append(channel->name);
	}

	qStableSort(channels.begin(), channels.end(), localeAwareLessThan3);

	channelBox = new KComboBox(widget);
	channelBox->addItems(channels);
	channelBox->setCurrentIndex(channels.indexOf(recording->channelName));
	connect(channelBox, SIGNAL(currentIndexChanged(int)), this, SLOT(checkValid()));
	gridLayout->addWidget(channelBox, 1, 1);

	label = new QLabel(i18n("Channel:"), widget);
	label->setBuddy(channelBox);
	gridLayout->addWidget(label, 1, 0);

	beginEdit = new DateTimeEdit(widget);
	beginEdit->setDateTime(recording->begin.toLocalTime());
	beginEdit->setCurrentSection(DateTimeEdit::HourSection);
	connect(beginEdit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(beginChanged(QDateTime)));
	gridLayout->addWidget(beginEdit, 2, 1);

	label = new QLabel(i18n("Begin:"), widget);
	label->setBuddy(beginEdit);
	gridLayout->addWidget(label, 2, 0);

	durationEdit = new DurationEdit(widget);
	durationEdit->setTime(recording->duration);
	connect(durationEdit, SIGNAL(timeChanged(QTime)), this, SLOT(durationChanged(QTime)));
	gridLayout->addWidget(durationEdit, 3, 1);

	label = new QLabel(i18n("Duration:"), widget);
	label->setBuddy(durationEdit);
	gridLayout->addWidget(label, 3, 0);

	endEdit = new DateTimeEdit(widget);
	endEdit->setCurrentSection(DateTimeEdit::HourSection);
	connect(endEdit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(endChanged(QDateTime)));
	gridLayout->addWidget(endEdit, 4, 1);

	label = new QLabel(i18n("End:"), widget);
	label->setBuddy(endEdit);
	gridLayout->addWidget(label, 4, 0);

	gridLayout->addWidget(new QLabel(i18n("Repeat:"), widget), 5, 0);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QPushButton *pushButton = new QPushButton(
		i18nc("button next to the 'Repeat:' label", "Never"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(repeatNever()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(i18nc("button next to the 'Repeat:' label", "Daily"), widget);
	connect(pushButton, SIGNAL(clicked(bool)), this, SLOT(repeatDaily()));
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

	beginChanged(beginEdit->dateTime());
	checkValid();

	if (recording->isRunning()) {
		nameEdit->setEnabled(false);
		channelBox->setEnabled(false);
		beginEdit->setEnabled(false);
	}

	for (int i = 0; i < 7; ++i) {
		if ((recording->repeat & (1 << i)) != 0) {
			dayCheckBoxes[i]->setChecked(true);
		}
	}

	if (recording->name.isEmpty()) {
		nameEdit->setFocus();
	}

	setMainWidget(widget);
}

DvbRecordingEditor::~DvbRecordingEditor()
{
}

void DvbRecordingEditor::updateRecording(DvbRecording *recording) const
{
	recording->name = nameEdit->text();
	recording->channelName = channelBox->currentText();
	recording->begin = beginEdit->dateTime();
	recording->duration = durationEdit->time();
	recording->end = endEdit->dateTime();
	recording->repeat = 0;

	for (int i = 0; i < 7; ++i) {
		if (dayCheckBoxes[i]->isChecked()) {
			recording->repeat |= (1 << i);
		}
	}
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

void DvbRecordingEditor::checkValid()
{
	enableButtonOk(!nameEdit->text().isEmpty() && (channelBox->currentIndex() != -1));
}
