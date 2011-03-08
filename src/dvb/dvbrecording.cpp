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
#include "dvbdevice.h"
#include "dvbmanager.h"

bool DvbRecording::validate()
{
	if (!name.isEmpty() && channel.isValid() && begin.isValid() &&
	    (begin.timeSpec() == Qt::UTC) && duration.isValid()) {
		// the seconds and milliseconds aren't visible --> set them to zero
		begin = begin.addMSecs(-(QTime().msecsTo(begin.time()) % 60000));
		end = begin.addSecs(QTime().secsTo(duration));
		repeat &= ((1 << 7) - 1);
		return true;
	}

	return false;
}

DvbRecordingModel::DvbRecordingModel(DvbManager *manager_, QObject *parent) :
	QObject(parent), manager(manager_)
{
	sqlInit("RecordingSchedule",
		QStringList() << "Name" << "Channel" << "Begin" << "Duration" << "Repeat");

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed

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

	DvbChannelModel *channelModel = manager->getChannelModel();
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
		DvbRecording recording;
		QString channelName;
		stream >> channelName;
		recording.channel = channelModel->findChannelByName(channelName);
		stream >> recording.begin;
		recording.begin = recording.begin.toUTC();
		stream >> recording.duration;
		QDateTime end;
		stream >> end;

		if (version != 0) {
			stream >> recording.repeat;
		}

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid recordings in file" << file.fileName();
			break;
		}

		addRecording(recording);
	}

	if (!file.remove()) {
		kWarning() << "cannot remove file" << file.fileName();
	}
}

DvbRecordingModel::~DvbRecordingModel()
{
	sqlFlush();
}

bool DvbRecordingModel::hasRecordings() const
{
	return !recordings.isEmpty();
}

bool DvbRecordingModel::hasActiveRecordings() const
{
	return !recordingFiles.isEmpty();
}

DvbSharedRecording DvbRecordingModel::findRecordingByKey(const SqlKey &key) const
{
	return recordings.value(key);
}

QMap<SqlKey, DvbSharedRecording> DvbRecordingModel::listProgramSchedule() const
{
	return recordings;
}

DvbSharedRecording DvbRecordingModel::addRecording(DvbRecording &recording)
{
	if (!recording.validate()) {
		kWarning() << "invalid recording";
		return DvbSharedRecording();
	}

	recording.setSqlKey(sqlFindFreeKey(recordings));

	if (!updateStatus(recording)) {
		return DvbSharedRecording();
	}

	DvbSharedRecording newRecording(new DvbRecording(recording));
	recordings.insert(*newRecording, newRecording);
	sqlInsert(*newRecording);
	emit recordingAdded(newRecording);
	return newRecording;
}

void DvbRecordingModel::updateRecording(const DvbSharedRecording &recording,
	DvbRecording &modifiedRecording)
{
	if (!recording.isValid() || (recordings.value(*recording) != recording) ||
	    !modifiedRecording.validate()) {
		kWarning() << "invalid recording";
		return;
	}

	modifiedRecording.setSqlKey(*recording);

	if (!updateStatus(modifiedRecording)) {
		recordings.remove(*recording);
		recordingFiles.remove(*recording);
		sqlRemove(*recording);
		emit recordingRemoved(recording);
		return;
	}

	DvbRecording oldRecording = *recording;
	*const_cast<DvbRecording *>(recording.data()) = modifiedRecording;
	sqlUpdate(*recording);
	emit recordingUpdated(recording, oldRecording);
}

void DvbRecordingModel::removeRecording(const DvbSharedRecording &recording)
{
	if (!recording.isValid() || (recordings.value(*recording) != recording)) {
		kWarning() << "invalid recording";
		return;
	}

	recordings.remove(*recording);
	recordingFiles.remove(*recording);
	sqlRemove(*recording);
	emit recordingRemoved(recording);
}

