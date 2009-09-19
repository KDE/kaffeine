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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "mediawidget.h"

#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDBusInterface>
#include <QLabel>
#include <QPushButton>
#include <QTimeEdit>
#include <QTimer>
#include <Phonon/AbstractMediaStream>
#include <Phonon/AudioOutput>
#include <Phonon/MediaController>
#include <Phonon/MediaObject>
#include <Phonon/SeekSlider>
#include <Phonon/VideoWidget>
#include <KAction>
#include <KActionCollection>
#include <KComboBox>
#include <KDialog>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include <KToolBar>
#include "osdwidget.h"

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

MediaWidget::MediaWidget(KMenu *menu_, KAction *fullScreenAction, KToolBar *toolBar,
	KActionCollection *collection, QWidget *parent) : QWidget(parent), menu(menu_),
	dvbFeed(NULL), playing(true)
{
	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);

	setAcceptDrops(true);
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

	osdWidget = new OsdWidget(this);

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
	actionStop->setShortcut(KShortcut(Qt::Key_Backspace, Qt::Key_MediaStop));
	connect(actionStop, SIGNAL(triggered(bool)), this, SLOT(stop()));
	toolBar->addAction(collection->addAction("controls_stop", actionStop));
	menu->addAction(actionStop);

	actionNext = new KAction(KIcon("media-skip-forward"), i18n("Next"), this);
	actionNext->setShortcut(KShortcut(Qt::Key_PageDown, Qt::Key_MediaNext));
	connect(actionNext, SIGNAL(triggered(bool)), this, SLOT(next()));
	toolBar->addAction(collection->addAction("controls_next", actionNext));
	menu->addAction(actionNext);
	menu->addSeparator();

	menu->addAction(collection->addAction("view_fullscreen", fullScreenAction));

	KMenu *autoResizeMenu = new KMenu(i18n("Automatic Resize"), this);
	QActionGroup *autoResizeGroup = new QActionGroup(this);
	// we need an event even if you select the currently selected item
	autoResizeGroup->setExclusive(false);
	connect(autoResizeGroup, SIGNAL(triggered(QAction*)), this, SLOT(autoResize(QAction*)));

	KAction *action = new KAction(i18nc("automatic resize", "Off"), autoResizeGroup);
	action->setCheckable(true);
	autoResizeMenu->addAction(collection->addAction("controls_autoresize_off", action));

	action = new KAction(i18nc("automatic resize", "Original Size"), autoResizeGroup);
	action->setCheckable(true);
	autoResizeMenu->addAction(collection->addAction("controls_autoresize_original", action));

	action = new KAction(i18nc("automatic resize", "Double Size"), autoResizeGroup);
	action->setCheckable(true);
	autoResizeMenu->addAction(collection->addAction("controls_autoresize_double", action));

	autoResizeFactor = KGlobal::config()->group("MediaObject").readEntry("AutoResizeFactor", 0);

	if ((autoResizeFactor < 0) || (autoResizeFactor > 2)) {
		autoResizeFactor = 0;
	}

	autoResizeGroup->actions().at(autoResizeFactor)->setChecked(true);

	menu->addMenu(autoResizeMenu);
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

	KMenu *audioMenu = new KMenu(i18n("Audio"), this);

	action = new KAction(KIcon("audio-volume-high"), i18n("Increase Volume"), this);
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

	KMenu *aspectMenu = new KMenu(i18n("Aspect Ratio"), this);
	QActionGroup *aspectGroup = new QActionGroup(this);

	action = new KAction(i18nc("aspect ratio", "Automatic"), aspectGroup);
	action->setCheckable(true);
	action->setChecked(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatioAuto()));
	aspectMenu->addAction(collection->addAction("controls_aspect_auto", action));

	action = new KAction(i18nc("aspect ratio", "Fit to Window"), aspectGroup);
	action->setCheckable(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatioWidget()));
	aspectMenu->addAction(collection->addAction("controls_aspect_widget", action));

	action = new KAction(i18nc("aspect ratio", "4:3"), aspectGroup);
	action->setCheckable(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatio4_3()));
	aspectMenu->addAction(collection->addAction("controls_aspect_4_3", action));

	action = new KAction(i18nc("aspect ratio", "16:9"), aspectGroup);
	action->setCheckable(true);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(aspectRatio16_9()));
	aspectMenu->addAction(collection->addAction("controls_aspect_16_9", action));

	menu->addMenu(aspectMenu);
	menu->addSeparator();

	action = new KAction(i18n("Volume Slider"), this);
	volumeSlider = new QSlider(toolBar);
	volumeSlider->setFocusPolicy(Qt::NoFocus);
	volumeSlider->setOrientation(Qt::Horizontal);
	volumeSlider->setRange(0, 100);
	volumeSlider->setToolTip(action->text());
	connect(volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(changeVolume(int)));
	volumeSlider->setValue(KConfigGroup(KGlobal::config(), "MediaObject").
		readEntry("Volume", 100));
	connect(audioOutput, SIGNAL(volumeChanged(qreal)), this, SLOT(volumeChanged(qreal)));
	action->setDefaultWidget(volumeSlider);
	toolBar->addAction(collection->addAction("controls_volume_slider", action));

	navigationMenu = new KMenu(i18nc("playback menu", "Skip"), this);

	shortSkipDuration =
		KGlobal::config()->group("MediaObject").readEntry("ShortSkipDuration", 15);
	longSkipDuration =
		KGlobal::config()->group("MediaObject").readEntry("LongSkipDuration", 60);

	longSkipBackwardAction = new KAction(KIcon("media-skip-backward"),
		i18nc("submenu of 'Skip'", "Skip %1s Backward", longSkipDuration), this);
	longSkipBackwardAction->setShortcut(Qt::SHIFT + Qt::Key_Left);
	connect(longSkipBackwardAction, SIGNAL(triggered(bool)), this, SLOT(longSkipBackward()));
	navigationMenu->addAction(collection->addAction("controls_long_skip_backward", longSkipBackwardAction));

	shortSkipBackwardAction = new KAction(KIcon("media-skip-backward"),
		i18nc("submenu of 'Skip'", "Skip %1s Backward", shortSkipDuration), this);
	shortSkipBackwardAction->setShortcut(Qt::Key_Left);
	connect(shortSkipBackwardAction, SIGNAL(triggered(bool)), this, SLOT(skipBackward()));
	navigationMenu->addAction(collection->addAction("controls_skip_backward", shortSkipBackwardAction));

	shortSkipForwardAction = new KAction(KIcon("media-skip-forward"),
		i18nc("submenu of 'Skip'", "Skip %1s Forward", shortSkipDuration), this);
	shortSkipForwardAction->setShortcut(Qt::Key_Right);
	connect(shortSkipForwardAction, SIGNAL(triggered(bool)), this, SLOT(skipForward()));
	navigationMenu->addAction(collection->addAction("controls_skip_forward", shortSkipForwardAction));

	longSkipForwardAction = new KAction(KIcon("media-skip-forward"),
		i18nc("submenu of 'Skip'", "Skip %1s Forward", longSkipDuration), this);
	longSkipForwardAction->setShortcut(Qt::SHIFT + Qt::Key_Right);
	connect(longSkipForwardAction, SIGNAL(triggered(bool)), this, SLOT(longSkipForward()));
	navigationMenu->addAction(collection->addAction("controls_long_skip_forward", longSkipForwardAction));
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

	QTimer *timer = new QTimer(this);
	timer->start(50000);
	connect(timer, SIGNAL(timeout()), this, SLOT(checkScreenSaver()));

	stateChanged(Phonon::StoppedState);
}

