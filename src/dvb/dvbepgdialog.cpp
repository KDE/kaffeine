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

#include "../log.h"

#include <KConfigGroup>
#include <QAction>
#include <QBoxLayout>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "dvbepgdialog.h"
#include "dvbepgdialog_p.h"
#include "dvbmanager.h"
#include "../iso-codes.h"

DvbEpgDialog::DvbEpgDialog(DvbManager *manager_, QWidget *parent) : QDialog(parent),
	manager(manager_)
{
	setWindowTitle(i18nc("@title:window", "Program Guide"));

	QWidget *mainWidget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout;
	setLayout(mainLayout);

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	mainLayout->addWidget(mainWidget);

	QWidget *widget = new QWidget(this);

	epgChannelTableModel = new DvbEpgChannelTableModel(this);
	epgChannelTableModel->setManager(manager);
	channelView = new QTreeView(widget);
	channelView->setMaximumWidth(30 * fontMetrics().averageCharWidth());
	channelView->setModel(epgChannelTableModel);
	channelView->setRootIsDecorated(false);
	connect(channelView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	mainLayout->addWidget(channelView);

	QBoxLayout *rightLayout = new QVBoxLayout();
	QBoxLayout *boxLayout = new QHBoxLayout();
	QBoxLayout *langLayout = new QHBoxLayout();

	QLabel *label = new QLabel(i18n("EPG language:"), mainWidget);
	langLayout->addWidget(label);

	languageBox = new QComboBox(mainWidget);
	languageBox->addItem("");
	QHashIterator<QString, bool> i(manager_->languageCodes);
	int j = 1;
	while (i.hasNext()) {
		i.next();
		QString lang = i.key();
		if (lang != FIRST_LANG) {
			languageBox->addItem(lang);
			if (manager_->currentEpgLanguage == lang) {
				languageBox->setCurrentIndex(j);
				currentLanguage = lang;
			}
			j++;
		}
	}
	langLayout->addWidget(languageBox);
	connect(languageBox, SIGNAL(currentTextChanged(QString)),
		this, SLOT(languageChanged(QString)));
	connect(manager_->getEpgModel(), SIGNAL(languageAdded(const QString)),
		this, SLOT(languageAdded(const QString)));

	languageLabel = new QLabel(mainWidget);
	langLayout->addWidget(languageLabel);
	languageLabel->setBuddy(languageBox);
	QString languageString;
	if (IsoCodes::getLanguage(currentLanguage, &languageString))
		languageLabel->setText(languageString);
	else if (currentLanguage == "")
		languageLabel->setText(i18n("Any language"));
	else
		languageLabel->setText("");

	rightLayout->addLayout(langLayout);

	QAction *scheduleAction = new QAction(QIcon::fromTheme(QLatin1String("media-record"), QIcon(":media-record")),
		i18nc("@action:inmenu tv show", "Record Show"), this);
	connect(scheduleAction, SIGNAL(triggered()), this, SLOT(scheduleProgram()));

	QPushButton *pushButton =
		new QPushButton(scheduleAction->icon(), scheduleAction->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(scheduleProgram()));
	boxLayout->addWidget(pushButton);

	boxLayout->addWidget(new QLabel(i18nc("@label:textbox", "Search:"), widget));

	epgTableModel = new DvbEpgTableModel(this);
	epgTableModel->setEpgModel(manager->getEpgModel());
	epgTableModel->setLanguage(currentLanguage);
	connect(epgTableModel, SIGNAL(layoutChanged()), this, SLOT(checkEntry()));
	QLineEdit *lineEdit = new QLineEdit(widget);
	lineEdit->setClearButtonEnabled(true);
	connect(lineEdit, SIGNAL(textChanged(QString)),
		epgTableModel, SLOT(setContentFilter(QString)));
	boxLayout->addWidget(lineEdit);
	rightLayout->addLayout(boxLayout);

	epgView = new QTreeView(widget);
	epgView->addAction(scheduleAction);
	epgView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
	mainLayout->addWidget(widget);

	rightLayout->addWidget(buttonBox);
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

void DvbEpgDialog::languageChanged(const QString lang)
{
	// Handle the any language case
	if (languageBox->currentIndex() <= 0)
		currentLanguage = "";
	else
		currentLanguage = lang;
	QString languageString;
	if (IsoCodes::getLanguage(currentLanguage, &languageString))
		languageLabel->setText(languageString);
	else if (currentLanguage == "")
		languageLabel->setText(i18n("Any language"));
	else
		languageLabel->setText("");

	epgTableModel->setLanguage(currentLanguage);
	epgView->setCurrentIndex(epgTableModel->index(0, 0));
	entryActivated(epgTableModel->index(0, 0));
	manager->currentEpgLanguage = currentLanguage;
}

void DvbEpgDialog::languageAdded(const QString lang)
{
	if (lang != FIRST_LANG)
		languageBox->addItem(lang);
}

void DvbEpgDialog::entryActivated(const QModelIndex &index)
{
	const DvbSharedEpgEntry &entry = epgTableModel->value(index.row());

	if (!entry.isValid()) {
		contentLabel->setText(QString());
		return;
	}

	QString text = "<font color=#008000 size=\"+1\">" + entry->title(currentLanguage) + "</font>";

	if (!entry->subheading().isEmpty()) {
		text += "<br/><font color=#808000>" + entry->subheading(currentLanguage) + "</font>";
	}

	QDateTime begin = entry->begin.toLocalTime();
	QTime end = entry->begin.addSecs(QTime(0, 0, 0).secsTo(entry->duration)).toLocalTime().time();
	text += "<br/><br/><font color=#800080>" + QLocale().toString(begin, QLocale::LongFormat) + " - " + QLocale().toString(end) + "</font>";

	if (!entry->details(currentLanguage).isEmpty() && entry->details(currentLanguage) !=  entry->title(currentLanguage)) {
		text += "<br/><br/>" + entry->details(currentLanguage);
	}

	if (!entry->content.isEmpty()) {
		text += "<br/><br/><font color=#000080>" + entry->content + "</font>";
	}
	if (!entry->parental.isEmpty()) {
		text += "<br/><br/><font color=#800000>" + entry->parental + "</font>";
	}

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

	if (x->title(FIRST_LANG) != y->title(FIRST_LANG)) {
		return (x->title(FIRST_LANG) < y->title(FIRST_LANG));
	}

	if (x->subheading(FIRST_LANG) != y->subheading(FIRST_LANG)) {
		return (x->subheading(FIRST_LANG) < y->subheading(FIRST_LANG));
	}

	if (x->details(FIRST_LANG) < y->details(FIRST_LANG)) {
		return (x->details(FIRST_LANG) < y->details(FIRST_LANG));
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
		return ((contentFilter.indexIn(entry->title(FIRST_LANG)) >= 0) ||
			(contentFilter.indexIn(entry->subheading(FIRST_LANG)) >= 0) ||
			(contentFilter.indexIn(entry->details(FIRST_LANG)) >= 0));
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
		qCWarning(logEpg, "EPG model already set");
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

void DvbEpgTableModel::setLanguage(QString lang)
{
	currentLanguage = lang;
	reset(epgModel->getEntries());
}

QVariant DvbEpgTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedEpgEntry &entry = value(index);

	if (entry.isValid()) {
		switch (role) {
		case Qt::DecorationRole:
			if ((index.column() == 2) && entry->recording.isValid()) {
				return QIcon::fromTheme(QLatin1String("media-record"), QIcon(":media-record"));
			}

			break;
		case Qt::DisplayRole:
			switch (index.column()) {
			case 0:
				return QLocale().toString((entry->begin.toLocalTime()), QLocale::NarrowFormat);
			case 1:
				return entry->duration.toString("HH:mm");
			case 2:
				return entry->title(currentLanguage);
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
