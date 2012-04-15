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
#include "../log.h"
#include "dvbmanager.h"

DvbEpgDialog::DvbEpgDialog(DvbManager *manager_, QWidget *parent) : KDialog(parent),
	manager(manager_)
{
	setButtons(KDialog::Close);
	setCaption(i18nc("@title:window", "Program Guide"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout(widget);

	epgChannelTableModel = new DvbEpgChannelTableModel(this);
	epgChannelTableModel->setManager(manager);
	channelView = new QTreeView(widget);
	channelView->setMaximumWidth(30 * fontMetrics().averageCharWidth());
	channelView->setModel(epgChannelTableModel);
	channelView->setRootIsDecorated(false);
	channelView->setUniformRowHeights(true);
	connect(channelView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	mainLayout->addWidget(channelView);

	QBoxLayout *rightLayout = new QVBoxLayout();
	QBoxLayout *boxLayout = new QHBoxLayout();

	KAction *scheduleAction = new KAction(KIcon(QLatin1String("media-record")),
		i18nc("@action:inmenu tv show", "Record Show"), this);
	connect(scheduleAction, SIGNAL(triggered()), this, SLOT(scheduleProgram()));

	QPushButton *pushButton =
		new QPushButton(scheduleAction->icon(), scheduleAction->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(scheduleProgram()));
	boxLayout->addWidget(pushButton);

	boxLayout->addWidget(new QLabel(i18nc("@label:textbox", "Search:"), widget));

	epgTableModel = new DvbEpgTableModel(this);
	epgTableModel->setEpgModel(manager->getEpgModel());
	connect(epgTableModel, SIGNAL(layoutChanged()), this, SLOT(checkEntry()));
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
	epgView->setMinimumWidth(75 * fontMetrics().averageCharWidth());
	epgView->setModel(epgTableModel);
	epgView->setRootIsDecorated(false);
	epgView->setUniformRowHeights(true);
	connect(epgView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(entryActivated(QModelIndex)));
	rightLayout->addWidget(epgView);

	contentLabel = new QLabel(widget);
	contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	contentLabel->setMargin(5);
	contentLabel->setWordWrap(true);

	QScrollArea *scrollArea = new QScrollArea(widget);
	scrollArea->setBackgroundRole(QPalette::Light);
	scrollArea->setMinimumHeight(12 * fontMetrics().height());
	scrollArea->setWidget(contentLabel);
	scrollArea->setWidgetResizable(true);
	rightLayout->addWidget(scrollArea);
	mainLayout->addLayout(rightLayout);
	setMainWidget(widget);
}

DvbEpgDialog::~DvbEpgDialog()
{
}

void DvbEpgDialog::setCurrentChannel(const DvbSharedChannel &channel)
{
	channelView->setCurrentIndex(epgChannelTableModel->find(channel));
}

void DvbEpgDialog::channelActivated(const QModelIndex &index)
{
	if (!index.isValid()) {
		epgTableModel->setChannelFilter(DvbSharedChannel());
		return;
	}

	epgTableModel->setChannelFilter(epgChannelTableModel->value(index));
	epgView->setCurrentIndex(epgTableModel->index(0, 0));
}

void DvbEpgDialog::entryActivated(const QModelIndex &index)
{
	const DvbSharedEpgEntry &entry = epgTableModel->value(index.row());

	if (!entry.isValid()) {
		contentLabel->setText(QString());
		return;
	}

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

void DvbEpgDialog::checkEntry()
{
	if (!epgView->currentIndex().isValid()) {
		// FIXME workaround --> file bug
		contentLabel->setText(QString());
	}
}

void DvbEpgDialog::scheduleProgram()
{
	const DvbSharedEpgEntry &entry = epgTableModel->value(epgView->currentIndex());

	if (entry.isValid()) {
		manager->getEpgModel()->scheduleProgram(entry, manager->getBeginMargin(),
			manager->getEndMargin());
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

DvbEpgChannelTableModel::DvbEpgChannelTableModel(QObject *parent) :
	TableModel<DvbEpgChannelTableModelHelper>(parent)
{
}

DvbEpgChannelTableModel::~DvbEpgChannelTableModel()
{
}

void DvbEpgChannelTableModel::setManager(DvbManager *manager)
{
	DvbEpgModel *epgModel = manager->getEpgModel();
	connect(epgModel, SIGNAL(epgChannelAdded(DvbSharedChannel)),
		this, SLOT(epgChannelAdded(DvbSharedChannel)));
	connect(epgModel, SIGNAL(epgChannelRemoved(DvbSharedChannel)),
		this, SLOT(epgChannelRemoved(DvbSharedChannel)));
	// theoretically we should monitor the channel model for updated channels,
	// but it's very unlikely that this has practical relevance

	QHeaderView *headerView = manager->getChannelView()->header();
	DvbChannelLessThan::SortOrder sortOrder;

	if (headerView->sortIndicatorOrder() == Qt::AscendingOrder) {
		if (headerView->sortIndicatorSection() == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameAscending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberAscending;
		}
	} else {
		if (headerView->sortIndicatorSection() == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameDescending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberDescending;
		}
	}

	internalSort(sortOrder);
	resetFromKeys(epgModel->getEpgChannels());
}

QVariant DvbEpgChannelTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedChannel &channel = value(index);

	if (channel.isValid() && (role == Qt::DisplayRole) && (index.column() == 0)) {
		return channel->name;
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
	insert(channel);
}

void DvbEpgChannelTableModel::epgChannelRemoved(const DvbSharedChannel &channel)
{
	remove(channel);
}

bool DvbEpgTableModelHelper::filterAcceptsItem(const DvbSharedEpgEntry &entry) const
{
	switch (filterType) {
	case ChannelFilter:
		return (entry->channel == channelFilter);
	case ContentFilter:
		return ((contentFilter.indexIn(entry->title) >= 0) ||
			(contentFilter.indexIn(entry->subheading) >= 0) ||
			(contentFilter.indexIn(entry->details) >= 0));
	}

	return false;
}

DvbEpgTableModel::DvbEpgTableModel(QObject *parent) : TableModel<DvbEpgTableModelHelper>(parent),
	epgModel(NULL), contentFilterEventPending(false)
{
	helper.contentFilter.setCaseSensitivity(Qt::CaseInsensitive);
}

DvbEpgTableModel::~DvbEpgTableModel()
{
}

void DvbEpgTableModel::setEpgModel(DvbEpgModel *epgModel_)
{
	if (epgModel != NULL) {
		Log("DvbEpgTableModel::setEpgModel: epg model already set");
		return;
	}

	epgModel = epgModel_;
	connect(epgModel, SIGNAL(entryAdded(DvbSharedEpgEntry)),
		this, SLOT(entryAdded(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryAboutToBeUpdated(DvbSharedEpgEntry)),
		this, SLOT(entryAboutToBeUpdated(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryUpdated(DvbSharedEpgEntry)),
		this, SLOT(entryUpdated(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryRemoved(DvbSharedEpgEntry)),
		this, SLOT(entryRemoved(DvbSharedEpgEntry)));
}

void DvbEpgTableModel::setChannelFilter(const DvbSharedChannel &channel)
{
	helper.channelFilter = channel;
	helper.contentFilter.setPattern(QString());
	helper.filterType = DvbEpgTableModelHelper::ChannelFilter;
	reset(epgModel->getEntries());
}

QVariant DvbEpgTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedEpgEntry &entry = value(index);

	if (entry.isValid()) {
		switch (role) {
		case Qt::DecorationRole:
			if ((index.column() == 2) && entry->recording.isValid()) {
				return KIcon(QLatin1String("media-record"));
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
	helper.channelFilter = DvbSharedChannel();
	helper.contentFilter.setPattern(pattern);

	if (!pattern.isEmpty()) {
		helper.filterType = DvbEpgTableModelHelper::ContentFilter;

		if (!contentFilterEventPending) {
			contentFilterEventPending = true;
			QCoreApplication::postEvent(this, new QEvent(QEvent::User),
				Qt::LowEventPriority);
		}
	} else {
		// use channel filter so that content won't be unnecessarily filtered
		helper.filterType = DvbEpgTableModelHelper::ChannelFilter;
		reset(QMap<DvbEpgEntryId, DvbSharedEpgEntry>());
	}
}

void DvbEpgTableModel::entryAdded(const DvbSharedEpgEntry &entry)
{
	insert(entry);
}

void DvbEpgTableModel::entryAboutToBeUpdated(const DvbSharedEpgEntry &entry)
{
	aboutToUpdate(entry);
}

void DvbEpgTableModel::entryUpdated(const DvbSharedEpgEntry &entry)
{
	update(entry);
}

void DvbEpgTableModel::entryRemoved(const DvbSharedEpgEntry &entry)
{
	remove(entry);
}

void DvbEpgTableModel::customEvent(QEvent *event)
{
	Q_UNUSED(event)
	contentFilterEventPending = false;

	if (helper.filterType == DvbEpgTableModelHelper::ContentFilter) {
		reset(epgModel->getEntries());
	}
}
