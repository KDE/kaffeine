/*
 * vlcmediawidget_p.h
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

#ifndef VLCMEDIAWIDGET_P_H
#define VLCMEDIAWIDGET_P_H

#include "vlcmediawidget.h"

class libvlc_event_t;
class libvlc_instance_t;
class libvlc_media_player_t;

class VlcMediaWidget : public AbstractMediaWidget
{
private:
	explicit VlcMediaWidget(QWidget *parent);
	bool init();

public:
	~VlcMediaWidget();

	static VlcMediaWidget *createVlcMediaWidget(QWidget *parent);

	void setMuted(bool muted);
	void setVolume(int volume); // [0 - 100]
	void setAspectRatio(MediaWidget::AspectRatio aspectRatio);
	void setDeinterlacing(bool deinterlacing);

	// it is guaranteed that updatePlaybackStatus() will
	// be emitted after calling play() or stop()
	void play(MediaWidget::Source source, const KUrl &url, const KUrl &subtitleUrl);
	void stop();

	void setPaused(bool paused);
	void seek(int time);
	void setCurrentAudioChannel(int currentAudioChannel);
	void setCurrentSubtitle(int currentSubtitle);
	void setExternalSubtitle(const KUrl &subtitleUrl);
	void setCurrentTitle(int currentTitle); // first title = 0
	void setCurrentChapter(int currentChapter); // first chapter = 0
	void setCurrentAngle(int currentAngle); // first angle = 0
	bool jumpToPreviousChapter();
	bool jumpToNextChapter();
	void toggleMenu();

	QSize sizeHint() const;

protected:
	void customEvent(QEvent *event);

private:
	static void eventHandler(const libvlc_event_t *event, void *instance);

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

	void addDirtyFlags(uint changedDirtyFlags);
	void setDirtyFlags(uint newDirtyFlags);

	libvlc_instance_t *vlcInstance;
	libvlc_media_player_t *vlcMediaPlayer;
	QAtomicInt dirtyFlags;
};

#endif /* VLCMEDIAWIDGET_P_H */
