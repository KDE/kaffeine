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
#include <QLabel>
#include <QPushButton>
#include <QTimeEdit>
#include <Phonon/AbstractMediaStream>
#include <Phonon/AudioOutput>
#include <Phonon/MediaController>
#include <Phonon/MediaObject>
#include <Phonon/SeekSlider>
#include <Phonon/VideoWidget>
#include <KAction>
#include <KActionCollection>
#include <KComboBox>
#include <KConfigGroup>
#include <KDialog>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include <KToolBar>
#include <KUrl>

class DvbFeed : public Phonon::AbstractMediaStream
{
public:
	DvbFeed() : timeShiftActive(false), ignoreStop(false), audioChannelIndex(0),
		subtitleIndex(0)
	{
		setStreamSize(-1);
	}

	~DvbFeed() { }

	void endOfData()
	{
		Phonon::AbstractMediaStream::endOfData();
	}

	void writeData(const QByteArray &data)
	{
		Phonon::AbstractMediaStream::writeData(data);
	}

	bool timeShiftActive;
	bool ignoreStop;
	QStringList audioChannels;
	QStringList subtitles;
	int audioChannelIndex;
	int subtitleIndex;

private:
	void needData() { }
	void reset() { }
};

class JumpToPositionDialog : public KDialog
{
public:
	JumpToPositionDialog(const QTime &time, QWidget *parent);
	~JumpToPositionDialog();

	QTime getPosition() const;

private:
	QTimeEdit *timeEdit;
};

