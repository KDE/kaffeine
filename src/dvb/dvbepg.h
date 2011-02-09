/*
 * dvbepg.h
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

#ifndef DVBEPG_H
#define DVBEPG_H

#include <QSharedData> // qt 4.5 compatibility
#include "dvbrecording.h"

class AtscEitEntry;
class AtscEpgFilter;
class DvbChannel;
class DvbDevice;
class DvbEitEntry;
class DvbEpgFilter;

class DvbEpgEntry
{
public:
	DvbEpgEntry() { }
	~DvbEpgEntry() { }

	QString channelName;
	QDateTime begin; // UTC
	QTime duration;
	QString title;
	QString subheading;
	QString details;
	DvbRecordingKey recordingKey;
};

class DvbEpgModel : public QObject
{
	Q_OBJECT
public:
	DvbEpgModel(DvbManager *manager_, QObject *parent);
	~DvbEpgModel();

	QList<const DvbEpgEntry *> getCurrentNext(const QString &channelName) const;
	void startEventFilter(DvbDevice *device, const DvbChannel *channel);
	void stopEventFilter(DvbDevice *device, const DvbChannel *channel);

	QList<DvbEpgEntry> getEntries() const;
	QString findChannelNameByEitEntry(const DvbEitEntry &eitEntry) const;
	QString findChannelNameByEitEntry(const AtscEitEntry &eitEntry) const;
	void addEntry(const DvbEpgEntry &entry);
	void scheduleProgram(const DvbEpgEntry *entry, int extraSecondsBefore,
		int extraSecondsAfter);

signals:
	void entryAdded(const DvbEpgEntry *entry);
	// entryChanged() doesn't change entry pointers (oldEntry is a temporary copy)
	void entryChanged(const DvbEpgEntry *entry, const DvbEpgEntry &oldEntry);
	void entryAboutToBeRemoved(const DvbEpgEntry *entry);

private slots:
	void channelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
	void channelLayoutChanged();
	void channelModelReset();
	void channelRowsInserted(const QModelIndex &parent, int start, int end);
	void channelRowsRemoved(const QModelIndex &parent, int start, int end);
	void programRemoved(const DvbRecordingKey &recordingKey);

private:
	void timerEvent(QTimerEvent *event);

	void addChannelEitMapping(const DvbChannel *channel);
	void removeChannelEitMapping(const DvbChannel *channel);

	DvbManager *manager;
	QList<DvbEpgEntry> entries;
	QMap<DvbRecordingKey, const DvbEpgEntry *> recordingKeyMapping;
	QList<QExplicitlySharedDataPointer<const DvbChannel> > channels;
	QHash<DvbEitEntry, const DvbChannel *> dvbEitMapping;
	QHash<AtscEitEntry, const DvbChannel *> atscEitMapping;
	QDateTime currentDateTimeUtc;
	QList<DvbEpgFilter *> dvbEpgFilters;
	QList<AtscEpgFilter *> atscEpgFilters;
	mutable bool hasPendingOperation;
};

#endif /* DVBEPG_H */
