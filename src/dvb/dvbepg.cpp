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
#include <QLabel>
#include <QListView>
#include <QScrollArea>
#include <KLocale>
#include "dvbchannelui.h"
#include "dvbmanager.h"

class DvbEpgChannelModel : public QAbstractListModel
{
public:
	DvbEpgChannelModel(const QList<QSharedDataPointer<DvbChannel> > &channels_,
		QObject *parent);
	~DvbEpgChannelModel();

	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;

	QSharedDataPointer<DvbChannel> getChannel(int row) const;
	int indexOf(const QSharedDataPointer<DvbChannel> &channel) const;

private:
	QList<QSharedDataPointer<DvbChannel> > channels;
};

static bool localeAwareLessThan(const QSharedDataPointer<DvbChannel> &x,
	const QSharedDataPointer<DvbChannel> &y)
{
	return x->name.localeAwareCompare(y->name) < 0;
}

DvbEpgChannelModel::DvbEpgChannelModel(const QList<QSharedDataPointer<DvbChannel> > &channels_,
	QObject *parent) : QAbstractListModel(parent), channels(channels_)
{
	qSort(channels.begin(), channels.end(), localeAwareLessThan);
}

DvbEpgChannelModel::~DvbEpgChannelModel()
{
}

int DvbEpgChannelModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return channels.size();
}

QVariant DvbEpgChannelModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || (role != Qt::DisplayRole) || (index.column() != 0) ||
	    (index.row() >= channels.size())) {
		return QVariant();
	}

	return channels.at(index.row())->name;
}

QSharedDataPointer<DvbChannel> DvbEpgChannelModel::getChannel(int row) const
{
	return channels.at(row);
}

int DvbEpgChannelModel::indexOf(const QSharedDataPointer<DvbChannel> &channel) const
{
	return channels.indexOf(channel);
}

class DvbEpgEntryLess
{
public:
	bool operator()(const DvbEpgEntry &x, const DvbEpgEntry &y);
	bool operator()(const DvbEpgEntry &entry, const QSharedDataPointer<DvbChannel> &channel);
	bool operator()(const QSharedDataPointer<DvbChannel> &channel, const DvbEpgEntry &entry);
};

bool DvbEpgEntryLess::operator()(const DvbEpgEntry &x, const DvbEpgEntry &y)
{
	if (x.source != y.source) {
		return x.source < y.source;
	}

	if (x.serviceId != y.serviceId) {
		return x.serviceId < y.serviceId;
	}

	if (x.transportStreamId != y.transportStreamId) {
		return x.transportStreamId < y.transportStreamId;
	}

	if (x.networkId != y.networkId) {
		return x.networkId < y.networkId;
	}

	return x.begin < y.begin;
}

bool DvbEpgEntryLess::operator()(const DvbEpgEntry &entry,
	const QSharedDataPointer<DvbChannel> &channel)
{
	if (entry.source != channel->source) {
		return entry.source < channel->source;
	}

	if (entry.serviceId != channel->serviceId) {
		return entry.serviceId < channel->serviceId;
	}

	if (entry.transportStreamId != channel->transportStreamId) {
		return entry.transportStreamId < channel->transportStreamId;
	}

	return (entry.networkId < channel->networkId) && (channel->networkId != -1);
}

bool DvbEpgEntryLess::operator()(const QSharedDataPointer<DvbChannel> &channel,
	const DvbEpgEntry &entry)
{
	if (channel->source != entry.source) {
		return channel->source < entry.source;
	}

	if (channel->serviceId != entry.serviceId) {
		return channel->serviceId < entry.serviceId;
	}

	if (channel->transportStreamId != entry.transportStreamId) {
		return channel->transportStreamId < entry.transportStreamId;
	}

	return (channel->networkId < entry.networkId) && (channel->networkId != -1);
}

DvbEpgModel::DvbEpgModel(QObject *parent) : QAbstractTableModel(parent)
{
}

