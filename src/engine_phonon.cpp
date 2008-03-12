
#include "engine_phonon.h"
/*
 * engine_phonon.cpp
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

#include <Phonon/AbstractMediaStream>
#include <Phonon/AudioOutput>
#include <Phonon/MediaObject>
#include <Phonon/Path>
#include <Phonon/SeekSlider>
#include <Phonon/VideoWidget>
#include <Phonon/VolumeSlider>

#include "engine_phonon.h"

EnginePhonon::EnginePhonon(QObject *parent) : Engine(parent), liveFeed(NULL)
{
	mediaObject = new Phonon::MediaObject(this);
	audioOutput = new Phonon::AudioOutput(Phonon::VideoCategory, this);

	liveFeedPhonon = new DvbLiveFeedPhonon(this);

	Phonon::createPath(mediaObject, audioOutput);

	connect(mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
		this, SLOT(stateChanged(Phonon::State)));
}

EnginePhonon::~EnginePhonon()
{
}

QWidget *EnginePhonon::videoWidget()
{
	Phonon::VideoWidget *videoWidget = new Phonon::VideoWidget();
	Phonon::createPath(mediaObject, videoWidget);
	return videoWidget;
}

QWidget *EnginePhonon::positionSlider()
{
	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider();
	seekSlider->setMediaObject(mediaObject);
	return seekSlider;
}

QWidget *EnginePhonon::volumeSlider()
{
	Phonon::VolumeSlider *volumeSlider = new Phonon::VolumeSlider();
	volumeSlider->setAudioOutput(audioOutput);
	return volumeSlider;
}

void EnginePhonon::play(const KUrl &url)
{
	mediaObject->setCurrentSource(url);
	mediaObject->play();
}

void EnginePhonon::playAudioCd()
{
	mediaObject->setCurrentSource(Phonon::MediaSource(Phonon::Cd));
	mediaObject->play();
}

void EnginePhonon::playVideoCd()
{
	mediaObject->setCurrentSource(Phonon::MediaSource(Phonon::Vcd));
	mediaObject->play();
}

void EnginePhonon::playDvd()
{
	mediaObject->setCurrentSource(Phonon::MediaSource(Phonon::Dvd));
	mediaObject->play();
}

void EnginePhonon::playDvb(DvbLiveFeed *feed)
{
	liveFeed = feed;
	connect(liveFeed, SIGNAL(writeData(QByteArray)), liveFeedPhonon, SLOT(writeData(QByteArray)));
	mediaObject->setCurrentSource(Phonon::MediaSource(liveFeedPhonon));
	mediaObject->play();
}

void EnginePhonon::setPaused(bool paused)
{
	if (paused) {
		mediaObject->pause();
	} else {
		mediaObject->play();
	}
}

void EnginePhonon::stop()
{
	mediaObject->stop();
}

void EnginePhonon::setVolume(int volume)
{
	audioOutput->setVolume(volume / 100.0);
}

int EnginePhonon::getPosition()
{
	int totalTime = mediaObject->totalTime();

	if (totalTime == 0) {
		return 0;
	}

	return (mediaObject->currentTime() * 65535) / (totalTime * 1000);
}

void EnginePhonon::setPosition(int position)
{
	qint64 totalTime = mediaObject->totalTime();

	if (totalTime != 0) {
		mediaObject->seek((position * totalTime) / (65535 * 1000));
	}
}

void EnginePhonon::stateChanged(Phonon::State state)
{
	switch (state) {
		case Phonon::LoadingState:
			break;

		case Phonon::BufferingState:
		case Phonon::PlayingState:
			if (liveFeed != NULL) {
				liveFeed->livePaused(false);
			}
			break;

		case Phonon::PausedState:
			if (liveFeed != NULL) {
				liveFeed->livePaused(true);
			}
			break;

		case Phonon::StoppedState:
			if (liveFeed != NULL) {
				disconnect(liveFeed, SIGNAL(writeData(QByteArray)), liveFeedPhonon, SLOT(writeData(QByteArray)));
				liveFeed->liveStopped();
				liveFeed = NULL;
			}
			emit playbackFinished();
			break;

		case Phonon::ErrorState:
			if (liveFeed != NULL) {
				disconnect(liveFeed, SIGNAL(writeData(QByteArray)), liveFeedPhonon, SLOT(writeData(QByteArray)));
				liveFeed->liveStopped();
				liveFeed = NULL;
			}
			emit playbackFailed(mediaObject->errorString());
			break;
	}
}
