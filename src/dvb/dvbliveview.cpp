/*
 * dvbliveview.cpp
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

#include "dvbliveview.h"
#include "dvbliveview_p.h"

#include <QDir>
#include <QPainter>
#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include "../mediawidget.h"
#include "dvbchannel.h"
#include "dvbdevice.h"
#include "dvbmanager.h"

void DvbOsd::init(OsdLevel level_, const QString &channelName_,
	const QList<DvbEpgEntry> &epgEntries_)
{
	level = level_;
	channelName = channelName_;
	epgEntries = epgEntries_;
}

QPixmap DvbOsd::paintOsd(QRect &rect, const QFont &font, Qt::LayoutDirection)
{
	QFont osdFont = font;
	osdFont.setPointSize(20);

	QString timeString = KGlobal::locale()->formatTime(QTime::currentTime());
	QString entryString;
	int elapsedTime = 0;
	int totalTime = 0;

	const DvbEpgEntry *firstEntry = NULL;
	const DvbEpgEntry *secondEntry = NULL;

	if (!epgEntries.isEmpty()) {
		firstEntry = &epgEntries.at(0);
	}

	if (epgEntries.size() >= 2) {
		secondEntry = &epgEntries.at(1);
	}

	if (firstEntry != NULL) {
		entryString = KGlobal::locale()->formatTime(firstEntry->begin.toLocalTime().time())
			+ ' ' + firstEntry->title;
		elapsedTime = firstEntry->begin.secsTo(QDateTime::currentDateTime());
		totalTime = QTime().secsTo(firstEntry->duration);
	}

	if ((level == ShortOsd) && (secondEntry != NULL)) {
		entryString = entryString + '\n' +
			KGlobal::locale()->formatTime(secondEntry->begin.toLocalTime().time()) +
			' ' + secondEntry->title;
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

			if ((firstEntry != NULL) && !firstEntry->subheading.isEmpty()) {
				painter.drawText(entryRect.x(), boundingRect.bottom() + 1,
					entryRect.width(), lineHeight, Qt::AlignLeft,
					firstEntry->subheading, &boundingRect);
			}

			if ((firstEntry != NULL) && !firstEntry->details.isEmpty()) {
				painter.drawText(entryRect.x(), boundingRect.bottom() + 1,
					entryRect.width(),
					rect.height() - boundingRect.bottom() - 1,
					Qt::AlignLeft | Qt::TextWordWrap, firstEntry->details,
					&boundingRect);
			}

			if (boundingRect.bottom() < rect.bottom()) {
				rect.setBottom(boundingRect.bottom());
			}
		}
	}

	return pixmap;
}

void DvbLiveViewInternal::processData(const char data[188])
{
	buffer.append(data, 188);

	if (buffer.size() < (87 * 188)) {
		return;
	}

	if (!timeShiftFile.isOpen()) {
		mediaWidget->writeDvbData(buffer);
	} else {
		timeShiftFile.write(buffer); // FIXME avoid buffer reallocation
	}

	buffer.clear();
	buffer.reserve(87 * 188);
}

DvbLiveView::DvbLiveView(DvbManager *manager_, QObject *parent) : QObject(parent),
	manager(manager_), device(NULL), videoPid(-1), audioPid(-1), subtitlePid(-1)
{
	mediaWidget = manager->getMediaWidget();
	osdWidget = mediaWidget->getOsdWidget();

	internal = new DvbLiveViewInternal();
	internal->mediaWidget = mediaWidget;

	connect(&internal->pmtFilter, SIGNAL(pmtSectionChanged(DvbPmtSection)),
		this, SLOT(pmtSectionChanged(DvbPmtSection)));
	connect(&patPmtTimer, SIGNAL(timeout()), this, SLOT(insertPatPmt()));
	connect(&osdTimer, SIGNAL(timeout()), this, SLOT(osdTimeout()));

	connect(mediaWidget, SIGNAL(changeDvbAudioChannel(int)),
		this, SLOT(changeAudioStream(int)));
	connect(mediaWidget, SIGNAL(changeDvbSubtitle(int)), this, SLOT(changeSubtitle(int)));
	connect(mediaWidget, SIGNAL(prepareDvbTimeShift()), this, SLOT(prepareTimeShift()));
	connect(mediaWidget, SIGNAL(startDvbTimeShift()), this, SLOT(startTimeShift()));
	connect(mediaWidget, SIGNAL(dvbStopped()), this, SLOT(liveStopped()));
}

DvbLiveView::~DvbLiveView()
{
	delete internal;
}

const DvbChannel *DvbLiveView::getChannel() const
{
	return channel.constData();
}

DvbDevice *DvbLiveView::getDevice() const
{
	return device;
}

void DvbLiveView::playChannel(const DvbChannel *channel_)
{
	DvbDevice *newDevice = NULL;

	if ((channel.constData() != NULL) && (channel->source == channel_->source) &&
	    (channel->transponder.corresponds(channel_->transponder))) {
		newDevice = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Shared);
	}

	liveStopped();
	channel = channel_;
	device = newDevice;

	if (device == NULL) {
		device = manager->requestDevice(channel->source, channel->transponder,
			DvbManager::Shared);
	}

	if (device == NULL) {
		channel = NULL;
		mediaWidget->stop();
		KMessageBox::sorry(manager->getParentWidget(),
			i18nc("message box", "No available device found."));
		// FIXME better error message if devices are blocked by recordings
		return;
	}

	mediaWidget->playDvb(channel->name);

	internal->pmtFilter.setProgramNumber(channel->getServiceId());
	startDevice();

	internal->patGenerator.initPat(channel->transportStreamId, channel->getServiceId(),
		channel->pmtPid);
	videoPid = -1;
	audioPid = channel->audioPid;
	subtitlePid = -1;
	internal->pmtSectionData = channel->pmtSectionData;
	pmtSectionChanged();
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
			QString("%1 - %2").arg(channel->number).arg(channel->name),
			manager->getEpgModel()->getCurrentNext(channel->name));
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

void DvbLiveView::pmtSectionChanged(const DvbPmtSection &pmtSection)
{
	internal->pmtSectionData = pmtSection.toByteArray();
	pmtSectionChanged();
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

void DvbLiveView::changeAudioStream(int index)
{
	audioPid = audioPids.at(index);
	updatePids();
}

void DvbLiveView::changeSubtitle(int index)
{
	if (index < 0) {
		subtitlePid = -1;
	} else {
		subtitlePid = subtitlePids.at(index);
	}

	updatePids();
}

void DvbLiveView::prepareTimeShift()
{
	internal->timeShiftFile.setFileName(manager->getTimeShiftFolder() + "/TimeShift-" +
		QDateTime::currentDateTime().toString("yyyyMMddThhmmss") + ".m2t");

	if (internal->timeShiftFile.exists() ||
	    !internal->timeShiftFile.open(QIODevice::WriteOnly)) {
		kWarning() << "cannot open file" << internal->timeShiftFile.fileName();
		internal->timeShiftFile.setFileName(QDir::homePath() + "/TimeShift-" +
			QDateTime::currentDateTime().toString("yyyyMMddThhmmss") + ".m2t");

		if (internal->timeShiftFile.exists() ||
		    !internal->timeShiftFile.open(QIODevice::WriteOnly)) {
			kWarning() << "cannot open file" << internal->timeShiftFile.fileName();
			mediaWidget->stop();
			return;
		}
	}

	updatePids();

	// don't allow changes after starting time shift
	mediaWidget->updateDvbAudioChannels(QStringList(), 0);
	mediaWidget->updateDvbSubtitles(QStringList(), 0);
}

void DvbLiveView::startTimeShift()
{
	mediaWidget->play(internal->timeShiftFile.fileName());
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

void DvbLiveView::liveStopped()
{
	if (device != NULL) {
		stopDevice();
		manager->releaseDevice(device, DvbManager::Shared);
		device = NULL;
	}

	channel = NULL;
	pids.clear();
	patPmtTimer.stop();
	osdTimer.stop();

	internal->pmtSectionData.clear();
	internal->patGenerator = DvbSectionGenerator();
	internal->pmtGenerator = DvbSectionGenerator();
	internal->buffer.clear();
	internal->timeShiftFile.close();
	internal->dvbOsd.init(DvbOsd::Off, QString(), QList<DvbEpgEntry>());
	osdWidget->hideObject();
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

	manager->getEpgModel()->startEventFilter(device, channel.constData());
}

void DvbLiveView::stopDevice()
{
	manager->getEpgModel()->stopEventFilter(device, channel.constData());

	if (channel->isScrambled && !internal->pmtSectionData.isEmpty()) {
		device->stopDescrambling(internal->pmtSectionData, this);
	}

	foreach (int pid, pids) {
		device->removePidFilter(pid, internal);
	}

	device->removeSectionFilter(channel->pmtPid, &internal->pmtFilter);
	disconnect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
}

void DvbLiveView::pmtSectionChanged()
{
	DvbPmtParser pmtParser(DvbPmtSection(internal->pmtSectionData));
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

	QStringList audioChannels;
	audioPids.clear();

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.audioPids.at(i);

		if (!it.second.isEmpty()) {
			audioChannels.append(it.second);
		} else {
			audioChannels.append(QString::number(it.first));
		}

		audioPids.append(it.first);
	}

	mediaWidget->updateDvbAudioChannels(audioChannels, audioPids.indexOf(audioPid));

	QStringList subtitles;
	subtitlePids.clear();

	for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.subtitlePids.at(i);

		if (!it.second.isEmpty()) {
			subtitles.append(it.second);
		} else {
			subtitles.append(QString::number(it.first));
		}

		subtitlePids.append(it.first);
	}

	mediaWidget->updateDvbSubtitles(subtitles, subtitlePids.indexOf(subtitlePid));
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
