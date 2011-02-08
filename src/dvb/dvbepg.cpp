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
#include "dvbchannel.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbsi.h"

DvbEpg::DvbEpg(DvbManager *manager_, QObject *parent) : QObject(parent), manager(manager_),
	hasPendingOperation(false)
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	startTimer(54000);

	DvbChannelModel *channelModel = manager->getChannelModel();
	connect(channelModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
		this, SLOT(channelDataChanged(QModelIndex,QModelIndex)));
	connect(channelModel, SIGNAL(layoutChanged()), this, SLOT(channelLayoutChanged()));
	connect(channelModel, SIGNAL(modelReset()), this, SLOT(channelModelReset()));
	connect(channelModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
		this, SLOT(channelRowsInserted(QModelIndex,int,int)));
	connect(channelModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
		this, SLOT(channelRowsRemoved(QModelIndex,int,int)));
	connect(manager->getRecordingModel(), SIGNAL(programRemoved(DvbRecordingKey)),
		this, SLOT(programRemoved(DvbRecordingKey)));

	for (int row = 0; row < channelModel->rowCount(); ++row) {
		const DvbChannel *channel = channelModel->data(channelModel->index(row, 0),
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
		channels.insert(row, QExplicitlySharedDataPointer<const DvbChannel>(channel));
		addChannelEitMapping(channel);
	}

	QFile file(KStandardDirs::locateLocal("appdata", "epgdata.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "cannot open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);
	bool hasRecordingKey = true;
	int version;
	stream >> version;

	if (version == 0x1ce0eca7) {
		hasRecordingKey = false;
	} else if (version != 0x79cffd36) {
		kWarning() << "wrong version" << file.fileName();
		return;
	}

	QSet<QString> channelNames;

	foreach (const QExplicitlySharedDataPointer<const DvbChannel> &channel, channels) {
		channelNames.insert(channel->name);
	}

	while (!stream.atEnd()) {
		DvbEpgEntry entry;
		stream >> entry.channelName;
		stream >> entry.begin;
		entry.begin = entry.begin.toUTC();
		stream >> entry.duration;
		stream >> entry.title;
		stream >> entry.subheading;
		stream >> entry.details;

		if (hasRecordingKey) {
			stream >> entry.recordingKey.key;
		}

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "corrupt data" << file.fileName();
			break;
		}

		if (channelNames.contains(entry.channelName) &&
		    (entry.begin.addSecs(QTime().secsTo(entry.duration)) > currentDateTimeUtc)) {
			entries.append(entry);

			if (entry.recordingKey.isValid()) {
				recordingKeyMapping.insert(entry.recordingKey,
					&entries.at(entries.size() - 1));
			}
		}
	}

	qSort(entries.begin(), entries.end(), DvbEpgLessThan());
}

DvbEpg::~DvbEpg()
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);

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

	foreach (const DvbEpgEntry &entry, entries) {
		stream << entry.channelName;
		stream << entry.begin;
		stream << entry.duration;
		stream << entry.title;
		stream << entry.subheading;
		stream << entry.details;
		stream << entry.recordingKey.key;
	}
}

QList<const DvbEpgEntry *> DvbEpg::getCurrentNext(const QString &channelName) const
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	QList<const DvbEpgEntry *> result;

	for (int i = (qLowerBound(entries.constBegin(), entries.constEnd(), channelName,
	     DvbEpgLessThan()) - entries.constBegin()); i < entries.size(); ++i) {
		const DvbEpgEntry *entry = &entries.at(i);

		if (entry->channelName != channelName) {
			break;
		}

		result.append(entry);

		if (result.size() == 2) {
			break;
		}
	}

	return result;
}

void DvbEpg::startEventFilter(DvbDevice *device, const DvbChannel *channel)
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);

	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
		dvbEpgFilters.append(new DvbEpgFilter(this, device, channel));
		break;
	case DvbTransponderBase::Atsc:
		atscEpgFilters.append(new AtscEpgFilter(this, device, channel));
		break;
	}
}

void DvbEpg::stopEventFilter(DvbDevice *device, const DvbChannel *channel)
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);

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

QList<DvbEpgEntry> DvbEpg::getEntries() const
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	return entries;
}

QString DvbEpg::findChannelNameByEitEntry(const DvbEitEntry &eitEntry) const
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	const DvbChannel *channel = dvbEitMapping.value(eitEntry, NULL);

	if (channel != NULL) {
		return channel->name;
	}

	return QString();
}

QString DvbEpg::findChannelNameByEitEntry(const AtscEitEntry &eitEntry) const
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	const DvbChannel *channel = atscEitMapping.value(eitEntry, NULL);

	if (channel != NULL) {
		return channel->name;
	}

	return QString();
}

