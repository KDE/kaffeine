/*
 * dvbrecording.cpp
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

#include "dvbrecording.h"
#include "dvbrecording_p.h"

#include <QDir>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QVariant>
#include <KStandardDirs>
#include "../ensurenopendingoperation.h"
#include "../log.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbepg.h"

bool DvbRecording::validate()
{
	if (!name.isEmpty() && channel.isValid() && begin.isValid() &&
	    (begin.timeSpec() == Qt::UTC) && duration.isValid()) {
		// the seconds and milliseconds aren't visible --> set them to zero
		begin = begin.addMSecs(-(QTime().msecsTo(begin.time()) % 60000));
		end = begin.addSecs(QTime().secsTo(duration));
		beginEPG = beginEPG.addMSecs(-(QTime().msecsTo(beginEPG.time()) % 60000));
		endEPG = beginEPG.addSecs(QTime().secsTo(durationEPG));
		repeat &= ((1 << 7) - 1);
		return true;
	}

	return false;
}

DvbRecordingModel::DvbRecordingModel(DvbManager *manager_, QObject *parent) : QObject(parent),
	manager(manager_), hasPendingOperation(false)
{
	sqlInit(QLatin1String("RecordingSchedule"),
		QStringList() << QLatin1String("Name") << QLatin1String("Channel") << QLatin1String("Begin") <<
		QLatin1String("Duration") << QLatin1String("Repeat") << QLatin1String("Subheading") << QLatin1String("Details")
		<< QLatin1String("beginEPG") << QLatin1String("endEPG") << QLatin1String("durationEPG"));

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed

	startTimer(5000);

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("recordings.dvb")));

	if (!file.exists()) {
		return;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		Log("DvbRecordingModel::DvbRecordingModel: cannot open file") << file.fileName();
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
		stream >> recording.beginEPG;
		recording.begin = recording.beginEPG.toUTC();
		recording.beginEPG = recording.beginEPG.toLocalTime();
		stream >> recording.duration;
		recording.durationEPG = recording.duration;
		QDateTime end;
		stream >> end;

		if (version != 0) {
			stream >> recording.repeat;
		}

		stream >> recording.subheading;
		stream >> recording.details;

		if (stream.status() != QDataStream::Ok) {
			Log("DvbRecordingModel::DvbRecordingModel: invalid recordings in file") <<
				file.fileName();
			break;
		}

		addRecording(recording);
	}

	if (!file.remove()) {
		Log("DvbRecordingModel::DvbRecordingModel: cannot remove file") << file.fileName();
	}
}

DvbRecordingModel::~DvbRecordingModel()
{
	if (hasPendingOperation) {
		Log("DvbRecordingModel::~DvbRecordingModel: illegal recursive call");
	}

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

DvbSharedRecording DvbRecordingModel::findRecordingByKey(const SqlKey &sqlKey) const
{
	return recordings.value(sqlKey);
}

DvbRecording DvbRecordingModel::getCurrentRecording()
{
	return currentRecording;
}

void DvbRecordingModel::setCurrentRecording(DvbRecording _currentRecording)
{
	currentRecording = _currentRecording;
}

QMap<SqlKey, DvbSharedRecording> DvbRecordingModel::getRecordings() const
{
	return recordings;
}

QList<DvbSharedRecording> DvbRecordingModel::getUnwantedRecordings() const
{
	return unwantedRecordings;
}

DvbSharedRecording DvbRecordingModel::addRecording(DvbRecording &recording, bool checkForRecursion)
{
	if (checkForRecursion) {
		if (hasPendingOperation) {
			Log("DvbRecordingModel::addRecording: illegal recursive call");
			return DvbSharedRecording();
		}

		EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	}

	if (!recording.validate()) {
		Log("DvbRecordingModel::addRecording: invalid recording");
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

void DvbRecordingModel::updateRecording(DvbSharedRecording recording,
	DvbRecording &modifiedRecording)
{
	if (hasPendingOperation) {
		Log("DvbRecordingModel::updateRecording: illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (!recording.isValid() || (recordings.value(*recording) != recording) ||
	    !modifiedRecording.validate()) {
		Log("DvbRecordingModel::updateRecording: invalid recording");
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

	emit recordingAboutToBeUpdated(recording);
	*const_cast<DvbRecording *>(recording.constData()) = modifiedRecording;
	sqlUpdate(*recording);
	emit recordingUpdated(recording);
}

void DvbRecordingModel::removeRecording(DvbSharedRecording recording)
{
	if (hasPendingOperation) {
		Log("DvbRecordingModel::removeRecording: illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (!recording.isValid() || (recordings.value(*recording) != recording)) {
		Log("DvbRecordingModel::removeRecording: invalid recording");
		return;
	}

	recordings.remove(*recording);
	recordingFiles.remove(*recording);
	sqlRemove(*recording);
	emit recordingRemoved(recording);
	executeActionAfterRecording(*recording);
	findNewRecordings();
}

void DvbRecordingModel::addToUnwantedRecordings(DvbSharedRecording recording)
{
	unwantedRecordings.append(recording);
}

void DvbRecordingModel::executeActionAfterRecording(DvbRecording recording)
{
	QString stopCommand = manager->getActionAfterRecording();

	stopCommand.replace("%filename", recording.filename);
	if (!stopCommand.isEmpty())
	{
		QProcess* child = new QProcess();
		child->start(stopCommand);
		Log("DvbRecordingModel::shutdownWhenEmpty:could not execute cmd");
	}
	Log("DvbRecordingModel::executeActionAfterRecording executed.");


}

bool DvbRecordingModel::existsSimilarRecording(DvbEpgEntry recording)
{
	bool found = false;

	DvbEpgEntry entry = recording;
	DvbEpgModel *epgModel = manager->getEpgModel();
	QMap<DvbSharedRecording, DvbSharedEpgEntry> recordingMap = epgModel->getRecordings();
	foreach(DvbSharedRecording key, recordingMap.keys())
	{
		DvbEpgEntry loopEntry = *(recordingMap.value(key));
		int loopLength = 60 * 60 * loopEntry.duration.hour() + 60 * loopEntry.duration.minute() + loopEntry.duration.second();
		int length = 60 * 60 * entry.duration.hour() + 60 * entry.duration.minute() + entry.duration.second();
		QDateTime end = entry.begin.addSecs(length);
		QDateTime loopEnd = loopEntry.begin.addSecs(loopLength);

		if (QString::compare(entry.channel->name, loopEntry.channel->name) == 0) {
			// Is included in an existing recording
			if (entry.begin <= loopEntry.begin && end >= loopEnd) {
				found = true;
				break;
			// Includes an existing recording
			} else if (entry.begin >= loopEntry.begin && end <= loopEnd) {
				removeRecording(key);
				continue;
			}
		}
	}

	QList<DvbSharedRecording> unwantedRecordingsList = manager->getRecordingModel()->getUnwantedRecordings();
	foreach(DvbSharedRecording unwanted, unwantedRecordingsList)
	{
		DvbRecording loopEntry = *unwanted;
		if (entry.begin == loopEntry.begin
				&& entry.channel == loopEntry.channel
				&& entry.duration == loopEntry.duration) {
			found = true;
			break;
		}
	}

	return found;
}

void DvbRecordingModel::findNewRecordings()
{
	DvbEpgModel *epgModel = manager->getEpgModel();
	QMap<DvbEpgEntryId, DvbSharedEpgEntry> epgMap = epgModel->getEntries();
	foreach(DvbEpgEntryId key, epgMap.keys())
	{
		DvbEpgEntry entry = *(epgMap.value(key));
		QString title = entry.title;
		QRegExp recordingRegex = QRegExp(manager->getRecordingRegex());
		if (!recordingRegex.isEmpty())
			{
			if (recordingRegex.indexIn(title) != -1)
			{
				if (!DvbRecordingModel::existsSimilarRecording(*epgMap.value(key)))
				{
				epgModel->scheduleProgram(epgMap.value(key), manager->getBeginMargin(),
						manager->getEndMargin(), false);
				Log("DvbRecordingModel::findNewRecordings: scheduled") << title;
				}
			}
		}
	}

	Log("DvbRecordingModel::findNewRecordings executed.");
}

void DvbRecordingModel::timerEvent(QTimerEvent *event)
{
	Q_UNUSED(event)
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
		Log("DvbRecordingModel::bindToSqlQuery: invalid recording");
		return;
	}

	query.bindValue(index++, recording->name);
	query.bindValue(index++, recording->channel->name);
	query.bindValue(index++, recording->begin.toString(Qt::ISODate) + QLatin1Char('Z'));
	query.bindValue(index++, recording->duration.toString(Qt::ISODate));
	query.bindValue(index++, recording->repeat);
	query.bindValue(index++, recording->subheading);
	query.bindValue(index++, recording->details);
	query.bindValue(index++, recording->beginEPG.toString(Qt::ISODate));
	query.bindValue(index++, recording->endEPG.toString(Qt::ISODate));
	query.bindValue(index++, recording->durationEPG.toString(Qt::ISODate));
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
	recording->subheading = query.value(index++).toString();
	recording->details = query.value(index++).toString();
	recording->beginEPG =
		QDateTime::fromString(query.value(index++).toString(), Qt::ISODate).toLocalTime();
	recording->endEPG =
		QDateTime::fromString(query.value(index++).toString(), Qt::ISODate).toLocalTime();
	recording->durationEPG = QTime::fromString(query.value(index++).toString(), Qt::ISODate);

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

bool DvbRecordingFile::start(DvbRecording &recording)
{
	if (!file.isOpen()) {
		QString folder = manager->getRecordingFolder();
		QDate currentDate = QDate::currentDate();
		QTime currentTime = QTime::currentTime();

		QString filename = manager->getNamingFormat();
		filename = filename.replace("%year", currentDate.toString("yyyy"));
		filename = filename.replace("%month", currentDate.toString("MM"));
		filename = filename.replace("%day", currentDate.toString("dd"));
		filename = filename.replace("%hour", currentTime.toString("hh"));
		filename = filename.replace("%min", currentTime.toString("mm"));
		filename = filename.replace("%sec", currentTime.toString("ss"));
		filename = filename.replace("%channel", recording.channel->name);
		filename = filename.replace("%title", QString(recording.name));
		filename = filename.replace(QLatin1Char('/'), QLatin1Char('_'));
		if (filename == "") {
			filename = QString(recording.name);
		}

		QString path = folder + QLatin1Char('/') + filename;


		for (int attempt = 0; attempt < 100; ++attempt) {
			if (attempt == 0) {
				file.setFileName(path + QLatin1String(".m2t"));
				recording.filename = filename + QLatin1String(".m2t");
			} else {
				file.setFileName(path + QLatin1Char('-') + QString::number(attempt) +
					QLatin1String(".m2t"));
				recording.filename = filename + QLatin1Char('-') + QString::number(attempt) +
					QLatin1String(".m2t");
			}

			if (file.exists()) {
				continue;
			}

			if (file.open(QIODevice::WriteOnly)) {
				break;
			} else {
				Log("DvbRecordingFile::start: cannot open file") <<
					file.fileName();
			}

			if ((attempt == 0) && !QDir(folder).exists()) {
				if (QDir().mkpath(folder)) {
					attempt = -1;
					continue;
				} else {
					Log("DvbRecordingFile::start: cannot create folder") <<
						folder;
				}
			}

			if (folder != QDir::homePath()) {
				folder = QDir::homePath();
				path = folder + QLatin1Char('/') +
					QString(recording.name).replace(QLatin1Char('/'), QLatin1Char('_'));
				attempt = -1;
				continue;
			}

			break;
		}

		if (manager->createInfoFile()) {
			QString infoFile = path + ".txt";
			QFile file(infoFile);
			if (file.open(QIODevice::ReadWrite))
			{
			    QTextStream stream(&file);
			    stream << "EPG info" << endl;
			    stream << "Title: " + QString(recording.name) << endl;
			    stream << "Description: " + QString(recording.subheading) << endl;
			    //stream << "Details: " + QString(recording.details) << endl; // Details is almost always empty
			    stream << "Channel: " + QString(recording.channel->name) << endl;
			    stream << "Date: " + recording.beginEPG.toLocalTime().toString("yyyy-MM-dd") << endl;
			    stream << "Start time: " + recording.beginEPG.toLocalTime().toString("hh:mm:ss") << endl;
			    stream << "Duration: " + recording.durationEPG.toString("HH:mm:ss") << endl;
			}
		}

		if (!file.isOpen()) {
			Log("DvbRecordingFile::start: cannot open file") << file.fileName();
			return false;
		}
	}

	if (device == NULL) {
		channel = recording.channel;
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Prioritized);

		if (device == NULL) {
			Log("DvbRecordingFile::start: cannot find a suitable device");
			return false;
		}

		connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
		pmtFilter.setProgramNumber(channel->serviceId);
		device->addSectionFilter(channel->pmtPid, &pmtFilter);
		pmtSectionData = channel->pmtSectionData;
		patGenerator.initPat(channel->transportStreamId, channel->serviceId,
			channel->pmtPid);

		if (channel->isScrambled && !pmtSectionData.isEmpty()) {
			device->startDescrambling(pmtSectionData, this);
		}
	}

	manager->getRecordingModel()->setCurrentRecording(recording);

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

	manager->getRecordingModel()->executeActionAfterRecording(manager->getRecordingModel()->getCurrentRecording());
	manager->getRecordingModel()->findNewRecordings();
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
			// TODO

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
