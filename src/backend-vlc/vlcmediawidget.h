/*
 * vlcmediawidget.h
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

#ifndef VLCMEDIAWIDGET_H
#define VLCMEDIAWIDGET_H

#include "../mediawidget.h"

class AbstractMediaWidget : public QWidget
{
	Q_OBJECT
protected:
	explicit AbstractMediaWidget(QWidget *parent);

public:
	~AbstractMediaWidget();

	static AbstractMediaWidget *createVlcMediaWidget(QWidget *parent);

	virtual void setMuted(bool muted);
	virtual void setVolume(int volume); // [0 - 100]
	virtual void setAspectRatio(MediaWidget::AspectRatio aspectRatio);
	virtual void setDeinterlacing(bool deinterlacing);

	// it is guaranteed that updatePlaybackStatus() will
	// be emitted after calling play() or stop()
	virtual void play(MediaWidget::Source source, const KUrl &url, const KUrl &subtitleUrl);
	virtual void stop();

	virtual void setPaused(bool paused);
	virtual void seek(int time);
	virtual void setCurrentAudioChannel(int currentAudioChannel);
	virtual void setCurrentSubtitle(int currentSubtitle);
	virtual void setExternalSubtitle(const KUrl &subtitleUrl);
	virtual void setCurrentTitle(int currentTitle); // first title = 0
	virtual void setCurrentChapter(int currentChapter); // first chapter = 0
	virtual void setCurrentAngle(int currentAngle); // first angle = 0
	virtual bool jumpToPreviousChapter();
	virtual bool jumpToNextChapter();
	virtual void toggleMenu();

signals:
	void playbackFinished();
	void updatePlaybackStatus(MediaWidget::PlaybackStatus playbackStatus);
	void updateTotalTime(int totalTime); // milliseconds
	void updateCurrentTime(int currentTime); // milliseconds
	void updateMetadata(const QMap<MediaWidget::MetadataType, QString> &metadata);
	void updateSeekable(bool seekable);
	// first audio channel = 0
	void updateAudioChannels(const QStringList &audioChannels, int currentAudioChannel);
	// first subtitle = 0
	void updateSubtitles(const QStringList &subtitles, int currentSubtitle);
	void updateTitles(int titleCount, int currentTitle);
	void updateChapters(int chapterCount, int currentChapter);
	void updateAngles(int angleCount, int currentAngle);
	void updateDvdPlayback(bool playingDvd);
	void updateVideoSize();
};

#endif /* VLCMEDIAWIDGET_H */
