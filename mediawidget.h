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

#include <config.h>

#include <QWidget>

#include <phonon/videopath.h>
#include <phonon/audiooutput.h>
#include <phonon/audiopath.h>
#include <phonon/mediaobject.h>
#include <phonon/ui/videowidget.h>
#include <phonon/ui/seekslider.h>
#include <phonon/ui/volumeslider.h>
#include <kurl.h>

using namespace Phonon;

enum MediaState{ MediaPlaying, MediaPaused, MediaStopped, MediaBuffering, MediaLoading, MediaError };

class MediaWidget : public QWidget
{
	Q_OBJECT
	
public:
	MediaWidget( QWidget *parent );
	~MediaWidget() { }
	void play( const KUrl &url );
	QWidget* getPositionSlider();
	QWidget* getVolumeSlider();
	
public slots:
	void play();
	void togglePause( bool b );
	void stop();
	void setVolume( int val );
	
signals:
	void volumeHasChanged( int );
	void newState( MediaState );
	
private:
	VideoWidget *vw;
	VideoPath *vp;
	AudioOutput *ao;
	AudioPath *ap;
	MediaObject *media;
	SeekSlider *seekSlider;
	VolumeSlider *volumeSlider;
	
	KUrl currentUrl;
	
private slots:
	void volumeChanged( float val );
	void playbackFinished();
	void stateChanged( Phonon::State, Phonon::State );

};

#endif /* MEDIAWIDGET_H */
