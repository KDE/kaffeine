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
#include "dvbrecording_p.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <KAction>
#include <KCalendarSystem>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KStandardDirs>
#include "../datetimeedit.h"
#include "dvbchannelui.h"
#include "dvbmanager.h"

DvbRecording::DvbRecording(DvbManager *manager_) : repeat(0), manager(manager_), device(NULL)
{
	connect(&patPmtTimer, SIGNAL(timeout()), this, SLOT(insertPatPmt()));
	connect(&pmtFilter, SIGNAL(pmtSectionChanged(DvbPmtSection)),
		this, SLOT(pmtSectionChanged(DvbPmtSection)));
}

DvbRecording::~DvbRecording()
{
	stop();
}

bool DvbRecording::isRunning() const
{
	return (device != NULL);
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
		QString folder = manager->getRecordingFolder();
		QString path = folder + '/' + name;

		for (int attempt = 0; attempt < 100; ++attempt) {
			if (attempt == 0) {
				file.setFileName(path + ".m2t");
			} else {
				file.setFileName(path + '-' + QString::number(attempt) + ".m2t");
			}

			if (file.exists()) {
				continue;
			}

			if (file.open(QIODevice::WriteOnly)) {
				break;
			}

			if ((attempt == 0) && !QDir(folder).exists()) {
				if (QDir().mkpath(folder)) {
					attempt = -1;
					continue;
				}
			}

			if (folder != QDir::homePath()) {
				folder = QDir::homePath();
				path = folder + '/' + name;
				attempt = -1;
				continue;
			}

			break;
		}

		if (!file.isOpen()) {
			kWarning() << "couldn't open file" << (path + ".m2t");
			return;
		}
	}

	if (device == NULL) {
		device = manager->requestDevice(channel->transponder, DvbManager::Prioritized);

		if (device == NULL) {
			kWarning() << "couldn't find a suitable device";
			return;
		}

		connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));

		pmtFilter.setProgramNumber(channel->serviceId);
		device->addPidFilter(channel->pmtPid, &pmtFilter);

		patGenerator = DvbSectionGenerator();
		patGenerator.initPat(channel->transportStreamId, channel->serviceId,
			channel->pmtPid);
		pmtGenerator = DvbSectionGenerator();
		pmtValid = false;
	}
}

void DvbRecording::stop()
{
	patPmtTimer.stop();

	if (device != NULL) {
		foreach (int pid, pids) {
			device->removePidFilter(pid, this);
		}

		device->removePidFilter(channel->pmtPid, &pmtFilter);
		disconnect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
		manager->releaseDevice(device, DvbManager::Prioritized);
		device = NULL;
	}

	pids.clear();
	file.close();
	buffers.clear();
	channel = NULL;
}

void DvbRecording::deviceStateChanged()
{
	if (device->getDeviceState() == DvbDevice::DeviceReleased) {
		foreach (int pid, pids) {
			device->removePidFilter(pid, this);
		}

		device->removePidFilter(channel->pmtPid, &pmtFilter);
		disconnect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
		device = manager->requestDevice(channel->transponder, DvbManager::Prioritized);

		if (device != NULL) {
			connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
			device->addPidFilter(channel->pmtPid, &pmtFilter);

			foreach (int pid, pids) {
				device->addPidFilter(pid, this);
			}
		} else {
			stop();
		}
	}
}

void DvbRecording::pmtSectionChanged(const DvbPmtSection &pmtSection)
{
	DvbPmtParser pmtParser(pmtSection);
	QSet<int> newPids;

	if (pmtParser.videoPid != -1) {
		newPids.insert(pmtParser.videoPid);
	}

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		newPids.insert(pmtParser.audioPids.at(i).first);
	}

	for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
		newPids.insert(pmtParser.subtitlePids.at(i).first);
	}

	if (pmtParser.teletextPid != -1) {
		newPids.insert(pmtParser.teletextPid);
	}

	for (int i = 0; i < pids.size(); ++i) {
		int pid = pids.at(i);

		if (!newPids.remove(pid)) {
			device->removePidFilter(pid, this);
			pids.removeAt(i);
			--i;
		}
	}

	foreach (int pid, newPids) {
		device->addPidFilter(pid, this);
		pids.append(pid);
	}

	pmtGenerator.initPmt(channel->pmtPid, pmtSection, pids);

	if (!pmtValid) {
		pmtValid = true;
		file.write(patGenerator.generatePackets());
		file.write(pmtGenerator.generatePackets());

		foreach (const QByteArray &buffer, buffers) {
			file.write(buffer);
		}

		buffers.clear();
		patPmtTimer.start(500);
	}

	insertPatPmt();
}

