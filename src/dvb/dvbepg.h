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

#include "dvbchannel.h"
#include "dvbrecording.h"

class AtscEitEntry;
class AtscEpgFilter;
class DvbDevice;
class DvbEitEntry;
class DvbEpgFilter;

class DvbEpgEntry
{
public:
	DvbEpgEntry() { }
	~DvbEpgEntry() { }

	// recordingKey is ignored in comparisons
	bool operator==(const DvbEpgEntry &other) const;
	bool operator<(const DvbEpgEntry &other) const;

	DvbSharedChannel channel;
	QDateTime begin; // UTC
	QTime duration;
	QString title;
	QString subheading;
	QString details;
	DvbRecordingKey recordingKey;
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

	QList<const DvbEpgEntry *> getCurrentNext(const DvbChannel *channel) const;
	void startEventFilter(DvbDevice *device, const DvbChannel *channel);
	void stopEventFilter(DvbDevice *device, const DvbChannel *channel);

	QMap<DvbEpgEntry, DvbEpgEmptyClass> getEntries() const;
	QHash<DvbSharedChannel, int> getEpgChannels() const;
	const DvbChannel *findChannelByEitEntry(const DvbEitEntry &eitEntry) const;
	const DvbChannel *findChannelByEitEntry(const AtscEitEntry &eitEntry) const;
	void addEntry(const DvbEpgEntry &entry);
	void scheduleProgram(const DvbEpgEntry *entry, int extraSecondsBefore,
		int extraSecondsAfter);

signals:
	void entryAdded(const DvbEpgEntry *entry);
	// oldEntry becomes invalid -after- emitting entryChanged()
	void entryChanged(const DvbEpgEntry *oldEntry, const DvbEpgEntry *newEntry);
	void entryAboutToBeRemoved(const DvbEpgEntry *entry);
	void epgChannelAdded(const DvbChannel *channel);
	void epgChannelAboutToBeRemoved(const DvbChannel *channel);

private slots:
	void channelAdded(const DvbChannel *channel);
	void channelChanged(const DvbChannel *oldChannel, const DvbChannel *newChannel);
	void channelAboutToBeRemoved(const DvbChannel *channel);
	void programRemoved(const DvbRecordingKey &recordingKey);

private:
	void timerEvent(QTimerEvent *event);

	void internalAddEntry(const DvbEpgEntry &entry);
	Iterator internalChangeEntry(Iterator it, const DvbEpgEntry &entry);
	Iterator internalRemoveEntry(Iterator it);
	void addChannelEitMapping(const DvbChannel *channel);
	void removeChannelEitMapping(const DvbChannel *channel);

	DvbManager *manager;
	QMap<DvbEpgEntry, DvbEpgEmptyClass> entries;
	QMap<DvbRecordingKey, const DvbEpgEntry *> recordingKeyMapping;
	QHash<DvbSharedChannel, int> epgChannels;
	QHash<DvbEitEntry, DvbSharedChannel> dvbEitMapping;
	QHash<AtscEitEntry, DvbSharedChannel> atscEitMapping;
	QDateTime currentDateTimeUtc;
	QList<DvbEpgFilter *> dvbEpgFilters;
	QList<AtscEpgFilter *> atscEpgFilters;
	mutable bool hasPendingOperation;
};

#endif /* DVBEPG_H */