JumpToPositionDialog::JumpToPositionDialog(const QTime &time, QWidget *parent) : KDialog(parent)
{
	setCaption(i18n("Jump to Position"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *layout = new QVBoxLayout(widget);

	layout->addWidget(new QLabel(i18n("Enter a position:")));

	timeEdit = new QTimeEdit(time, this);
	timeEdit->setDisplayFormat("hh:mm:ss");
	layout->addWidget(timeEdit);

	setMainWidget(widget);
}

JumpToPositionDialog::~JumpToPositionDialog()
{
}

QTime JumpToPositionDialog::getPosition() const
{
	return timeEdit->time();
}

MediaWidget::MediaWidget(KMenu *menu, KToolBar *toolBar, KActionCollection *collection,
	QWidget *parent) : QWidget(parent), dvbFeed(NULL), playing(true), titleCount(0),
	chapterCount(0), audioChannelsReady(false), subtitlesReady(false)
{
	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);

	mediaObject = new Phonon::MediaObject(this);
	connect(mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
		this, SLOT(stateChanged(Phonon::State)));
	connect(mediaObject, SIGNAL(seekableChanged(bool)), this, SLOT(updateSeekable()));
	connect(mediaObject, SIGNAL(metaDataChanged()), this, SLOT(updateCaption()));
	connect(mediaObject, SIGNAL(currentSourceChanged(Phonon::MediaSource)),
		this, SLOT(stopDvb()));
	connect(mediaObject, SIGNAL(finished()), this, SLOT(playbackFinished()));

	audioOutput = new Phonon::AudioOutput(Phonon::VideoCategory, this);
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

	actionPrevious = new KAction(KIcon("media-skip-backward"), i18n("Previous"), this);
	actionPrevious->setShortcut(KShortcut(Qt::Key_PageUp, Qt::Key_MediaPrevious));
	connect(actionPrevious, SIGNAL(triggered(bool)), this, SLOT(previous()));
	toolBar->addAction(collection->addAction("controls_previous", actionPrevious));
	menu->addAction(actionPrevious);

	actionPlayPause = new KAction(this);
	actionPlayPause->setShortcut(KShortcut(Qt::Key_Space, Qt::Key_MediaPlay));
	textPlay = i18n("Play");
	textPause = i18n("Pause");
	iconPlay = KIcon("media-playback-start");
	iconPause = KIcon("media-playback-pause");
	connect(actionPlayPause, SIGNAL(triggered(bool)), this, SLOT(playPause(bool)));
	toolBar->addAction(collection->addAction("controls_play_pause", actionPlayPause));
	menu->addAction(actionPlayPause);

	actionStop = new KAction(KIcon("media-playback-stop"), i18n("Stop"), this);
	actionStop->setShortcut(KShortcut(Qt::Key_Delete, Qt::Key_MediaStop));
	connect(actionStop, SIGNAL(triggered(bool)), this, SLOT(stop()));
	toolBar->addAction(collection->addAction("controls_stop", actionStop));
	menu->addAction(actionStop);

	actionNext = new KAction(KIcon("media-skip-forward"), i18n("Next"), this);
	actionNext->setShortcut(KShortcut(Qt::Key_PageDown, Qt::Key_MediaNext));
	connect(actionNext, SIGNAL(triggered(bool)), this, SLOT(next()));
	toolBar->addAction(collection->addAction("controls_next", actionNext));
	menu->addAction(actionNext);
	menu->addSeparator();

	audioChannelBox = new KComboBox(toolBar);
	connect(audioChannelBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(changeAudioChannel(int)));
	toolBar->addWidget(audioChannelBox);

	subtitleBox = new KComboBox(toolBar);
	textSubtitlesOff = i18n("off");
	connect(subtitleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSubtitle(int)));
	toolBar->addWidget(subtitleBox);

	muteAction = new KAction(i18n("Mute volume"), this);
	mutedIcon = KIcon("audio-volume-muted");
	unmutedIcon = KIcon("audio-volume-medium");
	muteAction->setIcon(unmutedIcon);
	muteAction->setShortcut(KShortcut(Qt::Key_M, Qt::Key_VolumeMute));
	connect(muteAction, SIGNAL(triggered(bool)), this, SLOT(toggleMuted()));
	connect(audioOutput, SIGNAL(mutedChanged(bool)), this, SLOT(mutedChanged(bool)));
	toolBar->addAction(collection->addAction("controls_mute_volume", muteAction));

	KAction *action = new KAction(i18n("Volume slider"), this);
	volumeSlider = new QSlider(toolBar);
	volumeSlider->setFocusPolicy(Qt::NoFocus);
	volumeSlider->setOrientation(Qt::Horizontal);
	volumeSlider->setRange(0, 100);
	volumeSlider->setToolTip(action->text());
	connect(volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(changeVolume(int)));
	connect(audioOutput, SIGNAL(volumeChanged(qreal)), this, SLOT(volumeChanged(qreal)));
	volumeSlider->setValue(KConfigGroup(KGlobal::config(), "MediaObject").
		readEntry("Volume", 100));
	action->setDefaultWidget(volumeSlider);
	toolBar->addAction(collection->addAction("controls_volume_slider", action));

	action = new KAction(KIcon("audio-volume-high"), i18n("Increase Volume"), this);
	action->setShortcut(KShortcut(Qt::Key_Plus, Qt::Key_VolumeUp));
	connect(action, SIGNAL(triggered(bool)), this, SLOT(increaseVolume()));
	menu->addAction(collection->addAction("controls_increase_volume", action));

	action = new KAction(KIcon("audio-volume-low"), i18n("Decrease Volume"), this);
	action->setShortcut(KShortcut(Qt::Key_Minus, Qt::Key_VolumeDown));
	connect(action, SIGNAL(triggered(bool)), this, SLOT(decreaseVolume()));
	menu->addAction(collection->addAction("controls_decrease_volume", action));
	menu->addSeparator();

	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider(toolBar);
	seekSlider->setMediaObject(mediaObject);
	QSizePolicy sizePolicy = seekSlider->sizePolicy();
	sizePolicy.setHorizontalStretch(1);
	seekSlider->setSizePolicy(sizePolicy);
	toolBar->addWidget(seekSlider);

	longSkipBackwardAction = new KAction(KIcon("media-skip-backward"),
		i18n("Skip 1min Backward"), this);
	longSkipBackwardAction->setShortcut(Qt::SHIFT + Qt::Key_Left);
	connect(longSkipBackwardAction, SIGNAL(triggered(bool)), this, SLOT(longSkipBackward()));
	menu->addAction(collection->addAction("controls_long_skip_backward",
		longSkipBackwardAction));

	skipBackwardAction = new KAction(KIcon("media-skip-backward"), i18n("Skip 10s Backward"),
		this);
	skipBackwardAction->setShortcut(Qt::Key_Left);
	connect(skipBackwardAction, SIGNAL(triggered(bool)), this, SLOT(skipBackward()));
	menu->addAction(collection->addAction("controls_skip_backward", skipBackwardAction));

	skipForwardAction = new KAction(KIcon("media-skip-forward"), i18n("Skip 10s Forward"),
		this);
	skipForwardAction->setShortcut(Qt::Key_Right);
	connect(skipForwardAction, SIGNAL(triggered(bool)), this, SLOT(skipForward()));
	menu->addAction(collection->addAction("controls_skip_forward", skipForwardAction));

	longSkipForwardAction = new KAction(KIcon("media-skip-forward"), i18n("Skip 1min Forward"),
		this);
	longSkipForwardAction->setShortcut(Qt::SHIFT + Qt::Key_Right);
	connect(longSkipForwardAction, SIGNAL(triggered(bool)), this, SLOT(longSkipForward()));
	menu->addAction(collection->addAction("controls_long_skip_forward", longSkipForwardAction));

	jumpToPositionAction = new KAction(KIcon("go-jump"), i18n("Jump to Position"), this);
	jumpToPositionAction->setShortcut(Qt::CTRL + Qt::Key_J);
	connect(jumpToPositionAction, SIGNAL(triggered(bool)), this, SLOT(jumpToPosition()));
	menu->addAction(collection->addAction("controls_jump_to_position", jumpToPositionAction));

	action = new KAction(i18n("Switch between elapsed and remaining time display"), this);
	timeButton = new QPushButton(toolBar);
	timeButton->setFocusPolicy(Qt::NoFocus);
	timeButton->setToolTip(action->text());
	showElapsedTime = true;
	connect(timeButton, SIGNAL(clicked(bool)), this, SLOT(timeButtonClicked()));
	connect(mediaObject, SIGNAL(tick(qint64)), this, SLOT(updateTimeButton()));
	action->setDefaultWidget(timeButton);
	toolBar->addAction(collection->addAction("controls_time_button", action));

	stateChanged(Phonon::StoppedState);
}

