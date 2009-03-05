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
#include <QSplitter>
#include <Phonon/AbstractMediaStream>
#include <KAction>
#include <KActionCollection>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include "../mediawidget.h"
#include "../proxytreeview.h"
#include "dvbchannelview.h"
#include "dvbconfigdialog.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbrecording.h"
#include "dvbscandialog.h"
#include "dvbsi.h"

class DvbStream : public Phonon::AbstractMediaStream
{
public:
	DvbStream(DvbDevice *device_, const QSharedDataPointer<DvbChannel> &channel_,
		const QList<int> &pids) : device(device_), channel(channel_), injector(device,
		channel->transportStreamId, channel->serviceId, channel->pmtPid, pids)
	{
		setStreamSize(-1);
	}

	~DvbStream() { }

	void writeData(const QByteArray &data)
	{
		Phonon::AbstractMediaStream::writeData(data);
	}

	DvbDevice *device;
	QSharedDataPointer<DvbChannel> channel;
	DvbPatPmtInjector injector;

private:
	void needData() { }
	void reset() { }
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
	channelView->setModel(channelModel);
	channelView->sortByColumn(0, Qt::AscendingOrder);
	channelView->setSortingEnabled(true);
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

QSharedDataPointer<DvbChannel> DvbTab::getLiveChannel() const
{
	if (liveStream != NULL) {
		return liveStream->channel;
	}

	return QSharedDataPointer<DvbChannel>();
}

void DvbTab::playLive(const QModelIndex &index)
{
	playChannel(dvbManager->getChannelModel()->getChannel(channelView->mapToSource(index)));
}

void DvbTab::liveDataReady(const QByteArray &data)
{
	liveStream->writeData(data);
}

void DvbTab::liveStopped()
{
	DvbDevice *device = liveStream->device;
	device->stopDevice();
	dvbManager->releaseDevice(device);
	liveStream = NULL;
}

void DvbTab::playChannel(const QSharedDataPointer<DvbChannel> &channel)
{
	delete liveStream; // liveStream is set to NULL by liveStopped()

	if (channel == NULL) {
		return;
	}

	DvbDevice *device = dvbManager->requestDevice(channel->source);

	if (device == NULL) {
		KMessageBox::sorry(this, i18n("No suitable device found."));
		return;
	}

	device->tuneDevice(channel->transponder);

	QList<int> pids;

	if (channel->videoPid != -1) {
		pids.append(channel->videoPid);
	}

	if (channel->audioPid != -1) {
		pids.append(channel->audioPid);
	}

	liveStream = new DvbStream(device, channel, pids);
	connect(liveStream, SIGNAL(destroyed(QObject*)), this, SLOT(liveStopped()));
	connect(&liveStream->injector, SIGNAL(dataReady(QByteArray)),
		this, SLOT(liveDataReady(QByteArray)));

	// FIXME audio streams, subtitles, ...

	mediaWidget->playDvb(liveStream);
}
