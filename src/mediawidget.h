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
#include <Phonon/Global>
#include <KUrl>

#include "manager.h"

namespace Phonon
{
class AudioOutput;
class MediaObject;
class VideoWidget;
};
class DvbSourceHelper;

class DvbSource
{
	friend class MediaWidget;
public:
	DvbSource();
	virtual ~DvbSource();

	virtual void setPaused(bool paused) = 0;
	virtual void stop() = 0;

protected:
	void writeData(const QByteArray &data);

private:
	DvbSourceHelper *stream;
};

class MediaWidget : public QWidget
{
	Q_OBJECT
	
public:
	MediaWidget(Manager *manager_);
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
	 * ownership isn't taken; you have to delete it
	 * manually after DvbStream::stop() has been called
	 */

	void playDvb(DvbSource *stream);

public slots:
	void stop();

private:
	void mouseDoubleClickEvent(QMouseEvent *);

private:
	Phonon::VideoWidget *vw;
	Phonon::AudioOutput *ao;
	Phonon::MediaObject *media;

	Manager *manager;

	DvbSource *dvbSource;

private slots:
//	void newLength( qint64 );
	void playbackFinished();
	void stateChanged( Phonon::State, Phonon::State );

};

#endif /* MEDIAWIDGET_H */
