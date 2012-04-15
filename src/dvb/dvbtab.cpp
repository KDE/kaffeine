/*
 * dvbtab.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbtab.h"

#include <QBoxLayout>
#include <QDir>
#include <QHeaderView>
#include <QSplitter>
#include <QThread>
#include <QToolButton>
#include <KAction>
#include <KActionCollection>
#include <KLineEdit>
#include <KLocale>
#include <KMenu>
#include <KMessageBox>
#include "../log.h"
#include "../osdwidget.h"
#include "dvbchanneldialog.h"
#include "dvbconfigdialog.h"
#include "dvbepg.h"
#include "dvbepgdialog.h"
#include "dvbliveview.h"
#include "dvbmanager.h"
#include "dvbrecordingdialog.h"
#include "dvbscandialog.h"

class DvbTimeShiftCleaner : public QThread
{
public:
	explicit DvbTimeShiftCleaner(QObject *parent) : QThread(parent) { }

	~DvbTimeShiftCleaner()
	{
		wait();
	}

	void remove(const QString &path_, const QStringList &files_);

private:
	void run();

	QString path;
	QStringList files;
};

void DvbTimeShiftCleaner::remove(const QString &path_, const QStringList &files_)
{
	path = path_;
	files = files_;

	start();
}

void DvbTimeShiftCleaner::run()
{
	// delete files asynchronously because it may block for several seconds
	foreach (const QString &file, files) {
		QFile::remove(path + QLatin1Char('/') + file);
	}
}

DvbTab::DvbTab(KMenu *menu, KActionCollection *collection, MediaWidget *mediaWidget_) :
	mediaWidget(mediaWidget_)
{
	manager = new DvbManager(mediaWidget, this);

	KAction *channelsAction = new KAction(KIcon(QLatin1String("video-television")), i18n("Channels"), this);
	channelsAction->setShortcut(Qt::Key_C);
	connect(channelsAction, SIGNAL(triggered(bool)), this, SLOT(showChannelDialog()));
	menu->addAction(collection->addAction(QLatin1String("dvb_channels"), channelsAction));

	KAction *epgAction = new KAction(KIcon(QLatin1String("view-list-details")), i18n("Program Guide"), this);
	epgAction->setShortcut(Qt::Key_G);
	connect(epgAction, SIGNAL(triggered(bool)), this, SLOT(toggleEpgDialog()));
	menu->addAction(collection->addAction(QLatin1String("dvb_epg"), epgAction));

	KAction *osdAction = new KAction(KIcon(QLatin1String("dialog-information")), i18n("OSD"), this);
	osdAction->setShortcut(Qt::Key_O);
	connect(osdAction, SIGNAL(triggered(bool)), manager->getLiveView(), SLOT(toggleOsd()));
	menu->addAction(collection->addAction(QLatin1String("dvb_osd"), osdAction));

	KAction *recordingsAction = new KAction(KIcon(QLatin1String("view-pim-calendar")),
		i18nc("dialog", "Recording Schedule"), this);
	recordingsAction->setShortcut(Qt::Key_R);
	connect(recordingsAction, SIGNAL(triggered(bool)), this, SLOT(showRecordingDialog()));
	menu->addAction(collection->addAction(QLatin1String("dvb_recordings"), recordingsAction));

	menu->addSeparator();

	instantRecordAction = new KAction(KIcon(QLatin1String("document-save")), i18n("Instant Record"), this);
	instantRecordAction->setCheckable(true);
	connect(instantRecordAction, SIGNAL(triggered(bool)), this, SLOT(instantRecord(bool)));
	menu->addAction(collection->addAction(QLatin1String("dvb_instant_record"), instantRecordAction));

	menu->addSeparator();

	KAction *configureAction = new KAction(KIcon(QLatin1String("configure")),
		i18nc("@action:inmenu", "Configure Television..."), this);
	connect(configureAction, SIGNAL(triggered()), this, SLOT(configureDvb()));
	menu->addAction(collection->addAction(QLatin1String("settings_dvb"), configureAction));

	connect(manager->getLiveView(), SIGNAL(previous()), this, SLOT(previousChannel()));
	connect(manager->getLiveView(), SIGNAL(next()), this, SLOT(nextChannel()));

	connect(manager->getRecordingModel(), SIGNAL(recordingRemoved(DvbSharedRecording)),
		this, SLOT(recordingRemoved(DvbSharedRecording)));

	QBoxLayout *boxLayout = new QHBoxLayout(this);
	boxLayout->setMargin(0);

	splitter = new QSplitter(this);
	boxLayout->addWidget(splitter);

	QWidget *leftWidget = new QWidget(splitter);
	QBoxLayout *leftLayout = new QVBoxLayout(leftWidget);

	boxLayout = new QHBoxLayout();
	boxLayout->addWidget(new QLabel(i18n("Search:")));

	KLineEdit *lineEdit = new KLineEdit(leftWidget);
	lineEdit->setClearButtonShown(true);
	boxLayout->addWidget(lineEdit);
	leftLayout->addLayout(boxLayout);

	channelView = new DvbChannelView(leftWidget);
	channelView->setContextMenuPolicy(Qt::ActionsContextMenu);
	channelProxyModel = new DvbChannelTableModel(this);
	channelView->setModel(channelProxyModel);
	channelView->setRootIsDecorated(false);

	if (!channelView->header()->restoreState(QByteArray::fromBase64(
	    KGlobal::config()->group("DVB").readEntry("ChannelViewState", QByteArray())))) {
		channelView->sortByColumn(0, Qt::AscendingOrder);
	}

	channelView->setSortingEnabled(true);
	channelView->addEditAction();
	connect(channelView, SIGNAL(activated(QModelIndex)), this, SLOT(playChannel(QModelIndex)));
	channelProxyModel->setChannelModel(manager->getChannelModel());
	connect(lineEdit, SIGNAL(textChanged(QString)),
		channelProxyModel, SLOT(setFilter(QString)));
	manager->setChannelView(channelView);
	leftLayout->addWidget(channelView);

	boxLayout = new QHBoxLayout();

	QToolButton *toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(configureAction);
	toolButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(channelsAction);
	toolButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(epgAction);
	toolButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(recordingsAction);
	toolButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(instantRecordAction);
	toolButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	boxLayout->addWidget(toolButton);
	leftLayout->addLayout(boxLayout);

	QWidget *mediaContainer = new QWidget(splitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);
	splitter->setStretchFactor(1, 1);

	connect(mediaWidget, SIGNAL(osdKeyPressed(int)), this, SLOT(osdKeyPressed(int)));
	connect(&osdChannelTimer, SIGNAL(timeout()), this, SLOT(tuneOsdChannel()));

	lastChannel = KGlobal::config()->group("DVB").readEntry("LastChannel");

	splitter->restoreState(QByteArray::fromBase64(
		KGlobal::config()->group("DVB").readEntry("TabSplitterState", QByteArray())));

	timeShiftCleaner = new DvbTimeShiftCleaner(this);

	QTimer *timer = new QTimer(this);
	timer->start(30000);
	connect(timer, SIGNAL(timeout()), this, SLOT(cleanTimeShiftFiles()));
}

DvbTab::~DvbTab()
{
	KGlobal::config()->group("DVB").writeEntry("TabSplitterState",
		splitter->saveState().toBase64());
	KGlobal::config()->group("DVB").writeEntry("ChannelViewState",
		channelView->header()->saveState().toBase64());

	if (!currentChannel.isEmpty()) {
		lastChannel = currentChannel;
	}

	KGlobal::config()->group("DVB").writeEntry("LastChannel", lastChannel);
}

void DvbTab::playChannel(const QString &nameOrNumber)
{
	DvbChannelModel *channelModel = manager->getChannelModel();
	DvbSharedChannel channel;
	int number = nameOrNumber.toInt();

	if (number > 0) {
		channel = channelModel->findChannelByNumber(number);
	}

	if (!channel.isValid()) {
		channel = channelModel->findChannelByName(nameOrNumber);
	}

	if (channel.isValid()) {
		playChannel(channel, channelProxyModel->find(channel));
	}
}

void DvbTab::playLastChannel()
{
	if (!manager->getLiveView()->getChannel().isValid() && !currentChannel.isEmpty()) {
		lastChannel = currentChannel;
	}

	DvbSharedChannel channel = manager->getChannelModel()->findChannelByName(lastChannel);

	if (channel.isValid()) {
		playChannel(channel, channelProxyModel->find(channel));
	}
}

void DvbTab::toggleOsd()
{
	manager->getLiveView()->toggleOsd();
}

void DvbTab::toggleInstantRecord()
{
	instantRecordAction->trigger();
}

void DvbTab::enableDvbDump()
{
	manager->enableDvbDump();
}

void DvbTab::osdKeyPressed(int key)
{
	if ((key >= Qt::Key_0) && (key <= Qt::Key_9)) {
		osdChannel += QString::number(key - Qt::Key_0);
		osdChannelTimer.start(1500);
		mediaWidget->getOsdWidget()->showText(i18nc("osd", "Channel: %1_", osdChannel),
			1500);
	}
}

void DvbTab::mayCloseApplication(bool *ok, QWidget *parent)
{
	if (*ok) {
		DvbRecordingModel *recordingModel = manager->getRecordingModel();

		if (recordingModel->hasActiveRecordings()) {
			if (KMessageBox::warningYesNo(parent, i18nc("message box",
			    "Kaffeine is currently recording programs.\n"
			    "Do you really want to close the application?")) != KMessageBox::Yes) {
				*ok = false;
			}

			return;
		}

		if (recordingModel->hasRecordings()) {
			if (KMessageBox::questionYesNo(parent, i18nc("message box",
			    "Kaffeine has scheduled recordings.\n"
			    "Do you really want to close the application?"), QString(),
			    KStandardGuiItem::yes(), KStandardGuiItem::no(),
			    QLatin1String("ScheduledRecordings")) != KMessageBox::Yes) {
				*ok = false;
			}

			return;
		}
	}
}

void DvbTab::showChannelDialog()
{
	KDialog *dialog = new DvbScanDialog(manager, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbTab::showRecordingDialog()
{
	DvbRecordingDialog::showDialog(manager, this);
}

void DvbTab::toggleEpgDialog()
{
	if (epgDialog.isNull()) {
		epgDialog = new DvbEpgDialog(manager, this);
		epgDialog->setAttribute(Qt::WA_DeleteOnClose, true);
		epgDialog->setCurrentChannel(manager->getLiveView()->getChannel());
		epgDialog->setModal(false);
		epgDialog->show();
	} else {
		epgDialog->deleteLater();
		epgDialog = NULL;
	}
}

void DvbTab::instantRecord(bool checked)
{
	if (checked) {
		const DvbSharedChannel &channel = manager->getLiveView()->getChannel();

		if (!channel.isValid()) {
			instantRecordAction->setChecked(false);
			return;
		}

		DvbRecording recording;
		QList<DvbSharedEpgEntry> epgEntries =
			manager->getEpgModel()->getCurrentNext(channel);

		if (!epgEntries.isEmpty()) {
			recording.name = epgEntries.at(0)->title;
		}

		if (recording.name.isEmpty()) {
			recording.name =
				(channel->name + QTime::currentTime().toString(QLatin1String("-hhmmss")));
		}

		recording.channel = channel;
		recording.begin = QDateTime::currentDateTime().toUTC();
		recording.duration = QTime(12, 0);
		instantRecording = manager->getRecordingModel()->addRecording(recording);
		mediaWidget->getOsdWidget()->showText(i18nc("osd", "Instant Record Started"),
			1500);
	} else {
		manager->getRecordingModel()->removeRecording(instantRecording);
		mediaWidget->getOsdWidget()->showText(i18nc("osd", "Instant Record Stopped"),
			1500);
	}
}

void DvbTab::recordingRemoved(const DvbSharedRecording &recording)
{
	if (instantRecording == recording) {
		instantRecording = DvbSharedRecording();
		instantRecordAction->setChecked(false);
		mediaWidget->getOsdWidget()->showText(i18nc("osd", "Instant Record Stopped"),
			1500);
	}
}

void DvbTab::configureDvb()
{
	KDialog *dialog = new DvbConfigDialog(manager, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbTab::tuneOsdChannel()
{
	int number = osdChannel.toInt();
	osdChannel.clear();
	osdChannelTimer.stop();

	DvbSharedChannel channel = manager->getChannelModel()->findChannelByNumber(number);

	if (channel.isValid()) {
		playChannel(channel, channelProxyModel->find(channel));
	}
}

void DvbTab::playChannel(const QModelIndex &index)
{
	if (index.isValid()) {
		playChannel(channelProxyModel->value(index), index);
	}
}

void DvbTab::previousChannel()
{
	QModelIndex index = channelView->currentIndex();

	if (index.isValid()) {
		playChannel(index.sibling(index.row() - 1, index.column()));
	}
}

void DvbTab::nextChannel()
{
	QModelIndex index = channelView->currentIndex();

	if (index.isValid()) {
		playChannel(index.sibling(index.row() + 1, index.column()));
	}
}

void DvbTab::cleanTimeShiftFiles()
{
	if (timeShiftCleaner->isRunning()) {
		return;
	}

	QDir dir(manager->getTimeShiftFolder());
	QStringList entries =
		dir.entryList(QStringList(QLatin1String("TimeShift-*.m2t")), QDir::Files, QDir::Name);

	if (entries.count() < 2) {
		return;
	}

	entries.removeLast();

	timeShiftCleaner->remove(dir.path(), entries);
}

void DvbTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
	mediaWidget->setFocus();
}

void DvbTab::playChannel(const DvbSharedChannel &channel, const QModelIndex &index)
{
	if (!channel.isValid()) {
		Log("DvbTab::playChannel: channel is invalid");
		return;
	}

	if (!currentChannel.isEmpty()) {
		lastChannel = currentChannel;
	}

	channelView->setCurrentIndex(index);
	currentChannel = channel->name;
	manager->getLiveView()->playChannel(channel);
	
	if (!epgDialog.isNull()) {
		epgDialog->setCurrentChannel(manager->getLiveView()->getChannel());
	}
}