MediaWidget::~MediaWidget()
{
	KGlobal::config()->group("MediaObject").writeEntry("Volume", volumeSlider->value());
	KGlobal::config()->group("MediaObject").writeEntry("AutoResizeFactor", autoResizeFactor);
	KGlobal::config()->group("MediaObject").writeEntry("ShortSkipDuration", shortSkipDuration);
	KGlobal::config()->group("MediaObject").writeEntry("LongSkipDuration", longSkipDuration);
}

QString MediaWidget::extensionFilter()
{
	return QString(
		// generated from kaffeine.desktop's mime types
		"*.669 *.aac *.ac3 *.aif *.aifc *.aiff *.anim1 *.anim2 *.anim3 *.anim4 *.anim5 "
		"*.anim6 *.anim7 *.anim8 *.anim9 *.animj *.asf *.asx *.au *.avi *.divx *.dv *.flac "
		"*.flc *.fli *.flv *.it *.m15 *.m2t *.m3u *.m3u8 *.m4a *.m4b *.m4v *.med *.mka "
		"*.mkv *.mng *.mod *.moov *.mov *.mp+ *.mp2 *.mp3 *.mp4 *.mpc *.mpe *.mpeg *.mpg "
		"*.mpga *.mpp *.mtm *.nsv *.oga *.ogg *.ogm *.ogv *.ogx *.pls *.qt *.qtl *.qtvr "
		"*.ra *.ram *.rax *.rm *.rmj *.rmm *.rms *.rmvb *.rmx *.rv *.rvx *.s3m *.shn *.snd "
		"*.spx *.spx *.stm *.tta *.ult *.uni *.vlc *.vob *.voc *.wav *.wax *.wma *.wmp "
		"*.wmv *.wmx *.wv *.wvp *.wvx *.xm *.xspf "
		// manual entries
		"*.kaffeine *.iso|") + i18nc("file filter description", "Supported media files") +
		"\n*|" + i18nc("file filter description", "All files");
}