void DvbEpg::addEntry(const DvbEpgEntry &entry)
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);

	if (entry.begin.addSecs(QTime().secsTo(entry.duration)) > currentDateTimeUtc) {
		int index = (qLowerBound(entries.constBegin(), entries.constEnd(), entry,
			DvbEpgLessThan()) - entries.constBegin());

		if (index < entries.size()) {
			const DvbEpgEntry &existingEntry = entries.at(index);

 			if ((existingEntry.channelName == entry.channelName) &&
			    (existingEntry.begin == entry.begin) &&
			    (existingEntry.duration == entry.duration) &&
			    (existingEntry.title == entry.title) &&
			    (existingEntry.subheading == entry.subheading) &&
			    (existingEntry.details == entry.details)) {
				return;
			}
		}

		int updateIndex = (index - 1);

		while (updateIndex >= 0) {
			const DvbEpgEntry &existingEntry = entries.at(updateIndex);

 			if ((existingEntry.channelName == entry.channelName) &&
			    (existingEntry.begin == entry.begin) &&
			    (existingEntry.duration == entry.duration) &&
			    (existingEntry.title == entry.title)) {
				if (existingEntry.subheading.isEmpty() &&
				    existingEntry.details.isEmpty()) {
					break;
				}

				--updateIndex;
				continue;
			}

			updateIndex = -1;
			break;
		}

		if (updateIndex >= 0) {
			// needed for atsc epg
			DvbEpgEntry *existingEntry = &entries[updateIndex];
			DvbEpgEntry oldEntry = *existingEntry;
			existingEntry->subheading = entry.subheading;
			existingEntry->details = entry.details;

			if (index > updateIndex) {
				--index;
			}

			entries.move(updateIndex, index);
			emit entryChanged(existingEntry, oldEntry);
		} else {
			entries.insert(index, entry);
			DvbEpgEntry *newEntry = &entries[index];
			newEntry->recordingKey = DvbRecordingKey();
			emit entryAdded(newEntry);
		}
	}
}

void DvbEpg::scheduleProgram(const DvbEpgEntry *entry, int extraSecondsBefore,
	int extraSecondsAfter)
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	int index = (qBinaryFind(entries.constBegin(), entries.constEnd(), *entry,
		DvbEpgLessThan()) - entries.constBegin());

	if (index < entries.size()) {
		DvbEpgEntry *epgEntry = &entries[index];

		if (!epgEntry->recordingKey.isValid()) {
			DvbEpgEntry oldEntry = *epgEntry;
			DvbRecordingEntry recordingEntry;
			recordingEntry.name = epgEntry->title;
			recordingEntry.channelName = epgEntry->channelName;
			recordingEntry.begin = epgEntry->begin.addSecs(-extraSecondsBefore);
			recordingEntry.duration =
				epgEntry->duration.addSecs(extraSecondsBefore + extraSecondsAfter);
			epgEntry->recordingKey =
				manager->getRecordingModel()->scheduleProgram(recordingEntry);
			recordingKeyMapping.insert(epgEntry->recordingKey, epgEntry);
			emit entryChanged(epgEntry, oldEntry);
		} else {
			manager->getRecordingModel()->removeProgram(epgEntry->recordingKey);
			// programRemoved() does the rest
		}
	}
}

void DvbEpg::channelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	DvbChannelModel *channelModel = manager->getChannelModel();

	for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
		const DvbChannel *oldChannel = channels.at(row).constData();
		QString oldChannelName = oldChannel->name;
		removeChannelEitMapping(oldChannel);
		const DvbChannel *channel = channelModel->data(channelModel->index(row, 0),
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
		channels.replace(row, QExplicitlySharedDataPointer<const DvbChannel>(channel));
		addChannelEitMapping(channel);

		if (channel->name != oldChannelName) {
			QString channelName = channel->name;
			bool needsSorting = false;

			for (int i = (qLowerBound(entries.constBegin(), entries.constEnd(),
			     oldChannelName, DvbEpgLessThan()) - entries.constBegin());
			     i < entries.size(); ++i) {
				if (entries.at(i).channelName != oldChannelName) {
					break;
				}

				DvbEpgEntry *entry = &entries[i];
				DvbEpgEntry oldEntry = *entry;
				entry->channelName = channelName;
				needsSorting = true;
				emit entryChanged(entry, oldEntry);
			}

			if (needsSorting) {
				qSort(entries.begin(), entries.end(), DvbEpgLessThan());
			}
		}
	}
}

void DvbEpg::channelLayoutChanged()
{
	kFatal() << "not supported";
}

void DvbEpg::channelModelReset()
{
	kFatal() << "not supported";
}

