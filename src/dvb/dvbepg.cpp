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

#include "dvbepg.h"
#include "dvbepg_p.h"

#include <QFile>
#include <KStandardDirs>
#include "../ensurenopendingoperation.h"
#include "../log.h"
#include "dvbdevice.h"
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

	if (entry->duration != other.entry->duration) {
		return (entry->duration < other.entry->duration);
	}

	if (entry->title != other.entry->title) {
		return (entry->title < other.entry->title);
	}

	if (entry->subheading != other.entry->subheading) {
		return (entry->subheading < other.entry->subheading);
	}

	if (!entry->details.isEmpty() && !other.entry->details.isEmpty()) {
		return (entry->details < other.entry->details);
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

	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("epgdata.dvb")));

	if (!file.open(QIODevice::ReadOnly)) {
		Log("DvbEpgModel::DvbEpgModel: cannot open") << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);
	DvbRecordingModel *recordingModel = manager->getRecordingModel();
	bool hasRecordingKey = true;
	int version;
	stream >> version;

	if (version == 0x1ce0eca7) {
		hasRecordingKey = false;
	} else if (version != 0x79cffd36) {
		Log("DvbEpgModel::DvbEpgModel: wrong version") << file.fileName();
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
		stream >> entry.title;
		stream >> entry.subheading;
		stream >> entry.details;

		if (hasRecordingKey) {
			SqlKey recordingKey;
			stream >> recordingKey.sqlKey;

			if (recordingKey.isSqlKeyValid()) {
				entry.recording = recordingModel->findRecordingByKey(recordingKey);
			}
		}

		if (stream.status() != QDataStream::Ok) {
			Log("DvbEpgModel::DvbEpgModel: corrupt data") << file.fileName();
			break;
		}

		addEntry(entry);
	}
}

DvbEpgModel::~DvbEpgModel()
{
	if (hasPendingOperation) {
		Log("DvbEpgModel::~DvbEpgModel: illegal recursive call");
	}

	if (!dvbEpgFilters.isEmpty() || !atscEpgFilters.isEmpty()) {
		Log("DvbEpgModel::~DvbEpgModel: filter list not empty");
	}

	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("epgdata.dvb")));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		Log("DvbEpgModel::~DvbEpgModel: cannot open") << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);
	int version = 0x79cffd36;
	stream << version;

	foreach (const DvbSharedEpgEntry &entry, entries) {
		SqlKey recordingKey;

		if (entry->recording.isValid()) {
			recordingKey = *entry->recording;
		}

		stream << entry->channel->name;
		stream << entry->begin;
		stream << entry->duration;
		stream << entry->title;
		stream << entry->subheading;
		stream << entry->details;
		stream << recordingKey.sqlKey;
	}
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

