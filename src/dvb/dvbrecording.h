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
#include "dvbchannel.h"

class DvbManager;
class DvbRecordingFile;

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
	DvbSharedChannel channel;
	QDateTime begin; // UTC
	QDateTime end; // UTC, read-only
	QTime duration;
	int repeat; // (1 << 0) (monday) | (1 << 1) (tuesday) | ... | (1 << 6) (sunday)
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
	bool hasActiveRecordings() const;
	DvbSharedRecording findRecordingByKey(const SqlKey &sqlKey) const;
	QMap<SqlKey, DvbSharedRecording> getRecordings() const;
	DvbSharedRecording addRecording(DvbRecording &recording);
	void updateRecording(DvbSharedRecording recording, DvbRecording &modifiedRecording);
	void removeRecording(DvbSharedRecording recording);

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

	DvbManager *manager;
	QMap<SqlKey, DvbSharedRecording> recordings;
	QMap<SqlKey, QExplicitlySharedDataPointer<DvbRecordingFile> > recordingFiles;
	bool hasPendingOperation;
};

#endif /* DVBRECORDING_H */
