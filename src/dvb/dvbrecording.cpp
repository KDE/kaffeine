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
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <KAction>
#include <KCalendarSystem>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KMessageBox>
#include <KStandardDirs>
#include "../datetimeedit.h"
#include "dvbchannelui.h"
#include "dvbmanager.h"

DvbRecordingModel::DvbRecordingModel(DvbManager *manager_, QObject *parent) :
	QAbstractTableModel(parent), manager(manager_), hasActiveRecordings(false)
{
	sqlInterface = new DvbRecordingSqlInterface(manager, this, &recordings, this);

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed
	// we don't call checkStatus() here, because the devices maybe aren't set up yet

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(checkStatus()));
	timer->start(5000);

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
		DvbRecordingEntry entry;
		stream >> entry.name;
		stream >> entry.channelName;
		stream >> entry.begin;
		entry.begin = entry.begin.toUTC();
		stream >> entry.duration;
		QDateTime end;
		stream >> end;

		if (version != 0) {
			stream >> entry.repeat;
		}

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid recordings in file" << file.fileName();
			break;
		}

		scheduleProgram(entry);
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

DvbRecordingKey DvbRecordingModel::scheduleProgram(const DvbRecordingEntry &entry)
{
	int row = recordings.size();
	beginInsertRows(QModelIndex(), row, row);
	recordings.append(new DvbRecording(manager, entry));
	endInsertRows();
	QTimer::singleShot(0, this, SLOT(checkStatus()));
	return DvbRecordingKey(sqlInterface->keyForRow(row));
}

void DvbRecordingModel::removeProgram(const DvbRecordingKey &key)
{
	int row = sqlInterface->rowForKey(key.key);

	if (row >= 0) {
		removeRow(row);
	}
}

QAbstractItemModel *DvbRecordingModel::createProxyModel(QObject *parent)
{
	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(parent);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);
	proxyModel->setSortRole(SortRole);
	proxyModel->setSourceModel(this);
	return proxyModel;
}

