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
	QWidget *parent) : QWidget(parent), dvbFeed(NULL), playing(true)
{
	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);

	setFocusPolicy(Qt::StrongFocus);

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

	videoWidget = new Phonon::VideoWidget(this);
	Phonon::createPath(mediaObject, videoWidget);
	layout->addWidget(videoWidget);

	mediaController = new Phonon::MediaController(mediaObject);

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
	audioChannelsReady = false;
	connect(audioChannelBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(changeAudioChannel(int)));
	connect(mediaController, SIGNAL(availableAudioChannelsChanged()),
		this, SLOT(audioChannelsChanged()));
	toolBar->addWidget(audioChannelBox);

	subtitleBox = new KComboBox(toolBar);
	textSubtitlesOff = i18n("off");
	subtitlesReady = false;
	connect(subtitleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSubtitle(int)));
	connect(mediaController, SIGNAL(availableSubtitlesChanged()),
		this, SLOT(subtitlesChanged()));
	toolBar->addWidget(subtitleBox);

	KMenu *audioMenu = new KMenu(i18n("Audio"));

	KAction *action = new KAction(KIcon("audio-volume-high"), i18n("Increase Volume"), this);
	action->setShortcut(KShortcut(Qt::Key_Plus, Qt::Key_VolumeUp));
	connect(action, SIGNAL(triggered(bool)), this, SLOT(increaseVolume()));
	audioMenu->addAction(collection->addAction("controls_increase_volume", action));

	action = new KAction(KIcon("audio-volume-low"), i18n("Decrease Volume"), this);
	action->setShortcut(KShortcut(Qt::Key_Minus, Qt::Key_VolumeDown));
	connect(action, SIGNAL(triggered(bool)), this, SLOT(decreaseVolume()));
	audioMenu->addAction(collection->addAction("controls_decrease_volume", action));

	muteAction = new KAction(i18n("Mute Volume"), this);
	mutedIcon = KIcon("audio-volume-muted");
	unmutedIcon = KIcon("audio-volume-medium");
	muteAction->setIcon(unmutedIcon);
	muteAction->setShortcut(KShortcut(Qt::Key_M, Qt::Key_VolumeMute));
	connect(muteAction, SIGNAL(triggered(bool)), this, SLOT(toggleMuted()));
	connect(audioOutput, SIGNAL(mutedChanged(bool)), this, SLOT(mutedChanged(bool)));
	toolBar->addAction(collection->addAction("controls_mute_volume", muteAction));
	audioMenu->addAction(muteAction);

	menu->addMenu(audioMenu);
	menu->addSeparator();

	KMenu *aspectMenu = new KMenu(i18n("Aspect Ratio"));
	QActionGroup *aspectGroup = new QActionGroup(this);

	action = new KAction(i18n("Auto"), aspectGroup);
	action->setCheckable(true);
	action->setChecked(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatioAuto()));
	aspectMenu->addAction(collection->addAction("controls_aspect_auto", action));

	action = new KAction(i18n("4:3"), aspectGroup);
	action->setCheckable(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatio4_3()));
	aspectMenu->addAction(collection->addAction("controls_aspect_4_3", action));

	action = new KAction(i18n("16:9"), aspectGroup);
	action->setCheckable(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatio16_9()));
	aspectMenu->addAction(collection->addAction("controls_aspect_16_9", action));

	action = new KAction(i18n("Fit to Window"), aspectGroup);
	action->setCheckable(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatioWidget()));
	aspectMenu->addAction(collection->addAction("controls_aspect_widget", action));

	menu->addMenu(aspectMenu);
	menu->addSeparator();

	action = new KAction(i18n("Volume Slider"), this);
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

	navigationMenu = new KMenu(i18n("Navigation"));

	action = new KAction(KIcon("media-skip-backward"), i18n("Skip 1min Backward"), this);
	action->setShortcut(Qt::SHIFT + Qt::Key_Left);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(longSkipBackward()));
	navigationMenu->addAction(collection->addAction("controls_long_skip_backward", action));

	action = new KAction(KIcon("media-skip-backward"), i18n("Skip 10s Backward"), this);
	action->setShortcut(Qt::Key_Left);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(skipBackward()));
	navigationMenu->addAction(collection->addAction("controls_skip_backward", action));

	action = new KAction(KIcon("media-skip-forward"), i18n("Skip 10s Forward"), this);
	action->setShortcut(Qt::Key_Right);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(skipForward()));
	navigationMenu->addAction(collection->addAction("controls_skip_forward", action));

	action = new KAction(KIcon("media-skip-forward"), i18n("Skip 1min Forward"), this);
	action->setShortcut(Qt::SHIFT + Qt::Key_Right);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(longSkipForward()));
	navigationMenu->addAction(collection->addAction("controls_long_skip_forward", action));
	menu->addMenu(navigationMenu);

	jumpToPositionAction = new KAction(KIcon("go-jump"), i18n("Jump to Position"), this);
	jumpToPositionAction->setShortcut(Qt::CTRL + Qt::Key_J);
	connect(jumpToPositionAction, SIGNAL(triggered(bool)), this, SLOT(jumpToPosition()));
	menu->addAction(collection->addAction("controls_jump_to_position", jumpToPositionAction));
	menu->addSeparator();

	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider(toolBar);
	seekSlider->setMediaObject(mediaObject);
	QSizePolicy sizePolicy = seekSlider->sizePolicy();
	sizePolicy.setHorizontalStretch(1);
	seekSlider->setSizePolicy(sizePolicy);
	toolBar->addWidget(seekSlider);

	titleMenu = new KMenu(i18n("Title"), this);
	titleGroup = new QActionGroup(this);
	titleCount = 0;
	connect(titleGroup, SIGNAL(triggered(QAction*)), this, SLOT(changeTitle(QAction*)));
	connect(mediaController, SIGNAL(availableTitlesChanged(int)),
		this, SLOT(titleCountChanged(int)));
	connect(mediaController, SIGNAL(titleChanged(int)), this, SLOT(updateTitleMenu()));
	menu->addMenu(titleMenu);

	chapterMenu = new KMenu(i18n("Chapter"), this);
	chapterGroup = new QActionGroup(this);
	chapterCount = 0;
	connect(chapterGroup, SIGNAL(triggered(QAction*)), this, SLOT(changeChapter(QAction*)));
	connect(mediaController, SIGNAL(availableChaptersChanged(int)),
		this, SLOT(chapterCountChanged(int)));
	connect(mediaController, SIGNAL(chapterChanged(int)), this, SLOT(updateChapterMenu()));
	menu->addMenu(chapterMenu);

	angleMenu = new KMenu(i18n("Angle"), this);
	angleGroup = new QActionGroup(this);
	angleCount = 0;
	connect(angleGroup, SIGNAL(triggered(QAction*)), this, SLOT(changeAngle(QAction*)));
	connect(mediaController, SIGNAL(availableAnglesChanged(int)),
		this, SLOT(angleCountChanged(int)));
	connect(mediaController, SIGNAL(angleChanged(int)), this, SLOT(updateAngleMenu()));
	menu->addMenu(angleMenu);

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

