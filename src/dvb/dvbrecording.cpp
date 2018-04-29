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

#include "../log.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QEventLoop>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QVariant>

#include "../ensurenopendingoperation.h"
#include "dvbdevice.h"
#include "dvbepg.h"
#include "dvbliveview.h"
#include "dvbmanager.h"
#include "dvbrecording.h"
#include "dvbrecording_p.h"
#include "dvbtab.h"

bool DvbRecording::validate()
{
	if (!name.isEmpty() && channel.isValid() && begin.isValid() &&
	    (begin.timeSpec() == Qt::UTC) && duration.isValid()) {
		// the seconds and milliseconds aren't visible --> set them to zero
		begin = begin.addMSecs(-(QTime(0, 0, 0).msecsTo(begin.time()) % 60000));
		end = begin.addSecs(QTime(0, 0, 0).secsTo(duration));
		beginEPG = beginEPG.addMSecs(-(QTime(0, 0, 0).msecsTo(beginEPG.time()) % 60000));
		endEPG = beginEPG.addSecs(QTime(0, 0, 0).secsTo(durationEPG));
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
		<< QLatin1String("beginEPG") << QLatin1String("endEPG") << QLatin1String("durationEPG") << QLatin1String("Priority") << QLatin1String("Disabled"));

	// we regularly recheck the status of the recordings
	// this way we can keep retrying if the device was busy / tuning failed

	startTimer(5000);

	// compatibility code

	QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("/recordings.dvb"));

	if (!file.exists()) {
		return;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		qCWarning(logDvb, "Cannot open file %s", qPrintable(file.fileName()));
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
		recording.disabled = false;
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
			qCWarning(logDvb, "Invalid recordings in file %s", qPrintable(file.fileName()));
			break;
		}

		addRecording(recording);
	}

	if (!file.remove()) {
		qCWarning(logDvb, "Cannot remove file %s", qPrintable(file.fileName()));
	}
}