DvbEpgModel::~DvbEpgModel()
{
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
	if (!index.isValid() || (role != Qt::DisplayRole) ||
	    (index.row() >= filteredEntries.size())) {
		return QVariant();
	}

	const DvbEpgEntry *entry = filteredEntries.at(index.row());

	switch (index.column()) {
	case 0:
		return KGlobal::locale()->formatDateTime(entry->begin.toLocalTime());
	case 1:
		return KGlobal::locale()->formatTime(entry->duration, false, true);
	case 2:
		return entry->title;
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
	QList<DvbEpgEntry>::const_iterator it = qLowerBound(allEntries.constBegin(),
		allEntries.constEnd(), entry, DvbEpgEntryLess());

	if ((it != allEntries.constEnd()) &&
	    (it->source == entry.source) &&
	    (it->networkId == entry.networkId) &&
	    (it->transportStreamId == entry.transportStreamId) &&
	    (it->serviceId == entry.serviceId) &&
	    (it->begin == entry.begin)) {
		// FIXME handle overlap case better
		return;
	}

	allEntries.insert(it - allEntries.constBegin(), entry);
	// FIXME update filtered entries as well?
}

bool DvbEpgModel::contains(const QSharedDataPointer<DvbChannel> &channel)
{
	return (qBinaryFind(allEntries.constBegin(), allEntries.constEnd(), channel,
		DvbEpgEntryLess()) != allEntries.constEnd());
}

void DvbEpgModel::resetChannel()
{
	filteredEntries.clear();
	reset();
}

void DvbEpgModel::setChannel(const QSharedDataPointer<DvbChannel> &channel)
{
	// FIXME eliminate entries which are too old

	QList<DvbEpgEntry>::const_iterator it = qLowerBound(allEntries.constBegin(),
		allEntries.constEnd(), channel, DvbEpgEntryLess());
	QList<DvbEpgEntry>::const_iterator end = qUpperBound(allEntries.constBegin(),
		allEntries.constEnd(), channel, DvbEpgEntryLess());

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

DvbEitFilter::DvbEitFilter(const QString &source_, DvbEpgModel *model_) : source(source_),
	model(model_)
{
}

DvbEitFilter::~DvbEitFilter()
{
}

QTime DvbEitFilter::bcdToTime(int bcd)
{
	return QTime(((bcd >> 20) & 0x0f) * 10 + ((bcd >> 16) & 0x0f),
		     ((bcd >> 12) & 0x0f) * 10 + ((bcd >> 8) & 0x0f),
		     ((bcd >> 4) & 0x0f) * 10 + (bcd & 0x0f));
}

void DvbEitFilter::processSection(const DvbSectionData &data)
{
	DvbSection section(data);

	if (!section.isValid()) {
		return;
	}

	int tableId = section.tableId();

	if ((tableId < 0x4e) || (tableId > 0x6f)) {
		return;
	}

	DvbEitSection eitSection(section);

	if (!eitSection.isValid()) {
		return;
	}

	int serviceId = eitSection.serviceId();
	int transportStreamId = eitSection.transportStreamId();
	int networkId = eitSection.originalNetworkId();

	for (DvbEitSectionEntry entry = eitSection.entries(); entry.isValid(); entry.advance()) {
		DvbEpgEntry epgEntry;
		epgEntry.source = source;
		epgEntry.networkId = networkId;
		epgEntry.transportStreamId = transportStreamId;
		epgEntry.serviceId = serviceId;

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

		model->addEntry(epgEntry);
	}
}

DvbEpgDialog::DvbEpgDialog(DvbManager *manager,
	const QSharedDataPointer<DvbChannel> &currentChannel, QWidget *parent) : KDialog(parent)
{
	setButtons(KDialog::Close);
	setCaption(i18n("Program Guide"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout(widget);

	QBoxLayout *boxLayout = new QVBoxLayout();

	epgModel = manager->getEpgModel();
	QList<QSharedDataPointer<DvbChannel> > channels;

	foreach (const QSharedDataPointer<DvbChannel> &channel,
		 manager->getChannelModel()->getChannels()) {
		if (epgModel->contains(channel)) {
			channels.append(channel);
		}
	}

	channelModel = new DvbEpgChannelModel(channels, this);

	QListView *channelView = new QListView(widget);
	channelView->setModel(channelModel);
	channelView->setMaximumWidth(200);
	connect(channelView, SIGNAL(activated(QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	boxLayout->addWidget(channelView);
	mainLayout->addLayout(boxLayout);

	boxLayout = new QVBoxLayout();

	epgView = new QTreeView(widget);
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

	int currentRow = channelModel->indexOf(currentChannel);

	if (currentRow != -1) {
		channelView->setCurrentIndex(channelModel->index(currentRow, 0));
		epgModel->setChannel(currentChannel);
		epgView->resizeColumnToContents(0);
		epgView->resizeColumnToContents(1);
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
	QSharedDataPointer<DvbChannel> channel = channelModel->getChannel(index.row());

	if (channel == NULL) {
		return;
	}

	epgModel->setChannel(channel);
	epgView->resizeColumnToContents(0);
	epgView->resizeColumnToContents(1);
	contentLabel->setText(QString());
}

void DvbEpgDialog::entryActivated(const QModelIndex &index)
{
	const DvbEpgEntry *entry = epgModel->getEntry(index.row());
	QString text = i18n("<font color=#008000 size=\"+1\">%1</font><br>", entry->title);

	if (!entry->subheading.isEmpty()) {
		text += i18n("<font color=#808000>%1</font><br>", entry->subheading);
	}

	QString begin = KGlobal::locale()->formatDateTime(entry->begin, KLocale::LongDate);
	QString end = KGlobal::locale()->formatTime(entry->begin.time().addSecs(QTime().secsTo(entry->duration)));

	text += i18n("<font color=#800000>%1 - %2</font><br><br>", begin, end);

	text += entry->details;
	contentLabel->setText(text);
}
