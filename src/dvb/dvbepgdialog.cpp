/*
 * dvbepgdialog.cpp
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

#include "dvbepgdialog.h"
#include "dvbepgdialog_p.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <KAction>
#include <KLineEdit>
#include <KLocale>
#include "dvbmanager.h"

DvbEpgDialog::DvbEpgDialog(DvbManager *manager_, const QString &currentChannelName,
	QWidget *parent) : KDialog(parent), manager(manager_)
{
	setButtons(KDialog::Close);
	setCaption(i18nc("@title:window", "Program Guide"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout(widget);

	epgTableModel = new DvbEpgTableModel(manager->getEpgModel(), this);

	channelView = new QTreeView(widget);
	epgChannelTableModel = new DvbEpgChannelTableModel(manager, this);
	channelView->setModel(epgChannelTableModel);
	channelView->setMaximumWidth(200);
	channelView->setRootIsDecorated(false);
	connect(channelView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	mainLayout->addWidget(channelView);

	QBoxLayout *rightLayout = new QVBoxLayout();
	QBoxLayout *boxLayout = new QHBoxLayout();

	KAction *scheduleAction = new KAction(KIcon("media-record"),
		i18nc("@action:inmenu tv show", "Record Show"), this);
	connect(scheduleAction, SIGNAL(triggered()), this, SLOT(scheduleProgram()));

	QPushButton *pushButton =
		new QPushButton(scheduleAction->icon(), scheduleAction->text(), this);
	pushButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	connect(pushButton, SIGNAL(clicked()), this, SLOT(scheduleProgram()));
	boxLayout->addWidget(pushButton);

	boxLayout->addWidget(new QLabel(i18nc("@label:textbox", "Search:"), widget));

	KLineEdit *lineEdit = new KLineEdit(widget);
	lineEdit->setClearButtonShown(true);
	connect(lineEdit, SIGNAL(textChanged(QString)),
		epgTableModel, SLOT(setContentFilter(QString)));
	boxLayout->addWidget(lineEdit);
	rightLayout->addLayout(boxLayout);

	epgView = new QTreeView(widget);
	epgView->addAction(scheduleAction);
	epgView->header()->setResizeMode(QHeaderView::ResizeToContents);
	epgView->setContextMenuPolicy(Qt::ActionsContextMenu);
	epgView->setMinimumWidth(450);
	epgView->setModel(epgTableModel);
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

	QModelIndexList currentIndexes = epgChannelTableModel->match(
		epgChannelTableModel->index(0, 0), Qt::DisplayRole, currentChannelName, 1,
		Qt::MatchExactly);

	if (!currentIndexes.isEmpty()) {
		channelView->setCurrentIndex(currentIndexes.at(0));
	}

	setMainWidget(widget);
}

DvbEpgDialog::~DvbEpgDialog()
{
}

void DvbEpgDialog::showDialog(DvbManager *manager_, const QString &currentChannelName,
	QWidget *parent)
{
	KDialog *dialog = new DvbEpgDialog(manager_, currentChannelName, parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbEpgDialog::channelActivated(const QModelIndex &index)
{
	if (!index.isValid()) {
		epgTableModel->setChannelFilter(DvbSharedChannel());
		return;
	}

	epgTableModel->setChannelFilter(epgChannelTableModel->getChannel(index.row()));

	if (epgTableModel->rowCount() >= 1) {
		epgView->setCurrentIndex(epgTableModel->index(0, 0));
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

	DvbSharedEpgEntry entry = epgTableModel->getEntry(index.row());
	QString text = i18nc("@info tv show title",
		"<font color=#008000 size=\"+1\">%1</font><br>", entry->title);

	if (!entry->subheading.isEmpty()) {
		text += i18nc("@info tv show subheading", "<font color=#808000>%1</font><br>",
			entry->subheading);
	}

	QDateTime begin = entry->begin.toLocalTime();
	QTime end = entry->begin.addSecs(QTime().secsTo(entry->duration)).toLocalTime().time();

	text += i18nc("@info tv show start, end", "<font color=#800000>%1 - %2</font><br><br>",
		KGlobal::locale()->formatDateTime(begin, KLocale::LongDate),
		KGlobal::locale()->formatTime(end));

	text += entry->details;
	contentLabel->setText(text);
}

void DvbEpgDialog::scheduleProgram()
{
	QModelIndex index = epgView->currentIndex();

	if (index.isValid()) {
		manager->getEpgModel()->scheduleProgram(epgTableModel->getEntry(index.row()),
			manager->getBeginMargin(), manager->getEndMargin());
	}
}

bool DvbEpgEntryLessThan::operator()(const DvbSharedEpgEntry &x, const DvbSharedEpgEntry &y) const
{
	if (x->channel != y->channel) {
		return (x->channel->name.localeAwareCompare(y->channel->name) < 0);
	}

	if (x->begin != y->begin) {
		return (x->begin < y->begin);
	}

	if (x->duration != y->duration) {
		return (x->duration < y->duration);
	}

	if (x->title != y->title) {
		return (x->title < y->title);
	}

	if (x->subheading != y->subheading) {
		return (x->subheading < y->subheading);
	}

	if (x->details < y->details) {
		return (x->details < y->details);
	}

	return (x < y);
}

DvbEpgChannelTableModel::DvbEpgChannelTableModel(DvbManager *manager, QObject *parent) :
	QAbstractTableModel(parent)
{
	DvbEpgModel *epgModel = manager->getEpgModel();
	connect(epgModel, SIGNAL(epgChannelAdded(DvbSharedChannel)),
		this, SLOT(epgChannelAdded(DvbSharedChannel)));
	connect(epgModel, SIGNAL(epgChannelRemoved(DvbSharedChannel)),
		this, SLOT(epgChannelRemoved(DvbSharedChannel)));

	const QHash<DvbSharedChannel, int> epgChannels = epgModel->getEpgChannels();

	for (QHash<DvbSharedChannel, int>::ConstIterator it = epgChannels.constBegin();
	     it != epgChannels.constEnd(); ++it) {
		channels.append(it.key());
	}

	QHeaderView *headerView = manager->getChannelView()->header();

	if (headerView->sortIndicatorOrder() == Qt::AscendingOrder) {
		if (headerView->sortIndicatorSection() == 0) {
			lessThan.sortOrder = DvbChannelLessThan::ChannelNameAscending;
		} else {
			lessThan.sortOrder = DvbChannelLessThan::ChannelNumberAscending;
		}
	} else {
		if (headerView->sortIndicatorSection() == 0) {
			lessThan.sortOrder = DvbChannelLessThan::ChannelNameDescending;
		} else {
			lessThan.sortOrder = DvbChannelLessThan::ChannelNumberDescending;
		}
	}

	qSort(channels.begin(), channels.end(), lessThan);
}

DvbEpgChannelTableModel::~DvbEpgChannelTableModel()
{
}

const DvbSharedChannel &DvbEpgChannelTableModel::getChannel(int row) const
{
	return channels.at(row);
}

int DvbEpgChannelTableModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 1;
	}

	return 0;
}

int DvbEpgChannelTableModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return channels.size();
	}

	return 0;
}

QVariant DvbEpgChannelTableModel::data(const QModelIndex &index, int role) const
{
	if ((role == Qt::DisplayRole) && (index.column() == 0)) {
		return channels.at(index.row())->name;
	}

	return QVariant();
}

QVariant DvbEpgChannelTableModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if ((section == 0) && (orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		return i18nc("@title:column tv show", "Channel");
	}

	return QVariant();
}

void DvbEpgChannelTableModel::epgChannelAdded(const DvbSharedChannel &channel)
{
	int row = (qLowerBound(channels.constBegin(), channels.constEnd(), channel, lessThan) -
		channels.constBegin());

	if ((row >= channels.size()) || (channels.at(row) != channel)) {
		beginInsertRows(QModelIndex(), row, row);
		channels.insert(row, DvbSharedChannel(channel));
		endInsertRows();
	}
}

void DvbEpgChannelTableModel::epgChannelRemoved(const DvbSharedChannel &channel)
{
	int row = (qBinaryFind(channels.constBegin(), channels.constEnd(), channel, lessThan) -
		channels.constBegin());

	if (row < channels.size()) {
		beginRemoveRows(QModelIndex(), row, row);
		channels.removeAt(row);
		endRemoveRows();
	}
}

DvbEpgTableModel::DvbEpgTableModel(DvbEpgModel *epgModel_, QObject *parent) :
	QAbstractTableModel(parent), epgModel(epgModel_), contentFilterEventPending(false),
	updatingRow(0)
{
	contentFilter.setCaseSensitivity(Qt::CaseInsensitive);

	connect(epgModel, SIGNAL(entryAdded(DvbSharedEpgEntry)),
		this, SLOT(entryAdded(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryAboutToBeUpdated(DvbSharedEpgEntry)),
		this, SLOT(entryAboutToBeUpdated(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryUpdated(DvbSharedEpgEntry)),
		this, SLOT(entryUpdated(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryRemoved(DvbSharedEpgEntry)),
		this, SLOT(entryRemoved(DvbSharedEpgEntry)));
}

DvbEpgTableModel::~DvbEpgTableModel()
{
}

DvbSharedEpgEntry DvbEpgTableModel::getEntry(int row) const
{
	if ((row >= 0) && (row < entries.size())) {
		return entries.at(row);
	}

	return DvbSharedEpgEntry();
}

void DvbEpgTableModel::setChannelFilter(const DvbSharedChannel &channel)
{
	channelFilter = channel;
	contentFilter.setPattern(QString());

	if (!entries.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, entries.size() - 1);
		entries.clear();
		endRemoveRows();
	}

	if (!channelFilter.isValid()) {
		return;
	}

	typedef QMap<DvbEpgEntryId, DvbSharedEpgEntry>::ConstIterator ConstIterator;
	const QMap<DvbEpgEntryId, DvbSharedEpgEntry> allEntries = epgModel->getEntries();
	DvbEpgEntry fakeEntry(channelFilter);
	ConstIterator begin = allEntries.lowerBound(DvbEpgEntryId(&fakeEntry));
	int count = 0;

	for (ConstIterator it = begin; it != allEntries.constEnd(); ++it) {
		if ((*it)->channel != fakeEntry.channel) {
			break;
		}

		++count;
	}

	if (count > 0) {
		beginInsertRows(QModelIndex(), 0, count - 1);

		for (ConstIterator it = begin; count > 0; ++it, --count) {
			entries.append(*it);
		}

		// qSort isn't needed here
		endInsertRows();
	}
}

int DvbEpgTableModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 4;
	}

	return 0;
}

int DvbEpgTableModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return entries.size();
	}

	return 0;
}

QVariant DvbEpgTableModel::data(const QModelIndex &index, int role) const
{
	if ((index.row() >= 0) && (index.row() < entries.size())) {
		const DvbSharedEpgEntry &entry = entries.at(index.row());

		switch (role) {
		case Qt::DecorationRole:
			if ((index.column() == 2) && entry->recording.isValid()) {
				return KIcon("media-record");
			}

			break;
		case Qt::DisplayRole:
			switch (index.column()) {
			case 0:
				return KGlobal::locale()->formatDateTime(
					entry->begin.toLocalTime());
			case 1:
				return KGlobal::locale()->formatTime(entry->duration, false, true);
			case 2:
				return entry->title;
			case 3:
				return entry->channel->name;
			}

			break;
		}
	}

	return QVariant();
}

QVariant DvbEpgTableModel::headerData(int section, Qt::Orientation orientation, int role) const
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

void DvbEpgTableModel::setContentFilter(const QString &pattern)
{
	channelFilter = DvbSharedChannel();
	contentFilter.setPattern(pattern);

	if (!contentFilterEventPending) {
		contentFilterEventPending = true;
		QCoreApplication::postEvent(this, new QEvent(QEvent::User), Qt::LowEventPriority);
	}
}

void DvbEpgTableModel::entryAdded(const DvbSharedEpgEntry &entry)
{
	if ((channelFilter.isValid() && (channelFilter == entry->channel)) ||
	    (!contentFilter.pattern().isEmpty() &&
	     ((contentFilter.indexIn(entry->title) >= 0) ||
	      (contentFilter.indexIn(entry->subheading) >= 0) ||
	      (contentFilter.indexIn(entry->details) >= 0)))) {
		int row = (qLowerBound(entries.constBegin(), entries.constEnd(), entry,
			DvbEpgEntryLessThan()) - entries.constBegin());
		beginInsertRows(QModelIndex(), row, row);
		entries.insert(row, entry);
		endInsertRows();
	}
}

void DvbEpgTableModel::entryAboutToBeUpdated(const DvbSharedEpgEntry &entry)
{
	updatingRow = (qBinaryFind(entries.constBegin(), entries.constEnd(), entry,
		DvbEpgEntryLessThan()) - entries.constBegin());
}

void DvbEpgTableModel::entryUpdated(const DvbSharedEpgEntry &entry)
{
	if ((updatingRow >= 0) && (updatingRow < entries.size()) &&
	    (entries.at(updatingRow) == entry)) {
		emit dataChanged(index(updatingRow, 0), index(updatingRow, 3));
	}
}

void DvbEpgTableModel::entryRemoved(const DvbSharedEpgEntry &entry)
{
	int row = (qBinaryFind(entries.constBegin(), entries.constEnd(), entry,
		DvbEpgEntryLessThan()) - entries.constBegin());

	if ((row < entries.size()) && (entries.at(row) == entry)) {
		beginRemoveRows(QModelIndex(), row, row);
		entries.removeAt(row);
		endRemoveRows();
	}
}

void DvbEpgTableModel::customEvent(QEvent *event)
{
	Q_UNUSED(event)
	contentFilterEventPending = false;

	if (contentFilter.pattern().isEmpty()) {
		return;
	}

	if (!entries.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, entries.size() - 1);
		entries.clear();
		endRemoveRows();
	}

	typedef QMap<DvbEpgEntryId, DvbSharedEpgEntry>::ConstIterator ConstIterator;
	const QMap<DvbEpgEntryId, DvbSharedEpgEntry> allEntries = epgModel->getEntries();
	QList<DvbSharedEpgEntry> filteredEntries;

	for (ConstIterator it = allEntries.constBegin(); it != allEntries.constEnd(); ++it) {
		const DvbSharedEpgEntry &entry = *it;

		if (((contentFilter.indexIn(entry->title) >= 0) ||
		     (contentFilter.indexIn(entry->subheading) >= 0) ||
		     (contentFilter.indexIn(entry->details) >= 0))) {
			filteredEntries.append(entry);
		}
	}

	if (!filteredEntries.isEmpty()) {
		qSort(filteredEntries.begin(), filteredEntries.end(), DvbEpgEntryLessThan());
		beginInsertRows(QModelIndex(), 0, filteredEntries.size() - 1);
		entries = filteredEntries;
		endInsertRows();
	}
}
