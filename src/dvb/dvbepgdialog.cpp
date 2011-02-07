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
#include <QSortFilterProxyModel>
#include <KAction>
#include <KLineEdit>
#include <KLocale>
#include "dvbchannel.h"
#include "dvbmanager.h"

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
	int row = (qBinaryFind(channelNames, channelName) - channelNames.constBegin());

	if (row < channelNames.size()) {
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

DvbEpgModel::DvbEpgModel(DvbEpg *epg_, QObject *parent) : QAbstractTableModel(parent), epg(epg_),
	epgChannelModel(NULL), contentFilterEventPending(false)
{
	contentFilter.setCaseSensitivity(Qt::CaseInsensitive);
	connect(epg, SIGNAL(entryAdded(const DvbEpgEntry*)),
		this, SLOT(entryAdded(const DvbEpgEntry*)));
	connect(epg, SIGNAL(entryChanged(const DvbEpgEntry*,DvbEpgEntry)),
		this, SLOT(entryChanged(const DvbEpgEntry*,DvbEpgEntry)));
	connect(epg, SIGNAL(entryAboutToBeRemoved(const DvbEpgEntry*)),
		this, SLOT(entryAboutToBeRemoved(const DvbEpgEntry*)));

	foreach (const DvbEpgEntry &entry, epg->getEntries()) {
		if (++channelNameCount[entry.channelName] == 1) {
			epgChannelModel.insertChannelName(entry.channelName);
		}
	}
}

DvbEpgModel::~DvbEpgModel()
{
}

QAbstractItemModel *DvbEpgModel::createEpgProxyChannelModel(QObject *parent)
{
	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(parent);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);
	proxyModel->setSourceModel(&epgChannelModel);
	return proxyModel;
}

const DvbEpgEntry *DvbEpgModel::getEntry(int row) const
{
	return entries.at(row);
}

void DvbEpgModel::setChannelNameFilter(const QString &channelName)
{
	channelNameFilter = channelName;
	contentFilter.setPattern(QString());

	if (!entries.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, entries.size() - 1);
		entries.clear();
		endRemoveRows();
	}

	const QList<DvbEpgEntry> allEntries = epg->getEntries();
	QList<DvbEpgEntry>::ConstIterator begin = qLowerBound(allEntries.constBegin(),
		allEntries.constEnd(), channelName, DvbEpgEntryLessThan());
	QList<DvbEpgEntry>::ConstIterator end = allEntries.constEnd();

	for (QList<DvbEpgEntry>::ConstIterator it = begin; it != end; ++it) {
		if (it->channelName != channelName) {
			end = it;
			break;
		}
	}

	if (begin != end) {
		beginInsertRows(QModelIndex(), 0, end - begin - 1);

		for (QList<DvbEpgEntry>::ConstIterator it = begin; it != end; ++it) {
			entries.append(&(*it));
		}

		endInsertRows();
	}
}

void DvbEpgModel::scheduleProgram(int row, int extraSecondsBefore, int extraSecondsAfter)
{
	epg->scheduleProgram(entries.at(row), extraSecondsBefore, extraSecondsAfter);
}

int DvbEpgModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 4;
	}

	return 0;
}

int DvbEpgModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return entries.size();
	}

	return 0;
}

QVariant DvbEpgModel::data(const QModelIndex &index, int role) const
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
	}

	return QVariant();
}

QVariant DvbEpgModel::headerData(int section, Qt::Orientation orientation, int role) const
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

void DvbEpgModel::setContentFilter(const QString &pattern)
{
	channelNameFilter.clear();
	contentFilter.setPattern(pattern);

	if (!contentFilterEventPending) {
		contentFilterEventPending = true;
		QCoreApplication::postEvent(this, new QEvent(QEvent::User), Qt::LowEventPriority);
	}
}

void DvbEpgModel::entryAdded(const DvbEpgEntry *entry)
{
	if (++channelNameCount[entry->channelName] == 1) {
		epgChannelModel.insertChannelName(entry->channelName);
	}

	if ((entry->channelName == channelNameFilter) ||
	    (!contentFilter.pattern().isEmpty() &&
	     ((contentFilter.indexIn(entry->title) >= 0) ||
	      (contentFilter.indexIn(entry->subheading) >= 0) ||
	      (contentFilter.indexIn(entry->details) >= 0)))) {
		int row = (qLowerBound(entries.constBegin(), entries.constEnd(), *entry,
			DvbEpgEntryLessThan()) - entries.constBegin());
		beginInsertRows(QModelIndex(), row, row);
		entries.insert(row, entry);
		endInsertRows();
	}
}