void DvbRecording::insertPatPmt()
{
	if (!pmtValid) {
		pmtSectionChanged(DvbPmtSection(channel->pmtSection));
		return;
	}

	file.write(patGenerator.generatePackets());
	file.write(pmtGenerator.generatePackets());
}

void DvbRecording::processData(const char data[188])
{
	if (!pmtValid) {
		if (!patPmtTimer.isActive()) {
			patPmtTimer.start(1000);
			QByteArray nextBuffer;
			nextBuffer.reserve(348 * 188);
			buffers.append(nextBuffer);
		}

		QByteArray &buffer = buffers.last();
		buffer.append(QByteArray::fromRawData(data, 188));

		if (buffer.size() >= (348 * 188)) {
			QByteArray nextBuffer;
			nextBuffer.reserve(348 * 188);
			buffers.append(nextBuffer);
		}

		return;
	}

	file.write(data, 188);
}

DvbRecordingManager::DvbRecordingManager(DvbManager *manager_) : QObject(manager_),
	manager(manager_), instantRecordingActive(false)
{
	model = new DvbRecordingModel(manager, this);
	connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(checkStatus()));
	connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
		this, SLOT(checkInstantRecording()));

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed

	checkStatusTimer.start(5000);
	connect(&checkStatusTimer, SIGNAL(timeout()), this, SLOT(checkStatus()));

	// we don't call checkStatus() here, because the devices maybe aren't set up yet
	// and because of the compatibility code below

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", "recordings.dvb"));

	if (!file.exists()) {
		return;
	}

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

		model->append(recording);
	}

	if (!file.remove()) {
		kWarning() << "can't remove" << file.fileName();
	}
}

DvbRecordingManager::~DvbRecordingManager()
{
}

void DvbRecordingManager::scheduleProgram(const QString &name, const QString &channel,
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
	model->append(recording);
}

void DvbRecordingManager::startInstantRecording(const QString &name, const QString &channel)
{
	DvbRecording *recording = new DvbRecording(manager);
	recording->name = name;
	recording->channelName = channel;
	recording->begin = QDateTime::currentDateTime();
	// the seconds aren't visible --> set them to zero
	recording->begin = recording->begin.addSecs(-recording->begin.time().second());
	recording->duration = QTime(2, 0);
	recording->end = recording->begin.addSecs(QTime().secsTo(recording->duration));
	model->append(recording);

	instantRecordingActive = true;
	instantRecordingIndex = model->index(model->rowCount(QModelIndex()), 0);
}

void DvbRecordingManager::stopInstantRecording()
{
	if (instantRecordingActive) {
		instantRecordingActive = false; // don't emit instantRecordRemoved()
		model->remove(instantRecordingIndex.row());
	}
}

void DvbRecordingManager::showDialog()
{
	KDialog dialog(manager->getParentWidget());
	dialog.setButtons(KDialog::Close);
	dialog.setCaption(i18nc("dialog", "Recording Schedule"));

	QWidget *widget = new QWidget(&dialog);
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

	treeView = new ProxyTreeView(&dialog);
	treeView->setModel(model);
	treeView->setRootIsDecorated(false);
	treeView->sortByColumn(2, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);
	mainLayout->addWidget(treeView);

	KAction *action = new KAction(i18n("Edit"), &dialog);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(editRecording()));
	treeView->addAction(action);

	action = new KAction(i18nc("remove an item from a list", "Remove"), &dialog);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(removeRecording()));
	treeView->addAction(action);

	dialog.setMainWidget(widget);
	dialog.exec();
}

void DvbRecordingManager::checkStatus()
{
	QDateTime current = QDateTime::currentDateTime();

	for (int i = 0; i < model->rowCount(QModelIndex()); ++i) {
		DvbRecording *recording = model->at(i);

		if (recording->end <= current) {
			if (recording->repeat == 0) {
				model->remove(i);
				--i;
			} else {
				recording->stop();

				int days = recording->end.daysTo(current);

				if (recording->end.addDays(days) <= current) {
					++days;
				}

				// QDate::dayOfWeek() and our dayOfWeek differ by one
				int dayOfWeek = recording->begin.date().dayOfWeek() - 1 + days;

				for (int j = 0; j < 7; ++j) {
					if ((recording->repeat & (1 << (dayOfWeek % 7))) != 0) {
						break;
					}

					++days;
					++dayOfWeek;
				}

				recording->begin = recording->begin.addDays(days);
				recording->end = recording->end.addDays(days);

				model->rowUpdated(i);
			}
		}
	}

	for (int i = 0; i < model->rowCount(QModelIndex()); ++i) {
		DvbRecording *recording = model->at(i);

		if (recording->begin <= current) {
			if (recording->isRunning()) {
				continue;
			}

			recording->start();

			if (recording->isRunning()) {
				model->rowUpdatedTrivially(i); // don't trigger an SQL update
			}
		}
	}
}

