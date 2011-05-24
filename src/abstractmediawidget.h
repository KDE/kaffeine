/*
 * abstractmediawidget.h
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

#ifndef ABSTRACTMEDIAWIDGET_H
#define ABSTRACTMEDIAWIDGET_H

#include "mediawidget.h"

class AbstractMediaWidget : public QWidget
{
public:
	explicit AbstractMediaWidget(QWidget *parent);
	virtual ~AbstractMediaWidget();

	void setMediaWidget(MediaWidget *mediaWidget_);

	// zero-based numbering is used everywhere (e.g. first audio channel = 0)

	MediaWidget::PlaybackStatus getPlaybackStatus() const { return playbackStatus; }
	int getCurrentTime() const { return currentTime; } // milliseconds
	int getTotalTime() const { return totalTime; } // milliseconds
	bool isSeekable() const { return seekable; }
	QMap<MediaWidget::MetadataType, QString> getMetadata() const { return metadata; }
	QStringList getAudioChannels() const { return audioChannels; }
	int getCurrentAudioChannel() const { return currentAudioChannel; }
	QStringList getSubtitles() const { return subtitles; }
	int getCurrentSubtitle() const { return currentSubtitle; }
	int getTitleCount() const { return titleCount; }
	int getCurrentTitle() const { return currentTitle; }
	int getChapterCount() const { return chapterCount; }
	int getCurrentChapter() const { return currentChapter; }
	int getAngleCount() const { return angleCount; }
	int getCurrentAngle() const { return currentAngle; }
	bool hasDvdMenu() const { return dvdMenu; }
	QSize getVideoSize() const { return videoSize; }

	virtual void setMuted(bool muted) = 0;
	virtual void setVolume(int volume) = 0; // [0 - 200]
	virtual void setAspectRatio(MediaWidget::AspectRatio aspectRatio) = 0;
	virtual void setDeinterlacing(bool deinterlacing) = 0;
	virtual void play(const MediaSource &source) = 0;
	virtual void stop() = 0;
	virtual void setPaused(bool paused) = 0;
	virtual void seek(int time) = 0; // milliseconds
	virtual void setCurrentAudioChannel(int currentAudioChannel) = 0;
	virtual void setCurrentSubtitle(int currentSubtitle) = 0;
	virtual void setExternalSubtitle(const KUrl &subtitleUrl) = 0;
	virtual void setCurrentTitle(int currentTitle) = 0;
	virtual void setCurrentChapter(int currentChapter) = 0;
	virtual void setCurrentAngle(int currentAngle) = 0;
	virtual bool jumpToPreviousChapter() = 0;
	virtual bool jumpToNextChapter() = 0;
	virtual void showDvdMenu() = 0;

protected:
	void resetState();
	void updatePlaybackStatus(MediaWidget::PlaybackStatus playbackStatus_);
	void updateCurrentTotalTime(int currentTime_, int totalTime_);
	void updateSeekable(bool seekable_);
	void updateMetadata(const QMap<MediaWidget::MetadataType, QString> &metadata_);
	void updateAudioChannels(const QStringList &audioChannels_);
	void updateCurrentAudioChannel(int currentAudioChannel_);
	void updateSubtitles(const QStringList &subtitles_);
	void updateCurrentSubtitle(int currentSubtitle_);
	void updateTitleCount(int titleCount_);
	void updateCurrentTitle(int currentTitle_);
	void updateChapterCount(int chapterCount_);
	void updateCurrentChapter(int currentChapter_);
	void updateAngleCount(int angleCount_);
	void updateCurrentAngle(int currentAngle_);
	void updateDvdMenu(bool dvdMenu_);
	void updateVideoSize(const QSize &videoSize_);

private:
	enum PendingUpdate
	{
		PlaybackFinished = (1 << 0),
		UpdatePlaybackStatus = (1 << 1),
		UpdateCurrentTotalTime = (1 << 2),
		UpdateSeekable = (1 << 3),
		UpdateMetadata = (1 << 4),
		UpdateAudioChannels = (1 << 5),
		UpdateSubtitles = (1 << 6),
		UpdateTitles = (1 << 7),
		UpdateChapters = (1 << 8),
		UpdateAngles = (1 << 9),
		UpdateDvdMenu = (1 << 10),
		UpdateVideoSize = (1 << 11),
		ResetState = (UpdatePlaybackStatus | UpdateCurrentTotalTime | UpdateSeekable |
			UpdateMetadata | UpdateAudioChannels | UpdateSubtitles | UpdateTitles |
			UpdateChapters | UpdateAngles | UpdateDvdMenu | UpdateVideoSize)
	};

	Q_DECLARE_FLAGS(PendingUpdates, PendingUpdate)

	void addPendingUpdate(PendingUpdate pendingUpdate);
	void customEvent(QEvent *event);

	MediaWidget *mediaWidget;
	PendingUpdates pendingUpdates;
	MediaWidget::PlaybackStatus playbackStatus;
	int currentTime;
	int totalTime;
	bool seekable;
	QMap<MediaWidget::MetadataType, QString> metadata;
	QStringList audioChannels;
	int currentAudioChannel;
	QStringList subtitles;
	int currentSubtitle;
	int titleCount;
	int currentTitle;
	int chapterCount;
	int currentChapter;
	int angleCount;
	int currentAngle;
	bool dvdMenu;
	QSize videoSize;
};

class DummyMediaWidget : public AbstractMediaWidget
{
public:
	explicit DummyMediaWidget(QWidget *parent) : AbstractMediaWidget(parent) { }
	~DummyMediaWidget() { }

	// zero-based numbering is used everywhere (e.g. first audio channel = 0)

	void setMuted(bool muted);
	void setVolume(int volume); // [0 - 200]
	void setAspectRatio(MediaWidget::AspectRatio aspectRatio);
	void setDeinterlacing(bool deinterlacing);
	void play(const MediaSource &source);
	void stop();
	void setPaused(bool paused);
	void seek(int time); // milliseconds
	void setCurrentAudioChannel(int currentAudioChannel);
	void setCurrentSubtitle(int currentSubtitle);
	void setExternalSubtitle(const KUrl &subtitleUrl);
	void setCurrentTitle(int currentTitle);
	void setCurrentChapter(int currentChapter);
	void setCurrentAngle(int currentAngle);
	bool jumpToPreviousChapter();
	bool jumpToNextChapter();
	void showDvdMenu();
};

#endif /* ABSTRACTMEDIAWIDGET_H */