DvbRecordingModel::~DvbRecordingModel()
{
	if (hasPendingOperation) {
		qCWarning(logDvb, "Illegal recursive call");
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
			qCWarning(logDvb, "Illegal recursive call");
			return DvbSharedRecording();
		}

		EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	}

	if (!recording.validate()) {
		qCWarning(logDvb, "Invalid recording");
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
		qCWarning(logDvb, "Illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (!recording.isValid() || (recordings.value(*recording) != recording) ||
	    !modifiedRecording.validate()) {
		qCWarning(logDvb, "Invalid recording");
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
		qCWarning(logDvb, "Illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (!recording.isValid() || (recordings.value(*recording) != recording)) {
		qCWarning(logDvb, "Invalid recording");
		return;
	}

	recordings.remove(*recording);
	recordingFiles.remove(*recording);
	sqlRemove(*recording);
	emit recordingRemoved(recording);
	executeActionAfterRecording(*recording);
	findNewRecordings();
	removeDuplicates();
	disableConflicts();
}


void DvbRecordingModel::disableLessImportant(DvbSharedRecording &recording1, DvbSharedRecording &recording2)
{
	if (recording1->priority < recording2->priority) {
		DvbRecording rec1 = *(recording1.constData());
		rec1.disabled = true;
		qCWarning(logDvb, "Disabled %s because %s has more priority", qPrintable(recording1->name), qPrintable(recording2->name));
	}
	if (recording2->priority < recording1->priority) {
		DvbRecording rec2 = *(recording1.constData());
		rec2.disabled = true;
		qCWarning(logDvb, "Disabled %s because %s has more priority", qPrintable(recording2->name), qPrintable(recording1->name));
	}
}

void DvbRecordingModel::addToUnwantedRecordings(DvbSharedRecording recording)
{
	unwantedRecordings.append(recording);
	qCDebug(logDvb, "executed %s", qPrintable(recording->name));
}

void DvbRecordingModel::executeActionAfterRecording(DvbRecording recording)
{
	QString stopCommand = manager->getActionAfterRecording();

	stopCommand.replace("%filename", recording.filename);
	if (!stopCommand.isEmpty())
	{
		QProcess* child = new QProcess();
		child->start(stopCommand);
		qCWarning(logDvb, "Not executing command after recording");
	}
	qCDebug(logDvb, "executed.");


}

void DvbRecordingModel::removeDuplicates()
{
	QList<DvbSharedRecording> recordingList = QList<DvbSharedRecording>();
	DvbEpgModel *epgModel = manager->getEpgModel();

	if (!epgModel)
		return;

	QMap<DvbSharedRecording, DvbSharedEpgEntry> recordingMap = epgModel->getRecordings();
	foreach(DvbSharedRecording key, recordings.values())
	{
		recordingList.append(key);
	}
	int i = 0;
	foreach(DvbSharedRecording rec1, recordingList)
	{
		int j = 0;
		DvbRecording loopEntry1 = *rec1;
		foreach(DvbSharedRecording rec2, recordingList)
		{
			DvbRecording loopEntry2 = *rec2;
			if (i < j) {
				if (loopEntry1.begin == loopEntry2.begin
					&& loopEntry1.duration == loopEntry2.duration
					&& loopEntry1.channel->name == loopEntry2.channel->name
					&& loopEntry1.name == loopEntry2.name) {
					recordings.remove(recordings.key(rec1));
					recordingMap.remove(rec1);
					qCDebug(logDvb, "Removed. %s", qPrintable(loopEntry1.name));
				}
			}
			j = j + 1;
		}
		i = i + 1;
	}
	epgModel->setRecordings(recordingMap);

	qCDebug(logDvb, "executed.");

}

bool DvbRecordingModel::existsSimilarRecording(DvbEpgEntry recording)
{
	bool found = false;

	DvbEpgEntry entry = recording;
	DvbEpgModel *epgModel = manager->getEpgModel();

	if (!epgModel)
		return found;

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
				found = true;
				break;
			}
		}
	}

	QList<DvbSharedRecording> unwantedRecordingsList = manager->getRecordingModel()->getUnwantedRecordings();

	foreach(DvbSharedRecording unwanted, unwantedRecordingsList)
	{
		DvbRecording loopEntry = *unwanted;
		if (QString::compare(entry.begin.toString(), ((loopEntry.begin.addSecs(manager->getBeginMargin()))).toString()) == 0
				&& QString::compare(entry.channel->name, loopEntry.channel->name) == 0
				&& QString::compare((entry.duration).toString(),
						loopEntry.duration.addSecs(- manager->getBeginMargin() - manager->getEndMargin()).toString()) == 0) {
			qCDebug(logDvb, "Found from unwanteds %s", qPrintable(loopEntry.name));
			found = true;
			break;
		}
	}

	return found;
}

void DvbRecordingModel::disableConflicts()
{
	int maxSize = 1; // manager->getDeviceConfigs().size();
	// foreach(DvbDeviceConfig config, manager->getDeviceConfigs())
	// {
	//	maxSize = maxSize + config.numberOfTuners;
	// }

	QList<DvbSharedRecording> recordingList = QList<DvbSharedRecording>();
	foreach(DvbSharedRecording rec, recordings.values())
	{
		if (!(rec->disabled)) {
			recordingList.append(rec);
		}
	}

	foreach(DvbSharedRecording rec1, recordingList)
	{
		QList<DvbSharedRecording> conflictList = QList<DvbSharedRecording>();
		conflictList.append(rec1);

		foreach(DvbSharedRecording rec2, recordingList)
		{
			if (isInConflictWithAll(rec2, conflictList)) {
				conflictList.append(rec2);
				qCDebug(logDvb, "conflict: '%s' '%s' and '%s' '%s'", qPrintable(rec1->name), qPrintable(rec1->begin.toString()), qPrintable(rec2->name), qPrintable(rec2->begin.toString()));

			}

		}
		if (conflictList.size() > maxSize) {
			disableLeastImportants(conflictList);
		}
	}

}