void DvbEpg::channelRowsInserted(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	DvbChannelModel *channelModel = manager->getChannelModel();

	for (int row = start; row <= end; ++row) {
		const DvbChannel *channel = channelModel->data(channelModel->index(row, 0),
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
		channels.insert(row, QExplicitlySharedDataPointer<const DvbChannel>(channel));
		addChannelEitMapping(channel);
	}
}

void DvbEpg::channelRowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);

	for (int row = start; row <= end; ++row) {
		const DvbChannel *channel = channels.at(row).constData();
		removeChannelEitMapping(channel);

		int lowerBound = (qLowerBound(entries.constBegin(), entries.constEnd(),
			channel->name, DvbEpgLessThan()) - entries.constBegin());
		int upperBound = (qUpperBound(entries.constBegin(), entries.constEnd(),
			channel->name, DvbEpgLessThan()) - entries.constBegin());

		for (int i = lowerBound; i < upperBound; ++i) {
			const DvbEpgEntry *entry = &entries.at(i);
			emit entryAboutToBeRemoved(entry);
			recordingKeyMapping.remove(entry->recordingKey);
		}

		entries.erase(entries.begin() + lowerBound, entries.begin() + upperBound);
	}

	channels.erase(channels.begin() + start, channels.begin() + end + 1);
}

void DvbEpg::programRemoved(const DvbRecordingKey &recordingKey)
{
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	const DvbEpgEntry *constEntry = recordingKeyMapping.value(recordingKey, NULL);

	if (constEntry != NULL) {
		int index = (qBinaryFind(entries.constBegin(), entries.constEnd(), *constEntry,
			DvbEpgLessThan()) - entries.constBegin());

		if (index < entries.size()) {
			DvbEpgEntry *entry = &entries[index];
			DvbEpgEntry oldEntry = *entry;
			entry->recordingKey = DvbRecordingKey();
			emit entryChanged(entry, oldEntry);
		}
	}
}

void DvbEpg::timerEvent(QTimerEvent *event)
{
	Q_UNUSED(event)
	DvbEpgEnsureNoPendingOperation ensureNoPendingOperation(&hasPendingOperation);
	currentDateTimeUtc = QDateTime::currentDateTime().toUTC();

	for (int i = 0; i < entries.size(); ++i) {
		const DvbEpgEntry *entry = &entries.at(i);

		if (entry->begin.addSecs(QTime().secsTo(entry->duration)) <= currentDateTimeUtc) {
			emit entryAboutToBeRemoved(entry);
			recordingKeyMapping.remove(entry->recordingKey);
			entries.removeAt(i);
			--i;
		}
	}
}

void DvbEpg::addChannelEitMapping(const DvbChannel *channel)
{
	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
		dvbEitMapping.insert(DvbEitEntry(channel), channel);
		break;
	case DvbTransponderBase::Atsc:
		atscEitMapping.insert(AtscEitEntry(channel), channel);
		break;
	}
}

void DvbEpg::removeChannelEitMapping(const DvbChannel *channel)
{
	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2:
	case DvbTransponderBase::DvbT:
		dvbEitMapping.remove(DvbEitEntry(channel));
		break;
	case DvbTransponderBase::Atsc:
		atscEitMapping.remove(AtscEitEntry(channel));
		break;
	}
}

void DvbEpgEnsureNoPendingOperation::printFatalErrorMessage()
{
	kFatal() << "illegal recursive call";
}

DvbEitEntry::DvbEitEntry(const DvbChannel *channel)
{
	source = channel->source;
	networkId = channel->networkId;
	transportStreamId = channel->transportStreamId;
	serviceId = channel->getServiceId();
}

static uint qHash(const DvbEitEntry &eitEntry)
{
	return (qHash(eitEntry.source) ^ uint(eitEntry.networkId) ^
		(uint(eitEntry.transportStreamId) << 8) ^ (uint(eitEntry.serviceId) << 16));
}