void DvbRecordingManager::checkInstantRecording()
{
	if (instantRecordingActive && !instantRecordingIndex.isValid()) {
		instantRecordingActive = false;
		emit instantRecordingRemoved();
	}
}

void DvbRecordingManager::newRecording()
{
	DvbRecording *recording = new DvbRecording(manager);
	recording->begin = QDateTime::currentDateTime();
	// the seconds aren't visible --> set them to zero
	recording->begin = recording->begin.addSecs(-recording->begin.time().second());
	recording->duration = QTime(2, 0);

	DvbRecordingEditor editor(recording, manager->getChannelModel(), manager->getParentWidget());

	if (editor.exec() == QDialog::Accepted) {
		editor.updateRecording(recording);
		model->append(recording);
	} else {
		delete recording;
	}
}

void DvbRecordingManager::editRecording()
{
	int row = treeView->selectedRow();

	if (row < 0) {
		return;
	}

	DvbRecordingEditor editor(model->at(row), manager->getChannelModel(), manager->getParentWidget());

	if (editor.exec() == QDialog::Accepted) {
		editor.updateRecording(model->at(row));
		model->rowUpdated(row);
		checkStatus();
	}
}

void DvbRecordingManager::removeRecording()
{
	int row = treeView->selectedRow();

	if (row < 0) {
		return;
	}

	model->remove(row);
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

QStringList DvbRecordingHelper::modelHeaderLabels()
{
	return QStringList() << i18n("Name") << i18n("Channel") << i18n("Begin") <<
		i18n("Duration");
}

QVariant DvbRecordingHelper::modelData(const DvbRecording *recording, int column, int role)
{
	if (role == Qt::DecorationRole) {
		if (column == 0) {
			if (recording->isRunning()) {
				return KIcon("media-record");
			} else if (recording->repeat != 0) {
				return KIcon("view-refresh");
			}
		}
	} else if (role == Qt::DisplayRole) {
		switch (column) {
		case 0:
			return recording->name;
		case 1:
			return recording->channelName;
		case 2:
			return KGlobal::locale()->formatDateTime(recording->begin.toLocalTime());
		case 3:
			return KGlobal::locale()->formatTime(recording->duration, false, true);
		}
	}

	return QVariant();
}

void DvbRecordingHelper::deleteObject(const DvbRecording *recording)
{
	delete recording;
}

QString DvbRecordingHelper::sqlTableName()
{
	return "RecordingSchedule";
}

QStringList DvbRecordingHelper::sqlColumnNames()
{
	return QStringList() << "Name" << "Channel" << "Begin" << "Duration" << "Repeat";
}

bool DvbRecordingHelper::fromSqlQuery(DvbRecording *recording, const QSqlQuery &query, int index)
{
	recording->name = query.value(index++).toString();
	recording->channelName = query.value(index++).toString();
	recording->begin = QDateTime::fromString(query.value(index++).toString(), Qt::ISODate);
	recording->duration = QTime::fromString(query.value(index++).toString(), Qt::ISODate);
	recording->repeat = query.value(index++).toInt() & ((1 << 7) - 1);

	if (recording->name.isEmpty() || recording->channelName.isEmpty() ||
	    !recording->begin.isValid() || !recording->duration.isValid()) {
		return false;
	}

	recording->end = recording->begin.addSecs(QTime().secsTo(recording->duration));
	return true;
}

void DvbRecordingHelper::bindToSqlQuery(const DvbRecording *recording, QSqlQuery &query, int index)
{
	query.bindValue(index++, recording->name);
	query.bindValue(index++, recording->channelName);
	query.bindValue(index++, recording->begin.toString(Qt::ISODate));
	query.bindValue(index++, recording->duration.toString(Qt::ISODate));
	query.bindValue(index++, recording->repeat);
}

DvbRecordingModel::DvbRecordingModel(DvbManager *manager_, QObject *parent) :
	SqlTableModel<DvbRecordingHelper>(parent), manager(manager_)
{
	initSql();
}

DvbRecordingModel::~DvbRecordingModel()
{
}

void DvbRecordingModel::handleRow(qint64 key, const QSqlQuery &query, int index)
{
	DvbRecording *recording = new DvbRecording(manager);

	if (DvbRecordingHelper::fromSqlQuery(recording, query, index)) {
		insert(key, recording);
	} else {
		delete recording;
		insert(key, NULL);
	}
}
