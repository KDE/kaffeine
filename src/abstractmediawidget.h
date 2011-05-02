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
	explicit AbstractMediaWidget(QWidget *parent) : QWidget(parent), mediaWidget(NULL) { }
	virtual ~AbstractMediaWidget() { }

	void setMediaWidget(MediaWidget *mediaWidget_);
	void invalidateState(); // re-emit signals

	// zero-based numbering is used everywhere (e.g. first audio channel = 0)

	virtual MediaWidget::PlaybackStatus getPlaybackStatus() = 0;
	virtual int getTotalTime() = 0; // milliseconds
	virtual int getCurrentTime() = 0; // milliseconds
	virtual bool isSeekable() = 0;
	virtual QMap<MediaWidget::MetadataType, QString> getMetadata() = 0;
	virtual QStringList getAudioChannels() = 0;
	virtual int getCurrentAudioChannel() = 0;
	virtual QStringList getSubtitles() = 0;
	virtual int getCurrentSubtitle() = 0;
	virtual int getTitleCount() = 0;
	virtual int getCurrentTitle() = 0;
	virtual int getChapterCount() = 0;
	virtual int getCurrentChapter() = 0;
	virtual int getAngleCount() = 0;
	virtual int getCurrentAngle() = 0;
	virtual bool hasMenu() = 0;
	virtual QSize getVideoSize() = 0;
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
	virtual void toggleMenu() = 0;

	enum DirtyFlag
	{
		PlaybackFinished = (1 << 0),
		UpdatePlaybackStatus = (1 << 1),
		UpdateTotalTime = (1 << 2),
		UpdateCurrentTime = (1 << 3),
		UpdateSeekable = (1 << 4),
		UpdateMetadata = (1 << 5),
		UpdateAudioChannels = (1 << 6),
		UpdateSubtitles = (1 << 7),
		UpdateTitles = (1 << 8),
		UpdateChapters = (1 << 9),
		UpdateAngles = (1 << 10),
		UpdateDvdPlayback = (1 << 11),
		UpdateVideoSize = (1 << 12)
	};

	Q_DECLARE_FLAGS(DirtyFlags, DirtyFlag)

protected:
	void addDirtyFlags(DirtyFlags additionalDirtyFlags);

private:
	void customEvent(QEvent *event);

	QAtomicInt dirtyFlags;
	MediaWidget *mediaWidget;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractMediaWidget::DirtyFlags)

class DummyMediaWidget : public AbstractMediaWidget
{
public:
	explicit DummyMediaWidget(QWidget *parent) : AbstractMediaWidget(parent) { }
	~DummyMediaWidget() { }

	// zero-based numbering is used everywhere (e.g. first audio channel = 0)

	MediaWidget::PlaybackStatus getPlaybackStatus();
	int getTotalTime(); // milliseconds
	int getCurrentTime(); // milliseconds
	bool isSeekable();
	QMap<MediaWidget::MetadataType, QString> getMetadata();
	QStringList getAudioChannels();
	int getCurrentAudioChannel();
	QStringList getSubtitles();
	int getCurrentSubtitle();
	int getTitleCount();
	int getCurrentTitle();
	int getChapterCount();
	int getCurrentChapter();
	int getAngleCount();
	int getCurrentAngle();
	bool hasMenu();
	QSize getVideoSize();
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
	void toggleMenu();
};

#endif /* ABSTRACTMEDIAWIDGET_H */
