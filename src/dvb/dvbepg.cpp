/*
 * dvbepg.cpp
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

#include <QDataStream>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>

#include "../ensurenopendingoperation.h"
#include "dvbdevice.h"
#include "dvbepg.h"
#include "dvbepg_p.h"
#include "dvbmanager.h"
#include "dvbsi.h"

bool DvbEpgEntry::validate() const
{
	if (channel.isValid() && begin.isValid() && (begin.timeSpec() == Qt::UTC) &&
	    duration.isValid()) {
		return true;
	}

	return false;
}

bool DvbEpgEntryId::operator<(const DvbEpgEntryId &other) const
{
	if (entry->channel != other.entry->channel) {
		return (entry->channel < other.entry->channel);
	}

	if (entry->begin != other.entry->begin) {
		return (entry->begin < other.entry->begin);
	}
	return false;
}

DvbEpgModel::DvbEpgModel(DvbManager *manager_, QObject *parent) : QObject(parent),
	manager(manager_), hasPendingOperation(false)
{
	currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	startTimer(54000);

	DvbChannelModel *channelModel = manager->getChannelModel();
	connect(channelModel, SIGNAL(channelAboutToBeUpdated(DvbSharedChannel)),
		this, SLOT(channelAboutToBeUpdated(DvbSharedChannel)));
	connect(channelModel, SIGNAL(channelUpdated(DvbSharedChannel)),
		this, SLOT(channelUpdated(DvbSharedChannel)));
	connect(channelModel, SIGNAL(channelRemoved(DvbSharedChannel)),
		this, SLOT(channelRemoved(DvbSharedChannel)));
	connect(manager->getRecordingModel(), SIGNAL(recordingRemoved(DvbSharedRecording)),
		this, SLOT(recordingRemoved(DvbSharedRecording)));

	// TODO use SQL to store epg data

	QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("/epgdata.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		qCWarning(logEpg, "Cannot open %s", qPrintable(file.fileName()));
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);
	DvbRecordingModel *recordingModel = manager->getRecordingModel();
	bool hasRecordingKey = true, hasParental = true, hasMultilang = true;
	int version;
	stream >> version;

	if (version == 0x1ce0eca7) {
		hasRecordingKey = false;
	} else if (version == 0x79cffd36) {
		hasParental = false;
	} else if (version == 0x140c37b5) {
		hasMultilang = false;
	} else if (version != 0x20171105) {
		qCWarning(logEpg, "Wrong DB version for: %s", qPrintable(file.fileName()));
		return;
	}

	while (!stream.atEnd()) {
		DvbEpgEntry entry;
		QString channelName;
		stream >> channelName;
		entry.channel = channelModel->findChannelByName(channelName);
		stream >> entry.begin;
		entry.begin = entry.begin.toUTC();
		stream >> entry.duration;

		if (hasMultilang) {
			int i, count;

			stream >> count;

			for (i = 0; i < count; i++) {
				QString code;
				unsigned type;

				DvbEpgLangEntry langEntry;
				stream >> code;
				stream >> langEntry.title;
				stream >> langEntry.subheading;
				stream >> langEntry.details;

				stream >> type;
				stream >> entry.content;
				stream >> langEntry.parental;

				if (type <= DvbEpgEntry::EitLast)
					entry.type = DvbEpgEntry::EitType(type);
				else
					entry.type = DvbEpgEntry::EitActualTsSchedule;

				entry.langEntry[code] = langEntry;
			}


		} else {
			DvbEpgLangEntry langEntry;

			stream >> langEntry.title;
			stream >> langEntry.subheading;
			stream >> langEntry.details;

			entry.langEntry[FIRST_LANG] = langEntry;
		}

		if (hasRecordingKey) {
			SqlKey recordingKey;
			stream >> recordingKey.sqlKey;

			if (recordingKey.isSqlKeyValid()) {
				entry.recording = recordingModel->findRecordingByKey(recordingKey);
			}
		}

		if (hasParental && !hasMultilang) {
			unsigned tmp;

			stream >> tmp;
			stream >> entry.content;
			stream >> entry.langEntry[FIRST_LANG].parental;

			if (tmp <= DvbEpgEntry::EitLast)
				entry.type = DvbEpgEntry::EitType(tmp);
			else
				entry.type = DvbEpgEntry::EitActualTsSchedule;
		}

		if (stream.status() != QDataStream::Ok) {
			qCWarning(logEpg, "Corrupt data %s", qPrintable(file.fileName()));
			break;
		}

		addEntry(entry);
	}
}

DvbEpgModel::~DvbEpgModel()
{
	if (hasPendingOperation) {
		qCWarning(logEpg, "Illegal recursive call");
	}

	if (!dvbEpgFilters.isEmpty() || !atscEpgFilters.isEmpty()) {
		qCWarning(logEpg, "filter list not empty");
	}

	QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("/epgdata.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCWarning(logEpg, "Cannot open %s", qPrintable(file.fileName()));
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);
	int version = 0x20171105;
	stream << version;

	foreach (const DvbSharedEpgEntry &entry, entries) {
		SqlKey recordingKey;

		if (entry->recording.isValid()) {
			recordingKey = *entry->recording;
		}

		stream << entry->channel->name;
		stream << entry->begin;
		stream << entry->duration;

		stream << entry->langEntry.size();

		QHashIterator<QString, DvbEpgLangEntry> i(entry->langEntry);

		while (i.hasNext()) {
			i.next();

			stream << i.key();

			DvbEpgLangEntry langEntry = i.value();

			stream << langEntry.title;
			stream << langEntry.subheading;
			stream << langEntry.details;

			stream << int(entry->type);
			stream << entry->content;
			stream << langEntry.parental;
		}

		stream << recordingKey.sqlKey;
	}
}

QMap<DvbSharedRecording, DvbSharedEpgEntry> DvbEpgModel::getRecordings() const
{
	return recordings;
}

void DvbEpgModel::setRecordings(const QMap<DvbSharedRecording, DvbSharedEpgEntry> map)
{
	recordings = map;
}

QMap<DvbEpgEntryId, DvbSharedEpgEntry> DvbEpgModel::getEntries() const
{
	return entries;
}

QHash<DvbSharedChannel, int> DvbEpgModel::getEpgChannels() const
{
	return epgChannels;
}

QList<DvbSharedEpgEntry> DvbEpgModel::getCurrentNext(const DvbSharedChannel &channel) const
{
	QList<DvbSharedEpgEntry> result;
	DvbEpgEntry fakeEntry(channel);

	for (ConstIterator it = entries.lowerBound(DvbEpgEntryId(&fakeEntry));
	     it != entries.constEnd(); ++it) {
		const DvbSharedEpgEntry &entry = *it;

		if (entry->channel != channel) {
			break;
		}

		result.append(entry);

		if (result.size() == 2) {
			break;
		}
	}

	return result;
}

void DvbEpgModel::Debug(QString text, const DvbSharedEpgEntry &entry)
{
	if (!QLoggingCategory::defaultCategory()->isEnabled(QtDebugMsg))
		return;

	QDateTime begin = entry->begin.toLocalTime();
	QTime end = entry->begin.addSecs(QTime(0, 0, 0).secsTo(entry->duration)).toLocalTime().time();

	qCDebug(logEpg, "event %s: type %d, from %s to %s: %s: %s: %s : %s",
		qPrintable(text), entry->type, qPrintable(QLocale().toString(begin, QLocale::ShortFormat)), qPrintable(QLocale().toString(end)),
		qPrintable(entry->title()), qPrintable(entry->subheading()), qPrintable(entry->details()), qPrintable(entry->content));
}

DvbSharedEpgEntry DvbEpgModel::addEntry(const DvbEpgEntry &entry)
{
	if (!entry.validate()) {
		qCWarning(logEpg, "Invalid entry: channel is %s, begin is %s, duration is %s", entry.channel.isValid() ? "valid" : "invalid", entry.begin.isValid() ? "valid" : "invalid", entry.duration.isValid() ? "valid" : "invalid");
		return DvbSharedEpgEntry();
	}

	if (hasPendingOperation) {
		qCWarning(logEpg, "Iillegal recursive call");
		return DvbSharedEpgEntry();
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	// Check if the event was already recorded
	const QDateTime end = entry.begin.addSecs(QTime(0, 0, 0).secsTo(entry.duration));

	// Optimize duplicated register logic by using find, with is O(log n)
	Iterator it = entries.find(DvbEpgEntryId(&entry));
	while (it != entries.end()) {
		const DvbSharedEpgEntry &existingEntry = *it;

		// Don't do anything if the event already exists
		if (*existingEntry == entry)
			return DvbSharedEpgEntry();

		const QDateTime enEnd = existingEntry->begin.addSecs(QTime(0, 0, 0).secsTo(existingEntry->duration));

		// The logic here was simplified due to performance.
		// It won't check anymore if an event has its start time
		// switched, as that would require a O(n) loop, with is
		// too slow, specially on DVB-S/S2. So, we're letting the QMap
		// to use a key with just channel/begin time, identifying
		// obsolete entries only if the end time doesn't match.

		// A new event conflicts with an existing one
		if (end != enEnd) {
			Debug("removed", existingEntry);
			it = removeEntry(it);
			break;
		}
		// New event data for the same event
		if (existingEntry->details(FIRST_LANG).isEmpty() && !entry.details(FIRST_LANG).isEmpty()) {
			emit entryAboutToBeUpdated(existingEntry);

			QHashIterator<QString, DvbEpgLangEntry> i(entry.langEntry);

			while (i.hasNext()) {
				i.next();

				DvbEpgLangEntry langEntry = i.value();

				const_cast<DvbEpgEntry *>(existingEntry.constData())->langEntry[i.key()].details = langEntry.details;
			}
			emit entryUpdated(existingEntry);
			Debug("updated", existingEntry);
		}
		return existingEntry;
	}

	if (entry.begin.addSecs(QTime(0, 0, 0).secsTo(entry.duration)) > currentDateTimeUtc) {
		DvbSharedEpgEntry existingEntry = entries.value(DvbEpgEntryId(&entry));

		if (existingEntry.isValid()) {
			if (existingEntry->details(FIRST_LANG).isEmpty() && !entry.details(FIRST_LANG).isEmpty()) {
				// needed for atsc
				emit entryAboutToBeUpdated(existingEntry);

				QHashIterator<QString, DvbEpgLangEntry> i(entry.langEntry);

				while (i.hasNext()) {
					i.next();

					DvbEpgLangEntry langEntry = i.value();

					const_cast<DvbEpgEntry *>(existingEntry.constData())->langEntry[i.key()].details = langEntry.details;
				}
				emit entryUpdated(existingEntry);
				Debug("updated2", existingEntry);
			}

			return existingEntry;
		}

		DvbSharedEpgEntry newEntry(new DvbEpgEntry(entry));
		entries.insert(DvbEpgEntryId(newEntry), newEntry);

		if (newEntry->recording.isValid()) {
			recordings.insert(newEntry->recording, newEntry);
		}

		if (++epgChannels[newEntry->channel] == 1) {
			emit epgChannelAdded(newEntry->channel);
		}

		emit entryAdded(newEntry);
		Debug("new", newEntry);
		return newEntry;
	}

	return DvbSharedEpgEntry();
}

void DvbEpgModel::scheduleProgram(const DvbSharedEpgEntry &entry, int extraSecondsBefore,
	int extraSecondsAfter, bool checkForRecursion, int priority)
{
	if (!entry.isValid() || (entries.value(DvbEpgEntryId(entry)) != entry)) {
		qCWarning(logEpg, "Can't schedule program: invalid entry");
		return;
	}

	if (hasPendingOperation) {
		qCWarning(logEpg, "Illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	emit entryAboutToBeUpdated(entry);
	DvbSharedRecording oldRecording;

	if (!entry->recording.isValid()) {
		DvbRecording recording;
		recording.priority = priority;
		recording.name = entry->title(FIRST_LANG);
		recording.channel = entry->channel;
		recording.begin = entry->begin.addSecs(-extraSecondsBefore);
		recording.beginEPG = entry->begin;
		recording.duration =
			entry->duration.addSecs(extraSecondsBefore + extraSecondsAfter);
		recording.durationEPG =
			entry->duration;
		recording.subheading =
			entry->subheading(FIRST_LANG);
		recording.details =
			entry->details();
		recording.disabled = false;
		const_cast<DvbEpgEntry *>(entry.constData())->recording =
			manager->getRecordingModel()->addRecording(recording, checkForRecursion);
		recordings.insert(entry->recording, entry);
	} else {
		oldRecording = entry->recording;
		recordings.remove(entry->recording);
		const_cast<DvbEpgEntry *>(entry.constData())->recording = DvbSharedRecording();
	}

	emit entryUpdated(entry);

	if (oldRecording.isValid()) {
		// recordingRemoved() will be called
		hasPendingOperation = false;
		manager->getRecordingModel()->removeRecording(oldRecording);
	}
}

void DvbEpgModel::startEventFilter(DvbDevice *device, const DvbSharedChannel &channel)
{
	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
	case DvbTransponderBase::DvbT2:
	case DvbTransponderBase::IsdbT:
		dvbEpgFilters.append(QExplicitlySharedDataPointer<DvbEpgFilter>(
			new DvbEpgFilter(manager, device, channel)));
		break;
	case DvbTransponderBase::Atsc:
		atscEpgFilters.append(QExplicitlySharedDataPointer<AtscEpgFilter>(
			new AtscEpgFilter(manager, device, channel)));
		break;
	}
}

void DvbEpgModel::stopEventFilter(DvbDevice *device, const DvbSharedChannel &channel)
{
	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
	case DvbTransponderBase::DvbT2:
	case DvbTransponderBase::IsdbT:
		for (int i = 0; i < dvbEpgFilters.size(); ++i) {
			const DvbEpgFilter *epgFilter = dvbEpgFilters.at(i).constData();

			if ((epgFilter->device == device) &&
			    (epgFilter->source == channel->source) &&
			    (epgFilter->transponder.corresponds(channel->transponder))) {
				dvbEpgFilters.removeAt(i);
				break;
			}
		}

		break;
	case DvbTransponderBase::Atsc:
		for (int i = 0; i < atscEpgFilters.size(); ++i) {
			const AtscEpgFilter *epgFilter = atscEpgFilters.at(i).constData();

			if ((epgFilter->device == device) &&
			    (epgFilter->source == channel->source) &&
			    (epgFilter->transponder.corresponds(channel->transponder))) {
				atscEpgFilters.removeAt(i);
				break;
			}
		}

		break;
	}
}

void DvbEpgModel::channelAboutToBeUpdated(const DvbSharedChannel &channel)
{
	updatingChannel = *channel;
}

void DvbEpgModel::channelUpdated(const DvbSharedChannel &channel)
{
	if (hasPendingOperation) {
		qCWarning(logEpg, "Illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (DvbChannelId(channel) != DvbChannelId(&updatingChannel)) {
		DvbEpgEntry fakeEntry(channel);
		Iterator it = entries.lowerBound(DvbEpgEntryId(&fakeEntry));

		while ((ConstIterator(it) != entries.constEnd()) && ((*it)->channel == channel)) {
			it = removeEntry(it);
		}
	}
}

void DvbEpgModel::channelRemoved(const DvbSharedChannel &channel)
{
	if (hasPendingOperation) {
		qCWarning(logEpg, "Illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	DvbEpgEntry fakeEntry(channel);
	Iterator it = entries.lowerBound(DvbEpgEntryId(&fakeEntry));

	while ((ConstIterator(it) != entries.constEnd()) && ((*it)->channel == channel)) {
		it = removeEntry(it);
	}
}

void DvbEpgModel::recordingRemoved(const DvbSharedRecording &recording)
{
	if (hasPendingOperation) {
		qCWarning(logEpg, "Illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	DvbSharedEpgEntry entry = recordings.take(recording);

	if (entry.isValid()) {
		emit entryAboutToBeUpdated(entry);
		const_cast<DvbEpgEntry *>(entry.constData())->recording = DvbSharedRecording();
		emit entryUpdated(entry);
	}
}

void DvbEpgModel::timerEvent(QTimerEvent *event)
{
	Q_UNUSED(event)

	if (hasPendingOperation) {
		qCWarning(logEpg, "Illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	Iterator it = entries.begin();

	while (ConstIterator(it) != entries.constEnd()) {
		const DvbSharedEpgEntry &entry = *it;

		if (entry->begin.addSecs(QTime(0, 0, 0).secsTo(entry->duration)) > currentDateTimeUtc) {
			++it;
		} else {
			it = removeEntry(it);
		}
	}
}

DvbEpgModel::Iterator DvbEpgModel::removeEntry(Iterator it)
{
	const DvbSharedEpgEntry &entry = *it;

	if (entry->recording.isValid()) {
		recordings.remove(entry->recording);
	}

	if (--epgChannels[entry->channel] == 0) {
		epgChannels.remove(entry->channel);
		emit epgChannelRemoved(entry->channel);
	}

	emit entryRemoved(entry);
	return entries.erase(it);
}

DvbEpgFilter::DvbEpgFilter(DvbManager *manager_, DvbDevice *device_,
	const DvbSharedChannel &channel) : device(device_)
{
	manager = manager_;
	source = channel->source;
	transponder = channel->transponder;
	device->addSectionFilter(0x12, this);
	channelModel = manager->getChannelModel();
	epgModel = manager->getEpgModel();
}

DvbEpgFilter::~DvbEpgFilter()
{
	device->removeSectionFilter(0x12, this);
}

QTime DvbEpgFilter::bcdToTime(int bcd)
{
	return QTime(((bcd >> 20) & 0x0f) * 10 + ((bcd >> 16) & 0x0f),
		((bcd >> 12) & 0x0f) * 10 + ((bcd >> 8) & 0x0f),
		((bcd >> 4) & 0x0f) * 10 + (bcd & 0x0f));
}

static const QByteArray contentStr[16][16] = {
	[0] = {},
	[1] = {
			/* Movie/Drama */
		{},
		{I18N_NOOP("Detective")},
		{I18N_NOOP("Adventure")},
		{I18N_NOOP("Science Fiction")},
		{I18N_NOOP("Comedy")},
		{I18N_NOOP("Soap")},
		{I18N_NOOP("Romance")},
		{I18N_NOOP("Classical")},
		{I18N_NOOP("Adult")},
		{I18N_NOOP("User defined")},
	},
	[2] = {
			/* News/Current affairs */
		{},
		{I18N_NOOP("Weather")},
		{I18N_NOOP("Magazine")},
		{I18N_NOOP("Documentary")},
		{I18N_NOOP("Discussion")},
		{I18N_NOOP("User Defined")},
	},
	[3] = {
			/* Show/Game show */
		{},
		{I18N_NOOP("Quiz")},
		{I18N_NOOP("Variety")},
		{I18N_NOOP("Talk")},
		{I18N_NOOP("User Defined")},
	},
	[4] = {
			/* Sports */
		{},
		{I18N_NOOP("Events")},
		{I18N_NOOP("Magazine")},
		{I18N_NOOP("Football")},
		{I18N_NOOP("Tennis")},
		{I18N_NOOP("Team")},
		{I18N_NOOP("Athletics")},
		{I18N_NOOP("Motor")},
		{I18N_NOOP("Water")},
		{I18N_NOOP("Winter")},
		{I18N_NOOP("Equestrian")},
		{I18N_NOOP("Martial")},
		{I18N_NOOP("User Defined")},
	},
	[5] = {
			/* Children's/Youth */
		{},
		{I18N_NOOP("Preschool")},
		{I18N_NOOP("06 to 14")},
		{I18N_NOOP("10 to 16")},
		{I18N_NOOP("Educational")},
		{I18N_NOOP("Cartoons")},
		{I18N_NOOP("User Defined")},
	},
	[6] = {
			/* Music/Ballet/Dance */
		{},
		{I18N_NOOP("Poprock")},
		{I18N_NOOP("Classical")},
		{I18N_NOOP("Folk")},
		{I18N_NOOP("Jazz")},
		{I18N_NOOP("Opera")},
		{I18N_NOOP("Ballet")},
		{I18N_NOOP("User Defined")},
	},
	[7] = {
			/* Arts/Culture */
		{},
		{I18N_NOOP("Performance")},
		{I18N_NOOP("Fine Arts")},
		{I18N_NOOP("Religion")},
		{I18N_NOOP("Traditional")},
		{I18N_NOOP("Literature")},
		{I18N_NOOP("Cinema")},
		{I18N_NOOP("Experimental")},
		{I18N_NOOP("Press")},
		{I18N_NOOP("New Media")},
		{I18N_NOOP("Magazine")},
		{I18N_NOOP("Fashion")},
		{I18N_NOOP("User Defined")},
	},
	[8] = {
			/* Social/Political/Economics */
		{},
		{I18N_NOOP("Magazine")},
		{I18N_NOOP("Advisory")},
		{I18N_NOOP("People")},
		{I18N_NOOP("User Defined")},
	},
	[9] = {
			/* Education/Science/Factual */
		{},
		{I18N_NOOP("Nature")},
		{I18N_NOOP("Technology")},
		{I18N_NOOP("Medicine")},
		{I18N_NOOP("Foreign")},
		{I18N_NOOP("Social")},
		{I18N_NOOP("Further")},
		{I18N_NOOP("Language")},
		{I18N_NOOP("User Defined")},
	},
	[10] = {
			/* Leisure/Hobbies */
		{},
		{I18N_NOOP("Travel")},
		{I18N_NOOP("Handicraft")},
		{I18N_NOOP("Motoring")},
		{I18N_NOOP("Fitness")},
		{I18N_NOOP("Cooking")},
		{I18N_NOOP("Shopping")},
		{I18N_NOOP("Gardening")},
		{I18N_NOOP("User Defined")},
	},
	[11] = {
			/* Special characteristics */
		{I18N_NOOP("Original Language")},
		{I18N_NOOP("Black and White ")},
		{I18N_NOOP("Unpublished")},
		{I18N_NOOP("Live")},
		{I18N_NOOP("Planostereoscopic")},
		{I18N_NOOP("User Defined")},
		{I18N_NOOP("User Defined 1")},
		{I18N_NOOP("User Defined 2")},
		{I18N_NOOP("User Defined 3")},
		{I18N_NOOP("User Defined 4")}
	}
};

