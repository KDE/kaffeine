/*
 * mediawidget.cpp
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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
#include <Phonon/MediaController>
#include <Phonon/MediaObject>
#include <Phonon/SeekSlider>
#include <Phonon/VideoWidget>
#include <Phonon/VolumeSlider>
#include <KAction>
#include <KActionCollection>
#include <KComboBox>
#include <KLocalizedString>
#include <KMessageBox>
#include <KToolBar>
#include <KUrl>

MediaWidget::MediaWidget(KToolBar *toolBar, KActionCollection *collection, QWidget *parent) :
	QWidget(parent), playing(true), titleCount(0), chapterCount(0), audioChannelsReady(false),
	subtitlesReady(false)
{
	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);

	mediaObject = new Phonon::MediaObject(this);
	connect(mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
		this, SLOT(stateChanged(Phonon::State)));

	Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::VideoCategory, this);
	Phonon::createPath(mediaObject, audioOutput);

	Phonon::VideoWidget *videoWidget = new Phonon::VideoWidget(this);
	Phonon::createPath(mediaObject, videoWidget);
	layout->addWidget(videoWidget);

	mediaController = new Phonon::MediaController(mediaObject);
	connect(mediaController, SIGNAL(availableTitlesChanged(int)),
		this, SLOT(titleCountChanged(int)));
	connect(mediaController, SIGNAL(availableChaptersChanged(int)),
		this, SLOT(chapterCountChanged(int)));
	connect(mediaController, SIGNAL(availableAudioChannelsChanged()),
		this, SLOT(audioChannelsChanged()));
	connect(mediaController, SIGNAL(availableSubtitlesChanged()),
		this, SLOT(subtitlesChanged()));

	actionPrevious = new KAction(KIcon("media-skip-backward"), i18n("Previous"), collection);
	connect(actionPrevious, SIGNAL(triggered(bool)), this, SLOT(previous()));
	toolBar->addAction(collection->addAction("controls_previous", actionPrevious));

	actionPlayPause = new KAction(collection);
	actionPlayPause->setShortcut(Qt::Key_Space);
	textPlay = i18n("Play");
	textPause = i18n("Pause");
	iconPlay = KIcon("media-playback-start");
	iconPause = KIcon("media-playback-pause");
	connect(actionPlayPause, SIGNAL(triggered(bool)), this, SLOT(playPause(bool)));
	toolBar->addAction(collection->addAction("controls_play_pause", actionPlayPause));

	actionStop = new KAction(KIcon("media-playback-stop"), i18n("Stop"), collection);
	connect(actionStop, SIGNAL(triggered(bool)), this, SLOT(stop()));
	toolBar->addAction(collection->addAction("controls_stop", actionStop));

	actionNext = new KAction(KIcon("media-skip-forward"), i18n("Next"), collection);
	connect(actionNext, SIGNAL(triggered(bool)), this, SLOT(next()));
	toolBar->addAction(collection->addAction("controls_next", actionNext));

	audioChannelBox = new KComboBox(toolBar);
	connect(audioChannelBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(changeAudioChannel(int)));
	toolBar->addWidget(audioChannelBox);

	subtitleBox = new KComboBox(toolBar);
	textSubtitlesOff = i18n("off");
	connect(subtitleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSubtitle(int)));
	toolBar->addWidget(subtitleBox);

	Phonon::VolumeSlider *volumeSlider = new Phonon::VolumeSlider(toolBar);
	volumeSlider->setAudioOutput(audioOutput);
	toolBar->addWidget(volumeSlider);

	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider(toolBar);
	seekSlider->setMediaObject(mediaObject);
	QSizePolicy sizePolicy = seekSlider->sizePolicy();
	sizePolicy.setHorizontalStretch(1);
	seekSlider->setSizePolicy(sizePolicy);
	toolBar->addWidget(seekSlider);

	stateChanged(Phonon::StoppedState);
}

MediaWidget::~MediaWidget()
{
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

void MediaWidget::playDvb(Phonon::AbstractMediaStream *feed)
{
	dvbFeed = feed;
	Phonon::MediaSource source(feed);
	source.setAutoDelete(true);
	mediaObject->setCurrentSource(source);
	mediaObject->play();
}

void MediaWidget::stateChanged(Phonon::State state)
{
	bool newPlaying = false;

	switch (state) {
		case Phonon::LoadingState: // FIXME used for different purposes??
		case Phonon::BufferingState:
		case Phonon::PlayingState:
		case Phonon::PausedState:
			newPlaying = true;
			break;

		case Phonon::StoppedState:
			break;

		case Phonon::ErrorState:
			// FIXME check errorType
			delete dvbFeed; // dvbFeed is a QPointer
			KMessageBox::error(this, mediaObject->errorString());
			break;
	}

	if (playing == newPlaying) {
		return;
	}

	playing = newPlaying;

	if (playing) {
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

	updatePreviousNext();
	updateAudioChannelBox();
	updateSubtitleBox();
}

void MediaWidget::previous()
{
	mediaController->previousTitle();
}

void MediaWidget::playPause(bool paused)
{
	if (playing) {
		if (paused) {
			mediaObject->pause();
		} else {
			mediaObject->play();
		}
	} else {
		// FIXME do some special actions - play playlist, ask for input ...
	}
}

void MediaWidget::stop()
{
	mediaObject->stop();
	delete dvbFeed; // dvbFeed is a QPointer
}

void MediaWidget::next()
{
	mediaController->nextTitle();
}

void MediaWidget::changeAudioChannel(int index)
{
	if (audioChannelsReady) {
		mediaController->setCurrentAudioChannel(audioChannels.at(index));
	}
}

void MediaWidget::changeSubtitle(int index)
{
	if (subtitlesReady) {
		mediaController->setCurrentSubtitle(subtitles.at(index));
	}
}

void MediaWidget::titleCountChanged(int count)
{
	titleCount = count;
	updatePreviousNext();
}

void MediaWidget::chapterCountChanged(int count)
{
	chapterCount = count;
	updatePreviousNext();
}

void MediaWidget::audioChannelsChanged()
{
	audioChannels = mediaController->availableAudioChannels();
	updateAudioChannelBox();
}

void MediaWidget::subtitlesChanged()
{
	subtitles = mediaController->availableSubtitles();
	subtitles.prepend(Phonon::SubtitleDescription()); // helper for switching subtitles off
	updateSubtitleBox();
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	emit toggleFullscreen();
}

void MediaWidget::updatePreviousNext()
{
	bool enabled = playing && ((titleCount > 1) || (chapterCount > 1));
	actionPrevious->setEnabled(enabled);
	actionNext->setEnabled(enabled);
}

void MediaWidget::updateAudioChannelBox()
{
	audioChannelsReady = false;
	audioChannelBox->clear();
	audioChannelBox->setEnabled(playing && (audioChannels.size() > 1));

	if (playing) {
		Phonon::AudioChannelDescription current = mediaController->currentAudioChannel();
		int index = -1;

		for (int i = 0; i < audioChannels.size(); ++i) {
			const Phonon::AudioChannelDescription &description = audioChannels.at(i);
			audioChannelBox->addItem(description.name());

			if (description == current) {
				index = i;
			}
		}

		audioChannelBox->setCurrentIndex(index);
		audioChannelsReady = true;
	}
}

void MediaWidget::updateSubtitleBox()
{
	subtitlesReady = false;
	subtitleBox->clear();
	subtitleBox->setEnabled(playing && (subtitles.size() > 1));

	if (playing) {
		Phonon::SubtitleDescription current = mediaController->currentSubtitle();
		int index = 0;

		subtitleBox->addItem(textSubtitlesOff);

		for (int i = 1; i < subtitles.size(); ++i) {
			const Phonon::SubtitleDescription &description = subtitles.at(i);
			subtitleBox->addItem(description.name());

			if (description == current) {
				index = i;
			}
		}

		subtitleBox->setCurrentIndex(index);
		subtitlesReady = true;
	}
}
