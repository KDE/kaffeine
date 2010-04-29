/*
 * dvbepg.cpp
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#include <QBoxLayout>
#include <QFile>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QScrollArea>
#include <QStringListModel>
#include <KAction>
#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include <KStandardDirs>
#include "dvbchannelui.h"
#include "dvbmanager.h"
#include "dvbrecording.h"

class DvbEitEntry
{
public:
	QString source;
	int transportStreamId;
	int serviceId;
	int networkId;

	bool operator<(const DvbEitEntry &x) const
	{
		if (source != x.source) {
			return source < x.source;
		}

		if (transportStreamId != x.transportStreamId) {
			return transportStreamId < x.transportStreamId;
		}

		if (serviceId != x.serviceId) {
			return serviceId < x.serviceId;
		}

		return (networkId != -1) && (x.networkId != -1) && (networkId < x.networkId);
	}
};

class DvbEpgEntryLess
{
public:
	bool operator()(const DvbEpgEntry &x, const QString &y);
	bool operator()(const QString &x, const DvbEpgEntry &y);
};

bool DvbEpgEntryLess::operator()(const DvbEpgEntry &x, const QString &y)
{
	return x.channel < y;
}

bool DvbEpgEntryLess::operator()(const QString &x, const DvbEpgEntry &y)
{
	return x < y.channel;
}

DvbEpgModel::DvbEpgModel(DvbManager *manager_) : QAbstractTableModel(manager_), manager(manager_)
{
	QFile file(KStandardDirs::locateLocal("appdata", "epgdata.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	bool hasRecordingIndex = true;
	int version;
	stream >> version;

	if (version == 0x1ce0eca7) {
		hasRecordingIndex = false;
	} else if (version != 0x79cffd36) {
		kWarning() << "wrong version" << file.fileName();
		return;
	}

	QDateTime currentDateTime = QDateTime::currentDateTime();

	while (!stream.atEnd()) {
		DvbEpgEntry entry;
		stream >> entry.channel;
		stream >> entry.begin;
		stream >> entry.duration;
		entry.end = entry.begin.addMSecs(QTime().msecsTo(entry.duration));
		stream >> entry.title;
		stream >> entry.subheading;
		stream >> entry.details;

		if (hasRecordingIndex) {
			stream >> entry.recordingIndex.key;
		}

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "corrupt data" << file.fileName();
			break;
		}

		if (entry.end < currentDateTime) {
			continue;
		}

		allEntries.append(entry);

		if (entry.recordingIndex.isValid()) {
			recordingIndexes.insert(entry.recordingIndex, &allEntries.last());
		}
	}

	qSort(allEntries);
	connect(manager->getRecordingModel(), SIGNAL(programRemoved(DvbRecordingIndex)),
		this, SLOT(programRemoved(DvbRecordingIndex)));
}

DvbEpgModel::~DvbEpgModel()
{
	QFile file(KStandardDirs::locateLocal("appdata", "epgdata.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	int version = 0x79cffd36;
	stream << version;

	QDateTime currentDateTime = QDateTime::currentDateTime();

	foreach (const DvbEpgEntry &entry, allEntries) {
		if (entry.end < currentDateTime) {
			continue;
		}

		stream << entry.channel;
		stream << entry.begin;
		stream << entry.duration;
		stream << entry.title;
		stream << entry.subheading;
		stream << entry.details;
		stream << entry.recordingIndex.key;
	}
}

int DvbEpgModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 3;
}

int DvbEpgModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return filteredEntries.size();
}

QVariant DvbEpgModel::data(const QModelIndex &index, int role) const
{
	switch (role) {
	case Qt::DecorationRole:
		if (index.column() == 2) {
			const DvbEpgEntry *entry = filteredEntries.at(index.row());

			if (entry->recordingIndex.isValid()) {
				return KIcon("media-record");
			}
		}

		break;
	case Qt::DisplayRole: {
		const DvbEpgEntry *entry = filteredEntries.at(index.row());

		switch (index.column()) {
		case 0:
			return KGlobal::locale()->formatDateTime(entry->begin.toLocalTime());
		case 1:
			return KGlobal::locale()->formatTime(entry->duration, false, true);
		case 2:
			return entry->title;
		}

		break;
	    }
	}

	return QVariant();
}

QVariant DvbEpgModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18n("Begin");
	case 1:
		return i18n("Duration");
	case 2:
		return i18n("Title");
	}

	return QVariant();
}

void DvbEpgModel::addEntry(const DvbEpgEntry &entry)
{
	QList<DvbEpgEntry>::const_iterator it = qLowerBound(allEntries, entry);

	if ((it != allEntries.constEnd()) &&
	    (it->channel == entry.channel) &&
	    (it->begin == entry.begin)) {
		// FIXME handle overlap case better
		return;
	}

	allEntries.insert(it - allEntries.constBegin(), entry);
}

bool DvbEpgModel::contains(const QString &channel) const
{
	return qBinaryFind(allEntries.constBegin(), allEntries.constEnd(), channel,
		DvbEpgEntryLess()) != allEntries.constEnd();
}

void DvbEpgModel::resetChannel()
{
	filteredEntries.clear();
	reset();
}

void DvbEpgModel::setChannel(const QString &channel)
{
	QList<DvbEpgEntry>::iterator it = qLowerBound(allEntries.begin(), allEntries.end(),
		channel, DvbEpgEntryLess());
	QList<DvbEpgEntry>::iterator end = qUpperBound(it, allEntries.end(), channel,
		DvbEpgEntryLess());

	QDateTime currentDateTime = QDateTime::currentDateTime().toUTC();

	while ((it != end) && (it->end <= currentDateTime)) {
		++it;
	}

	filteredEntries.clear();

	while (it != end) {
		filteredEntries.append(&(*it));
		++it;
	}

	reset();
}

const DvbEpgEntry *DvbEpgModel::getEntry(int row) const
{
	return filteredEntries.at(row);
}

QList<DvbEpgEntry> DvbEpgModel::getCurrentNext(const QString &channel) const
{
	QList<DvbEpgEntry>::const_iterator it = qLowerBound(allEntries.constBegin(),
		allEntries.constEnd(), channel, DvbEpgEntryLess());
	QList<DvbEpgEntry>::const_iterator end = qUpperBound(it, allEntries.constEnd(), channel,
		DvbEpgEntryLess());

	QDateTime currentDateTime = QDateTime::currentDateTime().toUTC();

	while ((it != end) && (it->end <= currentDateTime)) {
		++it;
	}

	QList<DvbEpgEntry> result;

	while ((it != end) && (result.size() < 2)) {
		result.append(*it);
		++it;
	}

	return result;
}

void DvbEpgModel::scheduleProgram(int row, int extraSecondsBefore, int extraSecondsAfter)
{
	DvbEpgEntry *entry = filteredEntries.at(row);

	if (!entry->recordingIndex.isValid()) {
		entry->recordingIndex = manager->getRecordingModel()->scheduleProgram(entry->title,
			entry->channel,
			entry->begin.toLocalTime().addSecs(-extraSecondsBefore),
			entry->duration.addSecs(extraSecondsBefore + extraSecondsAfter));
		recordingIndexes.insert(entry->recordingIndex, entry);
	} else {
		recordingIndexes.remove(entry->recordingIndex);
		manager->getRecordingModel()->removeProgram(entry->recordingIndex);
		entry->recordingIndex = DvbRecordingIndex();
	}

	QModelIndex modelIndex = index(row, 2);
	emit dataChanged(modelIndex, modelIndex);
}

void DvbEpgModel::programRemoved(const DvbRecordingIndex &recordingIndex)
{
	DvbEpgEntry *entry = recordingIndexes.value(recordingIndex, NULL);

	if (entry != NULL) {
		entry->recordingIndex = DvbRecordingIndex();
		int row = filteredEntries.indexOf(entry);

		if (row >= 0) {
			QModelIndex modelIndex = index(row, 2);
			emit dataChanged(modelIndex, modelIndex);
		}
	}
}

DvbEitFilter::DvbEitFilter() : manager(NULL), model(NULL)
{
}

DvbEitFilter::~DvbEitFilter()
{
}

void DvbEitFilter::setManager(DvbManager *manager_)
{
	manager = manager_;
	model = manager->getEpgModel();
	channelMapping.clear();

	foreach (const QSharedDataPointer<DvbChannel> &channel,
		 manager->getChannelModel()->getChannels()) {
		DvbEitEntry entry;
		entry.source = channel->source;
		entry.transportStreamId = channel->transportStreamId;
		entry.serviceId = channel->getServiceId();
		entry.networkId = channel->networkId;
		channelMapping.insert(entry, channel->name);
	}

	// FIXME monitor the channel model?
}

void DvbEitFilter::setSource(const QString &source_)
{
	source = source_;
}

QTime DvbEitFilter::bcdToTime(int bcd)
{
	return QTime(((bcd >> 20) & 0x0f) * 10 + ((bcd >> 16) & 0x0f),
		     ((bcd >> 12) & 0x0f) * 10 + ((bcd >> 8) & 0x0f),
		     ((bcd >> 4) & 0x0f) * 10 + (bcd & 0x0f));
}

void DvbEitFilter::processSection(const QByteArray &data)
{
	DvbEitSection eitSection(data);

	if (!eitSection.isValid() ||
	    (eitSection.tableId() < 0x4e) || (eitSection.tableId() > 0x6f)) {
		return;
	}

	int transportStreamId = eitSection.transportStreamId();
	int serviceId = eitSection.serviceId();
	int networkId = eitSection.originalNetworkId();

	for (DvbEitSectionEntry entry = eitSection.entries(); entry.isValid(); entry.advance()) {
		DvbEitEntry eitEntry;
		eitEntry.source = source;
		eitEntry.transportStreamId = transportStreamId;
		eitEntry.serviceId = serviceId;
		eitEntry.networkId = networkId;

		QString channel = channelMapping.value(eitEntry);

		if (channel.isEmpty()) {
			continue;
		}

		DvbEpgEntry epgEntry;
		epgEntry.channel = channel;
		epgEntry.begin = QDateTime(QDate::fromJulianDay(entry.startDate() + 2400001),
					   bcdToTime(entry.startTime()), Qt::UTC);
		epgEntry.duration = bcdToTime(entry.duration());
		epgEntry.end = epgEntry.begin.addSecs(QTime().secsTo(epgEntry.duration));

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

		model->addEntry(epgEntry);
	}
}

DvbEpgDialog::DvbEpgDialog(DvbManager *manager_, const QString &currentChannel, QWidget *parent) :
	KDialog(parent), manager(manager_)
{
	setButtons(KDialog::Close);
	setCaption(i18n("Program Guide"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout(widget);

	QBoxLayout *boxLayout = new QVBoxLayout();

	QPushButton *pushButton = new QPushButton(KIcon("view-refresh"), i18nc("epg", "Refresh"),
		this);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(refresh()));
	boxLayout->addWidget(pushButton);

	channelModel = new QStringListModel(this);
	channelView = new QListView(widget);
	channelView->setModel(channelModel);
	channelView->setMaximumWidth(200);
	connect(channelView, SIGNAL(activated(QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	boxLayout->addWidget(channelView);
	mainLayout->addLayout(boxLayout);

	boxLayout = new QVBoxLayout();

	KAction *scheduleAction = new KAction(KIcon("media-record"),
		i18nc("program guide", "Schedule Program"), this);
	connect(scheduleAction, SIGNAL(triggered()), this, SLOT(scheduleProgram()));

	pushButton = new QPushButton(KIcon("media-record"),
		i18nc("program guide", "Schedule Program"), this);
	pushButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	connect(pushButton, SIGNAL(clicked()), this, SLOT(scheduleProgram()));
	boxLayout->addWidget(pushButton);

	epgModel = manager->getEpgModel();
	epgView = new QTreeView(widget);
	epgView->addAction(scheduleAction);
	epgView->setContextMenuPolicy(Qt::ActionsContextMenu);
	epgView->setMinimumWidth(450);
	epgView->setModel(epgModel);
	epgView->setRootIsDecorated(false);
	connect(epgView, SIGNAL(activated(QModelIndex)), this, SLOT(entryActivated(QModelIndex)));
	boxLayout->addWidget(epgView);

	contentLabel = new QLabel(widget);
	contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	contentLabel->setMargin(5);
	contentLabel->setWordWrap(true);

	QScrollArea *scrollArea = new QScrollArea(widget);
	scrollArea->setBackgroundRole(QPalette::Light);
	scrollArea->setMinimumHeight(175);
	scrollArea->setWidget(contentLabel);
	scrollArea->setWidgetResizable(true);
	boxLayout->addWidget(scrollArea);
	mainLayout->addLayout(boxLayout);

	refresh();

	int currentRow = channelModel->stringList().indexOf(currentChannel);

	if (currentRow != -1) {
		QModelIndex index = channelModel->index(currentRow, 0);
		channelView->setCurrentIndex(index);
		channelActivated(index);
	} else {
		epgModel->resetChannel();
	}

	setMainWidget(widget);
}

DvbEpgDialog::~DvbEpgDialog()
{
}

static bool localeAwareLessThan(const QString &x, const QString &y)
{
	return x.localeAwareCompare(y) < 0;
}

void DvbEpgDialog::refresh()
{
	QString currentChannel;
	QModelIndex currentIndex = channelView->currentIndex();

	if (currentIndex.isValid()) {
		currentChannel = channelModel->stringList().at(currentIndex.row());
	}

	QStringList channels;

	foreach (const QSharedDataPointer<DvbChannel> &channel,
		 manager->getChannelModel()->getChannels()) {
		if (epgModel->contains(channel->name)) {
			channels.append(channel->name);
		}
	}

	qSort(channels.begin(), channels.end(), localeAwareLessThan);
	channelModel->setStringList(channels);

	if (!currentChannel.isEmpty()) {
		int index = channelModel->stringList().indexOf(currentChannel);

		if (index != -1) {
			currentIndex = channelModel->index(index, 0);
			channelView->setCurrentIndex(currentIndex);
			channelActivated(currentIndex);
		}
	}
}

void DvbEpgDialog::channelActivated(const QModelIndex &index)
{
	epgModel->setChannel(channelModel->stringList().at(index.row()));
	epgView->resizeColumnToContents(0);
	epgView->resizeColumnToContents(1);

	if (epgModel->rowCount(QModelIndex()) >= 1) {
		QModelIndex index = epgModel->index(0, 0);
		epgView->setCurrentIndex(index);
		entryActivated(index);
	} else {
		contentLabel->setText(QString());
	}
}

void DvbEpgDialog::entryActivated(const QModelIndex &index)
{
	const DvbEpgEntry *entry = epgModel->getEntry(index.row());
	QString text = i18n("<font color=#008000 size=\"+1\">%1</font><br>", entry->title);

	if (!entry->subheading.isEmpty()) {
		text += i18n("<font color=#808000>%1</font><br>", entry->subheading);
	}

	text += i18n("<font color=#800000>%1 - %2</font><br><br>",
		   KGlobal::locale()->formatDateTime(entry->begin.toLocalTime(), KLocale::LongDate),
		   KGlobal::locale()->formatTime(entry->end.toLocalTime().time()));

	text += entry->details;
	contentLabel->setText(text);
}

void DvbEpgDialog::scheduleProgram()
{
	QModelIndex index = epgView->currentIndex();

	if (index.isValid()) {
		epgModel->scheduleProgram(index.row(), 300, 600);
	}
}
