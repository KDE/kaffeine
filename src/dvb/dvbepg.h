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

class DvbEpgEntry
{
public:
	DvbEpgEntry() { }
	explicit DvbEpgEntry(const DvbSharedChannel &channel_) : channel(channel_) { }
	~DvbEpgEntry() { }

	// recording is ignored in comparisons
	bool operator==(const DvbEpgEntry &other) const;
	bool operator<(const DvbEpgEntry &other) const;

	DvbSharedChannel channel;
	QDateTime begin; // UTC
	QTime duration;
	QString title;
	QString subheading;
	QString details;
	DvbSharedRecording recording;
};

class DvbEpgEmptyClass
{
};

class DvbEpgModel : public QObject
{
	Q_OBJECT
	typedef QMap<DvbEpgEntry, DvbEpgEmptyClass>::ConstIterator ConstIterator;
	typedef QMap<DvbEpgEntry, DvbEpgEmptyClass>::Iterator Iterator;
public:
	DvbEpgModel(DvbManager *manager_, QObject *parent);
	~DvbEpgModel();

	QList<const DvbEpgEntry *> getCurrentNext(const DvbSharedChannel &channel) const;
	void startEventFilter(DvbDevice *device, const DvbChannel *channel);
	void stopEventFilter(DvbDevice *device, const DvbChannel *channel);

	QMap<DvbEpgEntry, DvbEpgEmptyClass> getEntries() const;
	QHash<DvbSharedChannel, int> getEpgChannels() const;
	void addEntry(const DvbEpgEntry &entry);
	void scheduleProgram(const DvbEpgEntry *entry, int extraSecondsBefore,
		int extraSecondsAfter);

signals:
	void entryAdded(const DvbEpgEntry *entry);
	// oldEntry is a temporary copy (entry remains the same)
	void entryUpdated(const DvbEpgEntry *entry, const DvbEpgEntry &oldEntry);
	void entryRemoved(const DvbEpgEntry *entry);
	void epgChannelAdded(const DvbSharedChannel &channel);
	void epgChannelRemoved(const DvbSharedChannel &channel);

private slots:
	void channelUpdated(const DvbSharedChannel &channel, const DvbChannel &oldChannel);
	void channelRemoved(const DvbSharedChannel &channel);
	void recordingRemoved(const DvbSharedRecording &recording);

private:
	void timerEvent(QTimerEvent *event);

	void internalAddEntry(const DvbEpgEntry &entry);
	void internalEntryUpdated(const DvbEpgEntry *entry, const DvbEpgEntry &oldEntry);
	Iterator internalRemoveEntry(Iterator it);

	DvbManager *manager;
	QDateTime currentDateTimeUtc;
	QMap<DvbEpgEntry, DvbEpgEmptyClass> entries;
	QMap<DvbSharedRecording, const DvbEpgEntry *> recordingMapping;
	QHash<DvbSharedChannel, int> epgChannels;
	QList<DvbEpgFilter *> dvbEpgFilters;
	QList<AtscEpgFilter *> atscEpgFilters;
	mutable bool hasPendingOperation;
};

#endif /* DVBEPG_H */
