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

#include <KLazyLocalizedString>

#include <QDataStream>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>

#include "../ensurenopendingoperation.h"
#include "../iso-codes.h"
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

	QFile file(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QLatin1String("/epgdata.dvb"));

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
	} else if (version != 0x20171112) {
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

				DvbEpgLangEntry langEntry;
				stream >> code;
				stream >> langEntry.title;
				stream >> langEntry.subheading;
				stream >> langEntry.details;

				entry.langEntry[code] = langEntry;

				if (!langEntry.title.isEmpty() && !manager->languageCodes.contains(code))
					manager->languageCodes[code] = true;
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

		if (hasParental) {
			unsigned type;

			stream >> type;
			stream >> entry.content;
			stream >> entry.parental;

			if (type <= DvbEpgEntry::EitLast)
				entry.type = DvbEpgEntry::EitType(type);
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

	QFile file(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QLatin1String("/epgdata.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCWarning(logEpg, "Cannot open %s", qPrintable(file.fileName()));
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);
	int version = 0x20171112;
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
		}

		stream << recordingKey.sqlKey;
		stream << int(entry->type);
		stream << entry->content;
		stream << entry->parental;
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
		qCWarning(logEpg, "Illegal recursive call");
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
		recording.name = entry->title(manager->currentEpgLanguage);
		recording.channel = entry->channel;
		recording.begin = entry->begin.addSecs(-extraSecondsBefore);
		recording.beginEPG = entry->begin;
		recording.duration =
			entry->duration.addSecs(extraSecondsBefore + extraSecondsAfter);
		recording.durationEPG =
			entry->duration;
		recording.subheading =
			entry->subheading(manager->currentEpgLanguage);
		recording.details =
			entry->details(manager->currentEpgLanguage);
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
	if (manager->disableEpg())
		return;

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
	int h = ((bcd >> 20) & 0x0f) * 10 + ((bcd >> 16) & 0x0f);
	int m = ((bcd >> 12) & 0x0f) * 10 + ((bcd >> 8) & 0x0f);
	int s = ((bcd >> 4) & 0x0f) * 10 + (bcd & 0x0f);
	int t = s + m * 60 + h * 3600;
	static bool first = true;

	// Just in case seconds or minutes would be greater than 60
	s = t % 60;
	m = (t / 60) % 60;
	h = t / 3600;

	// Maximum value supported by QTime()
	if (h > 23) {
		if (first) {
			qCWarning(logEpg, "Warning: some EIT event last longer than 24 hours. Truncating them.");
			first = false;
		}
		h = 23;
		m = 59;
		s = 59;
	}

	return QTime(h, m, s);
}

static const QByteArray contentStr[16][16] = {
	[0] = {},
	[1] = {
			/* Movie/Drama */
		{},
		{kli18n("Detective").toString().toLatin1()},
		{kli18n("Adventure").toString().toLatin1()},
		{kli18n("Science Fiction").toString().toLatin1()},
		{kli18n("Comedy").toString().toLatin1()},
		{kli18n("Soap").toString().toLatin1()},
		{kli18n("Romance").toString().toLatin1()},
		{kli18n("Classical").toString().toLatin1()},
		{kli18n("Adult").toString().toLatin1()},
		{kli18n("User defined").toString().toLatin1()},
	},
	[2] = {
			/* News/Current affairs */
		{},
		{kli18n("Weather").toString().toLatin1()},
		{kli18n("Magazine").toString().toLatin1()},
		{kli18n("Documentary").toString().toLatin1()},
		{kli18n("Discussion").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[3] = {
			/* Show/Game show */
		{},
		{kli18n("Quiz").toString().toLatin1()},
		{kli18n("Variety").toString().toLatin1()},
		{kli18n("Talk").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[4] = {
			/* Sports */
		{},
		{kli18n("Events").toString().toLatin1()},
		{kli18n("Magazine").toString().toLatin1()},
		{kli18n("Football").toString().toLatin1()},
		{kli18n("Tennis").toString().toLatin1()},
		{kli18n("Team").toString().toLatin1()},
		{kli18n("Athletics").toString().toLatin1()},
		{kli18n("Motor").toString().toLatin1()},
		{kli18n("Water").toString().toLatin1()},
		{kli18n("Winter").toString().toLatin1()},
		{kli18n("Equestrian").toString().toLatin1()},
		{kli18n("Martial").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[5] = {
			/* Children's/Youth */
		{},
		{kli18n("Preschool").toString().toLatin1()},
		{kli18n("06 to 14").toString().toLatin1()},
		{kli18n("10 to 16").toString().toLatin1()},
		{kli18n("Educational").toString().toLatin1()},
		{kli18n("Cartoons").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[6] = {
			/* Music/Ballet/Dance */
		{},
		{kli18n("Poprock").toString().toLatin1()},
		{kli18n("Classical").toString().toLatin1()},
		{kli18n("Folk").toString().toLatin1()},
		{kli18n("Jazz").toString().toLatin1()},
		{kli18n("Opera").toString().toLatin1()},
		{kli18n("Ballet").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[7] = {
			/* Arts/Culture */
		{},
		{kli18n("Performance").toString().toLatin1()},
		{kli18n("Fine Arts").toString().toLatin1()},
		{kli18n("Religion").toString().toLatin1()},
		{kli18n("Traditional").toString().toLatin1()},
		{kli18n("Literature").toString().toLatin1()},
		{kli18n("Cinema").toString().toLatin1()},
		{kli18n("Experimental").toString().toLatin1()},
		{kli18n("Press").toString().toLatin1()},
		{kli18n("New Media").toString().toLatin1()},
		{kli18n("Magazine").toString().toLatin1()},
		{kli18n("Fashion").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[8] = {
			/* Social/Political/Economics */
		{},
		{kli18n("Magazine").toString().toLatin1()},
		{kli18n("Advisory").toString().toLatin1()},
		{kli18n("People").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[9] = {
			/* Education/Science/Factual */
		{},
		{kli18n("Nature").toString().toLatin1()},
		{kli18n("Technology").toString().toLatin1()},
		{kli18n("Medicine").toString().toLatin1()},
		{kli18n("Foreign").toString().toLatin1()},
		{kli18n("Social").toString().toLatin1()},
		{kli18n("Further").toString().toLatin1()},
		{kli18n("Language").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[10] = {
			/* Leisure/Hobbies */
		{},
		{kli18n("Travel").toString().toLatin1()},
		{kli18n("Handicraft").toString().toLatin1()},
		{kli18n("Motoring").toString().toLatin1()},
		{kli18n("Fitness").toString().toLatin1()},
		{kli18n("Cooking").toString().toLatin1()},
		{kli18n("Shopping").toString().toLatin1()},
		{kli18n("Gardening").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
	},
	[11] = {
			/* Special characteristics */
		{kli18n("Original Language").toString().toLatin1()},
		{kli18n("Black and White ").toString().toLatin1()},
		{kli18n("Unpublished").toString().toLatin1()},
		{kli18n("Live").toString().toLatin1()},
		{kli18n("Planostereoscopic").toString().toLatin1()},
		{kli18n("User Defined").toString().toLatin1()},
		{kli18n("User Defined 1").toString().toLatin1()},
		{kli18n("User Defined 2").toString().toLatin1()},
		{kli18n("User Defined 3").toString().toLatin1()},
		{kli18n("User Defined 4").toString().toLatin1()}
	}
};

static const QByteArray nibble1Str[16] = {
	[0]  = {kli18n("Undefined").toString().toLatin1()},
	[1]  = {kli18n("Movie").toString().toLatin1()},
	[2]  = {kli18n("News").toString().toLatin1()},
	[3]  = {kli18n("Show").toString().toLatin1()},
	[4]  = {kli18n("Sports").toString().toLatin1()},
	[5]  = {kli18n("Children").toString().toLatin1()},
	[6]  = {kli18n("Music").toString().toLatin1()},
	[7]  = {kli18n("Culture").toString().toLatin1()},
	[8]  = {kli18n("Social").toString().toLatin1()},
	[9]  = {kli18n("Education").toString().toLatin1()},
	[10] = {kli18n("Leisure").toString().toLatin1()},
	[11] = {kli18n("Special").toString().toLatin1()},
	[12] = {kli18n("Reserved").toString().toLatin1()},
	[13] = {kli18n("Reserved").toString().toLatin1()},
	[14] = {kli18n("Reserved").toString().toLatin1()},
	[15] = {kli18n("User defined").toString().toLatin1()},
};

static const QByteArray braNibble1Str[16] = {
	[0]  = {kli18n("News").toString().toLatin1()},
	[1]  = {kli18n("Sports").toString().toLatin1()},
	[2]  = {kli18n("Education").toString().toLatin1()},
	[3]  = {kli18n("Soap opera").toString().toLatin1()},
	[4]  = {kli18n("Mini-series").toString().toLatin1()},
	[5]  = {kli18n("Series").toString().toLatin1()},
	[6]  = {kli18n("Variety").toString().toLatin1()},
	[7]  = {kli18n("Reality show").toString().toLatin1()},
	[8]  = {kli18n("Information").toString().toLatin1()},
	[9]  = {kli18n("Comical").toString().toLatin1()},
	[10] = {kli18n("Children").toString().toLatin1()},
	[11] = {kli18n("Erotic").toString().toLatin1()},
	[12] = {kli18n("Movie").toString().toLatin1()},
	[13] = {kli18n("Raffle, television sales, prizing").toString().toLatin1()},
	[14] = {kli18n("Debate/interview").toString().toLatin1()},
	[15] = {kli18n("Other").toString().toLatin1()},
};

// Using the terms from the English version of NBR 15603-2:2007
// The table omits nibble2="Other", as it is better to show nibble 1
// definition instead.
// when nibble2[x][0] == nibble1[x] and it has no other definition,
// except for "Other", the field will be kept in blank, as the logic
// will fall back to the definition at nibble 1.
static QByteArray braNibble2Str[16][16] = {
	[0] = {
		{kli18n("News").toString().toLatin1()},
		{kli18n("Report").toString().toLatin1()},
		{kli18n("Documentary").toString().toLatin1()},
		{kli18n("Biography").toString().toLatin1()},
	},
	[1] = {},
	[2] = {
		{kli18n("Educative").toString().toLatin1()},
	},
	[3] = {},
	[4] = {},
	[5] = {},
	[6] = {
		{kli18n("Auditorium").toString().toLatin1()},
		{kli18n("Show").toString().toLatin1()},
		{kli18n("Musical").toString().toLatin1()},
		{kli18n("Making of").toString().toLatin1()},
		{kli18n("Feminine").toString().toLatin1()},
		{kli18n("Game show").toString().toLatin1()},
	},
	[7] = {},
	[8] = {
		{kli18n("Cooking").toString().toLatin1()},
		{kli18n("Fashion").toString().toLatin1()},
		{kli18n("Country").toString().toLatin1()},
		{kli18n("Health").toString().toLatin1()},
		{kli18n("Travel").toString().toLatin1()},
	},
	[9] = {},
	[10] = {},
	[11] = {},
	[12] = {},
	[13] = {
		{kli18n("Raffle").toString().toLatin1()},
		{kli18n("Television sales").toString().toLatin1()},
		{kli18n("Prizing").toString().toLatin1()},
	},
	[14] = {
		{kli18n("Discussion").toString().toLatin1()},
		{kli18n("Interview").toString().toLatin1()},
	},
	[15] = {
		{kli18n("Adult cartoon").toString().toLatin1()},
		{kli18n("Interactive").toString().toLatin1()},
		{kli18n("Policy").toString().toLatin1()},
		{kli18n("Religion").toString().toLatin1()},
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
			if (s.isEmpty())
				s = braNibble1Str[nibble1];
			if (!s.isEmpty())
				content += i18n(s) + '\n';
		} else {
			s = contentStr[nibble1][nibble2];
			if (s.isEmpty())
				s = nibble1Str[nibble1];
			if (!s.isEmpty())
				content += i18n(s) + '\n';
		}
	}

	if (!content.isEmpty()) {
		// xgettext:no-c-format
		return (i18n("Genre: %1", content));
	}
	return content;
}

/* As defined at ABNT NBR 15603-2 */
static const QByteArray braRating[] = {
	[0] = {kli18n("reserved").toString().toLatin1()},
	[1] = {kli18n("all audiences").toString().toLatin1()},
	[2] = {kli18n("10 years").toString().toLatin1()},
	[3] = {kli18n("12 years").toString().toLatin1()},
	[4] = {kli18n("14 years").toString().toLatin1()},
	[5] = {kli18n("16 years").toString().toLatin1()},
	[6] = {kli18n("18 years").toString().toLatin1()},
};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

QString DvbEpgFilter::getParental(DvbParentalRatingDescriptor &descriptor)
{
	QString parental;

	for (DvbParentalRatingEntry entry = descriptor.contents(); entry.isValid(); entry.advance()) {
		QString code;
		code.append(QChar(entry.languageCode1()));
		code.append(QChar(entry.languageCode2()));
		code.append(QChar(entry.languageCode3()));

		QString country;
		IsoCodes::getCountry(code, &country);
		if (country.isEmpty())
			country = code;

		// Rating from 0x10 to 0xff are broadcaster's specific
		if (entry.rating() == 0) {
			// xgettext:no-c-format
			parental += i18n("Country %1: not rated\n", country);
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
					GenStr = " (" + GenStr + ')';
				}

				QString ratingStr = i18n(braRating[entry.rating()]);
				// xgettext:no-c-format
				parental += i18n("Country %1: rating: %2%3\n", country, ratingStr, GenStr);
			} else {
				// xgettext:no-c-format
				parental += i18n("Country %1: rating: %2 years.\n", country, entry.rating() + 3);
			}
		}
	}
	return parental;
}

DvbEpgLangEntry *DvbEpgFilter::getLangEntry(DvbEpgEntry &epgEntry,
					    int code1, int code2, int code3,
					    bool add_code,
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
		if (add_code) {
			if (!manager->languageCodes.contains(code)) {
				manager->languageCodes[code] = true;
				emit epgModel->languageAdded(code);
			}
		}
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

				epgEntry.parental += getParental(eventDescriptor);
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
			int index = (std::lower_bound(newEitPids.constBegin(), newEitPids.constEnd(), pid) - newEitPids.constBegin());

			if ((index >= newEitPids.size()) || (newEitPids.at(index) != pid)) {
				newEitPids.insert(index, pid);
			}
		}

		if ((tableType >= 0x0200) && (tableType <= 0x027f)) {
			int pid = entry.pid();
			int index = (std::lower_bound(newEttPids.constBegin(), newEttPids.constEnd(), pid) - newEttPids.constBegin());

			if ((index >= newEttPids.size()) || (newEttPids.at(index) != pid)) {
				newEttPids.insert(index, pid);
			}
		}
		if (i < entryCount - 1)
			entry.advance();
	}

	for (int i = 0; i < eitPids.size(); ++i) {
		int pid = eitPids.at(i);
		int index = (std::lower_bound(newEitPids.constBegin(), newEitPids.constEnd(), pid) - newEitPids.constBegin());

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
		int index = (std::lower_bound(newEttPids.constBegin(), newEttPids.constEnd(), pid) - newEttPids.constBegin());

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

		/* Should be similar to DvbEpgFilter::getLangEntry */
		if (!epgEntry.langEntry.contains(FIRST_LANG)) {
			DvbEpgLangEntry e;
			epgEntry.langEntry.insert(FIRST_LANG, e);
		}
		langEntry = &epgEntry.langEntry[FIRST_LANG];

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

#include "moc_dvbepg.cpp"