void DvbRecordingModel::checkStatus()
{
	QDateTime currentDateTime = QDateTime::currentDateTime().toUTC();

	foreach (const DvbSharedRecording &recording, recordings) {
		if (recording->end <= currentDateTime) {
			DvbRecording modifiedRecording = *recording;
			updateRecording(recording, modifiedRecording);
		}
	}

	foreach (const DvbSharedRecording &recording, recordings) {
		if ((recording->status != DvbRecording::Recording) &&
		    (recording->begin <= currentDateTime)) {
			DvbRecording modifiedRecording = *recording;
			updateRecording(recording, modifiedRecording);
		}
	}
}

void DvbRecordingModel::bindToSqlQuery(SqlKey sqlKey, QSqlQuery &query, int index) const
{
	DvbSharedRecording recording = recordings.value(sqlKey);

	if (!recording.isValid()) {
		kError() << "invalid recording";
		return;
	}

	query.bindValue(index++, recording->name);
	query.bindValue(index++, recording->channel->name);
	query.bindValue(index++, recording->begin.toString(Qt::ISODate) + 'Z');
	query.bindValue(index++, recording->duration.toString(Qt::ISODate));
	query.bindValue(index++, recording->repeat);
}

bool DvbRecordingModel::insertFromSqlQuery(SqlKey sqlKey, const QSqlQuery &query, int index)
{
	DvbRecording *recording = new DvbRecording();
	DvbSharedRecording newRecording(recording);
	recording->name = query.value(index++).toString();
	recording->channel =
		manager->getChannelModel()->findChannelByName(query.value(index++).toString());
	recording->begin =
		QDateTime::fromString(query.value(index++).toString(), Qt::ISODate).toUTC();
	recording->duration = QTime::fromString(query.value(index++).toString(), Qt::ISODate);
	recording->repeat = query.value(index++).toInt();

	if (recording->validate()) {
		recording->setSqlKey(sqlKey);
		recordings.insert(*newRecording, newRecording);
		return true;
	}

	return false;
}

bool DvbRecordingModel::updateStatus(DvbRecording &recording)
{
	QDateTime currentDateTimeLocal = QDateTime::currentDateTime();
	QDateTime currentDateTime = currentDateTimeLocal.toUTC();

	if (recording.end <= currentDateTime) {
		if (recording.repeat == 0) {
			return false;
		}

		recordingFiles.remove(recording);

		// take care of DST switches
		QDateTime beginLocal = recording.begin.toLocalTime();
		QDateTime endLocal = recording.end.toLocalTime();
		int days = endLocal.daysTo(currentDateTimeLocal);

		if (endLocal.addDays(days) <= currentDateTimeLocal) {
			++days;
		}

		// QDate::dayOfWeek() and our dayOfWeek differ by one
		int dayOfWeek = (beginLocal.date().dayOfWeek() - 1 + days);

		for (int j = 0; j < 7; ++j) {
			if ((recording.repeat & (1 << (dayOfWeek % 7))) != 0) {
				break;
			}

			++days;
			++dayOfWeek;
		}

		recording.begin = recording.begin.addDays(days);
		recording.end = recording.end.addDays(days);
	}

	if (recording.begin <= currentDateTime) {
		QExplicitlySharedDataPointer<DvbRecordingFile> recordingFile =
			recordingFiles.value(recording);

		if (recordingFile.constData() == NULL) {
			recordingFile = new DvbRecordingFile(manager);
			recordingFiles.insert(recording, recordingFile);
		}

		if (recordingFile->start(recording)) {
			recording.status = DvbRecording::Recording;
		} else {
			recording.status = DvbRecording::Error;
		}
	} else {
		recording.status = DvbRecording::Inactive;
		recordingFiles.remove(recording);
	}

	return true;
}

DvbRecordingFile::DvbRecordingFile(DvbManager *manager_) : manager(manager_), device(NULL),
	pmtValid(false)
{
	connect(&pmtFilter, SIGNAL(pmtSectionChanged(QByteArray)),
		this, SLOT(pmtSectionChanged(QByteArray)));
	connect(&patPmtTimer, SIGNAL(timeout()), this, SLOT(insertPatPmt()));
}

DvbRecordingFile::~DvbRecordingFile()
{
	stop();
}

