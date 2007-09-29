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

#include <QVBoxLayout>
#include <Phonon/AbstractMediaStream>
#include <Phonon/AudioOutput>
#include <Phonon/MediaObject>
#include <Phonon/Path>
#include <Phonon/VideoWidget>
#include <Phonon/SeekSlider>
#include <Phonon/VolumeSlider>
#include <KMessageBox>

#include "kaffeine.h"
#include "mediawidget.h"
#include "mediawidget.moc"

class DvbSourceHelper : public Phonon::AbstractMediaStream
{
public:
	DvbSourceHelper()
	{
		setStreamSize(-1);
	}

	~DvbSourceHelper() { }

	void reset() { }

	void needData() { }

	void writeData(const QByteArray &data)
	{
		Phonon::AbstractMediaStream::writeData(data);
	}
};

DvbSource::DvbSource()
{
	stream = new DvbSourceHelper();
}

DvbSource::~DvbSource()
{
	delete stream;
}

void DvbSource::writeData(const QByteArray &data)
{
	stream->writeData(data);
}

MediaWidget::MediaWidget(Manager *manager_) : manager(manager_), dvbSource(NULL)
{
	QVBoxLayout *box = new QVBoxLayout( this );
	box->setMargin(0);
	box->setSpacing(0);
	vw = new Phonon::VideoWidget( this );
	box->addWidget( vw );
	ao = new Phonon::AudioOutput( Phonon::VideoCategory, this );
	media = new Phonon::MediaObject( this );
	Phonon::createPath(media, vw);
	Phonon::createPath(media, ao);

	media->setTickInterval( 350 );

	connect(media, SIGNAL(stateChanged(Phonon::State, Phonon::State)), this, SLOT(stateChanged(Phonon::State)));
}

QWidget *MediaWidget::newPositionSlider()
{
	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider();
	seekSlider->setMediaObject(media);
	return seekSlider;
}

QWidget *MediaWidget::newVolumeSlider()
{
	Phonon::VolumeSlider *volumeSlider = new Phonon::VolumeSlider();
	volumeSlider->setAudioOutput(ao);
	return volumeSlider;
}

void MediaWidget::setFullscreen(bool fullscreen)
{
	if (fullscreen) {
		setWindowFlags((windowFlags() & (~Qt::WindowType_Mask)) | Qt::Dialog);
		showFullScreen();
	} else {
		setWindowFlags(windowFlags() & (~Qt::WindowType_Mask));
	}
}

void MediaWidget::setPaused(bool paused)
{
	if (paused) {
		media->pause();
	} else {
		media->play();
	}
}

void MediaWidget::play(const KUrl &url)
{
	media->setCurrentSource(url);
	media->play();
}

void MediaWidget::playAudioCd()
{
	media->setCurrentSource(Phonon::MediaSource(Phonon::Cd));
	media->play();
}

void MediaWidget::playVideoCd()
{
	media->setCurrentSource(Phonon::MediaSource(Phonon::Vcd));
	media->play();
}

void MediaWidget::playDvd()
{
	media->setCurrentSource(Phonon::MediaSource(Phonon::Dvd));
	media->play();
}

void MediaWidget::playDvb(DvbSource *source)
{
	dvbSource = source;
	media->setCurrentSource(Phonon::MediaSource(source->stream));
	media->play();
}

void MediaWidget::stop()
{
	media->stop();
}

void MediaWidget::stateChanged(Phonon::State state)
{
	switch (state) {
		case Phonon::LoadingState:
		case Phonon::BufferingState:
		case Phonon::PlayingState:
			manager->setPlaying();
			if (dvbSource != NULL) {
				dvbSource->setPaused(false);
			}
			break;

		case Phonon::PausedState:
			if (dvbSource != NULL) {
				dvbSource->setPaused(true);
			}
			break;

		case Phonon::StoppedState:
			manager->setStopped();
			if (dvbSource != NULL) {
				dvbSource->stop();
				dvbSource = NULL;
			}
			break;

		case Phonon::ErrorState:
			manager->setStopped();
			if (dvbSource != NULL) {
				dvbSource->stop();
				dvbSource = NULL;
			}
			KMessageBox::error(manager->getKaffeine(), media->errorString());
			break;
	}
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	manager->toggleFullscreen();
}
