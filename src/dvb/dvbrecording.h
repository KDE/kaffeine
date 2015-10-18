/*
 * dvbrecording.h
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

#ifndef DVBRECORDING_H
#define DVBRECORDING_H

#include <QDateTime>
#include <QTextStream>
#include "dvbchannel.h"

class DvbManager;
class DvbRecordingFile;
class DvbEpgEntry;

class DvbRecording : public SharedData, public SqlKey
{

public:
	DvbRecording() : repeat(0), status(Inactive) { }
	~DvbRecording() { }

	// checks that all variables are ok and updates 'end'
	// 'sqlKey' and 'status' are ignored
	bool validate();


	enum Status {
		Inactive,
		Recording,
		Error
	};

	QString name;
	QString filename;
	QString subheading;
	QString details;
	DvbSharedChannel channel;
	QDateTime begin; // UTC
	QDateTime end; // UTC, read-only
	QTime duration;
	QDateTime beginEPG; // What EPG claims to be the beginning of the program, local time
	QDateTime endEPG;
	QTime durationEPG;
	int repeat; // (1 << 0) (monday) | (1 << 1) (tuesday) | ... | (1 << 6) (sunday)
	int priority;
	bool disabled;
	Status status; // read-only
};

typedef ExplicitlySharedDataPointer<const DvbRecording> DvbSharedRecording;
Q_DECLARE_TYPEINFO(DvbSharedRecording, Q_MOVABLE_TYPE);

class DvbRecordingModel : public QObject, private SqlInterface
{
	Q_OBJECT
public:
	DvbRecordingModel(DvbManager *manager_, QObject *parent);
	~DvbRecordingModel();

	bool hasRecordings() const;
	void setEmptyAction(QString emptyAction);
	QString getEmptyAction();
	bool hasActiveRecordings() const;
	DvbSharedRecording findRecordingByKey(const SqlKey &sqlKey) const;
	QMap<SqlKey, DvbSharedRecording> getRecordings() const;
	QList<DvbSharedRecording> getUnwantedRecordings() const;
	DvbSharedRecording addRecording(DvbRecording &recording, bool checkForRecursion=false);
	void updateRecording(DvbSharedRecording recording, DvbRecording &modifiedRecording);
	void removeRecording(DvbSharedRecording recording);
	void addToUnwantedRecordings(DvbSharedRecording recording);
	void findNewRecordings();
	void removeDuplicates();
	void executeActionAfterRecording(DvbRecording recording);
	DvbRecording getCurrentRecording();
	void setCurrentRecording(DvbRecording _currentRecording);
	void disableLessImportant(DvbSharedRecording &recording1, DvbSharedRecording &recording2);
	bool areInConflict(DvbSharedRecording recording1, DvbSharedRecording recording2);
	bool isInConflictWithAll(DvbSharedRecording rec, QList<DvbSharedRecording> recList);
	int getNumberOfLeastImportants(QList<DvbSharedRecording> recList);
	DvbSharedRecording getLeastImportant(QList<DvbSharedRecording> recList);
	void disableLeastImportants(QList<DvbSharedRecording> recList);
	void disableConflicts();
	int getSecondsUntilNextRecording() const;
	bool isScanWhenIdle() const;
	bool shouldWeScanChannels() const;
	void scanChannels();

signals:
	void recordingAdded(const DvbSharedRecording &recording);
	// updating doesn't change the recording pointer (modifies existing content)
	void recordingAboutToBeUpdated(const DvbSharedRecording &recording);
	void recordingUpdated(const DvbSharedRecording &recording);
	void recordingRemoved(const DvbSharedRecording &recording);

private:
	void timerEvent(QTimerEvent *event);

	void bindToSqlQuery(SqlKey sqlKey, QSqlQuery &query, int index) const;
	bool insertFromSqlQuery(SqlKey sqlKey, const QSqlQuery &query, int index);
	bool updateStatus(DvbRecording &recording);
	bool existsSimilarRecording(DvbEpgEntry recording);

	DvbManager *manager;
	QMap<SqlKey, DvbSharedRecording> recordings;
	QList<DvbSharedRecording> unwantedRecordings;
	QMap<SqlKey, QExplicitlySharedDataPointer<DvbRecordingFile> > recordingFiles;
	bool hasPendingOperation;
	DvbRecording currentRecording;
};

void delay(int seconds);

#endif /* DVBRECORDING_H */
