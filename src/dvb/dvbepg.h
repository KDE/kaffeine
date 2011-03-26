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

#include "dvbrecording.h"

class AtscEpgFilter;
class DvbDevice;
class DvbEpgFilter;

class DvbEpgEntry : public SharedData
{
public:
	DvbEpgEntry() { }
	explicit DvbEpgEntry(const DvbSharedChannel &channel_) : channel(channel_) { }
	~DvbEpgEntry() { }

	// checks that all variables are ok
	bool validate() const;

	DvbSharedChannel channel;
	QDateTime begin; // UTC
	QTime duration;
	QString title;
	QString subheading;
	QString details;
	DvbSharedRecording recording;
};

typedef ExplicitlySharedDataPointer<const DvbEpgEntry> DvbSharedEpgEntry;
Q_DECLARE_TYPEINFO(DvbSharedEpgEntry, Q_MOVABLE_TYPE);

class DvbEpgEntryId
{
public:
	explicit DvbEpgEntryId(const DvbEpgEntry *entry_) : entry(entry_) { }
	explicit DvbEpgEntryId(const DvbSharedEpgEntry &entry_) : entry(entry_.constData()) { }
	~DvbEpgEntryId() { }

	// compares entries, 'recording' is ignored
	// if one 'details' is empty, 'details' is ignored

	bool operator<(const DvbEpgEntryId &other) const;

private:
	const DvbEpgEntry *entry;
};

class DvbEpgModel : public QObject
{
	Q_OBJECT
	typedef QMap<DvbEpgEntryId, DvbSharedEpgEntry>::Iterator Iterator;
	typedef QMap<DvbEpgEntryId, DvbSharedEpgEntry>::ConstIterator ConstIterator;
public:
	DvbEpgModel(DvbManager *manager_, QObject *parent);
	~DvbEpgModel();

	QMap<DvbEpgEntryId, DvbSharedEpgEntry> getEntries() const;
	QHash<DvbSharedChannel, int> getEpgChannels() const;
	QList<DvbSharedEpgEntry> getCurrentNext(const DvbSharedChannel &channel) const;

	DvbSharedEpgEntry addEntry(const DvbEpgEntry &entry);
	void scheduleProgram(const DvbSharedEpgEntry &entry, int extraSecondsBefore,
		int extraSecondsAfter);

	void startEventFilter(DvbDevice *device, const DvbSharedChannel &channel);
	void stopEventFilter(DvbDevice *device, const DvbSharedChannel &channel);

signals:
	void entryAdded(const DvbSharedEpgEntry &entry);
	// updating doesn't change the entry pointer (modifies existing content)
	void entryAboutToBeUpdated(const DvbSharedEpgEntry &entry);
	void entryUpdated(const DvbSharedEpgEntry &entry);
	void entryRemoved(const DvbSharedEpgEntry &entry);
	void epgChannelAdded(const DvbSharedChannel &channel);
	void epgChannelRemoved(const DvbSharedChannel &channel);

private slots:
	void channelAboutToBeUpdated(const DvbSharedChannel &channel);
	void channelUpdated(const DvbSharedChannel &channel);
	void channelRemoved(const DvbSharedChannel &channel);
	void recordingRemoved(const DvbSharedRecording &recording);

private:
	void timerEvent(QTimerEvent *event);

	Iterator removeEntry(Iterator it);

	DvbManager *manager;
	QDateTime currentDateTimeUtc;
	QMap<DvbEpgEntryId, DvbSharedEpgEntry> entries;
	QMap<DvbSharedRecording, DvbSharedEpgEntry> recordings;
	QHash<DvbSharedChannel, int> epgChannels;
	QList<QExplicitlySharedDataPointer<DvbEpgFilter> > dvbEpgFilters;
	QList<QExplicitlySharedDataPointer<AtscEpgFilter> > atscEpgFilters;
	DvbChannel updatingChannel;
	bool hasPendingOperation;
};

#endif /* DVBEPG_H */
