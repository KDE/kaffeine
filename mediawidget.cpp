/*
 * mediawidget.cpp
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

#include <config.h>

#include <QVBoxLayout>
#include "mediawidget.h"
#include "mediawidget.moc"

using namespace Phonon;

MediaWidget::MediaWidget( QWidget *parent ) : QWidget(parent)
{
	QVBoxLayout *box = new QVBoxLayout( this );
	box->setMargin(0);
	box->setSpacing(0);
	vw = new VideoWidget( this );
	box->addWidget( vw );
	vp = new VideoPath( this );
	ao = new AudioOutput( Phonon::VideoCategory, this );
	ap = new AudioPath( this );
	media = new MediaObject( this );
	media->addVideoPath( vp );
	vp->addOutput( vw );
	media->addAudioPath( ap );
	ap->addOutput( ao );
	
	media->setTickInterval( 350 );
	
//	connect( media, SIGNAL( length( qint64 ) ),this, SLOT( newLength( qint64 ) ) );
	connect( media, SIGNAL(finished()), this, SLOT(playbackFinished()) );
	connect( media, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(stateChanged(Phonon::State,Phonon::State)) );
}

/*
void MediaWidget::newLength( qint64 )
{
}
*/

QWidget *MediaWidget::newPositionSlider()
{
	SeekSlider *seekSlider = new SeekSlider();
	seekSlider->setMediaProducer(media);
	return seekSlider;
}

QWidget *MediaWidget::newVolumeSlider()
{
	VolumeSlider *volumeSlider = new VolumeSlider();
	volumeSlider->setAudioOutput(ao);
	return volumeSlider;
}

void MediaWidget::play(const KUrl &url)
{
	media->setUrl( url );
	currentUrl = url;
	media->play();
}

void MediaWidget::play()
{
	if ( !currentUrl.path().isEmpty() ) {
		media->setUrl( currentUrl );
		media->play();
	}
}

void MediaWidget::togglePause( bool b )
{
	if ( b && (media->state()==PlayingState) )
		media->pause();
	else if ( media->state()==PausedState )
		media->play();
}

void MediaWidget::stop()
{
	media->stop();
}

void MediaWidget::playbackFinished()
{
}

void MediaWidget::stateChanged( Phonon::State status, Phonon::State )
{
	MediaState state=MediaStopped;
	
	switch (status) {
		case PlayingState:
			state = MediaPlaying;
			break;
		case PausedState:
			state = MediaPaused;
			break;
		case StoppedState:
			state = MediaStopped;
			break;
		case LoadingState:
			state = MediaLoading;
			break;
		case BufferingState:
			state = MediaBuffering;
			break;
		case ErrorState:
			state = MediaError;
			break;
	}
	emit newState( state );
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	vw->setFullScreen(!vw->isFullScreen());
}
