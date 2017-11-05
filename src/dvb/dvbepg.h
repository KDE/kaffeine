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

#define FIRST_LANG "first"

class DvbEpgLangEntry
{
public:
	QString title;
	QString subheading;
	QString details;
	QString parental;
};

class DvbEpgEntry : public SharedData
{
public:
	enum EitType {
		EitActualTsPresentFollowing = 0,
		EitOtherTsPresentFollowing = 1,
		EitActualTsSchedule = 2,
		EitOtherTsSchedule = 3,

		EitLast = 3
	};
	DvbEpgEntry(): type(EitActualTsSchedule) { }
	explicit DvbEpgEntry(const DvbSharedChannel &channel_) : channel(channel_) { }
	~DvbEpgEntry() { }

	// checks that all variables are ok
	bool validate() const;

	DvbSharedChannel channel;
	EitType type;
	QDateTime begin; // UTC
	QTime duration;
	QString content;

	QHash<QString, DvbEpgLangEntry> langEntry;

	DvbSharedRecording recording;

	QString title(QString lang = QString()) const {
		QString s;

		if (!lang.isEmpty() && lang != FIRST_LANG)
			return langEntry[lang].title;

		QHashIterator<QString, DvbEpgLangEntry> i(langEntry);
		bool first = true;

		while (i.hasNext()) {
			i.next();

			QString code = i.key();
			DvbEpgLangEntry entry = i.value();

			if (!entry.title.isEmpty()) {
				if (first)
					first = false;
				else
					s += "/";

				if (lang != FIRST_LANG) {
					s += code;
					s += ": ";
				}
				s += entry.title;
			}

			if (lang == FIRST_LANG)
				break;
		}
		return s;
	}

	QString subheading(QString lang = QString()) const {
		QString s;

		if (!lang.isEmpty() && lang != FIRST_LANG)
			return langEntry[lang].subheading;

		QHashIterator<QString, DvbEpgLangEntry> i(langEntry);
		bool first = true;

		while (i.hasNext()) {
			i.next();

			QString code = i.key();
			DvbEpgLangEntry entry = i.value();

			if (!entry.subheading.isEmpty()) {
				if (first)
					first = false;
				else
					s += "/";

				if (lang != FIRST_LANG) {
					s += code;
					s += ": ";
				}
				s += entry.subheading;
			}

			if (lang == FIRST_LANG)
				break;
		}
		return s;
	}

	QString details(QString lang = QString()) const {
		QString s;

		if (!lang.isEmpty() && lang != FIRST_LANG)
			return langEntry[lang].details;

		QHashIterator<QString, DvbEpgLangEntry> i(langEntry);
		bool first = true;

		while (i.hasNext()) {
			i.next();

			QString code = i.key();
			DvbEpgLangEntry entry = i.value();

			if (!entry.details.isEmpty()) {
				if (first)
					first = false;
				else
					s += "\n\n";

				if (lang != FIRST_LANG) {
					s += code;
					s += ": ";
				}
				s += entry.details;
			}

			if (lang == FIRST_LANG)
				break;

			s += "\n\n";
		}
		return s;
	}

	QString parental(QString lang = QString()) const {
		QString s;

		if (!lang.isEmpty() && lang != FIRST_LANG)
			return langEntry[lang].parental;

		QHashIterator<QString, DvbEpgLangEntry> i(langEntry);
		bool first = true;

		while (i.hasNext()) {
			i.next();

			QString code = i.key();
			DvbEpgLangEntry entry = i.value();

			if (!entry.parental.isEmpty()) {
				if (first)
					first = false;
				else
					s += " / ";

				if (lang != FIRST_LANG) {
					s += code;
					s += ": ";
				}
				s += entry.parental;
			}

			if (lang == FIRST_LANG)
				break;

			s += "\n\n";
		}
		return s;
	}

	// Check only the user-visible elements
	bool operator==(const DvbEpgEntry &other) const
	{
		if (channel != other.channel)
			return false;
		if (begin != other.begin)
			return false;
		if (duration != other.duration)
			return false;
		if (content != other.content)
			return false;

		QHashIterator<QString, DvbEpgLangEntry> i(langEntry);
		while (i.hasNext()) {
			i.next();

			QString code = i.key();

			if (!other.langEntry.contains(code))
				return false;

			DvbEpgLangEntry thisEntry = i.value();
			DvbEpgLangEntry otherEntry = other.langEntry[code];

			if (thisEntry.title != otherEntry.title)
				return false;
			if (thisEntry.subheading != otherEntry.subheading)
				return false;
			if (thisEntry.details != otherEntry.details)
				return false;
			if (thisEntry.parental != otherEntry.parental)
				return false;

			// If first language matches, assume entries are identical
			return true;
		}

		return true;
	}
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
	QMap<DvbSharedRecording, DvbSharedEpgEntry> getRecordings() const;
	void setRecordings(const QMap<DvbSharedRecording, DvbSharedEpgEntry> map);
	QHash<DvbSharedChannel, int> getEpgChannels() const;
	QList<DvbSharedEpgEntry> getCurrentNext(const DvbSharedChannel &channel) const;

	DvbSharedEpgEntry addEntry(const DvbEpgEntry &entry);
	void scheduleProgram(const DvbSharedEpgEntry &entry, int extraSecondsBefore,
		int extraSecondsAfter, bool checkForRecursion=false, int priority=10);

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
	void Debug(QString text, const DvbSharedEpgEntry &entry);

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
