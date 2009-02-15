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

#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <KAction>
#include <KActionCollection>
#include <KDebug>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include "../mediawidget.h"
#include "../proxytreeview.h"
#include "dvbchannel.h"
#include "dvbchannelview.h"
#include "dvbconfigdialog.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbrecording.h"
#include "dvbscandialog.h"

// FIXME - DvbStream is just a demo hack

class DvbStream : public DvbLiveFeed, public DvbPidFilter
{
public:
	DvbStream(DvbManager *dvbManager_, DvbDevice *device_, DvbSharedChannel channel_) :
		dvbManager(dvbManager_), device(device_), channel(channel_), bufferPos(0)
	{
		setStreamSize(-1);
		buffer.resize(188 * 64);
	}

	~DvbStream() { }

	void livePaused(bool /*paused*/)
	{
		// FIXME timeshift & co
	}

	void liveStopped()
	{
		if (device != NULL) {
			device->stopDevice();
			dvbManager->releaseDevice(device);
			device = NULL;
		}
	}

	DvbManager *dvbManager;
	DvbDevice *device;
	DvbSharedChannel channel;

private:
	void needData() { }
	void reset() { }

	void processData(const char data[188])
	{
		// FIXME too hacky
		memcpy(buffer.data() + bufferPos, data, 188);
		bufferPos += 188;
		if (bufferPos == (188 * 64)) {
			emit writeData(buffer);
			buffer.clear();
			buffer.resize(188 * 64);
			bufferPos = 0;
		}
	}

	QByteArray buffer;
	int bufferPos;
};

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
	channelView->setIndentation(0);
	channelView->setSortingEnabled(true);
	channelView->sortByColumn(0, Qt::AscendingOrder);
	channelView->setModel(channelModel);
	channelView->addContextActions(channelModel->createContextActions());
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

void DvbTab::playChannel(const QString &name)
{
	// FIXME ugly

	const DvbSharedChannel *channel = dvbManager->getChannelModel()->channelForName(name);

	if (channel != NULL) {
		playChannel(*channel);
	} else {
		playChannel(DvbSharedChannel(NULL));
	}
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
	DvbConfigDialog *dialog = new DvbConfigDialog(this, dvbManager);
	dialog->setModal(true);
	dialog->show();
}

void DvbTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
}

DvbDevice *DvbTab::getLiveDevice() const
{
	if (liveStream != NULL) {
		return liveStream->device;
	}

	return NULL;
}

DvbSharedChannel DvbTab::getLiveChannel() const
{
	if (liveStream != NULL) {
		return liveStream->channel;
	}

	return DvbSharedChannel();
}

void DvbTab::playLive(const QModelIndex &index)
{
	playChannel(dvbManager->getChannelModel()->getChannel(channelView->mapToSource(index)));
}

void DvbTab::liveStopped()
{
	liveStream = NULL;
}

void DvbTab::playChannel(const DvbSharedChannel &channel)
{
	if (liveStream != NULL) {
		liveStream->liveStopped();
		// FIXME hacky; but ok for now
		disconnect(liveStream, SIGNAL(destroyed(QObject *)), this, SLOT(liveStopped()));
		liveStream = NULL;
	}

	// don't call mediaWidget->stop() here
	// otherwise the media widget runs into trouble
	// because the stop event arrives after starting playback ...

	if (channel == NULL) {
		mediaWidget->stop();
		return;
	}

	DvbDevice *device = dvbManager->requestDevice(channel->source);

	if (device == NULL) {
		mediaWidget->stop();
		KMessageBox::sorry(this, i18n("No suitable device found."));
		return;
	}

	liveStream = new DvbStream(dvbManager, device, channel);
	connect(liveStream, SIGNAL(destroyed(QObject *)), this, SLOT(liveStopped()));

	device->tuneDevice(channel->transponder);

	if (channel->videoPid != -1) {
		device->addPidFilter(channel->videoPid, liveStream);
	}

	if (channel->audioPid != -1) {
		device->addPidFilter(channel->audioPid, liveStream);
	}

	// FIXME synthesize pat / pmt, audio streams, subtitles, ...

	mediaWidget->playDvb(liveStream);
}