MediaWidget::~MediaWidget()
{
	KConfigGroup(KGlobal::config(), "MediaObject").writeEntry("Volume", volumeSlider->value());
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

void MediaWidget::playDvb()
{
	DvbFeed *feed = new DvbFeed();
	mediaObject->setCurrentSource(Phonon::MediaSource(feed));
	mediaObject->play();
	dvbFeed = feed; // don't set dvbFeed before setCurrentSource
}

void MediaWidget::writeDvbData(const QByteArray &data)
{
	dvbFeed->writeData(data);
}

void MediaWidget::updateDvbAudioChannels(const QStringList &audioChannels, int currentIndex)
{
	dvbFeed->audioChannels = audioChannels;
	dvbFeed->audioChannelIndex = currentIndex;
	updateAudioChannelBox();
}

void MediaWidget::updateDvbSubtitles(const QStringList &subtitles, int currentIndex)
{
	dvbFeed->subtitles = subtitles;
	dvbFeed->subtitleIndex = currentIndex;
	updateSubtitleBox();
}

void MediaWidget::stopDvb()
{
	if ((dvbFeed != NULL) && !dvbFeed->ignoreStop) {
		delete dvbFeed;
		dvbFeed = NULL;
		emit dvbStopped();
	}
}

void MediaWidget::stateChanged(Phonon::State state)
{
	bool newPlaying = false;

	switch (state) {
		case Phonon::BufferingState:
		case Phonon::PlayingState:
		case Phonon::PausedState:
			newPlaying = true;
			break;

		case Phonon::LoadingState:
		case Phonon::StoppedState:
			// user has to be able to stop dvb playback even if no data has arrived yet
			newPlaying = (dvbFeed != NULL);
			break;

		case Phonon::ErrorState:
			// FIXME check errorType
			stopDvb();
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
	updateSeekable();
	updateTimeButton();
	updateCaption();
}

void MediaWidget::playbackFinished()
{
	if ((dvbFeed != NULL) && dvbFeed->timeShiftActive) {
		mediaObject->play();
	} else {
		emit playlistNext();
	}
}

void MediaWidget::previous()
{
	if (dvbFeed != NULL) {
		// FIXME
	} else if ((titleCount > 1) || (chapterCount > 1)) {
		mediaController->previousTitle();
	} else {
		emit playlistPrevious();
	}
}

void MediaWidget::playPause(bool paused)
{
	if (playing) {
		if (paused) {
			mediaObject->pause();

			if ((dvbFeed != NULL) && !dvbFeed->timeShiftActive) {
				dvbFeed->endOfData();
				emit prepareDvbTimeShift();
			}
		} else {
			if ((dvbFeed != NULL) && !dvbFeed->timeShiftActive) {
				dvbFeed->timeShiftActive = true;
				dvbFeed->ignoreStop = true;
				emit startDvbTimeShift();
				dvbFeed->ignoreStop = false;
			} else {
				mediaObject->play();
			}
		}
	} else {
		emit playlistPlay();
	}
}

void MediaWidget::stop()
{
	mediaObject->stop();
	stopDvb();
}

void MediaWidget::next()
{
	if (dvbFeed != NULL) {
		// FIXME
	} else if ((titleCount > 1) || (chapterCount > 1)) {
		mediaController->nextTitle();
	} else {
		emit playlistNext();
	}
}

void MediaWidget::changeAudioChannel(int index)
{
	if (audioChannelsReady) {
		if ((dvbFeed != NULL) && !dvbFeed->audioChannels.isEmpty()) {
			emit changeDvbAudioChannel(index);
		} else {
			mediaController->setCurrentAudioChannel(audioChannels.at(index));
		}
	}
}

void MediaWidget::changeSubtitle(int index)
{
	if (subtitlesReady) {
		if ((dvbFeed != NULL) && !dvbFeed->subtitles.isEmpty()) {
			emit changeDvbSubtitle(index);
		} else {
			mediaController->setCurrentSubtitle(subtitles.at(index));
		}
	}
}

void MediaWidget::toggleMuted()
{
	audioOutput->setMuted(!audioOutput->isMuted());
}

void MediaWidget::mutedChanged(bool muted)
{
	if (muted) {
		muteAction->setIcon(mutedIcon);
	} else {
		muteAction->setIcon(unmutedIcon);
	}
}

void MediaWidget::changeVolume(int volume)
{
	audioOutput->setVolume(volume * qreal(0.01));
}

void MediaWidget::volumeChanged(qreal volume)
{
	volumeSlider->setValue(volume * 100 + qreal(0.5));
}

void MediaWidget::increaseVolume()
{
	// QSlider ensures that the value is within the range
	volumeSlider->setValue(volumeSlider->value() + 5);
}

void MediaWidget::decreaseVolume()
{
	// QSlider ensures that the value is within the range
	volumeSlider->setValue(volumeSlider->value() - 5);
}

void MediaWidget::updateSeekable()
{
	bool seekable = playing && mediaObject->isSeekable();

	longSkipBackwardAction->setEnabled(seekable);
	skipBackwardAction->setEnabled(seekable);
	skipForwardAction->setEnabled(seekable);
	longSkipForwardAction->setEnabled(seekable);
	jumpToPositionAction->setEnabled(seekable);
}

void MediaWidget::longSkipBackward()
{
	qint64 time = mediaObject->currentTime() - 60000;

	if (time < 0) {
		time = 0;
	}

	mediaObject->seek(time);
}

void MediaWidget::skipBackward()
{
	qint64 time = mediaObject->currentTime() - 10000;

	if (time < 0) {
		time = 0;
	}

	mediaObject->seek(time);
}

void MediaWidget::skipForward()
{
	mediaObject->seek(mediaObject->currentTime() + 10000);
}

void MediaWidget::longSkipForward()
{
	mediaObject->seek(mediaObject->currentTime() + 60000);
}

void MediaWidget::jumpToPosition()
{
	JumpToPositionDialog dialog(QTime().addMSecs(mediaObject->currentTime()), this);

	if (dialog.exec() == QDialog::Accepted) {
		mediaObject->seek(QTime().msecsTo(dialog.getPosition()));
	}
}

void MediaWidget::timeButtonClicked()
{
	showElapsedTime = !showElapsedTime;
	updateTimeButton();
}

void MediaWidget::updateTimeButton()
{
	bool enabled = playing && ((dvbFeed == NULL) || dvbFeed->timeShiftActive);
	qint64 time = 0;

	if (enabled) {
		if (showElapsedTime) {
			// show elapsed time
			time = mediaObject->currentTime();
		} else {
			// show remaining time
			time = mediaObject->remainingTime();
		}

		if (time < 0) {
			time = 0;
		}
	}

	timeButton->setEnabled(enabled);

	if (showElapsedTime) {
		// show elapsed time
		timeButton->setText(' ' + QTime(0, 0).addMSecs(time).toString());
	} else {
		// show remaining time
		timeButton->setText('-' + QTime(0, 0).addMSecs(time).toString());
	}
}

void MediaWidget::updateCaption()
{
	QString caption;

	if (playing) {
		if (dvbFeed != NULL) {
			// FIXME
		} else {
			// FIXME include artist?
			QStringList strings = mediaObject->metaData(Phonon::TitleMetaData);

			if (!strings.isEmpty() && !strings.at(0).isEmpty()) {
				caption = strings.at(0);
			} else {
				caption = KUrl(mediaObject->currentSource().url()).fileName();
			}
		}
	}

	emit changeCaption(caption);
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

	if ((dvbFeed != NULL) && !dvbFeed->subtitles.isEmpty() && (subtitles.size() > 1)) {
		mediaController->setCurrentSubtitle(subtitles.at(1));
	}
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	emit toggleFullscreen();
}

void MediaWidget::updatePreviousNext()
{
	// bool enabled = playing && ((titleCount > 1) || (chapterCount > 1));
	bool enabled = playing; // hmm - ok for now
	actionPrevious->setEnabled(enabled);
	actionNext->setEnabled(enabled);
}

void MediaWidget::updateAudioChannelBox()
{
	audioChannelsReady = false;
	audioChannelBox->clear();

	if ((dvbFeed != NULL) && !dvbFeed->audioChannels.isEmpty()) {
		audioChannelBox->addItems(dvbFeed->audioChannels);
		audioChannelBox->setCurrentIndex(dvbFeed->audioChannelIndex);
		audioChannelBox->setEnabled(dvbFeed->audioChannels.size() > 1);
		audioChannelsReady = true;
	} else if (playing) {
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
		audioChannelBox->setEnabled(audioChannels.size() > 1);
		audioChannelsReady = true;
	} else {
		audioChannelBox->setEnabled(false);
	}
}

void MediaWidget::updateSubtitleBox()
{
	subtitlesReady = false;
	subtitleBox->clear();

	if ((dvbFeed != NULL) && !dvbFeed->subtitles.isEmpty()) {
		subtitleBox->addItems(dvbFeed->subtitles);
		subtitleBox->setCurrentIndex(dvbFeed->subtitleIndex);
		subtitleBox->setEnabled(dvbFeed->subtitles.size() > 1);
		subtitlesReady = true;
	} else if (playing) {
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
		subtitleBox->setEnabled(subtitles.size() > 1);
		subtitlesReady = true;
	} else {
		subtitleBox->setEnabled(false);
	}
}