bool DvbRecordingFile::start(const DvbRecording &recording)
{
	if (!file.isOpen()) {
		QString folder = manager->getRecordingFolder();
		QString path = folder + '/' + QString(recording.name).replace('/', '_');

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
			} else {
				kWarning() << "cannot open file" << file.fileName();
			}

			if ((attempt == 0) && !QDir(folder).exists()) {
				if (QDir().mkpath(folder)) {
					attempt = -1;
					continue;
				} else {
					kWarning() << "cannot create folder" << folder;
				}
			}

			if (folder != QDir::homePath()) {
				folder = QDir::homePath();
				path = folder + '/' + QString(recording.name).replace('/', '_');
				attempt = -1;
				continue;
			}

			break;
		}

		if (!file.isOpen()) {
			kWarning() << "cannot open file" << file.fileName();
			return false;
		}
	}

	if (device == NULL) {
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Prioritized);

		if (device == NULL) {
			kWarning() << "cannot find a suitable device";
			return false;
		}

		connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
		pmtFilter.setProgramNumber(channel->getServiceId());
		device->addSectionFilter(channel->pmtPid, &pmtFilter);
		pmtSectionData = channel->pmtSectionData;
		patGenerator.initPat(channel->transportStreamId, channel->getServiceId(),
			channel->pmtPid);

		if (channel->isScrambled && !pmtSectionData.isEmpty()) {
			device->startDescrambling(pmtSectionData, this);
		}
	}

	return true;
}

void DvbRecordingFile::stop()
{
	if (device != NULL) {
		if (channel->isScrambled && !pmtSectionData.isEmpty()) {
			device->stopDescrambling(pmtSectionData, this);
		}

		foreach (int pid, pids) {
			device->removePidFilter(pid, this);
		}

		device->removeSectionFilter(channel->pmtPid, &pmtFilter);
		disconnect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
		manager->releaseDevice(device, DvbManager::Prioritized);
		device = NULL;
	}

	pmtValid = false;
	patPmtTimer.stop();
	patGenerator.reset();
	pmtGenerator.reset();
	pmtSectionData.clear();
	pids.clear();
	buffers.clear();
	file.close();
	channel = DvbSharedChannel();
}

void DvbRecordingFile::deviceStateChanged()
{
	if (device->getDeviceState() == DvbDevice::DeviceReleased) {
		foreach (int pid, pids) {
			device->removePidFilter(pid, this);
		}

		device->removeSectionFilter(channel->pmtPid, &pmtFilter);
		disconnect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));

		if (channel->isScrambled && !pmtSectionData.isEmpty()) {
			device->stopDescrambling(pmtSectionData, this);
		}

		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Prioritized);

		if (device != NULL) {
			connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
			device->addSectionFilter(channel->pmtPid, &pmtFilter);

			foreach (int pid, pids) {
				device->addPidFilter(pid, this);
			}

			if (channel->isScrambled && !pmtSectionData.isEmpty()) {
				device->startDescrambling(pmtSectionData, this);
			}
		} else {
			stop();
		}
	}
}

void DvbRecordingFile::pmtSectionChanged(const QByteArray &pmtSectionData_)
{
	pmtSectionData = pmtSectionData_;
	DvbPmtSection pmtSection(pmtSectionData);
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
		device->startDescrambling(pmtSectionData, this);
	}
}

void DvbRecordingFile::insertPatPmt()
{
	if (!pmtValid) {
		pmtSectionChanged(channel->pmtSectionData);
		return;
	}

	file.write(patGenerator.generatePackets());
	file.write(pmtGenerator.generatePackets());
}

void DvbRecordingFile::processData(const char data[188])
{
	if (!pmtValid) {
		if (!patPmtTimer.isActive()) {
			patPmtTimer.start(1000);
			QByteArray nextBuffer;
			nextBuffer.reserve(348 * 188);
			buffers.append(nextBuffer);
		}

		QByteArray &buffer = buffers.last();
		buffer.append(data, 188);

		if (buffer.size() >= (348 * 188)) {
			QByteArray nextBuffer;
			nextBuffer.reserve(348 * 188);
			buffers.append(nextBuffer);
		}

		return;
	}

	file.write(data, 188);
}
