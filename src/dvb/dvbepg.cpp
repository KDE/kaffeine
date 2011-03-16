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
#include <KDebug>
#include <KStandardDirs>
#include "../ensurenopendingoperation.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbsi.h"

bool DvbEpgEntry::operator==(const DvbEpgEntry &other) const
{
	return ((channel == other.channel) && (begin == other.begin) &&
		(duration == other.duration) && (title == other.title) &&
		(subheading == other.subheading) && (details == other.details));
}

bool DvbEpgEntry::operator<(const DvbEpgEntry &other) const
{
	if (channel != other.channel) {
		return (channel < other.channel);
	}

	if (begin != other.begin) {
		return (begin < other.begin);
	}

	if (duration != other.duration) {
		return (duration < other.duration);
	}

	if (title != other.title) {
		return (title < other.title);
	}

	if (subheading != other.subheading) {
		return (subheading < other.subheading);
	}

	return (details < other.details);
}

DvbEpgModel::DvbEpgModel(DvbManager *manager_, QObject *parent) : QObject(parent),
	manager(manager_), hasPendingOperation(false)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	startTimer(54000);

	DvbChannelModel *channelModel = manager->getChannelModel();
	connect(channelModel, SIGNAL(channelUpdated(DvbSharedChannel,DvbChannel)),
		this, SLOT(channelUpdated(DvbSharedChannel,DvbChannel)));
	connect(channelModel, SIGNAL(channelRemoved(DvbSharedChannel)),
		this, SLOT(channelRemoved(DvbSharedChannel)));
	connect(manager->getRecordingModel(), SIGNAL(recordingRemoved(DvbSharedRecording)),
		this, SLOT(recordingRemoved(DvbSharedRecording)));

	// TODO use SQL to store epg data

	QFile file(KStandardDirs::locateLocal("appdata", "epgdata.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "cannot open" << file.fileName();
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
		kWarning() << "wrong version" << file.fileName();
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
			kWarning() << "corrupt data" << file.fileName();
			break;
		}

		if (entry.channel.isValid() &&
		    (entry.begin.addSecs(QTime().secsTo(entry.duration)) > currentDateTimeUtc) &&
		    !entries.contains(entry)) {
			internalAddEntry(entry);
		}
	}
}

DvbEpgModel::~DvbEpgModel()
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (!dvbEpgFilters.isEmpty() || !atscEpgFilters.isEmpty()) {
		kWarning() << "filter list not empty";
		qDeleteAll(dvbEpgFilters);
		qDeleteAll(atscEpgFilters);
	}

	QFile file(KStandardDirs::locateLocal("appdata", "epgdata.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "cannot open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);
	int version = 0x79cffd36;
	stream << version;

	for (ConstIterator it = entries.constBegin(); it != entries.constEnd(); ++it) {
		const DvbEpgEntry &entry = it.key();
		SqlKey recordingKey;

		if (entry.recording.isValid()) {
			recordingKey = *entry.recording;
		}

		stream << entry.channel->name;
		stream << entry.begin;
		stream << entry.duration;
		stream << entry.title;
		stream << entry.subheading;
		stream << entry.details;
		stream << recordingKey.sqlKey;
	}
}

QList<const DvbEpgEntry *> DvbEpgModel::getCurrentNext(const DvbSharedChannel &channel) const
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	QList<const DvbEpgEntry *> result;

	for (ConstIterator it = entries.lowerBound(DvbEpgEntry(channel)); it != entries.constEnd();
	     ++it) {
		const DvbEpgEntry *entry = &it.key();

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

void DvbEpgModel::startEventFilter(DvbDevice *device, const DvbChannel *channel)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
		dvbEpgFilters.append(new DvbEpgFilter(manager, device, channel));
		break;
	case DvbTransponderBase::Atsc:
		atscEpgFilters.append(new AtscEpgFilter(manager, device, channel));
		break;
	}
}

void DvbEpgModel::stopEventFilter(DvbDevice *device, const DvbChannel *channel)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
		for (int i = 0; i < dvbEpgFilters.size(); ++i) {
			const DvbEpgFilter *epgFilter = dvbEpgFilters.at(i);

			if ((epgFilter->device == device) &&
			    (epgFilter->source == channel->source) &&
			    (epgFilter->transponder.corresponds(channel->transponder))) {
				dvbEpgFilters.removeAt(i);
				delete epgFilter;
				break;
			}
		}

		break;
	case DvbTransponderBase::Atsc:
		for (int i = 0; i < atscEpgFilters.size(); ++i) {
			const AtscEpgFilter *epgFilter = atscEpgFilters.at(i);

			if ((epgFilter->device == device) &&
			    (epgFilter->source == channel->source) &&
			    (epgFilter->transponder.corresponds(channel->transponder))) {
				atscEpgFilters.removeAt(i);
				delete epgFilter;
				break;
			}
		}

		break;
	}
}

