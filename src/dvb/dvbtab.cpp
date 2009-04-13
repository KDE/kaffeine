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
#include <QToolButton>
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
	KAction *channelsAction = new KAction(KIcon("view-list-details"), i18n("Channels"), this);
	channelsAction->setShortcut(Qt::Key_C);
	connect(channelsAction, SIGNAL(triggered(bool)), this, SLOT(showChannelDialog()));
	menu->addAction(collection->addAction("dvb_channels", channelsAction));

	KAction *recordingsAction = new KAction(KIcon("view-pim-calendar"), i18n("Recordings"),
		this);
	recordingsAction->setShortcut(Qt::Key_R);
	connect(recordingsAction, SIGNAL(triggered(bool)), this, SLOT(showRecordingDialog()));
	menu->addAction(collection->addAction("dvb_recordings", recordingsAction));

	menu->addSeparator();

	instantRecordAction = new KAction(KIcon("document-save"), i18n("Instant Record"), this);
	instantRecordAction->setCheckable(true);
	connect(instantRecordAction, SIGNAL(triggered(bool)), this, SLOT(instantRecord(bool)));
	menu->addAction(collection->addAction("dvb_instant_record", instantRecordAction));

	menu->addSeparator();

	KAction *configureAction = new KAction(KIcon("configure"), i18n("Configure DVB"), this);
	connect(configureAction, SIGNAL(triggered(bool)), this, SLOT(configureDvb()));
	menu->addAction(collection->addAction("settings_dvb", configureAction));

	connect(mediaWidget, SIGNAL(dvbStopped()), this, SLOT(liveStopped()));
	connect(mediaWidget, SIGNAL(prepareDvbTimeShift()), this, SLOT(prepareTimeShift()));
	connect(mediaWidget, SIGNAL(startDvbTimeShift()), this, SLOT(startTimeShift()));

	dvbManager = new DvbManager(this);

	connect(dvbManager->getRecordingModel(), SIGNAL(instantRecordRemoved()),
		this, SLOT(instantRecordRemoved()));

	QBoxLayout *boxLayout = new QHBoxLayout(this);
	boxLayout->setMargin(0);

	QSplitter *splitter = new QSplitter(this);
	boxLayout->addWidget(splitter);

	QWidget *leftWidget = new QWidget(splitter);
	QBoxLayout *leftLayout = new QVBoxLayout(leftWidget);

	boxLayout = new QHBoxLayout();
	boxLayout->addWidget(new QLabel(i18n("Search:")));

	KLineEdit *lineEdit = new KLineEdit(leftWidget);
	lineEdit->setClearButtonShown(true);
	boxLayout->addWidget(lineEdit);
	leftLayout->addLayout(boxLayout);

	DvbChannelModel *channelModel = dvbManager->getChannelModel();
	channelView = new ProxyTreeView(leftWidget);
	channelView->setModel(channelModel);
	channelView->setRootIsDecorated(false);
	channelView->sortByColumn(0, Qt::AscendingOrder);
	channelView->setSortingEnabled(true);
	channelView->setContextMenu(new DvbChannelContextMenu(channelModel, channelView));
	connect(channelView, SIGNAL(activated(QModelIndex)), this, SLOT(playLive(QModelIndex)));
	connect(lineEdit, SIGNAL(textChanged(QString)),
		channelView->model(), SLOT(setFilterRegExp(QString)));
	leftLayout->addWidget(channelView);

	boxLayout = new QHBoxLayout();

	QToolButton *toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(configureAction);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(channelsAction);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(recordingsAction);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(leftWidget);
	toolButton->setDefaultAction(instantRecordAction);
	boxLayout->addWidget(toolButton);
	leftLayout->addLayout(boxLayout);

	QWidget *mediaContainer = new QWidget(splitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);
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

void DvbTab::instantRecord(bool checked)
{
	if (checked) {
		if (liveStream == NULL) {
			instantRecordAction->setChecked(false);
			return;
		}

		dvbManager->getRecordingModel()->startInstantRecord(liveStream->getChannel());
	} else {
		dvbManager->getRecordingModel()->stopInstantRecord();
	}
}

void DvbTab::instantRecordRemoved()
{
	instantRecordAction->setChecked(false);
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
	instantRecordAction->setChecked(false);

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
