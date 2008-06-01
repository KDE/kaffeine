/*
 * dvbtab.cpp
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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
#include <KLocalizedString>
#include "../engine.h"
#include "../kaffeine.h"
#include "../manager.h"
#include "../mediawidget.h"
#include "dvbchannel.h"
#include "dvbconfig.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbscandialog.h"

// FIXME - DvbStream is just a demo hack

class DvbStream : public DvbLiveFeed, public DvbPidFilter
{
public:
	DvbStream(DvbDevice *device_) : device(device_), bufferPos(0)
	{
		buffer.resize(188 * 64);
	}

	~DvbStream() { }

private:
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

	DvbDevice *device;
	QByteArray buffer;
	int bufferPos;
};

DvbTab::DvbTab(Manager *manager_) : TabBase(manager_), liveDevice(NULL), dvbStream(NULL)
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
	delete dvbStream;
}

void DvbTab::configureChannels()
{
	DvbScanDialog dialog(this);
	dialog.exec();
}

void DvbTab::configureDvb()
{
	DvbConfigDialog dialog(this);
	dialog.exec();
}

void DvbTab::activate()
{
	mediaLayout->addWidget(manager->getMediaWidget());
}

// FIXME - just a demo hack
void DvbTab::playLive(const QModelIndex &index)
{
	QList<DvbDevice *> devices = dvbManager->getDeviceList();

	if (devices.isEmpty()) {
		return;
	}

	DvbDevice *device = devices.at(0);
	device->stopDevice();

	manager->getMediaWidget()->stop();

	delete dvbStream;
	dvbStream = new DvbStream(device);

	const DvbChannel *channel = dvbManager->getChannelModel()->getChannel(index);

	DvbSConfig config("test");
	device->tuneDevice(channel->getTransponder().data(), &config);

	device->addPidFilter(channel->videoPid, dvbStream);
	device->addPidFilter(channel->audioPid, dvbStream);

	liveDevice = device;
	liveChannel = channel;
	connect(dvbStream, SIGNAL(livePaused(bool)), this, SLOT(livePaused(bool)));
	connect(dvbStream, SIGNAL(liveStopped()), this, SLOT(liveStopped()));

	manager->getMediaWidget()->playDvb(dvbStream);
}

void DvbTab::livePaused(bool /*paused*/)
{
	// FIXME - timeshift & co
}

void DvbTab::liveStopped()
{
	liveDevice->stopDevice();
	liveDevice = NULL;
}
