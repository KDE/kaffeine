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
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <KAction>
#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include "dvbchannelui.h"
#include "dvbmanager.h"

class DvbEitEntry
{
public:
	DvbEitEntry() : networkId(0), transportStreamId(0), serviceId(0) { }
	explicit DvbEitEntry(const DvbChannel *channel);
	~DvbEitEntry() { }

	bool operator==(const DvbEitEntry &other) const
	{
		return ((source == other.source) && (networkId == other.networkId) &&
			(transportStreamId == other.transportStreamId) &&
			(serviceId == other.serviceId));
	}

	QString source;
	int networkId;
	int transportStreamId;
	int serviceId;
};

DvbEitEntry::DvbEitEntry(const DvbChannel *channel)
{
	source = channel->source;
	networkId = channel->networkId;
	transportStreamId = channel->transportStreamId;
	serviceId = channel->getServiceId();
}

static uint qHash(const DvbEitEntry &eitEntry)
{
	return (qHash(eitEntry.source) ^ (uint(eitEntry.networkId) << 5) ^
		(uint(eitEntry.transportStreamId) << 10) ^ (uint(eitEntry.serviceId) << 15));
}

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
	channelModel = manager->getChannelModel();
	connect(channelModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
		this, SLOT(channelDataChanged(QModelIndex,QModelIndex)));
	connect(channelModel, SIGNAL(layoutChanged()), this, SLOT(channelLayoutChanged()));
	connect(channelModel, SIGNAL(modelReset()), this, SLOT(channelModelReset()));
	connect(channelModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
		this, SLOT(channelRowsInserted(QModelIndex,int,int)));
	connect(channelModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
		this, SLOT(channelRowsRemoved(QModelIndex,int,int)));

	if (channelModel->rowCount() > 0) {
		channelRowsInserted(QModelIndex(), 0, channelModel->rowCount() - 1);
	}

	epgChannelModel = new QStringListModel(this);

	connect(manager->getRecordingModel(), SIGNAL(programRemoved(DvbRecordingKey)),
		this, SLOT(programRemoved(DvbRecordingKey)));

	QFile file(KStandardDirs::locateLocal("appdata", "epgdata.dvb"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "cannot open" << file.fileName();
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

		if (hasRecordingKey) {
			stream >> entry.recordingKey.key;
		}

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "corrupt data" << file.fileName();
			break;
		}

		if (entry.end < currentDateTime) {
			continue;
		}

		allEntries.append(entry);

		if (entry.recordingKey.isValid()) {
			recordingKeys.insert(entry.recordingKey, &allEntries.last());
		}
	}

	qSort(allEntries);

	foreach (const DvbEpgEntry &entry, allEntries) {
		if (!epgChannels.contains(entry.channel)) {
			epgChannels.insert(entry.channel);
			int row = epgChannelModel->rowCount();
			epgChannelModel->insertRow(row);
			epgChannelModel->setData(epgChannelModel->index(row, 0), entry.channel);
		}
	}
}

