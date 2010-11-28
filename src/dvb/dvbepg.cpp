/*
 * dvbepg.cpp
 *
 * Copyright (C) 2009-2010 Christoph Pfister <christophpfister@gmail.com>
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

#include <QBoxLayout>
#include <QCoreApplication>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <KAction>
#include <KDebug>
#include <KLineEdit>
#include <KLocale>
#include <KStandardDirs>
#include "dvbchannel.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbsi.h"

int DvbEpgEntry::compare(const DvbEpgEntry &other) const
{
	int result = channelName.compare(other.channelName);

	if (result != 0) {
		return result;
	}

	if (begin != other.begin) {
		if (begin < other.begin) {
			return -1;
		} else {
			return 1;
		}
	}

	if (duration != other.duration) {
		if (duration < other.duration) {
			return -1;
		} else {
			return 1;
		}
	}

	result = title.compare(other.title);

	if (result != 0) {
		return result;
	}

	result = subheading.compare(other.subheading);

	if (result != 0) {
		return result;
	}

	return details.compare(other.details);
}

DvbEpgModel::DvbEpgModel(DvbManager *manager_, QObject *parent) : QObject(parent),
	manager(manager_)
{
	epgChannelModel = new DvbEpgChannelModel(this);

	DvbChannelModel *channelModel = manager->getChannelModel();
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

	connect(manager->getRecordingModel(), SIGNAL(programRemoved(DvbRecordingKey)),
		this, SLOT(programRemoved(DvbRecordingKey)));

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

	QDateTime currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	QSet<QString> channelNames;

	foreach (const QExplicitlySharedDataPointer<const DvbChannel> &channel, channels) {
		channelNames.insert(channel->name);
	}

	while (!stream.atEnd()) {
		DvbEpgEntry entry;
		stream >> entry.channelName;
		stream >> entry.begin;
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
			epgChannelModel->insertChannelName(entry.channelName);

			if (entry.recordingKey.isValid()) {
				recordingKeyMapping.insert(entry.recordingKey, &entries.last());
			}
		}
	}

	qSort(entries);
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

	QDateTime currentDateTimeUtc = QDateTime::currentDateTime().toUTC();

	foreach (const DvbEpgEntry &entry, entries) {
		if (entry.begin.addSecs(QTime().secsTo(entry.duration)) > currentDateTimeUtc) {
			stream << entry.channelName;
			stream << entry.begin;
			stream << entry.duration;
			stream << entry.title;
			stream << entry.subheading;
			stream << entry.details;
			stream << entry.recordingKey.key;
		}
	}
}

QList<DvbEpgEntry> DvbEpgModel::getCurrentNext(const QString &channelName) const
{
	QDateTime currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	QList<DvbEpgEntry> result;

	for (int row = (qLowerBound(entries.constBegin(), entries.constEnd(), channelName,
	     DvbEpgEntryLessThan()) - entries.constBegin()); row < entries.size(); ++row) {
		const DvbEpgEntry &entry = entries.at(row);

		if (entry.channelName != channelName) {
			break;
		}

		if (entry.begin.addSecs(QTime().secsTo(entry.duration)) <= currentDateTimeUtc) {
			continue;
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
	// TODO deal with ATSC epg
	dvbEitFilters.append(DvbEitFilter(this));
	DvbEitFilter &eitFilter = dvbEitFilters.last();
	eitFilter.device = device;
	eitFilter.source = channel->source;
	eitFilter.transponder = channel->transponder;
	eitFilter.device->addSectionFilter(0x12, &eitFilter);
}

void DvbEpgModel::stopEventFilter(DvbDevice *device, const DvbChannel *channel)
{
	// TODO deal with ATSC epg

	for (int i = 0; i < dvbEitFilters.size(); ++i) {
		const DvbEitFilter &constEitFilter = dvbEitFilters.at(i);

		if ((constEitFilter.device == device) &&
		    (constEitFilter.source == channel->source) &&
		    (constEitFilter.transponder.corresponds(channel->transponder))) {
			DvbEitFilter &eitFilter = dvbEitFilters[i];
			eitFilter.device->removeSectionFilter(0x12, &eitFilter);
			dvbEitFilters.removeAt(i);
			return;
		}
	}
}

void DvbEpgModel::showDialog(const QString &currentChannelName, QWidget *parent)
{
	KDialog *dialog = new DvbEpgDialog(manager, currentChannelName, parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

QAbstractItemModel *DvbEpgModel::createEpgProxyChannelModel(QObject *parent)
{
	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(parent);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);
	proxyModel->setSourceModel(epgChannelModel);
	return proxyModel;
}

DvbEpgProxyModel *DvbEpgModel::createEpgProxyModel(QObject *parent)
{
	DvbEpgProxyModel *proxyModel = new DvbEpgProxyModel(this, &entries, parent);
	return proxyModel;
}

void DvbEpgModel::scheduleProgram(const DvbEpgEntry &constEntry, int extraSecondsBefore,
	int extraSecondsAfter)
{
	int row = (qBinaryFind(entries, constEntry) - entries.constBegin());

	if (row < entries.size()) {
		DvbEpgEntry &entry = entries[row];

		if (!entry.recordingKey.isValid()) {
			DvbRecordingEntry recordingEntry;
			recordingEntry.name = entry.title;
			recordingEntry.channelName = entry.channelName;
			recordingEntry.begin = entry.begin.addSecs(-extraSecondsBefore);
			recordingEntry.duration =
				entry.duration.addSecs(extraSecondsBefore + extraSecondsAfter);
			entry.recordingKey =
				manager->getRecordingModel()->scheduleProgram(recordingEntry);
			recordingKeyMapping.insert(entry.recordingKey, &entry);
		} else {
			recordingKeyMapping.remove(entry.recordingKey);
			manager->getRecordingModel()->removeProgram(entry.recordingKey);
			entry.recordingKey = DvbRecordingKey();
		}

		emit entryRecordingKeyChanged(&entry);
	}
}

QString DvbEpgModel::findChannelNameByDvbEitEntry(const DvbEitEntry &eitEntry)
{
	const DvbChannel *channel = dvbEitMapping.value(eitEntry, NULL);

	if (channel != NULL) {
		return channel->name;
	}

	return QString();
}

void DvbEpgModel::addEntry(const DvbEpgEntry &epgEntry)
{
	int row = (qLowerBound(entries, epgEntry) - entries.constBegin());

	if ((row >= entries.size()) || (entries.at(row) != epgEntry)) {
		entries.insert(row, epgEntry);
		epgChannelModel->insertChannelName(epgEntry.channelName);
		emit entryAdded(&entries.at(row));
	}
}

void DvbEpgModel::channelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	DvbChannelModel *channelModel = manager->getChannelModel();

	for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
		dvbEitMapping.remove(DvbEitEntry(channels.at(row).constData()));
		const DvbChannel *channel = channelModel->data(channelModel->index(row, 0),
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
		channels.replace(row, QExplicitlySharedDataPointer<const DvbChannel>(channel));
		dvbEitMapping.insert(DvbEitEntry(channel), channel);
		// TODO remove dead channels from entries; propagate changes to epgChannelModel
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
	DvbChannelModel *channelModel = manager->getChannelModel();

	for (int row = start; row <= end; ++row) {
		const DvbChannel *channel = channelModel->data(channelModel->index(row, 0),
			DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();
		channels.insert(row, QExplicitlySharedDataPointer<const DvbChannel>(channel));
		dvbEitMapping.insert(DvbEitEntry(channel), channel);
	}
}

void DvbEpgModel::channelRowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)

	for (int row = start; row <= end; ++row) {
		dvbEitMapping.remove(DvbEitEntry(channels.at(row).constData()));
	}

	channels.erase(channels.begin() + start, channels.begin() + end + 1);
	// TODO remove dead channels from entries; propagate changes to epgChannelModel
}

void DvbEpgModel::programRemoved(const DvbRecordingKey &recordingKey)
{
	const DvbEpgEntry *constEntry = recordingKeyMapping.value(recordingKey, NULL);

	if (constEntry != NULL) {
		int row = (qBinaryFind(entries, *constEntry) - entries.constBegin());

		if (row < entries.size()) {
			entries[row].recordingKey = DvbRecordingKey();
			emit entryRecordingKeyChanged(&entries.at(row));
		}
	}
}

DvbEpgChannelModel::DvbEpgChannelModel(QObject *parent) : QAbstractTableModel(parent)
{
}

DvbEpgChannelModel::~DvbEpgChannelModel()
{
}

void DvbEpgChannelModel::insertChannelName(const QString &channelName)
{
	int row = (qLowerBound(channelNames, channelName) - channelNames.constBegin());

	if ((row >= channelNames.size()) || (channelNames.at(row) != channelName)) {
		beginInsertRows(QModelIndex(), row, row);
		channelNames.insert(row, channelName);
		endInsertRows();
	}
}

void DvbEpgChannelModel::removeChannelName(const QString &channelName)
{
	int row = (qLowerBound(channelNames, channelName) - channelNames.constBegin());

	if ((row < channelNames.size()) && (channelNames.at(row) == channelName)) {
		beginRemoveRows(QModelIndex(), row, row);
		channelNames.removeAt(row);
		endRemoveRows();
	}
}

int DvbEpgChannelModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 1;
	}

	return 0;
}

int DvbEpgChannelModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return channelNames.size();
	}

	return 0;
}

QVariant DvbEpgChannelModel::data(const QModelIndex &index, int role) const
{
	if ((role == Qt::DisplayRole) && (index.column() == 0)) {
		return channelNames.at(index.row());
	}

	return QVariant();
}

QVariant DvbEpgChannelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((section == 0) && (orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		return i18nc("@title:column tv channel name", "Name");
	}

	return QVariant();
}

DvbEpgProxyModel::DvbEpgProxyModel(DvbEpgModel *epgModel_,
	const QList<DvbEpgEntry> *epgModelEntries_, QObject *parent) : QAbstractTableModel(parent),
	epgModel(epgModel_), epgModelEntries(epgModelEntries_), contentFilterEventPending(false)
{
	connect(epgModel, SIGNAL(entryAdded(const DvbEpgEntry*)),
		this, SLOT(sourceEntryAdded(const DvbEpgEntry*)));
	connect(epgModel, SIGNAL(entryRecordingKeyChanged(const DvbEpgEntry*)),
		this, SLOT(sourceEntryRecordingKeyChanged(const DvbEpgEntry*)));

	contentFilter.setCaseSensitivity(Qt::CaseInsensitive);
}

DvbEpgProxyModel::~DvbEpgProxyModel()
{
}

void DvbEpgProxyModel::setChannelNameFilter(const QString &channelName)
{
	channelNameFilter = channelName;
	contentFilter.setPattern(QString());

	if (!entries.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, entries.size() - 1);
		entries.clear();
		endRemoveRows();
	}

	QList<DvbEpgEntry>::ConstIterator it = qLowerBound(epgModelEntries->constBegin(),
		epgModelEntries->constEnd(), channelName, DvbEpgEntryLessThan());
	QList<DvbEpgEntry>::ConstIterator end = epgModelEntries->constEnd();
	QDateTime currentDateTimeUtc = QDateTime::currentDateTime().toUTC();

	while ((it != end) &&
	       (it->begin.addSecs(QTime().secsTo(it->duration)) <= currentDateTimeUtc)) {
		++it;
	}

	QList<DvbEpgEntry>::ConstIterator begin = it;

	while ((it != end) && (it->channelName == channelName)) {
		++it;
	}

	if (begin != it) {
		beginInsertRows(QModelIndex(), 0, it - begin - 1);

		while (begin != it) {
			entries.append(&(*begin));
			++begin;
		}

		endInsertRows();
	}
}

void DvbEpgProxyModel::scheduleProgram(int proxyRow, int extraSecondsBefore, int extraSecondsAfter)
{
	epgModel->scheduleProgram(*entries.at(proxyRow), extraSecondsBefore, extraSecondsAfter);
}

int DvbEpgProxyModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 4;
	}

	return 0;
}

int DvbEpgProxyModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return entries.size();
	}

	return 0;
}

QVariant DvbEpgProxyModel::data(const QModelIndex &index, int role) const
{
	const DvbEpgEntry *entry = entries.at(index.row());

	switch (role) {
	case Qt::DecorationRole:
		if ((index.column() == 2) && (entry->recordingKey.isValid())) {
			return KIcon("media-record");
		}

		break;
	case Qt::DisplayRole:
		switch (index.column()) {
		case 0:
			return KGlobal::locale()->formatDateTime(entry->begin.toLocalTime());
		case 1:
			return KGlobal::locale()->formatTime(entry->duration, false, true);
		case 2:
			return entry->title;
		case 3:
			return entry->channelName;
		}

		break;
	case DvbEpgEntryRole:
		return QVariant::fromValue(entry);
	}

	return QVariant();
}

QVariant DvbEpgProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		switch (section) {
		case 0:
			return i18nc("@title:column tv show", "Start");
		case 1:
			return i18nc("@title:column tv show", "Duration");
		case 2:
			return i18nc("@title:column tv show", "Title");
		case 3:
			return i18nc("@title:column tv show", "Channel");
		}
	}

	return QVariant();
}

void DvbEpgProxyModel::setContentFilter(const QString &pattern)
{
	channelNameFilter.clear();
	contentFilter.setPattern(pattern);

	if (!contentFilterEventPending) {
		contentFilterEventPending = true;
		QCoreApplication::postEvent(this, new QEvent(QEvent::User), Qt::LowEventPriority);
	}
}

void DvbEpgProxyModel::sourceEntryAdded(const DvbEpgEntry *entry)
{
	if ((entry->channelName == channelNameFilter) ||
	    (!contentFilter.pattern().isEmpty() &&
	     ((contentFilter.indexIn(entry->title) >= 0) ||
	      (contentFilter.indexIn(entry->subheading) >= 0) ||
	      (contentFilter.indexIn(entry->details) >= 0)))) {
		int proxyRow = (qLowerBound(entries.constBegin(), entries.constEnd(), entry,
			DvbEpgEntryLessThan()) - entries.constBegin());
		beginInsertRows(QModelIndex(), proxyRow, proxyRow);
		entries.insert(proxyRow, entry);
		endInsertRows();
	}
}

void DvbEpgProxyModel::sourceEntryRecordingKeyChanged(const DvbEpgEntry *entry)
{
	int proxyRow = (qBinaryFind(entries.constBegin(), entries.constEnd(), entry,
		DvbEpgEntryLessThan()) - entries.constBegin());

	if (proxyRow < entries.size()) {
		QModelIndex modelIndex = index(proxyRow, 2);
		emit dataChanged(modelIndex, modelIndex);
	}
}

void DvbEpgProxyModel::customEvent(QEvent *event)
{
	Q_UNUSED(event)

	if (!contentFilterEventPending) {
		return;
	}

	contentFilterEventPending = false;

	if (contentFilter.pattern().isEmpty()) {
		return;
	}

	if (!entries.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, entries.size() - 1);
		entries.clear();
		endRemoveRows();
	}

	QList<DvbEpgEntry>::ConstIterator it = epgModelEntries->constBegin();
	QList<DvbEpgEntry>::ConstIterator end = epgModelEntries->constEnd();
	QDateTime currentDateTimeUtc = QDateTime::currentDateTime().toUTC();
	QList<const DvbEpgEntry *> filteredEntries;

	for (; it != end; ++it) {
		if ((it->begin.addSecs(QTime().secsTo(it->duration)) > currentDateTimeUtc) &&
		    ((contentFilter.indexIn(it->title) >= 0) ||
		     (contentFilter.indexIn(it->subheading) >= 0) ||
		     (contentFilter.indexIn(it->details) >= 0))) {
			filteredEntries.append(&(*it));
		}
	}

	if (!filteredEntries.isEmpty()) {
		beginInsertRows(QModelIndex(), 0, filteredEntries.size() - 1);
		entries = filteredEntries;
		endInsertRows();
	}
}

DvbEpgDialog::DvbEpgDialog(DvbManager *manager_, const QString &currentChannelName,
	QWidget *parent) : KDialog(parent), manager(manager_)
{
	setButtons(KDialog::Close);
	setCaption(i18nc("@title:window", "Program Guide"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout(widget);

	DvbEpgModel *epgModel = manager->getEpgModel();
	epgProxyModel = epgModel->createEpgProxyModel(this);

	channelView = new QTreeView(widget);
	QAbstractItemModel *epgChannelModel = epgModel->createEpgProxyChannelModel(this);
	epgChannelModel->sort(0, Qt::AscendingOrder);
	channelView->setModel(epgChannelModel);
	channelView->setMaximumWidth(200);
	channelView->setRootIsDecorated(false);
	connect(channelView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	mainLayout->addWidget(channelView);

	QBoxLayout *rightLayout = new QVBoxLayout();
	QBoxLayout *boxLayout = new QHBoxLayout();

	KAction *scheduleAction = new KAction(KIcon("media-record"),
		i18nc("program guide", "Schedule Program"), this);
	connect(scheduleAction, SIGNAL(triggered()), this, SLOT(scheduleProgram()));

	QPushButton *pushButton =
		new QPushButton(scheduleAction->icon(), scheduleAction->text(), this);
	pushButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	connect(pushButton, SIGNAL(clicked()), this, SLOT(scheduleProgram()));
	boxLayout->addWidget(pushButton);

	boxLayout->addWidget(new QLabel(i18n("Search:"), widget));

	KLineEdit *lineEdit = new KLineEdit(widget);
	lineEdit->setClearButtonShown(true);
	connect(lineEdit, SIGNAL(textChanged(QString)),
		epgProxyModel, SLOT(setContentFilter(QString)));
	boxLayout->addWidget(lineEdit);
	rightLayout->addLayout(boxLayout);

	epgView = new QTreeView(widget);
	epgView->addAction(scheduleAction);
	epgView->header()->setResizeMode(QHeaderView::ResizeToContents);
	epgView->setContextMenuPolicy(Qt::ActionsContextMenu);
	epgView->setMinimumWidth(450);
	epgView->setModel(epgProxyModel);
	epgView->setRootIsDecorated(false);
	connect(epgView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(entryActivated(QModelIndex)));
	rightLayout->addWidget(epgView);

	contentLabel = new QLabel(widget);
	contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	contentLabel->setMargin(5);
	contentLabel->setWordWrap(true);

	QScrollArea *scrollArea = new QScrollArea(widget);
	scrollArea->setBackgroundRole(QPalette::Light);
	scrollArea->setMinimumHeight(175);
	scrollArea->setWidget(contentLabel);
	scrollArea->setWidgetResizable(true);
	rightLayout->addWidget(scrollArea);
	mainLayout->addLayout(rightLayout);

	QModelIndexList currentIndexes = epgChannelModel->match(epgChannelModel->index(0, 0),
		Qt::DisplayRole, currentChannelName, 1, Qt::MatchExactly);

	if (!currentIndexes.isEmpty()) {
		channelView->setCurrentIndex(currentIndexes.at(0));
	}

	setMainWidget(widget);
}

DvbEpgDialog::~DvbEpgDialog()
{
}

void DvbEpgDialog::channelActivated(const QModelIndex &index)
{
	if (!index.isValid()) {
		epgProxyModel->setChannelNameFilter(QString());
		return;
	}

	epgProxyModel->setChannelNameFilter(channelView->model()->data(index).toString());

	if (epgProxyModel->rowCount() >= 1) {
		epgView->setCurrentIndex(epgProxyModel->index(0, 0));
	} else {
		contentLabel->setText(QString());
	}
}

void DvbEpgDialog::entryActivated(const QModelIndex &index)
{
	if (!index.isValid()) {
		contentLabel->setText(QString());
		return;
	}

	const DvbEpgEntry *entry = epgProxyModel->data(index,
		DvbEpgProxyModel::DvbEpgEntryRole).value<const DvbEpgEntry *>();
	QString text = i18n("<font color=#008000 size=\"+1\">%1</font><br>", entry->title);

	if (!entry->subheading.isEmpty()) {
		text += i18n("<font color=#808000>%1</font><br>", entry->subheading);
	}

	text += i18n("<font color=#800000>%1 - %2</font><br><br>",
		KGlobal::locale()->formatDateTime(entry->begin.toLocalTime(), KLocale::LongDate),
		KGlobal::locale()->formatTime(
			entry->begin.addSecs(QTime().secsTo(entry->duration)).time()));

	text += entry->details;
	contentLabel->setText(text);
}

void DvbEpgDialog::scheduleProgram()
{
	QModelIndex index = epgView->currentIndex();

	if (index.isValid()) {
		epgProxyModel->scheduleProgram(index.row(), manager->getBeginMargin(),
			manager->getEndMargin());
	}
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
	return (qHash(eitEntry.source) ^ (uint(eitEntry.networkId) << 5) ^
		(uint(eitEntry.transportStreamId) << 10) ^ (uint(eitEntry.serviceId) << 15));
}

QTime DvbEitFilter::bcdToTime(int bcd)
{
	return QTime(((bcd >> 20) & 0x0f) * 10 + ((bcd >> 16) & 0x0f),
		((bcd >> 12) & 0x0f) * 10 + ((bcd >> 8) & 0x0f),
		((bcd >> 4) & 0x0f) * 10 + (bcd & 0x0f));
}

void DvbEitFilter::processSection(const char *data, int size, int crc)
{
	Q_UNUSED(crc)
	DvbEitSection eitSection(QByteArray::fromRawData(data, size));

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

		QString channelName = epgModel->findChannelNameByDvbEitEntry(eitEntry);

		if (channelName.isEmpty()) {
			eitEntry.networkId = -1;
			channelName = epgModel->findChannelNameByDvbEitEntry(eitEntry);
		}

		if (channelName.isEmpty()) {
			continue;
		}

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

		epgModel->addEntry(epgEntry);
	}
}