QMap<DvbEpgEntry, DvbEpgEmptyClass> DvbEpgModel::getEntries() const
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	return entries;
}

QHash<DvbSharedChannel, int> DvbEpgModel::getEpgChannels() const
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	return epgChannels;
}

void DvbEpgModel::addEntry(const DvbEpgEntry &entry)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (entry.begin.addSecs(QTime().secsTo(entry.duration)) > currentDateTimeUtc) {
		Iterator it = entries.lowerBound(entry);

		if ((ConstIterator(it) != entries.constEnd()) && (it.key() == entry)) {
			return;
		}

		if (entry.details.isEmpty()) {
			++it;

			if (ConstIterator(it) != entries.constEnd()) {
				const DvbEpgEntry *existingEntry = &it.key();

				if ((existingEntry->subheading == entry.subheading) &&
				    (existingEntry->title == entry.title) &&
				    (existingEntry->duration == entry.duration) &&
				    (existingEntry->begin == entry.begin) &&
				    (existingEntry->channel == entry.channel)) {
					// needed for atsc
					return;
				}
			}
		} else {
			if (ConstIterator(it) != entries.constBegin()) {
				--it;
				const DvbEpgEntry *existingEntry = &it.key();

				if (existingEntry->details.isEmpty() &&
				    (existingEntry->subheading == entry.subheading) &&
				    (existingEntry->title == entry.title) &&
				    (existingEntry->duration == entry.duration) &&
				    (existingEntry->begin == entry.begin) &&
				    (existingEntry->channel == entry.channel)) {
					// needed for atsc
					DvbEpgEntry oldEntry = *existingEntry;
					const_cast<DvbEpgEntry *>(existingEntry)->details =
						entry.details;
					internalEntryUpdated(existingEntry, oldEntry);
					return;
				}
			}
		}

		Q_ASSERT(entry.begin.timeSpec() == Qt::UTC);
		Q_ASSERT(!entry.recording.isValid());
		internalAddEntry(entry);
	}
}

void DvbEpgModel::scheduleProgram(const DvbEpgEntry *entry, int extraSecondsBefore,
	int extraSecondsAfter)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	Iterator it = entries.find(*entry);

	if (ConstIterator(it) != entries.constEnd()) {
		const DvbEpgEntry *existingEntry = &it.key();
		DvbEpgEntry oldEntry = *existingEntry;

		if (!oldEntry.recording.isValid()) {
			DvbRecording recording;
			recording.name = oldEntry.title;
			recording.channel = oldEntry.channel;
			recording.begin = oldEntry.begin.addSecs(-extraSecondsBefore);
			recording.duration =
				oldEntry.duration.addSecs(extraSecondsBefore + extraSecondsAfter);
			const_cast<DvbEpgEntry *>(existingEntry)->recording =
				manager->getRecordingModel()->addRecording(recording);
		} else {
			const_cast<DvbEpgEntry *>(existingEntry)->recording = DvbSharedRecording();
		}

		internalEntryUpdated(existingEntry, oldEntry);

		if (oldEntry.recording.isValid()) {
			hasPendingOperation = false; // recordingRemoved() will be called
			manager->getRecordingModel()->removeRecording(oldEntry.recording);
		}
	}
}

void DvbEpgModel::channelUpdated(const DvbSharedChannel &channel, const DvbChannel &oldChannel)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);

	if (DvbComparableChannel(channel) != DvbComparableChannel(&oldChannel)) {
		Iterator it = entries.lowerBound(DvbEpgEntry(channel));

		while ((ConstIterator(it) != entries.constEnd()) &&
		       (it.key().channel == channel)) {
			it = internalRemoveEntry(it);
		}
	}
}

void DvbEpgModel::channelRemoved(const DvbSharedChannel &channel)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	Iterator it = entries.lowerBound(DvbEpgEntry(channel));

	while ((ConstIterator(it) != entries.constEnd()) && (it.key().channel == channel)) {
		it = internalRemoveEntry(it);
	}
}

void DvbEpgModel::recordingRemoved(const DvbSharedRecording &recording)
{
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	const DvbEpgEntry *constEntry = recordingMapping.take(recording);

	if (constEntry != NULL) {
		Iterator it = entries.find(*constEntry);

		if (ConstIterator(it) != entries.constEnd()) {
			const DvbEpgEntry *existingEntry = &it.key();
			DvbEpgEntry oldEntry = *existingEntry;
			const_cast<DvbEpgEntry *>(existingEntry)->recording = DvbSharedRecording();
			internalEntryUpdated(existingEntry, oldEntry);
		}
	}
}