static const QByteArray nibble1Str[16] = {
	[0]  = {I18N_NOOP("Undefined")},
	[1]  = {I18N_NOOP("Movie")},
	[2]  = {I18N_NOOP("News")},
	[3]  = {I18N_NOOP("Show")},
	[4]  = {I18N_NOOP("Sports")},
	[5]  = {I18N_NOOP("Children")},
	[6]  = {I18N_NOOP("Music")},
	[7]  = {I18N_NOOP("Culture")},
	[8]  = {I18N_NOOP("Social")},
	[9]  = {I18N_NOOP("Education")},
	[10] = {I18N_NOOP("Leisure")},
	[11] = {I18N_NOOP("Special")},
	[12] = {I18N_NOOP("Reserved")},
	[13] = {I18N_NOOP("Reserved")},
	[14] = {I18N_NOOP("Reserved")},
	[15] = {I18N_NOOP("User defined")},
};

static const QByteArray braNibble1Str[16] = {
	[0]  = {I18N_NOOP("News")},
	[1]  = {I18N_NOOP("Sports")},
	[2]  = {I18N_NOOP("Education")},
	[3]  = {I18N_NOOP("Soap opera")},
	[4]  = {I18N_NOOP("Mini-series")},
	[5]  = {I18N_NOOP("Series")},
	[6]  = {I18N_NOOP("Variety")},
	[7]  = {I18N_NOOP("Reality show")},
	[8]  = {I18N_NOOP("Information")},
	[9]  = {I18N_NOOP("Comical")},
	[10] = {I18N_NOOP("Children")},
	[11] = {I18N_NOOP("Erotic")},
	[12] = {I18N_NOOP("Movie")},
	[13] = {I18N_NOOP("Raffle, television sales, prizing")},
	[14] = {I18N_NOOP("Debate/interview")},
	[15] = {I18N_NOOP("Other")},
};

