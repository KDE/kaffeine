/*
 * abstractmediawidget.cpp
 *
 * Copyright (C) 2010-2012 Christoph Pfister <christophpfister@gmail.com>
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

#include "abstractmediawidget.h"

#include <QCoreApplication>

AbstractMediaWidget::AbstractMediaWidget(QWidget *parent) : QWidget(parent), mediaWidget(NULL)
{
	playbackStatus = MediaWidget::Idle;
	currentTime = 0;
	totalTime = 0;
	seekable = false;
	currentAudioStream = -1;
	currentSubtitle = -1;
	titleCount = 0;
	currentTitle = -1;
	chapterCount = 0;
	currentChapter = -1;
	angleCount = 0;
	currentAngle = -1;
	dvdMenu = false;
}

AbstractMediaWidget::~AbstractMediaWidget()
{
}

void AbstractMediaWidget::connectToMediaWidget(MediaWidget *mediaWidget_)
{
	mediaWidget = mediaWidget_;
	addPendingUpdates(PlaybackStatus | CurrentTotalTime | Seekable | Metadata | AudioStreams |
		Subtitles | Titles | Chapters | Angles | DvdMenu | VideoSize);
}

void AbstractMediaWidget::addPendingUpdates(PendingUpdates pendingUpdatesToBeAdded)
{
	while (true) {
		int oldValue = pendingUpdates;
		int newValue = (oldValue | pendingUpdatesToBeAdded);

		if (!pendingUpdates.testAndSetRelaxed(oldValue, newValue)) {
			continue;
		}

		if (oldValue == 0) {
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		}

		break;
	}
}

void AbstractMediaWidget::customEvent(QEvent *event)
{
	Q_UNUSED(event)

	while (true) {
		int oldValue = pendingUpdates;
		int lowestPendingUpdate = (oldValue & (~(oldValue - 1)));
		int newValue = (oldValue & (~lowestPendingUpdate));

		if (!pendingUpdates.testAndSetRelaxed(oldValue, newValue)) {
			continue;
		}

		switch (static_cast<PendingUpdate>(lowestPendingUpdate)) {
		case PlaybackFinished:
			mediaWidget->playbackFinished();
			break;
		case PlaybackStatus:
			updatePlaybackStatus();
			mediaWidget->playbackStatusChanged();
			break;
		case CurrentTotalTime:
			updateCurrentTotalTime();
			mediaWidget->currentTotalTimeChanged();
			break;
		case Seekable:
			updateSeekable();
			mediaWidget->seekableChanged();
			break;
		case Metadata:
			updateMetadata();
			mediaWidget->metadataChanged();
			break;
		case AudioStreams:
			updateAudioStreams();
			mediaWidget->audioStreamsChanged();
			break;
		case Subtitles:
			updateSubtitles();
			mediaWidget->subtitlesChanged();
			break;
		case Titles:
			updateTitles();
			mediaWidget->titlesChanged();
			break;
		case Chapters:
			updateChapters();
			mediaWidget->chaptersChanged();
			break;
		case Angles:
			updateAngles();
			mediaWidget->anglesChanged();
			break;
		case DvdMenu:
			updateDvdMenu();
			mediaWidget->dvdMenuChanged();
			break;
		case VideoSize:
			updateVideoSize();
			mediaWidget->videoSizeChanged();
			break;
		}

		if (newValue == 0) {
			break;
		}
	}
}

DummyMediaWidget::DummyMediaWidget(QWidget *parent) : AbstractMediaWidget(parent)
{
}

DummyMediaWidget::~DummyMediaWidget()
{
}

void DummyMediaWidget::setMuted(bool muted)
{
	Q_UNUSED(muted)
}

void DummyMediaWidget::setVolume(int volume)
{
	Q_UNUSED(volume)
}

void DummyMediaWidget::setAspectRatio(MediaWidget::AspectRatio aspectRatio)
{
	Q_UNUSED(aspectRatio)
}

void DummyMediaWidget::setDeinterlacing(bool deinterlacing)
{
	Q_UNUSED(deinterlacing)
}

void DummyMediaWidget::play(const MediaSource &source)
{
	Q_UNUSED(source)
}

void DummyMediaWidget::stop()
{
}

void DummyMediaWidget::setPaused(bool paused)
{
	Q_UNUSED(paused)
}

void DummyMediaWidget::seek(int time)
{
	Q_UNUSED(time)
}

void DummyMediaWidget::setCurrentAudioStream(int currentAudioStream)
{
	Q_UNUSED(currentAudioStream)
}

void DummyMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	Q_UNUSED(currentSubtitle)
}

void DummyMediaWidget::setExternalSubtitle(const QUrl &subtitleUrl)
{
	Q_UNUSED(subtitleUrl)
}

void DummyMediaWidget::setCurrentTitle(int currentTitle)
{
	Q_UNUSED(currentTitle)
}

void DummyMediaWidget::setCurrentChapter(int currentChapter)
{
	Q_UNUSED(currentChapter)
}

void DummyMediaWidget::setCurrentAngle(int currentAngle)
{
	Q_UNUSED(currentAngle)
}

bool DummyMediaWidget::jumpToPreviousChapter()
{
	return false;
}

bool DummyMediaWidget::jumpToNextChapter()
{
	return false;
}

void DummyMediaWidget::showDvdMenu()
{
}

void DummyMediaWidget::updatePlaybackStatus()
{
}

void DummyMediaWidget::updateCurrentTotalTime()
{
}

void DummyMediaWidget::updateSeekable()
{
}

void DummyMediaWidget::updateMetadata()
{
}

void DummyMediaWidget::updateAudioStreams()
{
}

void DummyMediaWidget::updateSubtitles()
{
}

void DummyMediaWidget::updateTitles()
{
}

void DummyMediaWidget::updateChapters()
{
}

void DummyMediaWidget::updateAngles()
{
}

void DummyMediaWidget::updateDvdMenu()
{
}

void DummyMediaWidget::updateVideoSize()
{
}