DvbSharedEpgEntry DvbEpgModel::addEntry(const DvbEpgEntry &entry)
{
	if (!entry.validate()) {
		Log("DvbEpgModel::addEntry: invalid entry");
		return DvbSharedEpgEntry();
	}

	if (hasPendingOperation) {
		Log("DvbEpgModel::addEntry: illegal recursive call");
		return DvbSharedEpgEntry();
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (entry.begin.addSecs(QTime().secsTo(entry.duration)) > currentDateTimeUtc) {
		DvbSharedEpgEntry existingEntry = entries.value(DvbEpgEntryId(&entry));

		if (existingEntry.isValid()) {
			if (existingEntry->details.isEmpty() && !entry.details.isEmpty()) {
				// needed for atsc
				emit entryAboutToBeUpdated(existingEntry);
				const_cast<DvbEpgEntry *>(existingEntry.constData())->details =
					entry.details;
				emit entryUpdated(existingEntry);
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
		return newEntry;
	}

	return DvbSharedEpgEntry();
}

void DvbEpgModel::scheduleProgram(const DvbSharedEpgEntry &entry, int extraSecondsBefore,
	int extraSecondsAfter)
{
	if (!entry.isValid() || (entries.value(DvbEpgEntryId(entry)) != entry)) {
		Log("DvbEpgModel::scheduleProgram: invalid entry");
		return;
	}

	if (hasPendingOperation) {
		Log("DvbEpgModel::scheduleProgram: illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	emit entryAboutToBeUpdated(entry);
	DvbSharedRecording oldRecording;

	if (!entry->recording.isValid()) {
		DvbRecording recording;
		recording.name = entry->title;
		recording.channel = entry->channel;
		recording.begin = entry->begin.addSecs(-extraSecondsBefore);
		recording.duration =
			entry->duration.addSecs(extraSecondsBefore + extraSecondsAfter);
		const_cast<DvbEpgEntry *>(entry.constData())->recording =
			manager->getRecordingModel()->addRecording(recording);
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
		Log("DvbEpgModel::channelUpdated: illegal recursive call");
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
		Log("DvbEpgModel::channelRemoved: illegal recursive call");
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
		Log("DvbEpgModel::recordingRemoved: illegal recursive call");
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
		Log("DvbEpgModel::timerEvent: illegal recursive call");
		return;
	}

	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	Iterator it = entries.begin();

	while (ConstIterator(it) != entries.constEnd()) {
		const DvbSharedEpgEntry &entry = *it;

		if (entry->begin.addSecs(QTime().secsTo(entry->duration)) > currentDateTimeUtc) {
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

DvbEpgFilter::DvbEpgFilter(DvbManager *manager, DvbDevice *device_,
	const DvbSharedChannel &channel) : device(device_)
{
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

void DvbEpgFilter::processSection(const char *data, int size)
{
	unsigned char tableId = data[0];

	if ((tableId < 0x4e) || (tableId > 0x6f)) {
		return;
	}

	DvbEitSection eitSection(data, size);

	if (!eitSection.isValid()) {
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
		return;
	}

	for (DvbEitSectionEntry entry = eitSection.entries(); entry.isValid(); entry.advance()) {
		DvbEpgEntry epgEntry;
		epgEntry.channel = channel;
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

				epgEntry.title = eventDescriptor.eventName();
				epgEntry.subheading = eventDescriptor.text();
				break;
			    }
			case 0x4e: {
				DvbExtendedEventDescriptor eventDescriptor(descriptor);

				if (!eventDescriptor.isValid()) {
					break;
				}

				epgEntry.details += eventDescriptor.text();
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

	for (AtscMgtSectionEntry entry = mgtSection.entries(); (entryCount > 0) && entry.isValid();
	     --entryCount, entry.advance()) {
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
		return;
	}

	DvbChannel fakeChannel;
	fakeChannel.source = source;
	fakeChannel.transponder = transponder;
	fakeChannel.networkId = eitSection.sourceId();
	DvbSharedChannel channel = channelModel->findChannelById(fakeChannel);

	if (!channel.isValid()) {
		return;
	}

	int entryCount = eitSection.entryCount();
	// 1980-01-06T000000 minus 15 secs (= UTC - GPS in 2011)
	QDateTime baseDateTime = QDateTime(QDate(1980, 1, 5), QTime(23, 59, 45), Qt::UTC);

	for (AtscEitSectionEntry eitEntry = eitSection.entries();
	     (entryCount > 0) && eitEntry.isValid(); --entryCount, eitEntry.advance()) {
		DvbEpgEntry epgEntry;
		epgEntry.channel = channel;
		epgEntry.begin = baseDateTime.addSecs(eitEntry.startTime());
		epgEntry.duration = QTime().addSecs(eitEntry.duration());
		epgEntry.title = eitEntry.title();

		quint32 id = ((quint32(fakeChannel.networkId) << 16) | quint32(eitEntry.eventId()));
		DvbSharedEpgEntry entry = epgEntries.value(id);

		if (entry.isValid() && (entry->channel == epgEntry.channel) &&
		    (entry->begin == epgEntry.begin) && (entry->duration == epgEntry.duration) &&
		    (entry->title == epgEntry.title)) {
			continue;
		}

		entry = epgModel->addEntry(epgEntry);
		epgEntries.insert(id, entry);
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

		if (entry->details != details) {
			DvbEpgEntry modifiedEntry = *entry;
			modifiedEntry.details = details;
			entry = epgModel->addEntry(modifiedEntry);
			epgEntries.insert(id, entry);
		}
	}
}
