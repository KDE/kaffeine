/*
 * dvbtab.cpp
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dvbtab.h"

#include <QBoxLayout>
#include <QDir>
#include <QSplitter>
#include <KAction>
#include <KActionCollection>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include "../mediawidget.h"
#include "../proxytreeview.h"
#include "dvbchannelui.h"
#include "dvbconfigdialog.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbrecording.h"
#include "dvbscandialog.h"
#include "dvbsi.h"

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

private:
	void processData(const char data[188]);
	void timerEvent(QTimerEvent *);

	DvbDevice *device;
	QSharedDataPointer<DvbChannel> channel;
	MediaWidget *mediaWidget;
	QList<int> pids;

	DvbSectionGenerator patGenerator;
	DvbSectionGenerator pmtGenerator;
	QByteArray buffer;

	QFile timeShiftFile;
};

DvbLiveStream::DvbLiveStream(DvbDevice *device_, const QSharedDataPointer<DvbChannel> &channel_,
	MediaWidget *mediaWidget_, const QList<int> &pids_) : device(device_), channel(channel_),
	mediaWidget(mediaWidget_), pids(pids_)
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

DvbTab::DvbTab(KMenu *menu, KActionCollection *collection, MediaWidget *mediaWidget_) :
	mediaWidget(mediaWidget_), liveStream(NULL)
{
	KAction *action = new KAction(KIcon("view-list-details"), i18n("Channels"), collection);
	action->setShortcut(Qt::Key_C);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(showChannelDialog()));
	menu->addAction(collection->addAction("dvb_channels", action));

	action = new KAction(KIcon("view-pim-calendar"), i18n("Recordings"), collection);
	action->setShortcut(Qt::Key_R);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(showRecordingDialog()));
	menu->addAction(collection->addAction("dvb_recordings", action));

	menu->addSeparator();

	action = new KAction(KIcon("configure"), i18n("Configure DVB"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(configureDvb()));
	menu->addAction(collection->addAction("settings_dvb", action));

	connect(mediaWidget, SIGNAL(dvbStopped()), this, SLOT(liveStopped()));
	connect(mediaWidget, SIGNAL(prepareDvbTimeShift()), this, SLOT(prepareTimeShift()));
	connect(mediaWidget, SIGNAL(startDvbTimeShift()), this, SLOT(startTimeShift()));

	dvbManager = new DvbManager(this);

	QBoxLayout *widgetLayout = new QHBoxLayout(this);
	widgetLayout->setMargin(0);

	QSplitter *splitter = new QSplitter(this);
	widgetLayout->addWidget(splitter);

	QWidget *leftSideWidget = new QWidget(splitter);
	QBoxLayout *leftSideLayout = new QVBoxLayout(leftSideWidget);
	leftSideLayout->setMargin(3);

	QWidget *searchBoxWidget = new QWidget(leftSideWidget);
	QBoxLayout *searchBoxLayout = new QHBoxLayout(searchBoxWidget);
	searchBoxLayout->setMargin(0);
	leftSideLayout->addWidget(searchBoxWidget);

	searchBoxLayout->addWidget(new QLabel(i18n("Search:"), searchBoxWidget));

	KLineEdit *lineEdit = new KLineEdit(searchBoxWidget);
	lineEdit->setClearButtonShown(true);
	searchBoxLayout->addWidget(lineEdit);

	DvbChannelModel *channelModel = dvbManager->getChannelModel();
	channelView = new ProxyTreeView(leftSideWidget);
	channelView->setModel(channelModel);
	channelView->setRootIsDecorated(false);
	channelView->sortByColumn(0, Qt::AscendingOrder);
	channelView->setSortingEnabled(true);
	channelView->setContextMenu(new DvbChannelContextMenu(channelModel, channelView));
	connect(channelView, SIGNAL(activated(QModelIndex)), this, SLOT(playLive(QModelIndex)));
	connect(lineEdit, SIGNAL(textChanged(QString)),
		channelView->model(), SLOT(setFilterRegExp(QString)));
	leftSideLayout->addWidget(channelView);

	QWidget *mediaContainer = new QWidget(splitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);

	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
}

DvbTab::~DvbTab()
{
}

DvbDevice *DvbTab::getLiveDevice() const
{
	if (liveStream != NULL) {
		return liveStream->getDevice();
	}

	return NULL;
}

QSharedDataPointer<DvbChannel> DvbTab::getLiveChannel() const
{
	if (liveStream != NULL) {
		return liveStream->getChannel();
	}

	return QSharedDataPointer<DvbChannel>(NULL);
}

void DvbTab::playChannel(const QString &name)
{
	playChannel(dvbManager->getChannelModel()->channelForName(name));
}

void DvbTab::showChannelDialog()
{
	DvbScanDialog dialog(this);
	dialog.exec();
}

void DvbTab::showRecordingDialog()
{
	DvbRecordingDialog dialog(dvbManager, this);
	dialog.exec();
}

void DvbTab::configureDvb()
{
	DvbConfigDialog dialog(dvbManager, this);
	dialog.exec();
}

void DvbTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
}

void DvbTab::playLive(const QModelIndex &index)
{
	playChannel(dvbManager->getChannelModel()->getChannel(channelView->mapToSource(index)));
}

void DvbTab::prepareTimeShift()
{
	// FIXME .ts <--> .m2t ?
	QString fileName = dvbManager->getTimeShiftFolder() + "/TimeShift-" +
		QDateTime::currentDateTime().toString("yyyyMMddThhmmss") + ".ts";

	if (!liveStream->startTimeShift(fileName)) {
		// FIXME error message
		mediaWidget->stopDvb();
		return;
	}
}

void DvbTab::startTimeShift()
{
	mediaWidget->play(liveStream->getTimeShiftFileName());
}

void DvbTab::liveStopped()
{
	liveStream->removePidFilters();
	dvbManager->releaseDevice(liveStream->getDevice());
	delete liveStream;
	liveStream = NULL;
}

void DvbTab::playChannel(const QSharedDataPointer<DvbChannel> &channel)
{
	mediaWidget->stopDvb();

	if (channel == NULL) {
		return;
	}

	DvbDevice *device = dvbManager->requestDevice(channel->source, channel->transponder);

	if (device == NULL) {
		KMessageBox::sorry(this, i18n("No suitable device found."));
		return;
	}

	mediaWidget->playDvb();

	QList<int> pids;

	if (channel->videoPid != -1) {
		pids.append(channel->videoPid);
	}

	if (channel->audioPid != -1) {
		pids.append(channel->audioPid);
	}

	liveStream = new DvbLiveStream(device, channel, mediaWidget, pids);

	// FIXME audio streams, subtitles, ...
}
