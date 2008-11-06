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

#include "mediawidget.h"

#include <QHBoxLayout>
#include <Phonon/AbstractMediaStream>
#include <Phonon/AudioOutput>
#include <Phonon/MediaObject>
#include <Phonon/Path>
#include <Phonon/SeekSlider>
#include <Phonon/VideoWidget>
#include <Phonon/VolumeSlider>
#include <KMessageBox>
#include "kaffeine.h"
#include "manager.h"

MediaWidget::MediaWidget(Manager *manager_) : manager(manager_), liveFeed(NULL)
{
	QBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);

	mediaObject = new Phonon::MediaObject(this);

	audioOutput = new Phonon::AudioOutput(Phonon::VideoCategory, this);
	Phonon::createPath(mediaObject, audioOutput);

	Phonon::VideoWidget *videoWidget = new Phonon::VideoWidget(this);
	Phonon::createPath(mediaObject, videoWidget);
	layout->addWidget(videoWidget);

	connect(mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
		this, SLOT(stateChanged(Phonon::State)));
}

QWidget *MediaWidget::newPositionSlider()
{
	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider();
	seekSlider->setMediaObject(mediaObject);
	return seekSlider;
}

QWidget *MediaWidget::newVolumeSlider()
{
	Phonon::VolumeSlider *volumeSlider = new Phonon::VolumeSlider();
	volumeSlider->setAudioOutput(audioOutput);
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
		mediaObject->pause();
	} else {
		mediaObject->play();
	}
}

void MediaWidget::play(const KUrl &url)
{
	mediaObject->setCurrentSource(url);
	mediaObject->play();
}

void MediaWidget::playAudioCd()
{
	mediaObject->setCurrentSource(Phonon::MediaSource(Phonon::Cd));
	mediaObject->play();
}

void MediaWidget::playVideoCd()
{
	mediaObject->setCurrentSource(Phonon::MediaSource(Phonon::Vcd));
	mediaObject->play();
}

void MediaWidget::playDvd()
{
	mediaObject->setCurrentSource(Phonon::MediaSource(Phonon::Dvd));
	mediaObject->play();
}

void MediaWidget::playDvb(DvbLiveFeed *feed)
{
	if (liveFeed != NULL) {
		liveFeed->liveStopped();
		delete liveFeed;
		liveFeed = NULL;
	}

	liveFeed = feed;
	mediaObject->setCurrentSource(Phonon::MediaSource(feed));
	mediaObject->play();
}

void MediaWidget::stop()
{
	mediaObject->stop();
}

void MediaWidget::stateChanged(Phonon::State state)
{
	switch (state) {
		case Phonon::LoadingState:
		case Phonon::BufferingState:
		case Phonon::PlayingState:
		case Phonon::PausedState:
			if (liveFeed != NULL) {
				liveFeed->livePaused(state == Phonon::PausedState);
			}
			manager->setPlaying();
			break;

		case Phonon::StoppedState:
			if (liveFeed != NULL) {
				liveFeed->liveStopped();
				delete liveFeed;
				liveFeed = NULL;
			}
			manager->setStopped();
			break;

		case Phonon::ErrorState:
			if (liveFeed != NULL) {
				liveFeed->liveStopped();
				delete liveFeed;
				liveFeed = NULL;
			}
			manager->setStopped();
			// FIXME check errorType
			KMessageBox::error(manager->getKaffeine(), mediaObject->errorString());
			break;
	}
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	manager->toggleFullscreen();
}
