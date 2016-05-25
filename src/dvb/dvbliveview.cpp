/*
 * dvbliveview.cpp
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

#include <errno.h>
#include <fcntl.h>
#include <KLocalizedString>
#include <KMessageBox>
#include <QDebug>
#include <QDir>
#include <QLocale>
#include <QPainter>
#include <QSet>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <sys/stat.h>  // bsd compatibility
#include <sys/types.h>  // bsd compatibility
#include <unistd.h>

#include "dvbdevice.h"
#include "dvbliveview.h"
#include "dvbliveview_p.h"
#include "dvbmanager.h"

void DvbOsd::init(OsdLevel level_, const QString &channelName_,
	const QList<DvbSharedEpgEntry> &epgEntries)
{
	level = level_;
	channelName = channelName_;

	if (epgEntries.size() >= 1) {
		firstEntry = *epgEntries.at(0);
	}

	if (epgEntries.size() >= 2) {
		secondEntry = *epgEntries.at(1);
	}
}

QPixmap DvbOsd::paintOsd(QRect &rect, const QFont &font, Qt::LayoutDirection)
{
	QFont osdFont = font;
	osdFont.setPointSize(20);

	QString timeString = QLocale().toString(QTime::currentTime());
	QString entryString;
	int elapsedTime = 0;
	int totalTime = 0;

	if (firstEntry.channel.isValid()) {
		entryString = QLocale().toString(firstEntry.begin.toLocalTime().time())
			+ QLatin1Char(' ') + firstEntry.title;
		elapsedTime = firstEntry.begin.secsTo(QDateTime::currentDateTime());
		totalTime = QTime(0, 0, 0).secsTo(firstEntry.duration);
	}

	if ((level == ShortOsd) && secondEntry.channel.isValid()) {
		entryString = entryString + QLatin1Char('\n') +
			QLocale().toString(secondEntry.begin.toLocalTime().time()) +
			QLatin1Char(' ') + secondEntry.title;
	}

	int lineHeight = QFontMetrics(osdFont).height();
	QRect headerRect(5, 0, rect.width() - 10, lineHeight);
	QRect entryRect;

	if (level == ShortOsd) {
		entryRect = QRect(5, lineHeight + 9, rect.width() - 10, 2 * lineHeight);
		rect.setHeight(entryRect.bottom() + 1);
	} else {
		entryRect = QRect(5, lineHeight + 9, rect.width() - 10, lineHeight);
	}

	QPixmap pixmap(rect.size());

	{
		QPainter painter(&pixmap);
		painter.fillRect(rect, Qt::black);
		painter.setFont(osdFont);
		painter.setPen(Qt::white);
		painter.drawText(headerRect, Qt::AlignLeft, channelName);
		painter.drawText(headerRect, Qt::AlignRight, timeString);

		painter.fillRect(5, lineHeight + 2, rect.width() - 10, 5, Qt::gray);
		painter.fillRect(6, lineHeight + 3, rect.width() - 12, 3, Qt::black);

		if ((elapsedTime > 0) && (elapsedTime <= totalTime)) {
			int width = (((rect.width() - 12) * elapsedTime + (totalTime / 2)) /
				     totalTime);
			painter.fillRect(6, lineHeight + 3, width, 3, Qt::green);
		}

		painter.drawText(entryRect, Qt::AlignLeft, entryString);

		if (level == LongOsd) {
			QRect boundingRect = entryRect;

			if (!firstEntry.subheading.isEmpty()) {
				painter.drawText(entryRect.x(), boundingRect.bottom() + 1,
					entryRect.width(), lineHeight, Qt::AlignLeft,
					firstEntry.subheading, &boundingRect);
			}

			if (!firstEntry.details.isEmpty()) {
				painter.drawText(entryRect.x(), boundingRect.bottom() + 1,
					entryRect.width(),
					rect.height() - boundingRect.bottom() - 1,
					Qt::AlignLeft | Qt::TextWordWrap, firstEntry.details,
					&boundingRect);
			}

			if (boundingRect.bottom() < rect.bottom()) {
				rect.setBottom(boundingRect.bottom());
			}
		}
	}

	return pixmap;
}

DvbLiveView::DvbLiveView(DvbManager *manager_, QObject *parent) : QObject(parent),
	manager(manager_), device(NULL), videoPid(-1), audioPid(-1), subtitlePid(-1)
{
	mediaWidget = manager->getMediaWidget();
	osdWidget = mediaWidget->getOsdWidget();

	internal = new DvbLiveViewInternal(this);
	internal->mediaWidget = mediaWidget;

	connect(&internal->pmtFilter, SIGNAL(pmtSectionChanged(QByteArray)),
		this, SLOT(pmtSectionChanged(QByteArray)));
	connect(&patPmtTimer, SIGNAL(timeout()), this, SLOT(insertPatPmt()));
	connect(&osdTimer, SIGNAL(timeout()), this, SLOT(osdTimeout()));

	connect(internal, SIGNAL(currentAudioStreamChanged(int)),
		this, SLOT(currentAudioStreamChanged(int)));
	connect(internal, SIGNAL(currentSubtitleChanged(int)),
		this, SLOT(currentSubtitleChanged(int)));
	connect(internal, SIGNAL(replay()), this, SLOT(replay()));
	connect(internal, SIGNAL(playbackFinished()), this, SLOT(playbackFinished()));
	connect(internal, SIGNAL(playbackStatusChanged(MediaWidget::PlaybackStatus)),
		this, SLOT(playbackStatusChanged(MediaWidget::PlaybackStatus)));
	connect(internal, SIGNAL(previous()), this, SIGNAL(previous()));
	connect(internal, SIGNAL(next()), this, SIGNAL(next()));
}

DvbLiveView::~DvbLiveView()
{
}

void DvbLiveView::replay()
{
	// FIXME
}

void DvbLiveView::playbackFinished()
{
	mediaWidget->play(internal);
}

const DvbSharedChannel &DvbLiveView::getChannel() const
{
	return channel;
}

DvbDevice *DvbLiveView::getDevice() const
{
	return device;
}

void DvbLiveView::playChannel(const DvbSharedChannel &channel_)
{
	DvbDevice *newDevice = NULL;

	if ((channel.constData() != NULL) && (channel->source == channel_->source) &&
	    (channel->transponder.corresponds(channel_->transponder))) {
		newDevice = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Shared);
	}

	playbackStatusChanged(MediaWidget::Idle);
	channel = channel_;
	device = newDevice;

	if (device == NULL) {
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Shared);
	}

	if (device == NULL) {
		channel = DvbSharedChannel();
		mediaWidget->stop();

		if (manager->getRecordingModel()->hasActiveRecordings()) {
			KMessageBox::sorry(manager->getParentWidget(),
				i18nc("@info", "All devices are used for recordings."));
		} else {
			KMessageBox::information(manager->getParentWidget(),
				i18nc("@info", "No device found."));
		}

		return;
	}

	internal->channelName = channel->name;
	internal->resetPipe();
	mediaWidget->play(internal);

	internal->pmtFilter.setProgramNumber(channel->serviceId);
	startDevice();

	internal->patGenerator.initPat(channel->transportStreamId, channel->serviceId,
		channel->pmtPid);
	videoPid = -1;
	audioPid = channel->audioPid;
	subtitlePid = -1;
	pmtSectionChanged(channel->pmtSectionData);
	patPmtTimer.start(500);

	internal->buffer.reserve(87 * 188);
	QTimer::singleShot(2000, this, SLOT(showOsd()));
}

void DvbLiveView::toggleOsd()
{
	if (channel.constData() == NULL) {
		return;
	}

	switch (internal->dvbOsd.level) {
	case DvbOsd::Off:
		internal->dvbOsd.init(DvbOsd::ShortOsd,
			QString(QLatin1String("%1 - %2")).arg(channel->number).arg(channel->name),
			manager->getEpgModel()->getCurrentNext(channel));
		osdWidget->showObject(&internal->dvbOsd, 2500);
		osdTimer.start(2500);
		break;
	case DvbOsd::ShortOsd:
		internal->dvbOsd.level = DvbOsd::LongOsd;
		osdWidget->showObject(&internal->dvbOsd, -1);
		osdTimer.stop();
		break;
	case DvbOsd::LongOsd:
		internal->dvbOsd.level = DvbOsd::Off;
		osdWidget->hideObject();
		osdTimer.stop();
		break;
	}
}

void DvbLiveView::pmtSectionChanged(const QByteArray &pmtSectionData)
{
	internal->pmtSectionData = pmtSectionData;
	DvbPmtSection pmtSection(internal->pmtSectionData);
	DvbPmtParser pmtParser(pmtSection);
	videoPid = pmtParser.videoPid;

	for (int i = 0;; ++i) {
		if (i == pmtParser.audioPids.size()) {
			if (i > 0) {
				audioPid = pmtParser.audioPids.at(0).first;
			} else {
				audioPid = -1;
			}

			break;
		}

		if (pmtParser.audioPids.at(i).first == audioPid) {
			break;
		}
	}

	for (int i = 0;; ++i) {
		if (i == pmtParser.subtitlePids.size()) {
			subtitlePid = -1;
			break;
		}

		if (pmtParser.subtitlePids.at(i).first == subtitlePid) {
			break;
		}
	}

	updatePids(true);

	if (channel->isScrambled) {
		device->startDescrambling(internal->pmtSectionData, this);
	}

	if (internal->timeShiftFile.isOpen()) {
		return;
	}

	internal->audioStreams.clear();
	audioPids.clear();

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.audioPids.at(i);

		if (!it.second.isEmpty()) {
			internal->audioStreams.append(it.second);
		} else {
			internal->audioStreams.append(QString::number(it.first));
		}

		audioPids.append(it.first);
	}

	internal->currentAudioStream = audioPids.indexOf(audioPid);
	mediaWidget->audioStreamsChanged();

	internal->subtitles.clear();
	subtitlePids.clear();

	for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.subtitlePids.at(i);

		if (!it.second.isEmpty()) {
			internal->subtitles.append(it.second);
		} else {
			internal->subtitles.append(QString::number(it.first));
		}

		subtitlePids.append(it.first);
	}

	internal->currentSubtitle = subtitlePids.indexOf(subtitlePid);
	mediaWidget->subtitlesChanged();
}

void DvbLiveView::insertPatPmt()
{
	internal->buffer.append(internal->patGenerator.generatePackets());
	internal->buffer.append(internal->pmtGenerator.generatePackets());
}

void DvbLiveView::deviceStateChanged()
{
	switch (device->getDeviceState()) {
	case DvbDevice::DeviceReleased:
		stopDevice();
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Shared);

		if (device != NULL) {
			startDevice();
		} else {
			mediaWidget->stop();
			osdWidget->showText(i18nc("message box", "No available device found."),
				2500);
		}

		break;
	case DvbDevice::DeviceIdle:
	case DvbDevice::DeviceRotorMoving:
	case DvbDevice::DeviceTuning:
	case DvbDevice::DeviceTuned:
		break;
	}
}

void DvbLiveView::currentAudioStreamChanged(int currentAudioStream)
{
	audioPid = -1;

	if ((currentAudioStream >= 0) && (currentAudioStream < audioPids.size())) {
		audioPid = audioPids.at(currentAudioStream);
	}

	updatePids();
}

void DvbLiveView::currentSubtitleChanged(int currentSubtitle)
{
	subtitlePid = -1;

	if ((currentSubtitle >= 0) && (currentSubtitle < subtitlePids.size())) {
		subtitlePid = subtitlePids.at(currentSubtitle);
	}

	updatePids();
}

void DvbLiveView::playbackStatusChanged(MediaWidget::PlaybackStatus playbackStatus)
{
	switch (playbackStatus) {
	case MediaWidget::Idle:
		if (device != NULL) {
			stopDevice();
			manager->releaseDevice(device, DvbManager::Shared);
			device = NULL;
		}

		channel = DvbSharedChannel();
		pids.clear();
		patPmtTimer.stop();
		osdTimer.stop();

		internal->pmtSectionData.clear();
		internal->patGenerator = DvbSectionGenerator();
		internal->pmtGenerator = DvbSectionGenerator();
		internal->buffer.clear();
		internal->timeShiftFile.close();
		internal->dvbOsd.init(DvbOsd::Off, QString(), QList<DvbSharedEpgEntry>());
		osdWidget->hideObject();
		break;
	case MediaWidget::Playing:
		if (internal->timeShiftFile.isOpen()) {
			// FIXME
			mediaWidget->play(internal);
		}

		break;
	case MediaWidget::Paused:
		if (internal->timeShiftFile.isOpen()) {
			break;
		}

		internal->timeShiftFile.setFileName(manager->getTimeShiftFolder() + QLatin1String("/TimeShift-") +
			QDateTime::currentDateTime().toString(QLatin1String("yyyyMMddThhmmss")) +
			QLatin1String(".m2t"));

		if (internal->timeShiftFile.exists() ||
		    !internal->timeShiftFile.open(QIODevice::WriteOnly)) {
			qInfo() << "DvbLiveView::playbackStatusChanged: cannot open file" <<
				internal->timeShiftFile.fileName();
			internal->timeShiftFile.setFileName(QDir::homePath() + QLatin1String("/TimeShift-") +
				QDateTime::currentDateTime().toString(QLatin1String("yyyyMMddThhmmss")) +
				QLatin1String(".m2t"));

			if (internal->timeShiftFile.exists() ||
			    !internal->timeShiftFile.open(QIODevice::WriteOnly)) {
				qInfo() << "DvbLiveView::playbackStatusChanged: cannot open file" <<
					internal->timeShiftFile.fileName();
				mediaWidget->stop();
				break;
			}
		}

		updatePids();

		// don't allow changes after starting time shift
		internal->audioStreams.clear();
		internal->currentAudioStream = -1;
		mediaWidget->audioStreamsChanged();
		internal->subtitles.clear();
		internal->currentSubtitle = -1;
		mediaWidget->subtitlesChanged();
		break;
	}
}

void DvbLiveView::showOsd()
{
	if (internal->dvbOsd.level == DvbOsd::Off) {
		toggleOsd();
	}
}

void DvbLiveView::osdTimeout()
{
	internal->dvbOsd.level = DvbOsd::Off;
	osdTimer.stop();
}

void DvbLiveView::startDevice()
{
	foreach (int pid, pids) {
		device->addPidFilter(pid, internal);
	}

	device->addSectionFilter(channel->pmtPid, &internal->pmtFilter);
	connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));

	if (channel->isScrambled && !internal->pmtSectionData.isEmpty()) {
		device->startDescrambling(internal->pmtSectionData, this);
	}

	manager->getEpgModel()->startEventFilter(device, channel);
}

void DvbLiveView::stopDevice()
{
	manager->getEpgModel()->stopEventFilter(device, channel);

	if (channel->isScrambled && !internal->pmtSectionData.isEmpty()) {
		device->stopDescrambling(internal->pmtSectionData, this);
	}

	foreach (int pid, pids) {
		device->removePidFilter(pid, internal);
	}

	device->removeSectionFilter(channel->pmtPid, &internal->pmtFilter);
	disconnect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
}

void DvbLiveView::updatePids(bool forcePatPmtUpdate)
{
	DvbPmtSection pmtSection(internal->pmtSectionData);
	DvbPmtParser pmtParser(pmtSection);
	QSet<int> newPids;
	bool updatePatPmt = forcePatPmtUpdate;
	bool isTimeShifting = internal->timeShiftFile.isOpen();

	if (videoPid != -1) {
		newPids.insert(videoPid);
	}

	if (!isTimeShifting) {
		if (audioPid != -1) {
			newPids.insert(audioPid);
		}
	} else {
		for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
			newPids.insert(pmtParser.audioPids.at(i).first);
		}
	}

	if (!isTimeShifting) {
		if (subtitlePid != -1) {
			newPids.insert(subtitlePid);
		}
	} else {
		for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
			newPids.insert(pmtParser.subtitlePids.at(i).first);
		}

		if (pmtParser.teletextPid != -1) {
			newPids.insert(pmtParser.teletextPid);
		}
	}

	for (int i = 0; i < pids.size(); ++i) {
		int pid = pids.at(i);

		if (!newPids.remove(pid)) {
			device->removePidFilter(pid, internal);
			pids.removeAt(i);
			updatePatPmt = true;
			--i;
		}
	}

	foreach (int pid, newPids) {
		device->addPidFilter(pid, internal);
		pids.append(pid);
		updatePatPmt = true;
	}

	if (updatePatPmt) {
		internal->pmtGenerator.initPmt(channel->pmtPid, pmtSection, pids);
		insertPatPmt();
	}
}

DvbLiveViewInternal::DvbLiveViewInternal(QObject *parent) : QObject(parent), mediaWidget(NULL),
	readFd(-1), writeFd(-1)
{
	QString fileName = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("/dvbpipe.m2t");
	QFile::remove(fileName);
	url = QUrl::fromLocalFile(fileName);

	if (mkfifo(QFile::encodeName(fileName).constData(), 0600) != 0) {
		qInfo() << "DvbLiveViewInternal::DvbLiveViewInternal: mkfifo failed";
		return;
	}

	readFd = open(QFile::encodeName(fileName).constData(), O_RDONLY | O_NONBLOCK);

	if (readFd < 0) {
		qInfo() << "DvbLiveViewInternal::DvbLiveViewInternal: open failed";
		return;
	}

	writeFd = open(QFile::encodeName(fileName).constData(), O_WRONLY | O_NONBLOCK);

	if (writeFd < 0) {
		qInfo() << "DvbLiveViewInternal::DvbLiveViewInternal: open failed";
		return;
	}

	notifier = new QSocketNotifier(writeFd, QSocketNotifier::Write, this);
	notifier->setEnabled(false);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(writeToPipe()));
}

DvbLiveViewInternal::~DvbLiveViewInternal()
{
	if (writeFd >= 0) {
		close(writeFd);
	}

	if (readFd >= 0) {
		close(readFd);
	}
}

void DvbLiveViewInternal::resetPipe()
{
	if (!buffers.isEmpty()) {
		buffer = buffers.at(0);
		buffers.clear();
	}

	if (readFd >= 0) {
		if (buffer.isEmpty()) {
			buffer.resize(87 * 188);
		}

		while (read(readFd, buffer.data(), buffer.size()) > 0) {
		}
	}

	buffer.clear();
}

void DvbLiveViewInternal::writeToPipe()
{
	while (!buffers.isEmpty()) {
		const QByteArray &currentBuffer = buffers.at(0);
		int bytesWritten = int(write(writeFd, currentBuffer.constData(), currentBuffer.size()));

		if ((bytesWritten < 0) && (errno == EINTR)) {
			continue;
		}

		if (bytesWritten == currentBuffer.size()) {
			buffers.removeFirst();
			continue;
		}

		if (bytesWritten > 0) {
			buffers.first().remove(0, bytesWritten);
		}

		break;
	}

	if (!buffers.isEmpty()) {
		notifier->setEnabled(true);
	}
}

void DvbLiveViewInternal::processData(const char data[188])
{
	buffer.append(data, 188);

	if (buffer.size() < (87 * 188)) {
		return;
	}

	if (!timeShiftFile.isOpen()) {
		if (writeFd >= 0) {
			buffers.append(buffer);
			writeToPipe();
		}
	} else {
		timeShiftFile.write(buffer); // FIXME avoid buffer reallocation
	}

	buffer.clear();
	buffer.reserve(87 * 188);
}