DvbEpgModel::~DvbEpgModel()
{
	QFile file(KStandardDirs::locateLocal("appdata", "epgdata.dvb"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "cannot open" << file.fileName();
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
		stream << entry.recordingKey.key;
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

			if (entry->recordingKey.isValid()) {
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

	if (!epgChannels.contains(entry.channel)) {
		epgChannels.insert(entry.channel);
		int row = epgChannelModel->rowCount();
		epgChannelModel->insertRow(row);
		epgChannelModel->setData(epgChannelModel->index(row, 0), entry.channel);
	}
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

QAbstractItemModel *DvbEpgModel::createProxyEpgChannelModel(QObject *parent)
{
	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(parent);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);
	proxyModel->setSourceModel(epgChannelModel);
	return proxyModel;
}

const DvbEpgEntry *DvbEpgModel::getEntry(int row) const
{
	return filteredEntries.at(row);
}

const DvbChannel *DvbEpgModel::findChannelByEitEntry(const DvbEitEntry &eitEntry)
{
	return eitMapping.value(eitEntry, NULL);
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

	if (!entry->recordingKey.isValid()) {
		DvbRecordingEntry recordingEntry;
		recordingEntry.name = entry->title;
		recordingEntry.channelName = entry->channel;
		recordingEntry.begin = entry->begin.addSecs(-extraSecondsBefore);
		recordingEntry.duration =
			entry->duration.addSecs(extraSecondsBefore + extraSecondsAfter);
		entry->recordingKey =
			manager->getRecordingModel()->scheduleProgram(recordingEntry);
		recordingKeys.insert(entry->recordingKey, entry);
	} else {
		recordingKeys.remove(entry->recordingKey);
		manager->getRecordingModel()->removeProgram(entry->recordingKey);
		entry->recordingKey = DvbRecordingKey();
	}

	QModelIndex modelIndex = index(row, 2);
	emit dataChanged(modelIndex, modelIndex);
}

void DvbEpgModel::channelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	for (int currentRow = topLeft.row(); currentRow <= bottomRight.row(); ++currentRow) {
		eitMapping.remove(DvbEitEntry(channels.at(currentRow).constData()));
		const DvbChannel *channel = channelModel->data(channelModel->index(currentRow, 0),
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
		channels.replace(currentRow,
			QExplicitlySharedDataPointer<const DvbChannel>(channel));
		eitMapping.insert(DvbEitEntry(channel), channel);
	}
}

void DvbEpgModel::channelLayoutChanged()
{
	// not supported
	Q_ASSERT(false);
}

void DvbEpgModel::channelModelReset()
{
	// not supported
	Q_ASSERT(false);
}

void DvbEpgModel::channelRowsInserted(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)

	for (int currentRow = start; currentRow <= end; ++currentRow) {
		const DvbChannel *channel = channelModel->data(channelModel->index(currentRow, 0),
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
		channels.insert(currentRow,
			QExplicitlySharedDataPointer<const DvbChannel>(channel));
		eitMapping.insert(DvbEitEntry(channel), channel);
	}
}

void DvbEpgModel::channelRowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)

	for (int currentRow = start; currentRow <= end; ++currentRow) {
		eitMapping.remove(DvbEitEntry(channels.at(currentRow).constData()));
	}

	channels.erase(channels.begin() + start, channels.begin() + end + 1);
}

void DvbEpgModel::programRemoved(const DvbRecordingKey &recordingKey)
{
	DvbEpgEntry *entry = recordingKeys.value(recordingKey, NULL);

	if (entry != NULL) {
		entry->recordingKey = DvbRecordingKey();
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

		const DvbChannel *channel = model->findChannelByEitEntry(eitEntry);

		if (channel == NULL) {
			eitEntry.networkId = -1;
			channel = model->findChannelByEitEntry(eitEntry);
		}

		if (channel == NULL) {
			continue;
		}

		DvbEpgEntry epgEntry;
		epgEntry.channel = channel->name;
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

	epgModel = manager->getEpgModel();

	channelView = new QListView(widget);
	channelView->setEditTriggers(QListView::NoEditTriggers);
	QAbstractItemModel *channelModel = epgModel->createProxyEpgChannelModel(channelView);
	channelModel->sort(0, Qt::AscendingOrder);
	channelView->setModel(channelModel);
	channelView->setMaximumWidth(200);
	connect(channelView, SIGNAL(activated(QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	mainLayout->addWidget(channelView);

	QBoxLayout *boxLayout = new QVBoxLayout();

	KAction *scheduleAction = new KAction(KIcon("media-record"),
		i18nc("program guide", "Schedule Program"), this);
	connect(scheduleAction, SIGNAL(triggered()), this, SLOT(scheduleProgram()));

	QPushButton *pushButton = new QPushButton(KIcon("media-record"),
		i18nc("program guide", "Schedule Program"), this);
	pushButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	connect(pushButton, SIGNAL(clicked()), this, SLOT(scheduleProgram()));
	boxLayout->addWidget(pushButton);

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

	QModelIndexList currentIndexes = channelModel->match(channelModel->index(0, 0),
		Qt::DisplayRole, currentChannel, 1, Qt::MatchExactly);

	if (!currentIndexes.isEmpty()) {
		QModelIndex index = currentIndexes.at(0);
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

void DvbEpgDialog::channelActivated(const QModelIndex &index)
{
	epgModel->setChannel(channelView->model()->data(index).toString());
	epgView->resizeColumnToContents(0);
	epgView->resizeColumnToContents(1);

	if (epgModel->rowCount() >= 1) {
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
		epgModel->scheduleProgram(index.row(), manager->getBeginMargin(),
			manager->getEndMargin());
	}
}