// Using the terms from the English version of NBR 15603-2:2007
// The table omits nibble2="Other", as it is better to show nibble 1
// definition instead.
// when nibble2[x][0] == nibble1[x] and it has no other definition,
// except for "Other", the field will be kept in blank, as the logic
// will fall back to the definition at nibble 1.
static QByteArray braNibble2Str[16][16] = {
	[0] = {
		{I18N_NOOP("News")},
		{I18N_NOOP("Report")},
		{I18N_NOOP("Documentary")},
		{I18N_NOOP("Biography")},
	},
	[1] = {},
	[2] = {
		{I18N_NOOP("Educative")},
	},
	[3] = {},
	[4] = {},
	[5] = {},
	[6] = {
		{I18N_NOOP("Auditorium")},
		{I18N_NOOP("Show")},
		{I18N_NOOP("Musical")},
		{I18N_NOOP("Making of")},
		{I18N_NOOP("Feminine")},
		{I18N_NOOP("Game show")},
	},
	[7] = {},
	[8] = {
		{I18N_NOOP("Cooking")},
		{I18N_NOOP("Fashion")},
		{I18N_NOOP("Country")},
		{I18N_NOOP("Health")},
		{I18N_NOOP("Travel")},
	},
	[9] = {},
	[10] = {},
	[11] = {},
	[12] = {},
	[13] = {
		{I18N_NOOP("Raffle")},
		{I18N_NOOP("Television sales")},
		{I18N_NOOP("Prizing")},
	},
	[14] = {
		{I18N_NOOP("Discussion")},
		{I18N_NOOP("Interview")},
	},
	[15] = {
		{I18N_NOOP("Adult cartoon")},
		{I18N_NOOP("Interactive")},
		{I18N_NOOP("Policy")},
		{I18N_NOOP("Religion")},
	},
};

