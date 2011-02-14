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

	DvbEpgModel *epgModel = manager->getEpgModel();
	epgTableModel = new DvbEpgTableModel(epgModel, this);

	channelView = new QTreeView(widget);
	// TODO same sort order as main channel list
	QAbstractItemModel *epgChannelModel = new DvbEpgChannelModel(epgModel, this);
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
		epgTableModel->setChannelNameFilter(QString());
		return;
	}

	epgTableModel->setChannelNameFilter(channelView->model()->data(index).toString());

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

	const DvbEpgEntry *entry = epgTableModel->getEntry(index.row());
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
		manager->getEpgModel()->scheduleProgram(epgTableModel->getEntry(index.row()),
			manager->getBeginMargin(), manager->getEndMargin());
	}
}

DvbEpgChannelModel::DvbEpgChannelModel(DvbEpgModel *epgModel, QObject *parent) : QAbstractTableModel(parent)
{
	connect(epgModel, SIGNAL(epgChannelAdded(const DvbChannel*)),
		this, SLOT(epgChannelAdded(const DvbChannel*)));
	connect(epgModel, SIGNAL(epgChannelAboutToBeRemoved(const DvbChannel*)),
		this, SLOT(epgChannelAboutToBeRemoved(const DvbChannel*)));

	const QHash<DvbSharedChannel, int> epgChannels = epgModel->getEpgChannels();

	for (QHash<DvbSharedChannel, int>::ConstIterator it = epgChannels.constBegin();
	     it != epgChannels.constEnd(); ++it) {
		channels.append(it.key());
	}

	qSort(channels.begin(), channels.end(), lessThan);
}

DvbEpgChannelModel::~DvbEpgChannelModel()
{
}

void DvbEpgChannelModel::epgChannelAdded(const DvbChannel *channel)
{
	int row = (qLowerBound(channels.constBegin(), channels.constEnd(), channel, lessThan) -
		channels.constBegin());

	if ((row >= channels.size()) || (*channels.at(row) != *channel)) {
		beginInsertRows(QModelIndex(), row, row);
		channels.insert(row, DvbSharedChannel(channel));
		endInsertRows();
	}
}

void DvbEpgChannelModel::epgChannelAboutToBeRemoved(const DvbChannel *channel)
{
	int row = (qBinaryFind(channels.constBegin(), channels.constEnd(), channel, lessThan) -
		channels.constBegin());

	if (row < channels.size()) {
		beginRemoveRows(QModelIndex(), row, row);
		channels.removeAt(row);
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
		return channels.size();
	}

	return 0;
}