bool DvbRecordingModel::areInConflict(DvbSharedRecording recording1, DvbSharedRecording recording2)
{
	int length1 = 60 * 60 * recording1->duration.hour() + 60 * recording1->duration.minute() + recording1->duration.second();
	QDateTime end1 = recording1->begin.addSecs(length1);
	int length2 = 60 * 60 * recording2->duration.hour() + 60 * recording2->duration.minute() + recording2->duration.second();
	QDateTime end2 = recording2->begin.addSecs(length2);
	if (!recording1->disabled && !recording1->disabled) {
		if (recording1->channel->transportStreamId != recording2->channel->transportStreamId) {
			if (recording2->begin > recording1->begin && recording2->begin < end1) {
				return true;
			}
			if (recording1->begin > recording2->begin && recording1->begin < end2) {
				return true;
			}
		}
	}
	return false;
}
bool DvbRecordingModel::isInConflictWithAll(DvbSharedRecording rec, QList<DvbSharedRecording> recList)
{

	foreach(DvbSharedRecording listRec, recList)
	{
		if (!areInConflict(rec, listRec)) {
			return false;
		}
	}

	return true;
}

int DvbRecordingModel::getNumberOfLeastImportants(QList<DvbSharedRecording> recList)
{
	DvbSharedRecording leastImportantShared = getLeastImportant(recList);
	int leastImportance = leastImportantShared->priority;
	int count = 0;
	foreach(DvbSharedRecording listRecShared, recList)
	{
		if (listRecShared->priority == leastImportance) {
			count = count + 1;
		}
	}

	return count;
}

DvbSharedRecording DvbRecordingModel::getLeastImportant(QList<DvbSharedRecording> recList)
{
	DvbSharedRecording leastImportant = recList.value(0);
	foreach(DvbSharedRecording listRec, recList)
	{
		qCDebug(logDvb, "name and priority %s %s", qPrintable(listRec->name), qPrintable(listRec->priority));
		if (listRec->priority < leastImportant->priority) {
			leastImportant = listRec;
		}
	}

	qCDebug(logDvb, "least important: %s", qPrintable(leastImportant->name));
	return leastImportant;
}

void DvbRecordingModel::disableLeastImportants(QList<DvbSharedRecording> recList)
{
	int numberOfLeastImportants = getNumberOfLeastImportants(recList);
	if (numberOfLeastImportants < recList.size()) {
		DvbSharedRecording leastImportantShared = getLeastImportant(recList);
		int leastImportance = leastImportantShared->priority;

		foreach(DvbSharedRecording listRecShared, recList)
		{
			DvbRecording listRec = *(listRecShared.constData());
			if (listRecShared->priority == leastImportance) {
				listRec.disabled = true;
				updateRecording(listRecShared, listRec);
				qCDebug(logDvb, "disabled: %s %s", qPrintable(listRec.name), qPrintable(listRec.begin.toString()));
			}
		}
	}

}

void DvbRecordingModel::findNewRecordings()
{
	DvbEpgModel *epgModel = manager->getEpgModel();

	if (!epgModel)
		return;

	QMap<DvbEpgEntryId, DvbSharedEpgEntry> epgMap = epgModel->getEntries();
	foreach(DvbEpgEntryId key, epgMap.keys())
	{
		DvbEpgEntry entry = *(epgMap.value(key));
		QString title = entry.title(FIRST_LANG);
		QStringList regexList = manager->getRecordingRegexList();
		int i = 0;
		foreach(QString regex, regexList) {
			QRegExp recordingRegex = QRegExp(regex);
			if (!recordingRegex.isEmpty())
				{
				if (recordingRegex.indexIn(title) != -1)
				{
					if (!DvbRecordingModel::existsSimilarRecording(*epgMap.value(key)))
					{
					int priority = manager->getRecordingRegexPriorityList().value(i);
					epgModel->scheduleProgram(epgMap.value(key), manager->getBeginMargin(),
							manager->getEndMargin(), false, priority);
					qCDebug(logDvb, "scheduled %s", qPrintable(title));
					}
				}
			}
			i = i + 1;
		}
	}

	qCDebug(logDvb, "executed.");
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
		qCWarning(logDvb, "Invalid recording");
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
	query.bindValue(index++, recording->priority);
	query.bindValue(index++, recording->disabled);
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
	recording->priority = query.value(index++).toInt();
	recording->disabled = query.value(index++).toBool();

	if (recording->validate()) {
		recording->setSqlKey(sqlKey);
		recordings.insert(*newRecording, newRecording);
		return true;
	}

	return false;
}

/*
 * Returns -1 if no upcoming recordings.
 */