void DvbRecordingModel::showDialog(QWidget *parent)
{
	KDialog *dialog = new DvbRecordingDialog(manager, this, parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbRecordingModel::mayCloseApplication(bool *ok, QWidget *parent)
{
	if (*ok) {
		if (hasActiveRecordings) {
			if (KMessageBox::warningYesNo(parent, i18nc("message box",
			    "Kaffeine is currently recording programs.\n"
			    "Do you really want to close the application?")) != KMessageBox::Yes) {
				*ok = false;
			}

			return;
		}

		if (!recordings.isEmpty()) {
			if (KMessageBox::questionYesNo(parent, i18nc("message box",
			    "Kaffeine has scheduled recordings.\n"
			    "Do you really want to close the application?")) != KMessageBox::Yes) {
				*ok = false;
			}

			return;
		}
	}
}

void DvbRecordingModel::checkStatus()
{
	QDateTime currentLocalDateTime = QDateTime::currentDateTime();
	QDateTime currentDateTime = currentLocalDateTime.toUTC();

	for (int row = 0; row < recordings.size(); ++row) {
		DvbRecording *recording = recordings.at(row);
		const DvbRecordingEntry &entry = recording->getEntry();
		QDateTime end = entry.begin.addSecs(QTime().secsTo(entry.duration));

		if (end <= currentDateTime) {
			if (entry.repeat == 0) {
				removeRow(row);
				--row;
			} else {
				recording->stop();

				// take care of DST switches
				DvbRecordingEntry nextEntry = entry;
				nextEntry.begin = nextEntry.begin.toLocalTime();
				end = end.toLocalTime();
				int days = end.daysTo(currentLocalDateTime);

				if (end.addDays(days) <= currentLocalDateTime) {
					++days;
				}

				// QDate::dayOfWeek() and our dayOfWeek differ by one
				int dayOfWeek = (nextEntry.begin.date().dayOfWeek() - 1 + days);

				for (int j = 0; j < 7; ++j) {
					if ((nextEntry.repeat & (1 << (dayOfWeek % 7))) != 0) {
						break;
					}

					++days;
					++dayOfWeek;
				}

				nextEntry.begin = nextEntry.begin.addDays(days).toUTC();
				recording->setEntry(nextEntry);
				emit dataChanged(index(row, 0), index(row, 3));
			}
		}
	}

	hasActiveRecordings = false;

	for (int row = 0; row < recordings.size(); ++row) {
		DvbRecording *recording = recordings.at(row);
		const DvbRecordingEntry &entry = recording->getEntry();

		if (entry.begin <= currentDateTime) {
			hasActiveRecordings = true;

			if (!entry.isRunning) {
				recording->start();

				if (entry.isRunning) {
					emit dataChanged(index(row, 0), index(row, 3));
				}
			}
		}
	}
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
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
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
	const DvbRecordingEntry &entry = recordings.at(index.row())->getEntry();

	switch (role) {
	case Qt::DecorationRole:
		if (index.column() == 0) {
			if (entry.isRunning) {
				return KIcon("media-record");
			} else if (entry.repeat != 0) {
				return KIcon("view-refresh");
			}
		}

		break;
	case Qt::DisplayRole:
		switch (index.column()) {
		case 0:
			return entry.name;
		case 1:
			return entry.channelName;
		case 2:
			return KGlobal::locale()->formatDateTime(entry.begin.toLocalTime());
		case 3:
			return KGlobal::locale()->formatTime(entry.duration, false, true);
		}

		break;
	case SortRole:
		switch (index.column()) {
		case 0:
			return entry.name;
		case 1:
			return entry.channelName;
		case 2:
			return entry.begin;
		case 3:
			return entry.duration;
		}

		break;
	case DvbRecordingEntryRole:
		return QVariant::fromValue(&entry);
	}

	return QVariant();
}

bool DvbRecordingModel::removeRows(int row, int count, const QModelIndex &parent)
{
	Q_UNUSED(parent)
	QList<quint32> keys;

	for (int currentRow = row; currentRow < (row + count); ++currentRow) {
		keys.append(sqlInterface->keyForRow(currentRow));
	}

	beginRemoveRows(QModelIndex(), row, row + count - 1);
	qDeleteAll(recordings.begin() + row, recordings.begin() + row + count);
	recordings.erase(recordings.begin() + row, recordings.begin() + row + count);
	endRemoveRows();

	foreach (quint32 key, keys) {
		emit programRemoved(DvbRecordingKey(key));
	}

	return true;
}

bool DvbRecordingModel::setData(const QModelIndex &modelIndex, const QVariant &value, int role)
{
	Q_UNUSED(role)
	const DvbRecordingEntry *entry = value.value<const DvbRecordingEntry *>();

	if (entry != NULL) {
		if (modelIndex.isValid()) {
			int row = modelIndex.row();
			recordings.at(row)->setEntry(*entry);
			emit dataChanged(index(row, 0), index(row, 3));
		} else {
			scheduleProgram(*entry);
		}

		return true;
	}

	return false;
}

DvbRecording::DvbRecording(DvbManager *manager_, const DvbRecordingEntry &entry_) :
	manager(manager_), device(NULL), pmtSection(QByteArray()), pmtValid(false)
{
	setEntry(entry_);
	connect(&pmtFilter, SIGNAL(pmtSectionChanged(DvbPmtSection)),
		this, SLOT(pmtSectionChanged(DvbPmtSection)));
	connect(&patPmtTimer, SIGNAL(timeout()), this, SLOT(insertPatPmt()));
}

DvbRecording::~DvbRecording()
{
	stop();
}

const DvbRecordingEntry &DvbRecording::getEntry() const
{
	return entry;
}

void DvbRecording::setEntry(const DvbRecordingEntry &entry_)
{
	entry.name = entry_.name;
	entry.channelName = entry_.channelName;
	entry.begin = entry_.begin;
	entry.duration = entry_.duration;
	entry.repeat = entry_.repeat;
	// isRunning is read-only

	if (entry.begin.timeSpec() != Qt::UTC) {
		kWarning() << "wrong time spec";
		entry.begin = entry.begin.toUTC();
	}

	// the seconds and milliseconds aren't visible --> set them to zero
	entry.begin = entry.begin.addMSecs(-(QTime().msecsTo(entry.begin.time()) % 60000));
}

void DvbRecording::start()
{
	if (channel.constData() == NULL) {
		DvbChannelModel *channelModel = manager->getChannelModel();
		QModelIndex index = channelModel->findChannelByName(entry.channelName);

		if (!index.isValid()) {
			kWarning() << "cannot find channel" << entry.channelName;
			return;
		}

		channel = channelModel->data(index,
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
	}

	if (!file.isOpen()) {
		QString folder = manager->getRecordingFolder();
		QString path = folder + '/' + entry.name.replace('/', '_');

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
				path = folder + '/' + entry.name.replace('/', '_');
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
		pmtSection = DvbPmtSection(channel->pmtSection);
		patGenerator.initPat(channel->transportStreamId, channel->getServiceId(),
			channel->pmtPid);

		if (channel->isScrambled && pmtSection.isValid()) {
			device->startDescrambling(pmtSection, this);
		}
	}

	entry.isRunning = true;
}

void DvbRecording::stop()
{
	entry.isRunning = false;

	if (device != NULL) {
		if (channel->isScrambled && pmtSection.isValid()) {
			device->stopDescrambling(pmtSection.programNumber(), this);
		}

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
	pmtSection = DvbPmtSection(QByteArray());
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

		if (channel->isScrambled && pmtSection.isValid()) {
			device->stopDescrambling(pmtSection.programNumber(), this);
		}

		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Prioritized);

		if (device != NULL) {
			connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
			device->addPidFilter(channel->pmtPid, &pmtFilter);

			foreach (int pid, pids) {
				device->addPidFilter(pid, this);
			}

			if (channel->isScrambled && pmtSection.isValid()) {
				device->startDescrambling(pmtSection, this);
			}
		} else {
			stop();
		}
	}
}

void DvbRecording::pmtSectionChanged(const DvbPmtSection &pmtSection)
{
	DvbRecording::pmtSection = pmtSection;
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

	if (channel->isScrambled) {
		device->startDescrambling(pmtSection, this);
	}
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

DvbRecordingSqlInterface::DvbRecordingSqlInterface(DvbManager *manager_, QAbstractItemModel *model,
	QList<DvbRecording *> *recordings_, QObject *parent) : SqlTableModelInterface(parent),
	manager(manager_), recordings(recordings_)
{
	init(model, "RecordingSchedule",
		QStringList() << "Name" << "Channel" << "Begin" << "Duration" << "Repeat");
}

DvbRecordingSqlInterface::~DvbRecordingSqlInterface()
{
}

int DvbRecordingSqlInterface::insertFromSqlQuery(const QSqlQuery &query, int index)
{
	DvbRecordingEntry entry;
	entry.name = query.value(index++).toString();
	entry.channelName = query.value(index++).toString();
	entry.begin = QDateTime::fromString(query.value(index++).toString(), Qt::ISODate).toUTC();
	entry.duration = QTime::fromString(query.value(index++).toString(), Qt::ISODate);
	entry.repeat = (query.value(index++).toInt() & ((1 << 7) - 1));

	if (!entry.name.isEmpty() && !entry.channelName.isEmpty() &&
	    entry.begin.isValid() && entry.duration.isValid()) {
		int row = recordings->size();
		recordings->append(new DvbRecording(manager, entry));
		return row;
	}

	return -1;
}

void DvbRecordingSqlInterface::bindToSqlQuery(QSqlQuery &query, int index, int row) const
{
	const DvbRecordingEntry &entry = recordings->at(row)->getEntry();
	query.bindValue(index++, entry.name);
	query.bindValue(index++, entry.channelName);
	query.bindValue(index++, entry.begin.toString(Qt::ISODate) + 'Z');
	query.bindValue(index++, entry.duration.toString(Qt::ISODate));
	query.bindValue(index++, entry.repeat);
}

DvbRecordingDialog::DvbRecordingDialog(DvbManager *manager_, DvbRecordingModel *recordingModel,
	QWidget *parent) : KDialog(parent), manager(manager_)
{
	setButtons(KDialog::Close);
	setCaption(i18nc("dialog", "Recording Schedule"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);
	QBoxLayout *boxLayout = new QHBoxLayout();

	proxyModel = recordingModel->createProxyModel(widget);
	treeView = new QTreeView(widget);
	treeView->header()->setResizeMode(QHeaderView::ResizeToContents);
	treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
	treeView->setModel(proxyModel);
	treeView->setRootIsDecorated(false);
	treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	treeView->sortByColumn(2, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);

	KAction *action =
		new KAction(KIcon("list-add"), i18nc("add a new item to a list", "New"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(newRecording()));
	treeView->addAction(action);
	QPushButton *pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(newRecording()));
	boxLayout->addWidget(pushButton);

	action = new KAction(KIcon("configure"), i18n("Edit"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(editRecording()));
	treeView->addAction(action);
	pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(editRecording()));
	boxLayout->addWidget(pushButton);

	action = new KAction(KIcon("edit-delete"),
		i18nc("remove an item from a list", "Remove"), widget);
	connect(action, SIGNAL(triggered()), this, SLOT(removeRecording()));
	treeView->addAction(action);
	pushButton = new QPushButton(action->icon(), action->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(removeRecording()));
	boxLayout->addWidget(pushButton);

	boxLayout->addStretch();
	mainLayout->addLayout(boxLayout);
	mainLayout->addWidget(treeView);
	setMainWidget(widget);
	resize(100 * fontMetrics().averageCharWidth(), 20 * fontMetrics().height());
}

DvbRecordingDialog::~DvbRecordingDialog()
{
}

void DvbRecordingDialog::newRecording()
{
	KDialog *dialog = new DvbRecordingEditor(manager, proxyModel, QModelIndex(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbRecordingDialog::editRecording()
{
	QModelIndex index = treeView->currentIndex();

	if (index.isValid()) {
		KDialog *dialog = new DvbRecordingEditor(manager, proxyModel, index, this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
	}
}

void DvbRecordingDialog::removeRecording()
{
	QMap<int, char> selectedRows;

	foreach (const QModelIndex &modelIndex, treeView->selectionModel()->selectedIndexes()) {
		selectedRows.insert(modelIndex.row(), 0);
	}

	for (QMap<int, char>::ConstIterator it = selectedRows.end(); it != selectedRows.begin();) {
		--it;
		proxyModel->removeRow(it.key());
	}
}

DvbRecordingEditor::DvbRecordingEditor(DvbManager *manager, QAbstractItemModel *model_,
	const QModelIndex &modelIndex_, QWidget *parent) : KDialog(parent), model(model_),
	persistentIndex(modelIndex_)
{
	setCaption(i18nc("subdialog of 'Recording Schedule'", "Edit Schedule Entry"));

	QWidget *widget = new QWidget(this);
	QGridLayout *gridLayout = new QGridLayout(widget);

	nameEdit = new KLineEdit(widget);
	connect(nameEdit, SIGNAL(textChanged(QString)), this, SLOT(checkValidity()));
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18n("Name:"), widget);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	channelBox = new KComboBox(widget);
	QAbstractProxyModel *channelProxyModel =
		manager->getChannelModel()->createProxyModel(channelBox);
	QHeaderView *header = manager->getChannelView()->header();
	channelProxyModel->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
	channelBox->setModel(channelProxyModel);
	connect(channelBox, SIGNAL(currentIndexChanged(int)), this, SLOT(checkValidity()));
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
	QPushButton *pushButton =
		new QPushButton(i18nc("button next to the 'Repeat:' label", "Never"), widget);
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

	if (persistentIndex.isValid()) {
		const DvbRecordingEntry *entry =
			model->data(persistentIndex, DvbRecordingModel::DvbRecordingEntryRole).
			value<const DvbRecordingEntry *>();
		nameEdit->setText(entry->name);
		channelBox->setCurrentIndex(channelBox->findText(entry->channelName));
		beginEdit->setDateTime(entry->begin.toLocalTime());
		durationEdit->setTime(entry->duration);

		for (int i = 0; i < 7; ++i) {
			if ((entry->repeat & (1 << i)) != 0) {
				dayCheckBoxes[i]->setChecked(true);
			}
		}

		if (entry->isRunning) {
			nameEdit->setEnabled(false);
			channelBox->setEnabled(false);
			beginEdit->setEnabled(false);
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
	DvbRecordingEntry entry;
	entry.name = nameEdit->text();
	entry.channelName = channelBox->currentText();
	entry.begin = beginEdit->dateTime().toUTC();
	entry.duration = durationEdit->time();

	for (int i = 0; i < 7; ++i) {
		if (dayCheckBoxes[i]->isChecked()) {
			entry.repeat |= (1 << i);
		}
	}

	// if persistentIndex is invalid, the entry is appended
	const DvbRecordingEntry *constEntry = &entry;
	model->setData(persistentIndex, QVariant::fromValue(constEntry));

	KDialog::accept();
}
