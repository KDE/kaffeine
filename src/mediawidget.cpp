/*
 * mediawidget.cpp
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
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

#include <QBoxLayout>
#include <Phonon/AbstractMediaStream>
#include <Phonon/AudioOutput>
#include <Phonon/MediaObject>
#include <Phonon/Path>
#include <Phonon/SeekSlider>
#include <Phonon/VideoWidget>
#include <Phonon/VolumeSlider>
#include <KAction>
#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>
#include <KToolBar>
#include <KUrl>
#include "kaffeine.h"

MediaWidget::MediaWidget(QWidget *parent, KToolBar *toolBar, KActionCollection *collection) :
	QWidget(parent), liveFeed(NULL), playing(true)
{
	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);

	mediaObject = new Phonon::MediaObject(this);
	connect(mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
		this, SLOT(stateChanged()));

	Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::VideoCategory, this);
	Phonon::createPath(mediaObject, audioOutput);

	Phonon::VideoWidget *videoWidget = new Phonon::VideoWidget(this);
	Phonon::createPath(mediaObject, videoWidget);
	layout->addWidget(videoWidget);

	actionBackward = new KAction(KIcon("media-skip-backward"), i18n("Previous"), collection);
	toolBar->addAction(collection->addAction("controls_previous", actionBackward));

	actionPlayPause = new KAction(collection);
	connect(actionPlayPause, SIGNAL(triggered(bool)), this, SLOT(playPause(bool)));
	actionPlayPause->setShortcut(Qt::Key_Space);
	textPlay = i18n("Play");
	textPause = i18n("Pause");
	iconPlay = KIcon("media-playback-start");
	iconPause = KIcon("media-playback-pause");
	toolBar->addAction(collection->addAction("controls_play_pause", actionPlayPause));

	actionStop = new KAction(KIcon("media-playback-stop"), i18n("Stop"), collection);
	connect(actionStop, SIGNAL(triggered(bool)), this, SLOT(stop()));
	toolBar->addAction(collection->addAction("controls_stop", actionStop));

	actionForward = new KAction(KIcon("media-skip-forward"), i18n("Next"), collection);
	toolBar->addAction(collection->addAction("controls_next", actionForward));

	Phonon::VolumeSlider *volumeSlider = new Phonon::VolumeSlider();
	volumeSlider->setAudioOutput(audioOutput);
	toolBar->addWidget(volumeSlider);

	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider();
	seekSlider->setMediaObject(mediaObject);
	QSizePolicy sizePolicy = seekSlider->sizePolicy();
	sizePolicy.setHorizontalStretch(1);
	seekSlider->setSizePolicy(sizePolicy);
	toolBar->addWidget(seekSlider);

	stateChanged();
}

bool MediaWidget::isPlaying() const
{
	return playing;
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

void MediaWidget::stateChanged()
{
	bool newPlaying = false;

	switch (mediaObject->state()) {
		case Phonon::BufferingState:
		case Phonon::PlayingState:
		case Phonon::PausedState:
			newPlaying = true;
			break;

		case Phonon::LoadingState:
		case Phonon::StoppedState:
			break;

		case Phonon::ErrorState:
			// FIXME check errorType
			KMessageBox::error(this, mediaObject->errorString());
			break;
	}

	if (liveFeed && !newPlaying) {
		liveFeed->liveStopped();
		delete liveFeed;
		liveFeed = NULL;
	}

	if (playing == newPlaying) {
		return;
	}

	if (newPlaying) {
		actionPlayPause->setText(textPause);
		actionPlayPause->setIcon(iconPause);
		actionPlayPause->setCheckable(true);
		actionStop->setEnabled(true);
	} else {
		actionPlayPause->setText(textPlay);
		actionPlayPause->setIcon(iconPlay);
		actionPlayPause->setCheckable(false);
		actionStop->setEnabled(false);
	}

	// FIXME take care of skip forward / backward
	actionBackward->setEnabled(false);
	actionForward->setEnabled(false);

	playing = newPlaying;
}

void MediaWidget::playPause(bool paused)
{
	if (isPlaying()) {
		if (paused) {
			mediaObject->pause();
		} else {
			mediaObject->play();
		}
	} else {
		// FIXME do some special actions - play playlist, ask for input ...
	}
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	emit toggleFullscreen();
}