void MediaWidget::play(const KUrl &url)
{
	if (url.toLocalFile().endsWith(QLatin1String(".iso"), Qt::CaseInsensitive)) {
		// FIXME move into phonon
		KUrl copy(url);
		copy.setProtocol("dvd");
		mediaObject->setCurrentSource(copy);
		mediaObject->play();
		return;
	}

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

OsdWidget *MediaWidget::getOsdWidget()
{
	return osdWidget;
}

void MediaWidget::playDvb(const QString &channelName)
{
	DvbFeed *feed = new DvbFeed();
	mediaObject->setCurrentSource(Phonon::MediaSource(feed));
	mediaObject->play();
	dvbFeed = feed; // don't set dvbFeed before setCurrentSource

	if (!channelName.isEmpty()) {
		osdWidget->showText(channelName, 2500);
	}

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

int MediaWidget::getShortSkipDuration() const
{
	return shortSkipDuration;
}

int MediaWidget::getLongSkipDuration() const
{
	return longSkipDuration;
}

void MediaWidget::setShortSkipDuration(int duration)
{
	shortSkipDuration = duration;
	shortSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward", duration));
	shortSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", duration));
}

void MediaWidget::setLongSkipDuration(int duration)
{
	longSkipDuration = duration;
	longSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward", duration));
	longSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", duration));
}

void MediaWidget::stopDvb()
{
	if ((dvbFeed != NULL) && !dvbFeed->ignoreStop) {
		delete dvbFeed;
		dvbFeed = NULL;
		updateAudioChannelBox();
		updateSubtitleBox();
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

		if (autoResizeFactor > 0) {
			emit resizeToVideo(autoResizeFactor);
		}
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
			osdWidget->showText(i18nc("osd", "Paused"), 1500);
			mediaObject->pause();

			if ((dvbFeed != NULL) && !dvbFeed->timeShiftActive) {
				dvbFeed->endOfData();
				emit prepareDvbTimeShift();
			}
		} else {
			actionPlayPause->setIcon(iconPause);
			actionPlayPause->setText(textPause);
			osdWidget->showText(i18nc("osd", "Playing"), 1500);

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
	osdWidget->showText(i18nc("osd", "Stopped"), 1500);
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

void MediaWidget::autoResize(QAction *action)
{
	QList<QAction *> actions = action->actionGroup()->actions();

	foreach (QAction *it, actions) {
		it->setChecked(it == action);
	}

	autoResizeFactor = actions.indexOf(action);

	if (autoResizeFactor > 0) {
		emit resizeToVideo(autoResizeFactor);
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
		osdWidget->showText(i18nc("osd", "Mute On"), 1500);
	} else {
		muteAction->setIcon(unmutedIcon);
		osdWidget->showText(i18nc("osd", "Mute Off"), 1500);
	}
}

void MediaWidget::changeVolume(int volume)
{
	audioOutput->setVolume(volume * qreal(0.01));
}

void MediaWidget::volumeChanged(qreal volume)
{
	int percentage = volume * 100 + qreal(0.5);
	volumeSlider->setValue(percentage);
	osdWidget->showText(i18nc("osd", "Volume: %1%", percentage), 1500);
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
	qint64 time = mediaObject->currentTime() - 1000 * longSkipDuration;

	if (time < 0) {
		time = 0;
	}

	mediaObject->seek(time);
}

void MediaWidget::skipBackward()
{
	qint64 time = mediaObject->currentTime() - 1000 * shortSkipDuration;

	if (time < 0) {
		time = 0;
	}

	mediaObject->seek(time);
}

void MediaWidget::skipForward()
{
	mediaObject->seek(mediaObject->currentTime() + 1000 * shortSkipDuration);
}

void MediaWidget::longSkipForward()
{
	mediaObject->seek(mediaObject->currentTime() + 1000 * longSkipDuration);
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
	if (dvbFeed != NULL) {
		return;
	}

	QString caption;

	if (playing) {
		// FIXME include artist?
		QStringList strings = mediaObject->metaData(Phonon::TitleMetaData);

		if (!strings.isEmpty() && !strings.at(0).isEmpty()) {
			caption = strings.at(0);
		} else {
			caption = KUrl(mediaObject->currentSource().url()).fileName();
		}
	}

	if (!caption.isEmpty()) {
		osdWidget->showText(caption, 2500);
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

void MediaWidget::checkScreenSaver()
{
	if (playing && (mediaObject->state() != Phonon::PausedState) && mediaObject->hasVideo()) {
		QDBusInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
			"org.freedesktop.ScreenSaver").call(QDBus::NoBlock, "SimulateUserActivity");
	}
}

void MediaWidget::contextMenuEvent(QContextMenuEvent *event)
{
	menu->popup(event->globalPos());
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	emit toggleFullScreen();
}

void MediaWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
	}
}

void MediaWidget::dropEvent(QDropEvent *event)
{
	const QMimeData *mimeData = event->mimeData();

	if (mimeData->hasUrls()) {
		emit playlistUrlsDropped(KUrl::List::fromMimeData(mimeData));
		event->acceptProposedAction();
	}
}

void MediaWidget::keyPressEvent(QKeyEvent *event)
{
	int key = event->key();

	if ((key >= Qt::Key_0) && (key <= Qt::Key_9)) {
		emit osdKeyPressed(key);
	} else {
		QWidget::keyPressEvent(event);
	}
}

void MediaWidget::resizeEvent(QResizeEvent *event)
{
	osdWidget->resize(event->size());
	QWidget::resizeEvent(event);
}

void MediaWidget::wheelEvent(QWheelEvent *event)
{
	if (playing && mediaObject->isSeekable()) {
		qint64 time = mediaObject->currentTime() -
			(25 * shortSkipDuration * event->delta()) / 3;

		if (time < 0) {
			time = 0;
		}

		mediaObject->seek(time);
	}
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
