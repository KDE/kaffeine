/*
 * vlcmediawidget.h
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

#ifndef VLCMEDIAWIDGET_H
#define VLCMEDIAWIDGET_H

#include "../abstractmediawidget.h"

class libvlc_event_t;
class libvlc_instance_t;
class libvlc_media_player_t;
class QTimer;

class VlcMediaWidget : public AbstractMediaWidget
{
	Q_OBJECT
private:
	explicit VlcMediaWidget(QWidget *parent);
	bool init();
	QTimer *timer;

private slots:
	void hideMouse();

public:
	~VlcMediaWidget();

	static VlcMediaWidget *createVlcMediaWidget(QWidget *parent); // returns NULL if init fails

	// zero-based numbering is used everywhere (e.g. first audio channel = 0)

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
	void updateAudioStreams();
	void updateSubtitles();
	void updateTitles();
	void updateChapters();
	void updateAngles();
	void updateDvdMenu();
	void updateVideoSize();

private:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

	void vlcEvent(const libvlc_event_t *event);

	static void vlcEventHandler(const libvlc_event_t *event, void *instance);

	libvlc_instance_t *vlcInstance;
	libvlc_media_player_t *vlcMediaPlayer;
	bool playingDvd;
	bool mouseVisible;
};

#endif /* VLCMEDIAWIDGET_H */
