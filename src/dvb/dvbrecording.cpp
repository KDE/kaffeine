/*
 * dvbrecording.cpp
 *
 * Copyright (C) 2009-2010 Christoph Pfister <christophpfister@gmail.com>
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

DvbRecording::DvbRecording(DvbManager *manager_) : repeat(0), manager(manager_), device(NULL),
	pmtValid(false)
{
	connect(&pmtFilter, SIGNAL(pmtSectionChanged(DvbPmtSection)),
		this, SLOT(pmtSectionChanged(DvbPmtSection)));
	connect(&patPmtTimer, SIGNAL(timeout()), this, SLOT(insertPatPmt()));
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
	if (channel.constData() == NULL) {
		int row = manager->getChannelModel()->indexOfName(channelName);

		if (row < 0) {
			kWarning() << "cannot find channel" << channelName;
			return;
		}

		channel = manager->getChannelModel()->getChannel(row);
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
			kWarning() << "cannot open file" << (path + ".m2t");
			return;
		}
	}

	if (device == NULL) {
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Prioritized);

		if (device == NULL) {
			kWarning() << "cannot find a suitable device";
			return;
		}

		connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
		pmtFilter.setProgramNumber(channel->getServiceId());
		device->addPidFilter(channel->pmtPid, &pmtFilter);
		patGenerator.initPat(channel->transportStreamId, channel->getServiceId(),
			channel->pmtPid);
	}
}

void DvbRecording::stop()
{
	if (device != NULL) {
		foreach (int pid, pids) {
			device->removePidFilter(pid, this);
		}

		device->removePidFilter(channel->pmtPid, &pmtFilter);
		disconnect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
		manager->releaseDevice(device, DvbManager::Prioritized);
		device = NULL;
	}

	pmtValid = false;
	patPmtTimer.stop();
	patGenerator.reset();
	pmtGenerator.reset();
	pmtFilter.reset();
	pids.clear();
	buffers.clear();
	file.close();
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
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Prioritized);

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

DvbRecordingModel::DvbRecordingModel(DvbManager *manager_) : QAbstractTableModel(manager_),
	manager(manager_), scheduleDialog(NULL), treeView(NULL)
{
	sqlInterface = new DvbSqlRecordingModelInterface(manager, this);

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed

	checkStatusTimer.start(5000);
	connect(&checkStatusTimer, SIGNAL(timeout()), this, SLOT(checkStatus()));

	// we don't call checkStatus() here, because the devices maybe aren't set up yet

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", "recordings.dvb"));

	if (!file.exists()) {
		return;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "cannot open file" << file.fileName();
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
		recording->begin = recording->begin.toLocalTime();
		stream >> recording->duration;
		QDateTime end;
		stream >> end;
		recording->end = recording->begin.addSecs(QTime().secsTo(recording->duration));

		if (version != 0) {
			stream >> recording->repeat;
		}

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid recordings in file" << file.fileName();
			delete recording;
			break;
		}

		insert(recording);
	}

	if (!file.remove()) {
		kWarning() << "cannot remove file" << file.fileName();
	}
}

DvbRecordingModel::~DvbRecordingModel()
{
	sqlInterface->flush();
	qDeleteAll(recordings);
}

DvbRecordingIndex DvbRecordingModel::scheduleProgram(const QString &name, const QString &channel,
	const QDateTime &begin, const QTime &duration)
{
	Q_ASSERT(begin.timeSpec() == Qt::LocalTime);
	DvbRecording *recording = new DvbRecording(manager);
	recording->name = name;
	recording->channelName = channel;
	recording->begin = begin;
	// the seconds aren't visible --> set them to zero
	recording->begin = recording->begin.addSecs(-recording->begin.time().second());
	recording->duration = duration;
	recording->end = recording->begin.addSecs(QTime().secsTo(recording->duration));
	return insert(recording);
}

void DvbRecordingModel::removeProgram(const DvbRecordingIndex &recordingIndex)
{
	int row = sqlInterface->rowForKey(recordingIndex.key);

	if (row >= 0) {
		remove(row);
	}
}

void DvbRecordingModel::showDialog(QWidget *parent)
{
	scheduleDialog = new KDialog(parent);
	scheduleDialog->setButtons(KDialog::Close);
	scheduleDialog->setCaption(i18nc("dialog", "Recording Schedule"));

	QWidget *widget = new QWidget(scheduleDialog);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);
	QBoxLayout *boxLayout = new QHBoxLayout();

	QPushButton *pushButton = new QPushButton(KIcon("list-add"),
		i18nc("add a new item to a list", "New"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(newRecording()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(KIcon("configure"), i18n("Edit"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(editRecording()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(KIcon("edit-delete"),
		i18nc("remove an item from a list", "Remove"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(removeRecording()));
	boxLayout->addWidget(pushButton);

	boxLayout->addStretch(1);
	mainLayout->addLayout(boxLayout);

	treeView = new ProxyTreeView(widget);
	treeView->setModel(this);
	treeView->setRootIsDecorated(false);
	treeView->sortByColumn(2, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);
	mainLayout->addWidget(treeView);

	KAction *action = new KAction(KIcon("configure"), i18n("Edit"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(editRecording()));
	treeView->addAction(action);

	action = new KAction(KIcon("edit-delete"), i18nc("remove an item from a list", "Remove"),
		widget);
	connect(action, SIGNAL(triggered()), this, SLOT(removeRecording()));
	treeView->addAction(action);

	scheduleDialog->setMainWidget(widget);
	scheduleDialog->setAttribute(Qt::WA_DeleteOnClose, true);
	scheduleDialog->setModal(true);
	scheduleDialog->show();
}

int DvbRecordingModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 4;
	}

	return 0;
}

int DvbRecordingModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return recordings.size();
	}

	return 0;
}

QVariant DvbRecordingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((role == Qt::DisplayRole) && (orientation == Qt::Horizontal)) {
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
	}

	return QVariant();
}

QVariant DvbRecordingModel::data(const QModelIndex &index, int role) const
{
	switch (role) {
	case Qt::DecorationRole:
		if (index.column() == 0) {
			const DvbRecording *recording = recordings.at(index.row());

			if (recording->isRunning()) {
				return KIcon("media-record");
			} else if (recording->repeat != 0) {
				return KIcon("view-refresh");
			}
		}

		break;
	case Qt::DisplayRole: {
		const DvbRecording *recording = recordings.at(index.row());

		switch (index.column()) {
		case 0:
			return recording->name;
		case 1:
			return recording->channelName;
		case 2:
			return KGlobal::locale()->formatDateTime(recording->begin);
		case 3:
			return KGlobal::locale()->formatTime(recording->duration, false, true);
		}

		break;
	    }
	}

	return QVariant();
}

DvbRecordingIndex DvbRecordingModel::insert(DvbRecording *recording)
{
	int row = recordings.size();
	beginInsertRows(QModelIndex(), row, row);
	recordings.insert(row, recording);
	endInsertRows();
	checkStatus();
	return DvbRecordingIndex(sqlInterface->keyForRow(row));
}

void DvbRecordingModel::update(int row)
{
	emit dataChanged(index(row, 0), index(row, 3));
	checkStatus();
}

void DvbRecordingModel::remove(int row)
{
	DvbRecordingIndex recordingIndex(sqlInterface->keyForRow(row));
	beginRemoveRows(QModelIndex(), row, row);
	delete recordings.at(row);
	recordings.removeAt(row);
	endRemoveRows();
	emit programRemoved(recordingIndex);
}

void DvbRecordingModel::checkStatus()
{
	QDateTime current = QDateTime::currentDateTime();

	for (int row = 0; row < recordings.size(); ++row) {
		DvbRecording *recording = recordings.at(row);

		if (recording->end <= current) {
			if (recording->repeat == 0) {
				remove(row);
				--row;
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
				emit dataChanged(index(row, 0), index(row, 3));
			}
		}
	}

	for (int row = 0; row < recordings.size(); ++row) {
		DvbRecording *recording = recordings.at(row);

		if (recording->begin <= current) {
			if (recording->isRunning()) {
				continue;
			}

			recording->start();

			if (recording->isRunning()) {
				emit dataChanged(index(row, 0), index(row, 3));
			}
		}
	}
}

void DvbRecordingModel::newRecording()
{
	KDialog *dialog = new DvbRecordingEditor(manager, this, -1, scheduleDialog);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbRecordingModel::editRecording()
{
	int row = treeView->selectedRow();

	if (row >= 0) {
		KDialog *dialog = new DvbRecordingEditor(manager, this, row, scheduleDialog);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
	}
}

void DvbRecordingModel::removeRecording()
{
	int row = treeView->selectedRow();

	if (row >= 0) {
		remove(row);
	}
}

static bool localeAwareLessThan3(const QString &x, const QString &y)
{
	return (x.localeAwareCompare(y) < 0);
}

DvbRecordingEditor::DvbRecordingEditor(DvbManager *manager_, DvbRecordingModel *model_, int row_,
	QWidget *parent) : KDialog(parent), manager(manager_), model(model_), row(row_)
{
	setCaption(i18nc("subdialog of 'Recording Schedule'", "Edit Schedule Entry"));

	QWidget *widget = new QWidget(this);
	QGridLayout *gridLayout = new QGridLayout(widget);

	nameEdit = new KLineEdit(widget);
	connect(nameEdit, SIGNAL(textChanged(QString)), this, SLOT(checkValid()));
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18n("Name:"), widget);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	QStringList channels; // FIXME use proxy model?

	foreach (const QSharedDataPointer<DvbChannel> &channel,
		 manager->getChannelModel()->getChannels()) {
		channels.append(channel->name);
	}

	qSort(channels.begin(), channels.end(), localeAwareLessThan3);

	channelBox = new KComboBox(widget);
	channelBox->addItems(channels);
	connect(channelBox, SIGNAL(currentIndexChanged(int)), this, SLOT(checkValid()));
	gridLayout->addWidget(channelBox, 1, 1);

	label = new QLabel(i18n("Channel:"), widget);
	label->setBuddy(channelBox);
	gridLayout->addWidget(label, 1, 0);

	beginEdit = new DateTimeEdit(widget);
	beginEdit->setCurrentSection(DateTimeEdit::HourSection);
	connect(beginEdit, SIGNAL(dateTimeChanged(QDateTime)),
		this, SLOT(beginChanged(QDateTime)));
	gridLayout->addWidget(beginEdit, 2, 1);

	label = new QLabel(i18n("Begin:"), widget);
	label->setBuddy(beginEdit);
	gridLayout->addWidget(label, 2, 0);

	durationEdit = new DurationEdit(widget);
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
	connect(pushButton, SIGNAL(clicked()), this, SLOT(repeatNever()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(i18nc("button next to the 'Repeat:' label", "Daily"), widget);
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

	if (row >= 0) {
		const DvbRecording *recording = model->recordings.at(row);
		nameEdit->setText(recording->name);
		channelBox->setCurrentIndex(channels.indexOf(recording->channelName));
		beginEdit->setDateTime(recording->begin);
		durationEdit->setTime(recording->duration);

		for (int i = 0; i < 7; ++i) {
			if ((recording->repeat & (1 << i)) != 0) {
				dayCheckBoxes[i]->setChecked(true);
			}
		}

		if (recording->isRunning()) {
			nameEdit->setEnabled(false);
			channelBox->setEnabled(false);
			beginEdit->setEnabled(false);
		}
	} else {
		channelBox->setCurrentIndex(-1);
		QDateTime begin = QDateTime::currentDateTime();
		// the seconds aren't visible --> set them to zero
		beginEdit->setDateTime(begin.addSecs(-begin.time().second()));
		durationEdit->setTime(QTime(2, 0));
	}

	beginChanged(beginEdit->dateTime());
	checkValid();

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

void DvbRecordingEditor::checkValid()
{
	enableButtonOk(!nameEdit->text().isEmpty() && (channelBox->currentIndex() != -1));
}

void DvbRecordingEditor::accept()
{
	DvbRecording *recording;

	if (row >= 0) {
		recording = model->recordings.at(row);
	} else {
		recording = new DvbRecording(manager);
	}

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

	if (row >= 0) {
		model->update(row);
	} else {
		model->insert(recording);
	}

	KDialog::accept();
}

DvbSqlRecordingModelInterface::DvbSqlRecordingModelInterface(DvbManager *manager_,
	DvbRecordingModel *model_) : SqlTableModelInterface(model_), manager(manager_),
	model(model_)
{
	init(model, "RecordingSchedule",
		QStringList() << "Name" << "Channel" << "Begin" << "Duration" << "Repeat");
}

DvbSqlRecordingModelInterface::~DvbSqlRecordingModelInterface()
{
}

int DvbSqlRecordingModelInterface::insertFromSqlQuery(const QSqlQuery &query, int index)
{
	DvbRecording *recording = new DvbRecording(manager);
	recording->name = query.value(index++).toString();
	recording->channelName = query.value(index++).toString();
	recording->begin = QDateTime::fromString(query.value(index++).toString(), Qt::ISODate);
	recording->duration = QTime::fromString(query.value(index++).toString(), Qt::ISODate);
	recording->end = recording->begin.addSecs(QTime().secsTo(recording->duration));
	recording->repeat = (query.value(index++).toInt() & ((1 << 7) - 1));

	if (recording->name.isEmpty() || recording->channelName.isEmpty() ||
	    !recording->begin.isValid() || !recording->duration.isValid()) {
		delete recording;
		return -1;
	}

	int row = model->recordings.size();
	model->recordings.insert(row, recording);
	return row;
}

void DvbSqlRecordingModelInterface::bindToSqlQuery(QSqlQuery &query, int index, int row) const
{
	const DvbRecording *recording = model->recordings.at(row);
	query.bindValue(index++, recording->name);
	query.bindValue(index++, recording->channelName);
	query.bindValue(index++, recording->begin.toString(Qt::ISODate));
	query.bindValue(index++, recording->duration.toString(Qt::ISODate));
	query.bindValue(index++, recording->repeat);
}
