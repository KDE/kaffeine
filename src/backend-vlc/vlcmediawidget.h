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

#include <vlc/vlc.h>

#include "../abstractmediawidget.h"

class QTimer;

class VlcMediaWidget : public AbstractMediaWidget
{
	Q_OBJECT
private:
	explicit VlcMediaWidget(QWidget *parent);
	bool init();

private slots:
	void hideMouse();

public:
	~VlcMediaWidget();

	static VlcMediaWidget *createVlcMediaWidget(QWidget *parent); // returns NULL if init fails

	// zero-based numbering is used everywhere (e.g. first audio channel = 0)

	QStringList getAudioDevices() override;
	void setAudioDevice(QString device) override;
	void setMuted(bool muted) override;
	void setVolume(int volume) override; // [0 - 200]
	void setAspectRatio(MediaWidget::AspectRatio aspectRatio) override;
	void resizeToVideo(float resizeFactor) override;
	void setDeinterlacing(MediaWidget::DeinterlaceMode deinterlacing) override;
	void play(const MediaSource &source) override;
	void stop() override;
	void setPaused(bool paused) override;
	void seek(int time) override; // milliseconds
	void setCurrentAudioStream(int currentAudioStream) override;
	void setCurrentSubtitle(int currentSubtitle) override;
	void setExternalSubtitle(const QUrl &subtitleUrl) override;
	void setCurrentTitle(int currentTitle) override;
	void setCurrentChapter(int currentChapter) override;
	void setCurrentAngle(int currentAngle) override;
	bool jumpToPreviousChapter() override;
	bool jumpToNextChapter() override;
	void showDvdMenu() override;
	void dvdNavigate(int key) override;
	void playDirection(int direction);
	int makePlay();

	int updatePlaybackStatus() override;
	void updateCurrentTotalTime() override;
	void updateSeekable() override;
	void updateMetadata() override;
	void updateAudioStreams() override;
	void updateSubtitles() override;
	void updateTitles() override;
	void updateChapters() override;
	void updateAngles() override;
	void updateDvdMenu() override;
	void updateVideoSize() override;
	void unregisterEvents();
	bool registerEvents();

private:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;

	void vlcEvent(const libvlc_event_t *event);

	static void vlcEventHandler(const libvlc_event_t *event, void *instance);

	QTimer *timer;
	libvlc_instance_t *vlcInstance;
	libvlc_media_t *vlcMedia;
	libvlc_media_player_t *vlcMediaPlayer;
	libvlc_event_manager_t *eventManager;
	bool isPaused;
	bool playingDvd;
	bool urlIsAudioCd;
	QMap<int, int> subtitleId;
	QByteArray typeOfDevice;
	int trackNumber, numTracks;
	QVector<libvlc_event_e> eventType;
};

#endif /* VLCMEDIAWIDGET_H */