DvbEpgFilter::DvbEpgFilter(DvbEpg *epg_, DvbDevice *device_, const DvbChannel *channel) :
	epg(epg_), device(device_)
{
	source = channel->source;
	transponder = channel->transponder;
	device->addSectionFilter(0x12, this);
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

void DvbEpgFilter::processSection(const char *data, int size, int crc)
{
	Q_UNUSED(crc) // TODO check crc: (a) valid (b) invalid, but occurs more than once --> ok
	DvbEitSection eitSection(data, size);

	if (!eitSection.isValid() ||
	    (eitSection.tableId() < 0x4e) || (eitSection.tableId() > 0x6f)) {
		return;
	}

	DvbEitEntry eitEntry;
	eitEntry.source = source;
	eitEntry.networkId = eitSection.originalNetworkId();
	eitEntry.transportStreamId = eitSection.transportStreamId();
	eitEntry.serviceId = eitSection.serviceId();
	QString channelName = epg->findChannelNameByEitEntry(eitEntry);

	if (channelName.isEmpty()) {
		eitEntry.networkId = -1;
		channelName = epg->findChannelNameByEitEntry(eitEntry);
	}

	if (channelName.isEmpty()) {
		return;
	}

	for (DvbEitSectionEntry entry = eitSection.entries(); entry.isValid(); entry.advance()) {
		DvbEpgEntry epgEntry;
		epgEntry.channelName = channelName;
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

		epg->addEntry(epgEntry);
	}
}

AtscEitEntry::AtscEitEntry(const DvbChannel *channel)
{
	source = channel->source;
	sourceId = channel->networkId;
}

static uint qHash(const AtscEitEntry &eitEntry)
{
	return (qHash(eitEntry.source) ^ uint(eitEntry.sourceId));
}

void AtscEpgMgtFilter::processSection(const char *data, int size, int crc)
{
	epgFilter->processMgtSection(data, size, crc);
}

void AtscEpgEitFilter::processSection(const char *data, int size, int crc)
{
	epgFilter->processEitSection(data, size, crc);
}

void AtscEpgEttFilter::processSection(const char *data, int size, int crc)
{
	epgFilter->processEttSection(data, size, crc);
}

AtscEpgFilter::AtscEpgFilter(DvbEpg *epg_, DvbDevice *device_, const DvbChannel *channel) :
	epg(epg_), device(device_), mgtFilter(this), eitFilter(this), ettFilter(this)
{
	source = channel->source;
	transponder = channel->transponder;
	device->addSectionFilter(0x1ffb, &mgtFilter);
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

void AtscEpgFilter::processMgtSection(const char *data, int size, int crc)
{
	Q_UNUSED(crc) // TODO check crc: (a) valid (b) invalid, but occurs more than once --> ok
	AtscMgtSection mgtSection(data, size);

	if (!mgtSection.isValid() || (mgtSection.tableId() != 0xc7)) {
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

void AtscEpgFilter::processEitSection(const char *data, int size, int crc)
{
	Q_UNUSED(crc) // TODO check crc: (a) valid (b) invalid, but occurs more than once --> ok
	AtscEitSection eitSection(data, size);

	if (!eitSection.isValid() || (eitSection.tableId() != 0xcb)) {
		return;
	}

	AtscEitEntry eitEntry;
	eitEntry.source = source;
	eitEntry.sourceId = eitSection.sourceId();
	QString channelName = epg->findChannelNameByEitEntry(eitEntry);

	if (channelName.isEmpty()) {
		return;
	}

	int entryCount = eitSection.entryCount();
	// 1980-01-06T000000 minus 15 secs (= UTC - GPS in 2011)
	QDateTime baseDateTime = QDateTime(QDate(1980, 1, 5), QTime(23, 59, 45), Qt::UTC);

	for (AtscEitSectionEntry entry = eitSection.entries(); (entryCount > 0) && entry.isValid();
	     --entryCount, entry.advance()) {
		DvbEpgEntry epgEntry;
		epgEntry.channelName = channelName;
		epgEntry.begin = baseDateTime.addSecs(entry.startTime());
		epgEntry.duration = QTime().addSecs(entry.duration());
		epgEntry.title = entry.title();

		quint32 id = ((quint32(eitEntry.sourceId) << 16) | quint32(entry.eventId()));
		QHash<quint32, DvbEpgEntry>::ConstIterator it = epgEntryMapping.constFind(id);

		if (it != epgEntryMapping.constEnd()) {
			if ((epgEntry.channelName == it->channelName) &&
			    (epgEntry.begin == it->begin) && (epgEntry.duration == it->duration) &&
			    (epgEntry.title == it->title)) {
				continue;
			}
		}

		epgEntryMapping.insert(id, epgEntry);
		epg->addEntry(epgEntry);
	}
}

void AtscEpgFilter::processEttSection(const char *data, int size, int crc)
{
	Q_UNUSED(crc) // TODO check crc: (a) valid (b) invalid, but occurs more than once --> ok
	AtscEttSection ettSection(data, size);

	if (!ettSection.isValid() || (ettSection.tableId() != 0xcc)) {
		return;
	}

	if (ettSection.messageType() != 0x02) {
		return;
	}

	quint32 id = ((quint32(ettSection.sourceId()) << 16) | quint32(ettSection.eventId()));
	QHash<quint32, DvbEpgEntry>::Iterator it = epgEntryMapping.find(id);

	if (it != epgEntryMapping.end()) {
		QString text = ettSection.text();

		if (it->details != text) {
			it->details = text;
			epg->addEntry(*it);
		}
	}
}