QString DvbEpgFilter::getContent(DvbContentDescriptor &descriptor)
{
	QString content;

	for (DvbEitContentEntry entry = descriptor.contents(); entry.isValid(); entry.advance()) {
		const int nibble1 = entry.contentNibbleLevel1();
		const int nibble2 = entry.contentNibbleLevel2();
		QByteArray s;

		// FIXME: should do it only for ISDB-Tb (Brazilian variation),
		// as the Japanese variation uses the same codes as DVB
		if (transponder.getTransmissionType() == DvbTransponderBase::IsdbT) {
			s = braNibble2Str[nibble1][nibble2];
			if (s == "")
				s = braNibble1Str[nibble1];
			if (s != "")
				content += i18n(s) + "\n";
		} else {
			s = contentStr[nibble1][nibble2];
			if (s == "")
				s = nibble1Str[nibble1];
			if (s != "")
				content += i18n(s) + "\n";
		}
	}

	if (content != "") {
		// xgettext:no-c-format
		return (i18n("Genre: %1", content));
	}
	return content;
}

/* As defined at ABNT NBR 15603-2 */
static const QByteArray braRating[] = {
	[0] = {I18N_NOOP("reserved")},
	[1] = {I18N_NOOP("all audiences")},
	[2] = {I18N_NOOP("10 years")},
	[3] = {I18N_NOOP("12 years")},
	[4] = {I18N_NOOP("14 years")},
	[5] = {I18N_NOOP("16 years")},
	[6] = {I18N_NOOP("18 years")},
};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

