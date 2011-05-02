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

void AbstractMediaWidget::setMediaWidget(MediaWidget *mediaWidget_)
{
	mediaWidget = mediaWidget_;
}

void AbstractMediaWidget::invalidateState()
{
	DirtyFlags newDirtyFlags = (UpdatePlaybackStatus | UpdateTotalTime | UpdateCurrentTime |
		UpdateSeekable | UpdateMetadata | UpdateAudioChannels | UpdateSubtitles |
		UpdateTitles | UpdateChapters | UpdateAngles | UpdateDvdPlayback |
		UpdateVideoSize);

	if (dirtyFlags.fetchAndStoreRelaxed(newDirtyFlags) == 0) {
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
	}
}

void AbstractMediaWidget::addDirtyFlags(DirtyFlags additionalDirtyFlags)
{
	while (true) {
		uint oldDirtyFlags = dirtyFlags;
		uint newDirtyFlags = (oldDirtyFlags | additionalDirtyFlags);

		if (oldDirtyFlags == newDirtyFlags) {
			break;
		}

		if (!dirtyFlags.testAndSetRelaxed(oldDirtyFlags, newDirtyFlags)) {
			continue;
		}

		if (oldDirtyFlags == 0) {
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		}

		break;
	}
}

void AbstractMediaWidget::customEvent(QEvent *event)
{
	Q_UNUSED(event)

	while (true) {
		uint oldDirtyFlags = dirtyFlags;
		uint lowestDirtyFlag = (oldDirtyFlags & (~(oldDirtyFlags - 1)));
		uint newDirtyFlags = (oldDirtyFlags & (~lowestDirtyFlag));

		if (oldDirtyFlags == newDirtyFlags) {
			break;
		}

		if (!dirtyFlags.testAndSetRelaxed(oldDirtyFlags, newDirtyFlags)) {
			continue;
		}

		switch (static_cast<DirtyFlag>(lowestDirtyFlag)) {
		case PlaybackFinished:
			mediaWidget->playbackFinished();
			break;
		case UpdatePlaybackStatus:
			mediaWidget->updatePlaybackStatus(getPlaybackStatus());
			break;
		case UpdateTotalTime:
			mediaWidget->updateTotalTime(getTotalTime());
			break;
		case UpdateCurrentTime:
			mediaWidget->updateCurrentTime(getCurrentTime());
			break;
		case UpdateSeekable:
			mediaWidget->updateSeekable(isSeekable());
			break;
		case UpdateMetadata:
			mediaWidget->updateMetadata(getMetadata());
			break;
		case UpdateAudioChannels:
			mediaWidget->updateAudioChannels(getAudioChannels(),
				getCurrentAudioChannel());
			break;
		case UpdateSubtitles:
			mediaWidget->updateSubtitles(getSubtitles(), getCurrentSubtitle());
			break;
		case UpdateTitles:
			mediaWidget->updateTitles(getTitleCount(), getCurrentTitle());
			break;
		case UpdateChapters:
			mediaWidget->updateChapters(getChapterCount(), getCurrentChapter());
			break;
		case UpdateAngles:
			mediaWidget->updateAngles(getAngleCount(), getCurrentAngle());
			break;
		case UpdateDvdPlayback:
			mediaWidget->updateDvdPlayback(hasMenu());
			break;
		case UpdateVideoSize:
			// FIXME
			mediaWidget->updateVideoSize();
			break;
		}
	}
}

MediaWidget::PlaybackStatus DummyMediaWidget::getPlaybackStatus()
{
	return MediaWidget::Idle;
}

int DummyMediaWidget::getCurrentTime()
{
	return 0;
}

int DummyMediaWidget::getTotalTime()
{
	return 0;
}

bool DummyMediaWidget::isSeekable()
{
	return false;
}

QMap<MediaWidget::MetadataType, QString> DummyMediaWidget::getMetadata()
{
	return QMap<MediaWidget::MetadataType, QString>();
}

QStringList DummyMediaWidget::getAudioChannels()
{
	return QStringList();
}

int DummyMediaWidget::getCurrentAudioChannel()
{
	return -1;
}

QStringList DummyMediaWidget::getSubtitles()
{
	return QStringList();
}

int DummyMediaWidget::getCurrentSubtitle()
{
	return -1;
}

int DummyMediaWidget::getTitleCount()
{
	return 0;
}

int DummyMediaWidget::getCurrentTitle()
{
	return -1;
}

int DummyMediaWidget::getChapterCount()
{
	return 0;
}

int DummyMediaWidget::getCurrentChapter()
{
	return -1;
}

int DummyMediaWidget::getAngleCount()
{
	return 0;
}

int DummyMediaWidget::getCurrentAngle()
{
	return -1;
}

bool DummyMediaWidget::hasMenu()
{
	return false;
}

QSize DummyMediaWidget::getVideoSize()
{
	return QSize();
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