int DvbRecordingModel::getSecondsUntilNextRecording() const
{
	signed long timeUntil = -1;
	foreach (const DvbSharedRecording &recording, recordings) {
		DvbRecording rec = *recording;
		int length = 60 * 60 * rec.duration.hour() + 60 * rec.duration.minute() + rec.duration.second();
		QDateTime end = rec.begin.addSecs(length);
		if (rec.disabled || end < QDateTime::currentDateTime().toUTC()) {
			continue;
		}
		if (end > QDateTime::currentDateTime().toUTC() && rec.begin <= QDateTime::currentDateTime().toUTC()) {
			timeUntil = 0;
			qCDebug(logDvb, "Rec ongoing %s", qPrintable(rec.name));
			break;
		}
		if (rec.begin > QDateTime::currentDateTime().toUTC()) {
			if (timeUntil == -1 || timeUntil > rec.begin.toTime_t() - QDateTime::currentDateTime().toUTC().toTime_t()) {
				timeUntil = rec.begin.toTime_t() - QDateTime::currentDateTime().toUTC().toTime_t();
			}
		}

	}

	qCDebug(logDvb, "returned TRUE %ld", timeUntil);
	return timeUntil;
}

bool DvbRecordingModel::isScanWhenIdle() const
{
	return manager->isScanWhenIdle();
}

bool DvbRecordingModel::shouldWeScanChannels() const
{
	int numberOfChannels = manager->getChannelModel()->getChannels().size();
	int idleTime = 1000 * 3600 + 1; // TODO
	if (idleTime > 1000 * 3600) {
		if (DvbRecordingModel::getSecondsUntilNextRecording() > numberOfChannels * 10) {
			if (DvbRecordingModel::isScanWhenIdle()) {
				qCDebug(logDvb, "Scan on Idle enabled");
				return true;
			}
		}
	}

	return false;
}

void delay(int seconds)
{
	QTime dieTime= QTime::currentTime().addSecs(seconds);
	while (QTime::currentTime() < dieTime)
		QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

	qCInfo(logDvb, "Delayed for %d seconds", seconds);
}

void DvbRecordingModel::scanChannels()
{
	qCDebug(logDvb, "auto-scan channels");

	if (shouldWeScanChannels()) {
		DvbChannelModel *channelModel = manager->getChannelModel();
		QMap<int, DvbSharedChannel> channelMap = channelModel->getChannels();
		foreach (int channelInt, channelMap.keys()) {
			DvbSharedChannel channel;

			if (channelInt > 0) {
				channel = channelModel->findChannelByNumber(channelInt);
			}
			if (channel.isValid()) {
				// TODO update tab
				qCDebug(logDvb, "Executed %s", qPrintable(channel->name));
				manager->getLiveView()->playChannel(channel);
				delay(5);
			}
		}
	}
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

		recording.begin = beginLocal.addDays(days).toUTC();
		recording.end = recording.begin.addSecs(QTime().secsTo(recording.duration));
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
	if (recording.disabled) {
		return false;
	}

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
				qCWarning(logDvb, "Cannot open file %s. Error: %d", qPrintable(file.fileName()), errno);
			}

			if ((attempt == 0) && !QDir(folder).exists()) {
				if (QDir().mkpath(folder)) {
					attempt = -1;
					continue;
				} else {
					qCWarning(logDvb, "Cannot create folder %s", qPrintable(folder));
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
			qCWarning(logDvb, "Cannot open file %s", qPrintable(file.fileName()));
			return false;
		}
	}

	if (device == NULL) {
		channel = recording.channel;
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Prioritized);

		if (device == NULL) {
			qCWarning(logDvb, "Cannot find a suitable device");
			return false;
		}

		/*
		 * When there's not enough devices to record while
		 * watching, switch to the channel that will be recorded
		 */
		if (manager->hasReacquired())
			manager->getLiveView()->playChannel(channel);

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
	manager->getRecordingModel()->removeDuplicates();
	manager->getRecordingModel()->disableConflicts();
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
	int pcrPid = pmtSection.pcrPid();
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

	/* check PCR PID is set */
	if (pcrPid != 0x1fff) {
		/* Check not already in list */
		if (!newPids.contains(pcrPid))
			newPids.insert(pcrPid);
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

