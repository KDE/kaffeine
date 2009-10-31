/*
 * dvbtab.cpp
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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
#include "../mediawidget.h"
#include "../osdwidget.h"
#include "dvbchannelui.h"
#include "dvbconfigdialog.h"
#include "dvbepg.h"
#include "dvbliveview.h"
#include "dvbmanager.h"
#include "dvbrecording.h"
#include "dvbscandialog.h"

class DvbLiveStream : public DvbPidFilter, public QObject
{
public:
	DvbLiveStream(DvbDevice *device_, const QSharedDataPointer<DvbChannel> &channel_,
		MediaWidget *mediaWidget_, const QList<int> &pids_);
	~DvbLiveStream();

	DvbDevice *getDevice() const
	{
		return device;
	}

	QSharedDataPointer<DvbChannel> getChannel() const
	{
		return channel;
	}

	QString getTimeShiftFileName() const
	{
		return timeShiftFile.fileName();
	}

	void removePidFilters();
	bool startTimeShift(const QString &fileName);
	void replacePid(int oldPid, int newPid);

	int audioPid;
	int subtitlePid;
	QList<int> audioPids;
	QList<int> subtitlePids;
	DvbEitFilter *eitFilter;

private:
	void processData(const char data[188]);
	void timerEvent(QTimerEvent *);

	DvbDevice *device;
	const QSharedDataPointer<DvbChannel> channel;
	MediaWidget *mediaWidget;
	QList<int> pids;

	DvbSectionGenerator patGenerator;
	DvbSectionGenerator pmtGenerator;
	QByteArray buffer;

	QFile timeShiftFile;
};

DvbLiveStream::DvbLiveStream(DvbDevice *device_, const QSharedDataPointer<DvbChannel> &channel_,
	MediaWidget *mediaWidget_, const QList<int> &pids_) : audioPid(-1), subtitlePid(-1),
	device(device_), channel(channel_), mediaWidget(mediaWidget_), pids(pids_)
{
	foreach (int pid, pids) {
		device->addPidFilter(pid, this);
	}

	patGenerator.initPat(channel->transportStreamId, channel->serviceId, channel->pmtPid);
	pmtGenerator.initPmt(channel->pmtPid, DvbPmtSection(DvbSection(channel->pmtSection)), pids);

	buffer.reserve(64 * 188);
	buffer.append(patGenerator.generatePackets());
	buffer.append(pmtGenerator.generatePackets());

	startTimer(500);
}

DvbLiveStream::~DvbLiveStream()
{
}

void DvbLiveStream::removePidFilters()
{
	foreach (int pid, pids) {
		device->removePidFilter(pid, this);
	}
}

bool DvbLiveStream::startTimeShift(const QString &fileName)
{
	timeShiftFile.setFileName(fileName);

	if (timeShiftFile.exists() || !timeShiftFile.open(QIODevice::WriteOnly)) {
		return false;
	}

	timeShiftFile.write(patGenerator.generatePackets());
	timeShiftFile.write(pmtGenerator.generatePackets());

	return true;
}

void DvbLiveStream::replacePid(int oldPid, int newPid)
{
	if (oldPid == newPid) {
		return;
	}

	if (oldPid != -1) {
		device->removePidFilter(oldPid, this);
		pids.removeAll(oldPid);
	}

	if (newPid != -1) {
		device->addPidFilter(newPid, this);
		pids.append(newPid);
	}

	pmtGenerator.initPmt(channel->pmtPid, DvbPmtSection(DvbSection(channel->pmtSection)), pids);
	buffer.append(patGenerator.generatePackets());
	buffer.append(pmtGenerator.generatePackets());
}

void DvbLiveStream::processData(const char data[188])
{
	buffer.append(QByteArray::fromRawData(data, 188));

	if (buffer.size() < (64 * 188)) {
		return;
	}

	if (!timeShiftFile.isOpen()) {
		mediaWidget->writeDvbData(buffer);
	} else {
		timeShiftFile.write(buffer);
	}

	buffer.clear();
	buffer.reserve(64 * 188);
}

void DvbLiveStream::timerEvent(QTimerEvent *)
{
	buffer.append(patGenerator.generatePackets());
	buffer.append(pmtGenerator.generatePackets());
}

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
		QFile::remove(path + '/' + file);
	}
}

DvbTab::DvbTab(KMenu *menu, KActionCollection *collection, MediaWidget *mediaWidget_) :
	mediaWidget(mediaWidget_)
{
	manager = new DvbManager(mediaWidget, this);

	KAction *channelsAction = new KAction(KIcon("video-television"), i18n("Channels"), this);
	channelsAction->setShortcut(Qt::Key_C);
	connect(channelsAction, SIGNAL(triggered(bool)), this, SLOT(showChannelDialog()));
	menu->addAction(collection->addAction("dvb_channels", channelsAction));

	KAction *epgAction = new KAction(KIcon("view-list-details"), i18n("Program Guide"), this);
	epgAction->setShortcut(Qt::Key_G);
	connect(epgAction, SIGNAL(triggered(bool)), this, SLOT(showEpgDialog()));
	menu->addAction(collection->addAction("dvb_epg", epgAction));

	KAction *osdAction = new KAction(KIcon("dialog-information"), i18n("OSD"), this);
	osdAction->setShortcut(Qt::Key_O);
	connect(osdAction, SIGNAL(triggered(bool)), manager->getLiveView(), SLOT(toggleOsd()));
	menu->addAction(collection->addAction("dvb_osd", osdAction));

	KAction *recordingsAction = new KAction(KIcon("view-pim-calendar"),
						i18nc("dialog", "Recording Schedule"), this);
	recordingsAction->setShortcut(Qt::Key_R);
	connect(recordingsAction, SIGNAL(triggered(bool)), this, SLOT(showRecordingDialog()));
	menu->addAction(collection->addAction("dvb_recordings", recordingsAction));

	menu->addSeparator();

	instantRecordAction = new KAction(KIcon("document-save"), i18n("Instant Record"), this);
	instantRecordAction->setCheckable(true);
	connect(instantRecordAction, SIGNAL(triggered(bool)), this, SLOT(instantRecord(bool)));
	menu->addAction(collection->addAction("dvb_instant_record", instantRecordAction));

	menu->addSeparator();

	KAction *configureAction = new KAction(KIcon("configure"), i18n("Configure Television"), this);
	connect(configureAction, SIGNAL(triggered(bool)), this, SLOT(configureDvb()));
	menu->addAction(collection->addAction("settings_dvb", configureAction));

	connect(mediaWidget, SIGNAL(previousDvbChannel()), this, SLOT(previousChannel()));
	connect(mediaWidget, SIGNAL(nextDvbChannel()), this, SLOT(nextChannel()));

	connect(manager->getRecordingModel(), SIGNAL(instantRecordingRemoved()),
		this, SLOT(instantRecordingRemoved()));

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

	DvbChannelModel *channelModel = manager->getChannelModel();
	channelView = new DvbChannelView(channelModel, leftWidget);
	channelView->setModel(channelModel);
	channelView->setRootIsDecorated(false);

	if (!channelView->header()->restoreState(QByteArray::fromBase64(
	    KGlobal::config()->group("DVB").readEntry("ChannelViewState", QByteArray())))) {
		channelView->sortByColumn(0, Qt::AscendingOrder);
	}

	channelView->setSortingEnabled(true);
	connect(channelView, SIGNAL(activated(QModelIndex)), this, SLOT(playLive(QModelIndex)));
	connect(lineEdit, SIGNAL(textChanged(QString)),
		channelView->model(), SLOT(setFilterRegExp(QString)));
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
}

void DvbTab::playChannel(const QString &name)
{
	playChannel(manager->getChannelModel()->channelForName(name));
}

void DvbTab::playLastChannel()
{
	playChannel(KGlobal::config()->group("DVB").readEntry("LastChannel"));
}

void DvbTab::enableDvbDump()
{
	manager->enableDvbDump();
}

void DvbTab::showChannelDialog()
{
	DvbScanDialog dialog(manager, this);
	dialog.exec();
}

void DvbTab::showRecordingDialog()
{
	DvbRecordingDialog dialog(manager, this);
	dialog.exec();
}

void DvbTab::showEpgDialog()
{
	QSharedDataPointer<DvbChannel> channel = manager->getLiveView()->getChannel();
	QString channelName;

	if (channel != NULL) {
		channelName = channel->name;
	}

	DvbEpgDialog dialog(manager, channelName, this);
	dialog.exec();
}

void DvbTab::instantRecord(bool checked)
{
	if (checked) {
		QString channelName = manager->getLiveView()->getChannel()->name;

		if (channelName.isEmpty()) {
			instantRecordAction->setChecked(false);
			return;
		}

		// FIXME use epg for name
		manager->getRecordingModel()->startInstantRecording(
			channelName + QTime::currentTime().toString("-hhmmss"), channelName);

		mediaWidget->getOsdWidget()->showText(i18nc("osd", "Instant Record Started"), 1500);
	} else {
		manager->getRecordingModel()->stopInstantRecording();
		mediaWidget->getOsdWidget()->showText(i18nc("osd", "Instant Record Stopped"), 1500);
	}
}

void DvbTab::instantRecordingRemoved()
{
	instantRecordAction->setChecked(false);
	mediaWidget->getOsdWidget()->showText(i18nc("osd", "Instant Record Stopped"), 1500);
}

void DvbTab::configureDvb()
{
	DvbConfigDialog dialog(manager, this);
	dialog.exec();
}

void DvbTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
	mediaWidget->setFocus();
}

void DvbTab::playLive(const QModelIndex &index)
{
	playChannel(manager->getChannelModel()->getChannel(channelView->mapToSource(index)));
}

void DvbTab::previousChannel()
{
	// it's enough to look at a single element if you can only select a single row
	QModelIndex index = channelView->currentIndex();

	if (!index.isValid()) {
		return;
	}

	QModelIndex previous = index.sibling(index.row() - 1, index.column());
	int sourceIndex = channelView->mapToSource(previous);

	if (sourceIndex < 0) {
		return;
	}

	channelView->setCurrentIndex(previous);
	playChannel(manager->getChannelModel()->getChannel(sourceIndex));
}

void DvbTab::nextChannel()
{
	// it's enough to look at a single element if you can only select a single row
	QModelIndex index = channelView->currentIndex();

	if (!index.isValid()) {
		return;
	}

	QModelIndex next = index.sibling(index.row() + 1, index.column());
	int sourceIndex = channelView->mapToSource(next);

	if (sourceIndex < 0) {
		return;
	}

	channelView->setCurrentIndex(next);
	playChannel(manager->getChannelModel()->getChannel(sourceIndex));
}

void DvbTab::cleanTimeShiftFiles()
{
	if (timeShiftCleaner->isRunning()) {
		return;
	}

	QDir dir(manager->getTimeShiftFolder());
	QStringList entries = dir.entryList(QStringList("TimeShift-*.m2t"), QDir::Files, QDir::Name);

	if (entries.count() < 2) {
		return;
	}

	entries.removeLast();

	timeShiftCleaner->remove(dir.path(), entries);
}

void DvbTab::playChannel(const QSharedDataPointer<DvbChannel> &channel)
{
	if (channel != NULL) {
		manager->getLiveView()->playChannel(channel);
	}
}