QString DvbEpgFilter::getParental(QString code, DvbParentalRatingEntry &entry)
{
	QString parental;


	// Rating from 0x10 to 0xff are broadcaster's specific
	if (entry.rating() == 0) {
		// xgettext:no-c-format
		parental += i18n("not rated\n");
	} else if (entry.rating() < 0x10) {
		if (code == "BRA" && transponder.getTransmissionType() == DvbTransponderBase::IsdbT) {
			unsigned int rating = entry.rating();

			if (rating >= ARRAY_SIZE(braRating))
				rating = 0;	// Reserved

			QString GenStr;
			int genre = entry.rating() >> 4;

			if (genre & 0x2)
				GenStr = i18n("violence / ");
			if (genre & 0x4)
				GenStr = i18n("sex / ");
			if (genre & 0x1)
				GenStr = i18n("drugs / ");
			if (genre) {
				GenStr.truncate(GenStr.size() - 2);
				GenStr = " (" + GenStr + ")";
			}

			QString ratingStr = i18n(braRating[entry.rating()]);
			// xgettext:no-c-format
			parental += i18n("Parental rate: %2%3\n", ratingStr, GenStr);
		} else {
			// xgettext:no-c-format
			parental += i18n("Parental rate: %2 years.\n", entry.rating() + 3);
		}
	}

	return parental;
}

