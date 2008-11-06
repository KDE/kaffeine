/*
 * mediawidget.h
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef MEDIAWIDGET_H
#define MEDIAWIDGET_H

#include <QWidget>
#include <Phonon/AbstractMediaStream>
#include <KUrl>

namespace Phonon
{
class AudioOutput;
};

class DvbLiveFeed;
class Manager;

class MediaWidget : public QWidget
{
	Q_OBJECT
public:
	explicit MediaWidget(Manager *manager_);
	~MediaWidget() { }

	QWidget *newPositionSlider();
	QWidget *newVolumeSlider();

	void setFullscreen(bool fullscreen);
	void setPaused(bool paused);

	/*
	 * loads the media and starts playback
	 */

	void play(const KUrl &url);
	void playAudioCd();
	void playVideoCd();
	void playDvd();

	/*
	 * ownership is taken over by the media widget
	 */

	void playDvb(DvbLiveFeed *feed);

public slots:
	void stop();

private slots:
	void stateChanged(Phonon::State state);

private:
	void mouseDoubleClickEvent(QMouseEvent *);

	Manager *manager;
	Phonon::MediaObject *mediaObject;
	Phonon::AudioOutput *audioOutput;
	DvbLiveFeed *liveFeed;
};

class DvbLiveFeed : public Phonon::AbstractMediaStream
{
public:
	DvbLiveFeed() { }
	virtual ~DvbLiveFeed() { }

	virtual void livePaused(bool paused) = 0;
	virtual void liveStopped() = 0;
};

#endif /* MEDIAWIDGET_H */