void DvbEpgModel::entryChanged(const DvbEpgEntry *entry, const DvbEpgEntry &oldEntry)
{
	if (entry->channelName != oldEntry.channelName) {
		if (++channelNameCount[entry->channelName] == 1) {
			epgChannelModel.insertChannelName(entry->channelName);
		}

		if (--channelNameCount[oldEntry.channelName] == 0) {
			epgChannelModel.removeChannelName(oldEntry.channelName);
		}
	}

	int row = (qBinaryFind(entries.constBegin(), entries.constEnd(), oldEntry,
		DvbEpgEntryLessThan()) - entries.constBegin());

	if (row < entries.size()) {
		emit dataChanged(index(row, 0), index(row, 3));

		if (((row > 0) && !(entries.at(row - 1) < entries.at(row))) ||
		    ((row < (entries.size() - 1)) && !(entries.at(row) < entries.at(row + 1)))) {
			int targetRow = (qLowerBound(entries.constBegin(), entries.constEnd(),
				*entry, DvbEpgEntryLessThan()) - entries.constBegin());
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), targetRow);

			if (targetRow > row) {
				--targetRow;
			}

			entries.move(row, targetRow);
			endMoveRows();
		}
	}
}

void DvbEpgModel::entryAboutToBeRemoved(const DvbEpgEntry *entry)
{
	if (--channelNameCount[entry->channelName] == 0) {
		epgChannelModel.removeChannelName(entry->channelName);
	}

	int row = (qBinaryFind(entries.constBegin(), entries.constEnd(), *entry,
		DvbEpgEntryLessThan()) - entries.constBegin());

	if (row < entries.size()) {
		beginRemoveRows(QModelIndex(), row, row);
		entries.removeAt(row);
		endRemoveRows();
	}
}

void DvbEpgModel::customEvent(QEvent *event)
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

	const QList<DvbEpgEntry> allEntries = epg->getEntries();
	QList<DvbEpgEntry>::ConstIterator constEnd = allEntries.constEnd();
	QList<const DvbEpgEntry *> filteredEntries;

	for (QList<DvbEpgEntry>::ConstIterator it = allEntries.constBegin(); it != constEnd;
	     ++it) {
		const DvbEpgEntry &entry = *it;

		if (((contentFilter.indexIn(entry.title) >= 0) ||
		     (contentFilter.indexIn(entry.subheading) >= 0) ||
		     (contentFilter.indexIn(entry.details) >= 0))) {
			filteredEntries.append(&entry);
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

	DvbEpg *epg = manager->getEpg();
	epgModel = new DvbEpgModel(epg, this);

	channelView = new QTreeView(widget);
	QAbstractItemModel *epgChannelModel = epgModel->createEpgProxyChannelModel(this);
	epgChannelModel->sort(0, Qt::AscendingOrder); // TODO same sort order as main channel list
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
	connect(lineEdit, SIGNAL(textChanged(QString)), epgModel, SLOT(setContentFilter(QString)));
	boxLayout->addWidget(lineEdit);
	rightLayout->addLayout(boxLayout);

	epgView = new QTreeView(widget);
	epgView->addAction(scheduleAction);
	epgView->header()->setResizeMode(QHeaderView::ResizeToContents);
	epgView->setContextMenuPolicy(Qt::ActionsContextMenu);
	epgView->setMinimumWidth(450);
	epgView->setModel(epgModel);
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
		epgModel->setChannelNameFilter(QString());
		return;
	}

	epgModel->setChannelNameFilter(channelView->model()->data(index).toString());

	if (epgModel->rowCount() >= 1) {
		epgView->setCurrentIndex(epgModel->index(0, 0));
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

	const DvbEpgEntry *entry = epgModel->getEntry(index.row());
	QString text = i18n("<font color=#008000 size=\"+1\">%1</font><br>", entry->title);

	if (!entry->subheading.isEmpty()) {
		text += i18n("<font color=#808000>%1</font><br>", entry->subheading);
	}

	QDateTime begin = entry->begin.toLocalTime();
	QTime end = entry->begin.addSecs(QTime().secsTo(entry->duration)).toLocalTime().time();

	text += i18n("<font color=#800000>%1 - %2</font><br><br>",
		KGlobal::locale()->formatDateTime(begin, KLocale::LongDate),
		KGlobal::locale()->formatTime(end));

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