DvbEpgLangEntry *DvbEpgFilter::getLangEntry(DvbEpgEntry &epgEntry,
					    int code1, int code2, int code3,
					    QString *code_)
{
	DvbEpgLangEntry *langEntry;
	QString code;

	if (!code1 || code1 == 0x20)
		code = FIRST_LANG;
	else {
		code.append(QChar(code1));
		code.append(QChar(code2));
		code.append(QChar(code3));
		code = code.toUpper();
	}
	if (code_)
		code_ = new QString(code);

	if (!epgEntry.langEntry.contains(code)) {
		DvbEpgLangEntry e;
		epgEntry.langEntry.insert(code, e);
		if (!manager->languageCodes.contains(code))
			manager->languageCodes[code] = true;
	}
	langEntry = &epgEntry.langEntry[code];

	return langEntry;
}


void DvbEpgFilter::processSection(const char *data, int size)
{
	unsigned char tableId = data[0];

	if ((tableId < 0x4e) || (tableId > 0x6f)) {
		return;
	}

	DvbEitSection eitSection(data, size);

	if (!eitSection.isValid()) {
		qCDebug(logEpg, "section is invalid");
		return;
	}

	DvbChannel fakeChannel;
	fakeChannel.source = source;
	fakeChannel.transponder = transponder;
	fakeChannel.networkId = eitSection.originalNetworkId();
	fakeChannel.transportStreamId = eitSection.transportStreamId();
	fakeChannel.serviceId = eitSection.serviceId();
	DvbSharedChannel channel = channelModel->findChannelById(fakeChannel);

	if (!channel.isValid()) {
		fakeChannel.networkId = -1;
		channel = channelModel->findChannelById(fakeChannel);
	}

	if (!channel.isValid()) {
		qCDebug(logEpg, "channel invalid");
		return;
	}

	if (eitSection.entries().getLength())
		qCDebug(logEpg, "table 0x%02x, extension 0x%04x, session %d/%d, size %d", eitSection.tableId(), eitSection.tableIdExtension(), eitSection.sectionNumber(), eitSection.lastSectionNumber(), eitSection.entries().getLength());

	for (DvbEitSectionEntry entry = eitSection.entries(); entry.isValid(); entry.advance()) {
		DvbEpgEntry epgEntry;
		DvbEpgLangEntry *langEntry;

		if (tableId == 0x4e)
			epgEntry.type = DvbEpgEntry::EitActualTsPresentFollowing;
		else if (tableId == 0x4f)
			epgEntry.type = DvbEpgEntry::EitOtherTsPresentFollowing;
		else if (tableId < 0x60)
			epgEntry.type = DvbEpgEntry::EitActualTsSchedule;
		else
			epgEntry.type = DvbEpgEntry::EitOtherTsSchedule;

		epgEntry.channel = channel;

		/*
		 * ISDB-T Brazil uses time in UTC-3,
		 * as defined by ABNT NBR 15603-2:2007.
		 */
		if (channel->transponder.getTransmissionType() == DvbTransponderBase::IsdbT)
			epgEntry.begin = QDateTime(QDate::fromJulianDay(entry.startDate() + 2400001),
						   bcdToTime(entry.startTime()), Qt::OffsetFromUTC, -10800).toUTC();
		else
			epgEntry.begin = QDateTime(QDate::fromJulianDay(entry.startDate() + 2400001),
						   bcdToTime(entry.startTime()), Qt::UTC);
		epgEntry.duration = bcdToTime(entry.duration());

		for (DvbDescriptor descriptor = entry.descriptors(); descriptor.isValid();
		     descriptor.advance()) {
			switch (descriptor.descriptorTag()) {
			case 0x4d: {
				DvbShortEventDescriptor eventDescriptor(descriptor);

				if (!eventDescriptor.isValid()) {
					break;
				}

				langEntry = getLangEntry(epgEntry,
					     eventDescriptor.languageCode1(),
					     eventDescriptor.languageCode2(),
					     eventDescriptor.languageCode3());

				langEntry->title += eventDescriptor.eventName();
				langEntry->subheading += eventDescriptor.text();

				break;
			    }
			case 0x4e: {
				DvbExtendedEventDescriptor eventDescriptor(descriptor);

				if (!eventDescriptor.isValid()) {
					break;
				}

				langEntry = getLangEntry(epgEntry,
					     eventDescriptor.languageCode1(),
					     eventDescriptor.languageCode2(),
					     eventDescriptor.languageCode3());
				langEntry->details += eventDescriptor.text();
				break;
			    }
			case 0x54: {
				DvbContentDescriptor eventDescriptor(descriptor);

				if (!eventDescriptor.isValid()) {
					break;
				}

				epgEntry.content += getContent(eventDescriptor);
				break;
			    }
			case 0x55: {
				DvbParentalRatingDescriptor eventDescriptor(descriptor);

				if (!eventDescriptor.isValid()) {
					break;
				}

				for (DvbParentalRatingEntry entry = eventDescriptor.contents(); entry.isValid(); entry.advance()) {
					QString code;
					langEntry = getLangEntry(epgEntry,
						     entry.languageCode1(),
						     entry.languageCode2(),
						     entry.languageCode3(),
						     &code);
					langEntry->parental += getParental(code, entry);
				}
				break;
			    }
			}
		}

		epgModel->addEntry(epgEntry);
	}
}

