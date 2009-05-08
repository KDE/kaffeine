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
#include <QSplitter>
#include <QThread>
#include <QToolButton>
#include <KAction>
#include <KActionCollection>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include "../mediawidget.h"
#include "dvbchannelui.h"
#include "dvbconfigdialog.h"
#include "dvbepg.h"
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
	mediaWidget(mediaWidget_), liveStream(NULL)
{
	KAction *channelsAction = new KAction(KIcon("video-television"), i18n("Channels"), this);
	channelsAction->setShortcut(Qt::Key_C);
	connect(channelsAction, SIGNAL(triggered(bool)), this, SLOT(showChannelDialog()));
	menu->addAction(collection->addAction("dvb_channels", channelsAction));

	KAction *epgAction = new KAction(KIcon("view-list-details"), i18n("Program Guide"), this);
	epgAction->setShortcut(Qt::Key_G);
	connect(epgAction, SIGNAL(triggered(bool)), this, SLOT(showEpgDialog()));
	menu->addAction(collection->addAction("dvb_epg", epgAction));

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
	connect(mediaWidget, SIGNAL(prepareDvbTimeShift()), this, SLOT(prepareTimeShift()));
	connect(mediaWidget, SIGNAL(startDvbTimeShift()), this, SLOT(startTimeShift()));
	connect(mediaWidget, SIGNAL(changeDvbAudioChannel(int)),
		this, SLOT(changeAudioChannel(int)));
	connect(mediaWidget, SIGNAL(changeDvbSubtitle(int)), this, SLOT(changeSubtitle(int)));
	connect(mediaWidget, SIGNAL(dvbStopped()), this, SLOT(liveStopped()));

	dvbManager = new DvbManager(this);

	connect(dvbManager->getRecordingModel(), SIGNAL(instantRecordingRemoved()),
		this, SLOT(instantRecordingRemoved()));

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
	channelView = new DvbChannelView(channelModel, leftWidget);
	channelView->setModel(channelModel);
	channelView->setRootIsDecorated(false);
	channelView->sortByColumn(0, Qt::AscendingOrder);
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

	fastRetuneTimer = new QTimer(this);
	fastRetuneTimer->setInterval(500);
	connect(fastRetuneTimer, SIGNAL(timeout()), this, SLOT(fastRetuneTimeout()));

	timeShiftCleaner = new DvbTimeShiftCleaner(this);

	QTimer *timer = new QTimer(this);
	timer->start(30000);
	connect(timer, SIGNAL(timeout()), this, SLOT(cleanTimeShiftFiles()));
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

void DvbTab::showEpgDialog()
{
	DvbEpgDialog dialog(dvbManager, getLiveChannel(), this);
	dialog.exec();
}

void DvbTab::instantRecord(bool checked)
{
	if (checked) {
		if (liveStream == NULL) {
			instantRecordAction->setChecked(false);
			return;
		}

		QString channelName = liveStream->getChannel()->name;

		// FIXME use epg for name
		dvbManager->getRecordingModel()->startInstantRecording(
			channelName + QTime::currentTime().toString("-hhmmss"), channelName);
	} else {
		dvbManager->getRecordingModel()->stopInstantRecording();
	}
}

void DvbTab::instantRecordingRemoved()
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
	mediaWidget->setFocus();
}

void DvbTab::playLive(const QModelIndex &index)
{
	playChannel(dvbManager->getChannelModel()->getChannel(channelView->mapToSource(index)));
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
	playChannel(dvbManager->getChannelModel()->getChannel(sourceIndex));
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
	playChannel(dvbManager->getChannelModel()->getChannel(sourceIndex));
}

void DvbTab::prepareTimeShift()
{
	QString fileName = dvbManager->getTimeShiftFolder() + "/TimeShift-" +
		QDateTime::currentDateTime().toString("yyyyMMddThhmmss") + ".m2t";

	if (!liveStream->startTimeShift(fileName)) {
		// FIXME error message
		mediaWidget->stopDvb();
		return;
	}

	// don't allow changes after starting time shift
	mediaWidget->updateDvbAudioChannels(QStringList(), 0);
	mediaWidget->updateDvbSubtitles(QStringList(), 0);
}

void DvbTab::startTimeShift()
{
	mediaWidget->play(liveStream->getTimeShiftFileName());
}

void DvbTab::changeAudioChannel(int index)
{
	int audioPid = liveStream->audioPids.at(index);
	liveStream->replacePid(liveStream->audioPid, audioPid);
	liveStream->audioPid = audioPid;
}

void DvbTab::changeSubtitle(int index)
{
	int subtitlePid = liveStream->subtitlePids.at(index);
	liveStream->replacePid(liveStream->subtitlePid, subtitlePid);
	liveStream->subtitlePid = subtitlePid;
}

void DvbTab::liveStopped()
{
	liveStream->getDevice()->removePidFilter(0x0012, liveStream->eitFilter);
	liveStream->removePidFilters();
	dvbManager->releaseDevice(liveStream->getDevice());
	delete liveStream->eitFilter;
	delete liveStream;
	liveStream = NULL;
}

void DvbTab::fastRetuneTimeout()
{
	fastRetuneTimer->stop();
}

void DvbTab::cleanTimeShiftFiles()
{
	if (timeShiftCleaner->isRunning()) {
		return;
	}

	QDir dir(dvbManager->getTimeShiftFolder());
	QStringList entries = dir.entryList(QStringList("TimeShift-*.m2t"), QDir::Files, QDir::Name);

	if (entries.count() < 2) {
		return;
	}

	entries.removeLast();

	timeShiftCleaner->remove(dir.path(), entries);
}

void DvbTab::playChannel(const QSharedDataPointer<DvbChannel> &channel)
{
	if (fastRetuneTimer->isActive()) {
		// FIXME hack
		return;
	} else {
		fastRetuneTimer->start();
	}

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

	mediaWidget->playDvb(channel->name);

	QList<int> pids;

	if (channel->videoPid != -1) {
		pids.append(channel->videoPid);
	}

	if (channel->audioPid != -1) {
		pids.append(channel->audioPid);
	}

	liveStream = new DvbLiveStream(device, channel, mediaWidget, pids);

	DvbPmtParser pmtParser(DvbPmtSection(DvbSection(channel->pmtSection)));

	QStringList audioChannels;

	for (QMap<int, QString>::const_iterator it = pmtParser.audioPids.constBegin();
	     it != pmtParser.audioPids.constEnd(); ++it) {
		if (!it.value().isEmpty()) {
			audioChannels.append(it.value());
		} else {
			audioChannels.append(QString::number(it.key()));
		}

		liveStream->audioPids.append(it.key());
	}

	liveStream->audioPid = channel->audioPid;

	if (!audioChannels.isEmpty()) {
		mediaWidget->updateDvbAudioChannels(audioChannels,
			liveStream->audioPids.indexOf(channel->audioPid));
	}

	QStringList subtitles;
	subtitles.append(i18n("off"));
	liveStream->subtitlePids.append(-1);

	for (QMap<int, QString>::const_iterator it = pmtParser.subtitlePids.constBegin();
	     it != pmtParser.subtitlePids.constEnd(); ++it) {
		if (!it.value().isEmpty()) {
			subtitles.append(it.value());
		} else {
			subtitles.append(QString::number(it.key()));
		}

		liveStream->subtitlePids.append(it.key());
	}

	if (subtitles.size() > 1) {
		mediaWidget->updateDvbSubtitles(subtitles, 0);
	}

	liveStream->eitFilter = new DvbEitFilter(channel->source, dvbManager->getEpgModel());
	device->addPidFilter(0x0012, liveStream->eitFilter);
}
