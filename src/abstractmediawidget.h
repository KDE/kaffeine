/*
 * abstractmediawidget.h
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

#ifndef ABSTRACTMEDIAWIDGET_H
#define ABSTRACTMEDIAWIDGET_H

#include <QMap>
#include "mediawidget.h"

class AbstractMediaWidget : public QWidget
{
public:
	explicit AbstractMediaWidget(QWidget *parent);
	virtual ~AbstractMediaWidget();

	void connectToMediaWidget(MediaWidget *mediaWidget_);

	// zero-based numbering is used everywhere (e.g. first audio channel = 0)

	MediaWidget::PlaybackStatus getPlaybackStatus() const { return playbackStatus; }
	int getCurrentTime() const { return currentTime; } // milliseconds
	int getTotalTime() const { return totalTime; } // milliseconds
	bool isSeekable() const { return seekable; }
	QMap<MediaWidget::MetadataType, QString> getMetadata() const { return metadata; }
	QStringList getAudioStreams() const { return audioStreams; }
	int getCurrentAudioStream() const { return currentAudioStream; }
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

	virtual QStringList getAudioDevices() = 0;
	virtual void setAudioDevice(QString device) = 0;
	virtual void setMuted(bool muted) = 0;
	virtual void setVolume(int volume) = 0; // [0 - 200]
	virtual void setAspectRatio(MediaWidget::AspectRatio aspectRatio) = 0;
	virtual void setDeinterlacing(bool deinterlacing) = 0;
	virtual void play(const MediaSource &source) = 0;
	virtual void stop() = 0;
	virtual void setPaused(bool paused) = 0;
	virtual void seek(int time) = 0; // milliseconds
	virtual void setCurrentAudioStream(int currentAudioStream) = 0;
	virtual void setCurrentSubtitle(int currentSubtitle) = 0;
	virtual void setExternalSubtitle(const QUrl &subtitleUrl) = 0;
	virtual void setCurrentTitle(int currentTitle) = 0;
	virtual void setCurrentChapter(int currentChapter) = 0;
	virtual void setCurrentAngle(int currentAngle) = 0;
	virtual bool jumpToPreviousChapter() = 0;
	virtual bool jumpToNextChapter() = 0;
	virtual void showDvdMenu() = 0;

	enum PendingUpdate
	{
		PlaybackFinished = (1 << 0),
		PlaybackStatus = (1 << 1),
		CurrentTotalTime = (1 << 2),
		Seekable = (1 << 3),
		Metadata = (1 << 4),
		AudioStreams = (1 << 5),
		Subtitles = (1 << 6),
		Titles = (1 << 7),
		Chapters = (1 << 8),
		Angles = (1 << 9),
		DvdMenu = (1 << 10),
		VideoSize = (1 << 11)
	};

	Q_DECLARE_FLAGS(PendingUpdates, PendingUpdate)

protected:
	void addPendingUpdates(PendingUpdates pendingUpdatesToBeAdded); // thread-safe

	virtual int updatePlaybackStatus() = 0;
	virtual void updateCurrentTotalTime() = 0;
	virtual void updateSeekable() = 0;
	virtual void updateMetadata() = 0;
	virtual void updateAudioStreams() = 0;
	virtual void updateSubtitles() = 0;
	virtual void updateTitles() = 0;
	virtual void updateChapters() = 0;
	virtual void updateAngles() = 0;
	virtual void updateDvdMenu() = 0;
	virtual void updateVideoSize() = 0;

	MediaWidget::PlaybackStatus playbackStatus;
	int currentTime;
	int totalTime;
	bool seekable;
	QMap<MediaWidget::MetadataType, QString> metadata;
	QStringList audioStreams;
	int currentAudioStream;
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

private:
	void customEvent(QEvent *event);

	MediaWidget *mediaWidget;
	QAtomicInt pendingUpdates;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractMediaWidget::PendingUpdates)

class DummyMediaWidget : public AbstractMediaWidget
{
public:
	explicit DummyMediaWidget(QWidget *parent);
	~DummyMediaWidget();

	QStringList getAudioDevices();
	void setAudioDevice(QString device);
	void setMuted(bool muted);
	void setVolume(int volume); // [0 - 200]
	void setAspectRatio(MediaWidget::AspectRatio aspectRatio);
	void setDeinterlacing(bool deinterlacing);
	void play(const MediaSource &source);
	void stop();
	void setPaused(bool paused);
	void seek(int time); // milliseconds
	void setCurrentAudioStream(int currentAudioStream);
	void setCurrentSubtitle(int currentSubtitle);
	void setExternalSubtitle(const QUrl &subtitleUrl);
	void setCurrentTitle(int currentTitle);
	void setCurrentChapter(int currentChapter);
	void setCurrentAngle(int currentAngle);
	bool jumpToPreviousChapter();
	bool jumpToNextChapter();
	void showDvdMenu();

	int updatePlaybackStatus();
	void updateCurrentTotalTime();
	void updateSeekable();
	void updateMetadata();
	void updateAudioDevices();
	void updateAudioStreams();
	void updateSubtitles();
	void updateTitles();
	void updateChapters();
	void updateAngles();
	void updateDvdMenu();
	void updateVideoSize();
};

#endif /* ABSTRACTMEDIAWIDGET_H */