QVariant DvbEpgChannelModel::data(const QModelIndex &index, int role) const
{
	if ((role == Qt::DisplayRole) && (index.column() == 0)) {
		return channels.at(index.row())->name;
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

DvbEpgTableModel::DvbEpgTableModel(DvbEpgModel *epgModel_, QObject *parent) :
	QAbstractTableModel(parent), epgModel(epgModel_), contentFilterEventPending(false)
{
	contentFilter.setCaseSensitivity(Qt::CaseInsensitive);
	connect(epgModel, SIGNAL(entryAdded(const DvbEpgEntry*)),
		this, SLOT(entryAdded(const DvbEpgEntry*)));
	connect(epgModel, SIGNAL(entryChanged(const DvbEpgEntry*,const DvbEpgEntry*)),
		this, SLOT(entryChanged(const DvbEpgEntry*,const DvbEpgEntry*)));
	connect(epgModel, SIGNAL(entryAboutToBeRemoved(const DvbEpgEntry*)),
		this, SLOT(entryAboutToBeRemoved(const DvbEpgEntry*)));
}

DvbEpgTableModel::~DvbEpgTableModel()
{
}

const DvbEpgEntry *DvbEpgTableModel::getEntry(int row) const
{
	return entries.at(row).constData();
}

void DvbEpgTableModel::setChannelNameFilter(const QString &channelName)
{
	channelNameFilter = channelName;
	contentFilter.setPattern(QString());

	if (!entries.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, entries.size() - 1);
		entries.clear();
		endRemoveRows();
	}

	const QMap<DvbEpgEntry, DvbEpgEmptyClass> allEntries = epgModel->getEntries();
	DvbEpgEntry pseudoEntry;
	DvbChannel pseudoChannel; // FIXME (and adjust DvbSharedChannel afterwards)
	pseudoChannel.ref = 1;
	pseudoChannel.name = channelName;
	pseudoEntry.channel = DvbSharedChannel(&pseudoChannel);
	QMap<DvbEpgEntry, DvbEpgEmptyClass>::ConstIterator begin =
		allEntries.lowerBound(pseudoEntry);
	int count = 0;

	for (QMap<DvbEpgEntry, DvbEpgEmptyClass>::ConstIterator it = begin;
	     it != allEntries.constEnd(); ++it) {
		if (it.key().channel != pseudoEntry.channel) {
			break;
		}

		++count;
	}

	if (count > 0) {
		beginInsertRows(QModelIndex(), 0, count - 1);

		for (QMap<DvbEpgEntry, DvbEpgEmptyClass>::ConstIterator it = begin; count > 0;
		     ++it, --count) {
			entries.append(DvbEpgTableModelEntry(&it.key()));
		}

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
	const DvbEpgEntry *entry = entries.at(index.row()).constData();

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
			return entry->channel->name;
		}

		break;
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
	channelNameFilter.clear();
	contentFilter.setPattern(pattern);

	if (!contentFilterEventPending) {
		contentFilterEventPending = true;
		QCoreApplication::postEvent(this, new QEvent(QEvent::User), Qt::LowEventPriority);
	}
}

void DvbEpgTableModel::entryAdded(const DvbEpgEntry *entry)
{
	if ((entry->channel->name == channelNameFilter) ||
	    (!contentFilter.pattern().isEmpty() &&
	     ((contentFilter.indexIn(entry->title) >= 0) ||
	      (contentFilter.indexIn(entry->subheading) >= 0) ||
	      (contentFilter.indexIn(entry->details) >= 0)))) {
		int row = (qLowerBound(entries, DvbEpgTableModelEntry(entry)) -
			entries.constBegin());
		beginInsertRows(QModelIndex(), row, row);
		entries.insert(row, DvbEpgTableModelEntry(entry));
		endInsertRows();
	}
}

void DvbEpgTableModel::entryChanged(const DvbEpgEntry *oldEntry, const DvbEpgEntry *newEntry)
{
	int row = (qBinaryFind(entries, DvbEpgTableModelEntry(oldEntry)) - entries.constBegin());

	if (row < entries.size()) {
		entries[row] = DvbEpgTableModelEntry(newEntry);
		emit dataChanged(index(row, 0), index(row, 3));

		if (((row > 0) && !(entries.at(row - 1) < entries.at(row))) ||
		    ((row < (entries.size() - 1)) && !(entries.at(row) < entries.at(row + 1)))) {
			int targetRow = (qLowerBound(entries, DvbEpgTableModelEntry(newEntry)) -
				entries.constBegin());
			emit layoutAboutToBeChanged();

			if (targetRow > row) {
				--targetRow;
			}

			entries.move(row, targetRow);
			emit layoutChanged();
		}
	}
}

void DvbEpgTableModel::entryAboutToBeRemoved(const DvbEpgEntry *entry)
{
	int row = (qBinaryFind(entries, DvbEpgTableModelEntry(entry)) - entries.constBegin());

	if (row < entries.size()) {
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

	const QMap<DvbEpgEntry, DvbEpgEmptyClass> allEntries = epgModel->getEntries();
	QList<DvbEpgTableModelEntry> filteredEntries;

	for (QMap<DvbEpgEntry, DvbEpgEmptyClass>::ConstIterator it = allEntries.constBegin();
	     it != allEntries.constEnd(); ++it) {
		const DvbEpgEntry &entry = it.key();

		if (((contentFilter.indexIn(entry.title) >= 0) ||
		     (contentFilter.indexIn(entry.subheading) >= 0) ||
		     (contentFilter.indexIn(entry.details) >= 0))) {
			filteredEntries.append(DvbEpgTableModelEntry(&entry));
		}
	}

	if (!filteredEntries.isEmpty()) {
		beginInsertRows(QModelIndex(), 0, filteredEntries.size() - 1);
		entries = filteredEntries;
		endInsertRows();
	}
}