void AtscEpgMgtFilter::processSection(const char *data, int size)
{
	epgFilter->processMgtSection(data, size);
}

void AtscEpgEitFilter::processSection(const char *data, int size)
{
	epgFilter->processEitSection(data, size);
}

void AtscEpgEttFilter::processSection(const char *data, int size)
{
	epgFilter->processEttSection(data, size);
}

AtscEpgFilter::AtscEpgFilter(DvbManager *manager, DvbDevice *device_,
	const DvbSharedChannel &channel) : device(device_), mgtFilter(this), eitFilter(this),
	ettFilter(this)
{
	source = channel->source;
	transponder = channel->transponder;
	device->addSectionFilter(0x1ffb, &mgtFilter);
	channelModel = manager->getChannelModel();
	epgModel = manager->getEpgModel();
}

AtscEpgFilter::~AtscEpgFilter()
{
	foreach (int pid, eitPids) {
		device->removeSectionFilter(pid, &eitFilter);
	}

	foreach (int pid, ettPids) {
		device->removeSectionFilter(pid, &ettFilter);
	}

	device->removeSectionFilter(0x1ffb, &mgtFilter);
}

void AtscEpgFilter::processMgtSection(const char *data, int size)
{
	unsigned char tableId = data[0];

	if (tableId != 0xc7) {
		return;
	}

	AtscMgtSection mgtSection(data, size);

	if (!mgtSection.isValid()) {
		return;
	}

	int entryCount = mgtSection.entryCount();
	QList<int> newEitPids;
	QList<int> newEttPids;

	AtscMgtSectionEntry entry = mgtSection.entries();
	for (int i = 0; i < entryCount; i++) {
		if (!entry.isValid())
			break;

		int tableType = entry.tableType();

		if ((tableType >= 0x0100) && (tableType <= 0x017f)) {
			int pid = entry.pid();
			int index = (qLowerBound(newEitPids, pid) - newEitPids.constBegin());

			if ((index >= newEitPids.size()) || (newEitPids.at(index) != pid)) {
				newEitPids.insert(index, pid);
			}
		}

		if ((tableType >= 0x0200) && (tableType <= 0x027f)) {
			int pid = entry.pid();
			int index = (qLowerBound(newEttPids, pid) - newEttPids.constBegin());

			if ((index >= newEttPids.size()) || (newEttPids.at(index) != pid)) {
				newEttPids.insert(index, pid);
			}
		}
		if (i < entryCount - 1)
			entry.advance();
	}

	for (int i = 0; i < eitPids.size(); ++i) {
		int pid = eitPids.at(i);
		int index = (qBinaryFind(newEitPids, pid) - newEitPids.constBegin());

		if (index < newEitPids.size()) {
			newEitPids.removeAt(index);
		} else {
			device->removeSectionFilter(pid, &eitFilter);
			eitPids.removeAt(i);
			--i;
		}
	}

	for (int i = 0; i < ettPids.size(); ++i) {
		int pid = ettPids.at(i);
		int index = (qBinaryFind(newEttPids, pid) - newEttPids.constBegin());

		if (index < newEttPids.size()) {
			newEttPids.removeAt(index);
		} else {
			device->removeSectionFilter(pid, &ettFilter);
			ettPids.removeAt(i);
			--i;
		}
	}

	for (int i = 0; i < newEitPids.size(); ++i) {
		int pid = newEitPids.at(i);
		eitPids.append(pid);
		device->addSectionFilter(pid, &eitFilter);
	}

	for (int i = 0; i < newEttPids.size(); ++i) {
		int pid = newEttPids.at(i);
		ettPids.append(pid);
		device->addSectionFilter(pid, &ettFilter);
	}
}

