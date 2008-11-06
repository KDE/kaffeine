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
#include <KDebug>
#include <KLineEdit>
#include <KMessageBox>
#include "../kaffeine.h"
#include "../manager.h"
#include "../mediawidget.h"
#include "dvbchannel.h"
#include "dvbchannelview.h"
#include "dvbconfigdialog.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
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

DvbTab::DvbTab(Manager *manager_) : TabBase(manager_), liveStream(NULL)
{
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

	DvbChannelView *channels = new DvbChannelView(leftSideWidget);
	QSortFilterProxyModel *proxyModel = dvbManager->getChannelModel()->getProxyModel();
	channels->setModel(proxyModel);
	connect(channels, SIGNAL(activated(QModelIndex)), this, SLOT(playLive(QModelIndex)));
	connect(lineEdit, SIGNAL(textChanged(QString)), proxyModel, SLOT(setFilterRegExp(QString)));
	leftSideLayout->addWidget(channels);

	QWidget *mediaContainer = new QWidget(splitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);

	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
}

DvbTab::~DvbTab()
{
}

void DvbTab::configureChannels()
{
	DvbScanDialog dialog(this);
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
	mediaLayout->addWidget(manager->getMediaWidget());
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

// FIXME - just a demo hack
void DvbTab::playLive(const QModelIndex &index)
{
	if (liveStream != NULL) {
		liveStream->liveStopped();
		// FIXME hacky; but ok for now
		disconnect(liveStream, SIGNAL(destroyed(QObject *)), this, SLOT(liveStopped()));
		liveStream = NULL;
	}

	// don't call manager->getMediaWidget()->stop() here
	// otherwise the media widget runs into trouble
	// because the stop event arrives after starting playback ...

	DvbSharedChannel channel = *dvbManager->getChannelModel()->getChannel(index);

	if (channel == NULL) {
		manager->getMediaWidget()->stop();
		return;
	}

	DvbDevice *device = dvbManager->requestDevice(channel->source);

	if (device == NULL) {
		manager->getMediaWidget()->stop();
		KMessageBox::sorry(this, i18n("No suitable device found."));
		return;
	}

	liveStream = new DvbStream(dvbManager, device, channel);
	connect(liveStream, SIGNAL(destroyed(QObject *)), this, SLOT(liveStopped()));

	device->tuneDevice(channel->transponder);
	device->addPidFilter(channel->videoPid, liveStream);
	device->addPidFilter(channel->audioPid, liveStream);

	manager->getMediaWidget()->playDvb(liveStream);
}

void DvbTab::liveStopped()
{
	liveStream = NULL;
}