void DvbEpgModel::timerEvent(QTimerEvent *event)
{
	Q_UNUSED(event)
	EnsureNoPendingOperation ensureNoPendingOperation(hasPendingOperation);
	currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	Iterator it = entries.begin();

	while (ConstIterator(it) != entries.constEnd()) {
		const DvbEpgEntry &entry = it.key();

		if (entry.begin.addSecs(QTime().secsTo(entry.duration)) > currentDateTimeUtc) {
			++it;
		} else {
			it = internalRemoveEntry(it);
		}
	}
}

void DvbEpgModel::internalAddEntry(const DvbEpgEntry &entry)
{
	int oldSize = entries.size();
	const DvbEpgEntry *newEntry = &entries.insert(entry, DvbEpgEmptyClass()).key();

	if (entries.size() != oldSize) {
		if (newEntry->recording.isValid()) {
			recordingMapping.insert(newEntry->recording, newEntry);
		}

		if (++epgChannels[newEntry->channel] == 1) {
			emit epgChannelAdded(newEntry->channel);
		}

		emit entryAdded(newEntry);
	}
}

void DvbEpgModel::internalEntryUpdated(const DvbEpgEntry *entry, const DvbEpgEntry &oldEntry)
{
	if (entry->recording != oldEntry.recording) {
		if (oldEntry.recording.isValid()) {
			recordingMapping.remove(oldEntry.recording);
		}

		if (entry->recording.isValid()) {
			recordingMapping.insert(entry->recording, entry);
		}
	}

	Q_ASSERT(entry->channel == oldEntry.channel);
	emit entryUpdated(entry, oldEntry);
}

DvbEpgModel::Iterator DvbEpgModel::internalRemoveEntry(Iterator it)
{
	const DvbEpgEntry *entry = &it.key();

	if (entry->recording.isValid()) {
		recordingMapping.remove(entry->recording);
	}

	if (--epgChannels[entry->channel] == 0) {
		epgChannels.remove(entry->channel);
		emit epgChannelRemoved(entry->channel);
	}

	emit entryRemoved(entry);
	return entries.erase(it);
}

DvbEpgFilter::DvbEpgFilter(DvbManager *manager, DvbDevice *device_, const DvbChannel *channel) :
	device(device_)
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

	DvbChannel pseudoChannel;
	pseudoChannel.source = source;
	pseudoChannel.transponder = transponder;
	pseudoChannel.networkId = eitSection.originalNetworkId();
	pseudoChannel.transportStreamId = eitSection.transportStreamId();
	pseudoChannel.serviceId = eitSection.serviceId();
	DvbSharedChannel channel = channelModel->findChannelByContent(pseudoChannel);

	if (!channel.isValid()) {
		pseudoChannel.networkId = -1;
		channel = channelModel->findChannelByContent(pseudoChannel);
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

AtscEpgFilter::AtscEpgFilter(DvbManager *manager, DvbDevice *device_, const DvbChannel *channel) :
	device(device_), mgtFilter(this), eitFilter(this), ettFilter(this)
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

	DvbChannel pseudoChannel;
	pseudoChannel.source = source;
	pseudoChannel.transponder = transponder;
	pseudoChannel.networkId = eitSection.sourceId();
	DvbSharedChannel channel = channelModel->findChannelByContent(pseudoChannel);

	if (!channel.isValid()) {
		return;
	}

	int entryCount = eitSection.entryCount();
	// 1980-01-06T000000 minus 15 secs (= UTC - GPS in 2011)
	QDateTime baseDateTime = QDateTime(QDate(1980, 1, 5), QTime(23, 59, 45), Qt::UTC);

	for (AtscEitSectionEntry entry = eitSection.entries(); (entryCount > 0) && entry.isValid();
	     --entryCount, entry.advance()) {
		DvbEpgEntry epgEntry;
		epgEntry.channel = channel;
		epgEntry.begin = baseDateTime.addSecs(entry.startTime());
		epgEntry.duration = QTime().addSecs(entry.duration());
		epgEntry.title = entry.title();

		quint32 id = ((quint32(pseudoChannel.networkId) << 16) | quint32(entry.eventId()));
		QHash<quint32, DvbEpgEntry>::ConstIterator it = epgEntryMapping.constFind(id);

		if (it != epgEntryMapping.constEnd()) {
			if ((epgEntry.channel == it->channel) && (epgEntry.begin == it->begin) &&
			    (epgEntry.duration == it->duration) && (epgEntry.title == it->title)) {
				continue;
			}
		}

		epgEntryMapping.insert(id, epgEntry);
		epgModel->addEntry(epgEntry);
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
	QHash<quint32, DvbEpgEntry>::Iterator it = epgEntryMapping.find(id);

	if (it != epgEntryMapping.end()) {
		QString text = ettSection.text();

		if (it->details != text) {
			it->details = text;
			epgModel->addEntry(*it);
		}
	}
}