void AtscEpgFilter::processEitSection(const char *data, int size)
{
	unsigned char tableId = data[0];

	if (tableId != 0xcb) {
		return;
	}

	AtscEitSection eitSection(data, size);

	if (!eitSection.isValid()) {
		qCDebug(logEpg, "section is invalid");
		return;
	}

	DvbChannel fakeChannel;
	fakeChannel.source = source;
	fakeChannel.transponder = transponder;
	fakeChannel.networkId = eitSection.sourceId();
	DvbSharedChannel channel = channelModel->findChannelById(fakeChannel);

	if (!channel.isValid()) {
		qCDebug(logEpg, "channel is invalid");
		return;
	}

	qCDebug(logEpg, "Processing EIT section with size %d", size);

	int entryCount = eitSection.entryCount();
	// 1980-01-06T000000 minus 15 secs (= UTC - GPS in 2011)
	QDateTime baseDateTime = QDateTime(QDate(1980, 1, 5), QTime(23, 59, 45), Qt::UTC);

	AtscEitSectionEntry eitEntry = eitSection.entries();
	for (int i = 0; i < entryCount; i++) {
		if (!eitEntry.isValid())
			break;
		DvbEpgEntry epgEntry;
		epgEntry.channel = channel;
		epgEntry.begin = baseDateTime.addSecs(eitEntry.startTime());
		epgEntry.duration = QTime(0, 0, 0).addSecs(eitEntry.duration());


		DvbEpgLangEntry *langEntry;

		if (epgEntry.langEntry.contains(FIRST_LANG))
			langEntry = &epgEntry.langEntry[FIRST_LANG];
		else
			langEntry = new(DvbEpgLangEntry);

		langEntry->title = eitEntry.title();

		quint32 id = ((quint32(fakeChannel.networkId) << 16) | quint32(eitEntry.eventId()));
		DvbSharedEpgEntry entry = epgEntries.value(id);

		entry = epgModel->addEntry(epgEntry);
		epgEntries.insert(id, entry);
		if ( i < entryCount -1)
			eitEntry.advance();
	}
}

void AtscEpgFilter::processEttSection(const char *data, int size)
{
	unsigned char tableId = data[0];

	if (tableId != 0xcc) {
		return;
	}

	AtscEttSection ettSection(data, size);

	if (!ettSection.isValid() || (ettSection.messageType() != 0x02)) {
		return;
	}

	quint32 id = ((quint32(ettSection.sourceId()) << 16) | quint32(ettSection.eventId()));
	DvbSharedEpgEntry entry = epgEntries.value(id);

	if (entry.isValid()) {
		QString details = ettSection.text();

		if (entry->details() != details) {
			DvbEpgEntry modifiedEntry = *entry;

			DvbEpgLangEntry *langEntry;

			if (modifiedEntry.langEntry.contains(FIRST_LANG))
				langEntry = &modifiedEntry.langEntry[FIRST_LANG];
			else
				langEntry = new(DvbEpgLangEntry);

			langEntry->details = details;
			entry = epgModel->addEntry(modifiedEntry);
			epgEntries.insert(id, entry);
		}
	}
}