void MediaWidget::playDvb(const QString &channelName)
{
	DvbFeed *feed = new DvbFeed();
	mediaObject->setCurrentSource(Phonon::MediaSource(feed));
	mediaObject->play();
	dvbFeed = feed; // don't set dvbFeed before setCurrentSource
	emit changeCaption(channelName);
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
		actionPrevious->setEnabled(true);
		actionStop->setEnabled(true);
		actionNext->setEnabled(true);
	} else {
		actionPlayPause->setText(textPlay);
		actionPlayPause->setIcon(iconPlay);
		actionPlayPause->setCheckable(false);
		actionPrevious->setEnabled(false);
		actionStop->setEnabled(false);
		actionNext->setEnabled(false);
	}

	updateAudioChannelBox();
	updateSubtitleBox();
	updateTitleMenu();
	updateChapterMenu();
	updateAngleMenu();
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
		emit previousDvbChannel();
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
			actionPlayPause->setIcon(iconPlay);
			actionPlayPause->setText(textPlay);
			mediaObject->pause();

			if ((dvbFeed != NULL) && !dvbFeed->timeShiftActive) {
				dvbFeed->endOfData();
				emit prepareDvbTimeShift();
			}
		} else {
			actionPlayPause->setIcon(iconPause);
			actionPlayPause->setText(textPause);

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
		emit nextDvbChannel();
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

void MediaWidget::aspectRatioAuto()
{
	videoWidget->setAspectRatio(Phonon::VideoWidget::AspectRatioAuto);
}

void MediaWidget::aspectRatio4_3()
{
	videoWidget->setAspectRatio(Phonon::VideoWidget::AspectRatio4_3);
}

void MediaWidget::aspectRatio16_9()
{
	videoWidget->setAspectRatio(Phonon::VideoWidget::AspectRatio16_9);
}

void MediaWidget::aspectRatioWidget()
{
	videoWidget->setAspectRatio(Phonon::VideoWidget::AspectRatioWidget);
}

void MediaWidget::updateTitleMenu()
{
	if (playing && (titleCount > 1)) {
		QList<QAction *> actions = titleGroup->actions();

		if (actions.count() < titleCount) {
			for (int i = actions.count(); i < titleCount; ++i) {
				QAction *action = titleGroup->addAction(QString::number(i + 1));
				action->setCheckable(true);
				titleMenu->addAction(action);
			}
		} else if (actions.count() > titleCount) {
			for (int i = actions.count(); i > titleCount; --i) {
				delete actions.at(i - 1);
			}
		}

		int current = mediaController->currentTitle() - 1;

		if ((current >= 0) && (current < titleCount)) {
			titleGroup->actions().at(current)->setChecked(true);
		}

		titleMenu->setEnabled(true);
	} else {
		titleMenu->setEnabled(false);
	}
}

void MediaWidget::updateChapterMenu()
{
	if (playing && (chapterCount > 1)) {
		QList<QAction *> actions = chapterGroup->actions();

		if (actions.count() < chapterCount) {
			for (int i = actions.count(); i < chapterCount; ++i) {
				QAction *action = chapterGroup->addAction(QString::number(i + 1));
				action->setCheckable(true);
				chapterMenu->addAction(action);
			}
		} else if (actions.count() > chapterCount) {
			for (int i = actions.count(); i > chapterCount; --i) {
				delete actions.at(i - 1);
			}
		}

		int current = mediaController->currentChapter() - 1;

		if ((current >= 0) && (current < chapterCount)) {
			chapterGroup->actions().at(current)->setChecked(true);
		}

		chapterMenu->setEnabled(true);
	} else {
		chapterMenu->setEnabled(false);
	}
}

void MediaWidget::updateAngleMenu()
{
	if (playing && (angleCount > 1)) {
		QList<QAction *> actions = angleGroup->actions();

		if (actions.count() < angleCount) {
			for (int i = actions.count(); i < angleCount; ++i) {
				QAction *action = angleGroup->addAction(QString::number(i + 1));
				action->setCheckable(true);
				angleMenu->addAction(action);
			}
		} else if (actions.count() > angleCount) {
			for (int i = actions.count(); i > angleCount; --i) {
				delete actions.at(i - 1);
			}
		}

		int current = mediaController->currentAngle() - 1;

		if ((current >= 0) && (current < angleCount)) {
			angleGroup->actions().at(current)->setChecked(true);
		}

		angleMenu->setEnabled(true);
	} else {
		angleMenu->setEnabled(false);
	}
}

void MediaWidget::changeTitle(QAction *action)
{
	mediaController->setCurrentTitle(titleGroup->actions().indexOf(action) + 1);
}

void MediaWidget::changeChapter(QAction *action)
{
	mediaController->setCurrentChapter(chapterGroup->actions().indexOf(action) + 1);
}

void MediaWidget::changeAngle(QAction *action)
{
	mediaController->setCurrentAngle(angleGroup->actions().indexOf(action) + 1);
}

void MediaWidget::updateSeekable()
{
	if (playing && mediaObject->isSeekable()) {
		navigationMenu->setEnabled(true);
		jumpToPositionAction->setEnabled(true);
	} else {
		navigationMenu->setEnabled(false);
		jumpToPositionAction->setEnabled(false);
	}
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
			return;
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
	updateTitleMenu();
}

void MediaWidget::chapterCountChanged(int count)
{
	chapterCount = count;
	updateChapterMenu();
}

void MediaWidget::angleCountChanged(int count)
{
	angleCount = count;
	updateAngleMenu();
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
