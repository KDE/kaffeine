/*
 * abstractmediawidget.cpp
 *
 * Copyright (C) 2010-2011 Christoph Pfister <christophpfister@gmail.com>
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
	resetState();
}

AbstractMediaWidget::~AbstractMediaWidget()
{
}

void AbstractMediaWidget::setMediaWidget(MediaWidget *mediaWidget_)
{
	mediaWidget = mediaWidget_;
}

void AbstractMediaWidget::resetState()
{
	if (pendingUpdates == 0) {
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
	}

	pendingUpdates = ResetState;
	playbackStatus = MediaWidget::Idle;
	totalTime = 0;
	currentTime = 0;
	seekable = false;
	metadata.clear();
	audioChannels.clear();
	currentAudioChannel = -1;
	subtitles.clear();
	currentSubtitle = -1;
	titleCount = 0;
	currentTitle = -1;
	chapterCount = 0;
	currentChapter = -1;
	angleCount = 0;
	currentAngle = -1;
	dvdMenu = false;
	videoSize = QSize();
}

void AbstractMediaWidget::addPendingUpdate(PendingUpdate pendingUpdate)
{
	if (pendingUpdates == 0) {
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
	}

	pendingUpdates |= pendingUpdate;
}

void AbstractMediaWidget::updatePlaybackStatus(MediaWidget::PlaybackStatus playbackStatus_)
{
	playbackStatus = playbackStatus_;
	addPendingUpdate(UpdatePlaybackStatus);
}

void AbstractMediaWidget::updateTotalTime(int totalTime_)
{
	totalTime = totalTime_;

	if (totalTime < 0) {
		totalTime = 0;
	}

	addPendingUpdate(UpdateTotalTime);
}

void AbstractMediaWidget::updateCurrentTime(int currentTime_)
{
	currentTime = currentTime_;

	if (currentTime < 0) {
		currentTime = 0;
	}

	addPendingUpdate(UpdateCurrentTime);
}

void AbstractMediaWidget::updateSeekable(bool seekable_)
{
	seekable = seekable_;
	addPendingUpdate(UpdateSeekable);
}

void AbstractMediaWidget::updateMetadata(const QMap<MediaWidget::MetadataType, QString> &metadata_)
{
	metadata = metadata_;
	addPendingUpdate(UpdateMetadata);
}

void AbstractMediaWidget::updateAudioChannels(const QStringList &audioChannels_)
{
	audioChannels = audioChannels_;
	addPendingUpdate(UpdateAudioChannels);
}

void AbstractMediaWidget::updateCurrentAudioChannel(int currentAudioChannel_)
{
	currentAudioChannel = currentAudioChannel_;
	addPendingUpdate(UpdateAudioChannels);
}

void AbstractMediaWidget::updateSubtitles(const QStringList &subtitles_)
{
	subtitles = subtitles_;
	addPendingUpdate(UpdateSubtitles);
}

void AbstractMediaWidget::updateCurrentSubtitle(int currentSubtitle_)
{
	currentSubtitle = currentSubtitle_;
	addPendingUpdate(UpdateSubtitles);
}

void AbstractMediaWidget::updateTitleCount(int titleCount_)
{
	titleCount = titleCount_;
	addPendingUpdate(UpdateTitles);
}

void AbstractMediaWidget::updateCurrentTitle(int currentTitle_)
{
	currentTitle = currentTitle_;
	addPendingUpdate(UpdateTitles);
}

void AbstractMediaWidget::updateChapterCount(int chapterCount_)
{
	chapterCount = chapterCount_;
	addPendingUpdate(UpdateChapters);
}

void AbstractMediaWidget::updateCurrentChapter(int currentChapter_)
{
	currentChapter = currentChapter_;
	addPendingUpdate(UpdateChapters);
}

void AbstractMediaWidget::updateAngleCount(int angleCount_)
{
	angleCount = angleCount_;
	addPendingUpdate(UpdateAngles);
}

void AbstractMediaWidget::updateCurrentAngle(int currentAngle_)
{
	currentAngle = currentAngle_;
	addPendingUpdate(UpdateAngles);
}

void AbstractMediaWidget::updateDvdMenu(bool dvdMenu_)
{
	dvdMenu = dvdMenu_;
	addPendingUpdate(UpdateDvdMenu);
}

void AbstractMediaWidget::updateVideoSize(const QSize &videoSize_)
{
	videoSize = videoSize_;
	addPendingUpdate(UpdateVideoSize);
}

void AbstractMediaWidget::customEvent(QEvent *event)
{
	Q_UNUSED(event)

	while (pendingUpdates != 0) {
		uint lowestPendingUpdate = (pendingUpdates & (~(pendingUpdates - 1)));
		pendingUpdates &= ~lowestPendingUpdate;

		switch (static_cast<PendingUpdates>(lowestPendingUpdate)) {
		case PlaybackFinished:
			mediaWidget->playbackFinished();
			break;
		case UpdatePlaybackStatus:
			mediaWidget->updatePlaybackStatus();
			break;
		case UpdateTotalTime:
			mediaWidget->updateTotalTime();
			break;
		case UpdateCurrentTime:
			mediaWidget->updateCurrentTime();
			break;
		case UpdateSeekable:
			mediaWidget->updateSeekable();
			break;
		case UpdateMetadata:
			mediaWidget->updateMetadata();
			break;
		case UpdateAudioChannels:
			mediaWidget->updateAudioChannels();
			break;
		case UpdateSubtitles:
			mediaWidget->updateSubtitles();
			break;
		case UpdateTitles:
			mediaWidget->updateTitles();
			break;
		case UpdateChapters:
			mediaWidget->updateChapters();
			break;
		case UpdateAngles:
			mediaWidget->updateAngles();
			break;
		case UpdateDvdMenu:
			mediaWidget->updateDvdMenu();
			break;
		case UpdateVideoSize:
			mediaWidget->updateVideoSize();
			break;
		case ResetState:
			// this is a combination of flags
			break;
		}
	}
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
	resetState();
}

void DummyMediaWidget::stop()
{
	resetState();
}

void DummyMediaWidget::setPaused(bool paused)
{
	Q_UNUSED(paused)
}

void DummyMediaWidget::seek(int time)
{
	Q_UNUSED(time)
}

void DummyMediaWidget::setCurrentAudioChannel(int currentAudioChannel)
{
	Q_UNUSED(currentAudioChannel)
}

void DummyMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	Q_UNUSED(currentSubtitle)
}

void DummyMediaWidget::setExternalSubtitle(const KUrl &subtitleUrl)
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

void DummyMediaWidget::toggleMenu()
{
}
