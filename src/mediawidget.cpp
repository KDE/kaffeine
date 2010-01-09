/*
 * mediawidget.cpp
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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
#include "mediawidget_p.h"

#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDBusInterface>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSocketNotifier>
#include <QTimeEdit>
#include <QTimer>
#include <Solid/Block>
#include <Solid/Device>
#include <KAction>
#include <KActionCollection>
#include <KComboBox>
#include <KDebug>
#include <KLocalizedString>
#include <KMenu>
#include <KStandardDirs>
#include <KToolBar>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "backend-xine/xinemediawidget.h"
#include "osdwidget.h"

DvbFeed::DvbFeed(QObject *parent) : QObject(parent), timeShiftActive(false), ignoreStop(false),
	readFd(-1), writeFd(-1), notifier(NULL)
{
	QString fileName = KStandardDirs::locateLocal("appdata", "dvbpipe.m2t");
	QFile::remove(fileName);
	url = KUrl::fromLocalFile(fileName);
	url.setScheme("fifo");

	if (mkfifo(QFile::encodeName(fileName), 0600) != 0) {
		kError() << "mkfifo failed";
		return;
	}

	readFd = open(QFile::encodeName(fileName), O_RDONLY | O_NONBLOCK);

	if (readFd < 0) {
		kError() << "open failed";
		return;
	}

	writeFd = open(QFile::encodeName(fileName), O_WRONLY | O_NONBLOCK);

	if (writeFd < 0) {
		kError() << "open failed";
		close(readFd);
		readFd = -1;
	}

	notifier = new QSocketNotifier(writeFd, QSocketNotifier::Write);
	notifier->setEnabled(false);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(readyWrite()));
}

DvbFeed::~DvbFeed()
{
	endOfData();
}

KUrl DvbFeed::getUrl() const
{
	return url;
}

void DvbFeed::writeData(const QByteArray &data)
{
	if (writeFd >= 0) {
		buffers.append(data);

		if (!flush()) {
			notifier->setEnabled(true);
		}
	}
}

void DvbFeed::endOfData()
{
	if (writeFd >= 0) {
		notifier->setEnabled(false);
		close(writeFd);
		writeFd = -1;
	}

	if (readFd >= 0) {
		close(readFd);
		readFd = -1;
	}
}

void DvbFeed::readyWrite()
{
	if (flush()) {
		notifier->setEnabled(false);
	}
}

bool DvbFeed::flush()
{
	while (!buffers.isEmpty()) {
		const QByteArray &buffer = *buffers.constBegin();
		int bytesWritten = write(writeFd, buffer.constData(), buffer.size());

		if ((bytesWritten < 0) && (errno == EINTR)) {
			continue;
		}

		if (bytesWritten == buffer.size()) {
			buffers.removeFirst();
			continue;
		}

		if (bytesWritten > 0) {
			buffers.first().remove(0, bytesWritten);
		}

		break;
	}

	return buffers.isEmpty();
}

JumpToPositionDialog::JumpToPositionDialog(MediaWidget *mediaWidget_) : KDialog(mediaWidget_),
	mediaWidget(mediaWidget_)
{
	setCaption(i18n("Jump to Position"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *layout = new QVBoxLayout(widget);

	layout->addWidget(new QLabel(i18n("Enter a position:")));

	timeEdit = new QTimeEdit(this);
	timeEdit->setDisplayFormat("hh:mm:ss");
	timeEdit->setTime(QTime().addSecs(mediaWidget->getPosition() / 1000));
	layout->addWidget(timeEdit);

	timeEdit->setFocus();

	setMainWidget(widget);
}

JumpToPositionDialog::~JumpToPositionDialog()
{
}

void JumpToPositionDialog::accept()
{
	mediaWidget->setPosition(QTime().msecsTo(timeEdit->time()));
	KDialog::accept();
}

MediaWidget::MediaWidget(KMenu *menu_, KAction *fullScreenAction, KToolBar *toolBar,
	KActionCollection *collection, QWidget *parent) : QWidget(parent), menu(menu_),
	dvbFeed(NULL)
{
	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);

	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);

	QPalette palette = QWidget::palette();
	palette.setColor(backgroundRole(), Qt::black);
	setPalette(palette);
	setAutoFillBackground(true);

	backend = new XineMediaWidget(this);
	connect(backend, SIGNAL(playbackChanged(bool)), this, SLOT(playbackChanged(bool)));
	connect(backend, SIGNAL(seekableChanged(bool)), this, SLOT(seekableChanged(bool)));
	connect(backend, SIGNAL(totalTimeChanged(int)), this, SLOT(totalTimeChanged(int)));
	connect(backend, SIGNAL(currentTimeChanged(int)), this, SLOT(currentTimeChanged(int)));
	connect(backend, SIGNAL(metadataChanged()), this, SLOT(metadataChanged())); // FIXME
	connect(backend, SIGNAL(audioChannelsChanged(QStringList,int)),
		this, SLOT(audioChannelsChanged(QStringList,int)));
	connect(backend, SIGNAL(currentAudioChannelChanged(int)),
		this, SLOT(setCurrentAudioChannel(int)));
	connect(backend, SIGNAL(subtitlesChanged(QStringList,int)),
		this, SLOT(subtitlesChanged(QStringList,int)));
	connect(backend, SIGNAL(currentSubtitleChanged(int)),
		this, SLOT(setCurrentSubtitle(int)));
	connect(backend, SIGNAL(dvdPlaybackChanged(bool)), this, SLOT(dvdPlaybackChanged(bool)));
	connect(backend, SIGNAL(titlesChanged(int,int)), this, SLOT(titlesChanged(int,int)));
	connect(backend, SIGNAL(currentTitleChanged(int)), this, SLOT(setCurrentTitle(int)));
	connect(backend, SIGNAL(chaptersChanged(int,int)), this, SLOT(chaptersChanged(int,int)));
	connect(backend, SIGNAL(currentChapterChanged(int)),
		this, SLOT(setCurrentChapter(int)));
	connect(backend, SIGNAL(anglesChanged(int,int)), this, SLOT(anglesChanged(int,int)));
	connect(backend, SIGNAL(currentAngleChanged(int)), this, SLOT(setCurrentAngle(int)));
	connect(backend, SIGNAL(dvbPlaybackFinished()), this, SLOT(dvbPlaybackFinished()));
	connect(backend, SIGNAL(playbackFinished()), this, SLOT(playbackFinished()));
	layout->addWidget(backend);

	osdWidget = new OsdWidget(this);

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
	connect(actionPlayPause, SIGNAL(triggered(bool)), this, SLOT(pausedChanged(bool)));
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
	connect(autoResizeGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(autoResizeTriggered(QAction*)));

	KAction *action = new KAction(i18nc("automatic resize", "Off"), autoResizeGroup);
	action->setCheckable(true);
	action->setData(0);
	autoResizeMenu->addAction(collection->addAction("controls_autoresize_off", action));

	action = new KAction(i18nc("automatic resize", "Original Size"), autoResizeGroup);
	action->setCheckable(true);
	action->setData(1);
	autoResizeMenu->addAction(collection->addAction("controls_autoresize_original", action));

	action = new KAction(i18nc("automatic resize", "Double Size"), autoResizeGroup);
	action->setCheckable(true);
	action->setData(2);
	autoResizeMenu->addAction(collection->addAction("controls_autoresize_double", action));

	autoResizeFactor =
		KGlobal::config()->group("MediaObject").readEntry("AutoResizeFactor", 0);

	if ((autoResizeFactor < 0) || (autoResizeFactor > 2)) {
		autoResizeFactor = 0;
	}

	autoResizeGroup->actions().at(autoResizeFactor)->setChecked(true);

	menu->addMenu(autoResizeMenu);
	menu->addSeparator();

	audioChannelBox = new KComboBox(toolBar);
	audioChannelsReady = false;
	connect(audioChannelBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(currentAudioChannelChanged(int)));
	toolBar->addWidget(audioChannelBox);

	subtitleBox = new KComboBox(toolBar);
	textSubtitlesOff = i18nc("subtitle selection entry", "off");
	subtitlesReady = false;
	connect(subtitleBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(currentSubtitleChanged(int)));
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
	isMuted = false;
	connect(muteAction, SIGNAL(triggered()), this, SLOT(mutedChanged()));
	toolBar->addAction(collection->addAction("controls_mute_volume", muteAction));
	audioMenu->addAction(muteAction);

	menu->addMenu(audioMenu);
	menu->addSeparator();

	KMenu *aspectMenu = new KMenu(i18n("Aspect Ratio"), this);
	QActionGroup *aspectGroup = new QActionGroup(this);
	connect(aspectGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(aspectRatioChanged(QAction*)));

	action = new KAction(i18nc("aspect ratio", "Automatic"), aspectGroup);
	action->setCheckable(true);
	action->setChecked(true);
	action->setData(XineMediaWidget::AspectRatioAuto);
	aspectMenu->addAction(collection->addAction("controls_aspect_auto", action));

	action = new KAction(i18nc("aspect ratio", "Fit to Window"), aspectGroup);
	action->setCheckable(true);
	action->setData(XineMediaWidget::AspectRatioWidget);
	aspectMenu->addAction(collection->addAction("controls_aspect_widget", action));

	action = new KAction(i18nc("aspect ratio", "4:3"), aspectGroup);
	action->setCheckable(true);
	action->setData(XineMediaWidget::AspectRatio4_3);
	aspectMenu->addAction(collection->addAction("controls_aspect_4_3", action));

	action = new KAction(i18nc("aspect ratio", "16:9"), aspectGroup);
	action->setCheckable(true);
	action->setData(XineMediaWidget::AspectRatio16_9);
	aspectMenu->addAction(collection->addAction("controls_aspect_16_9", action));

	menu->addMenu(aspectMenu);
	menu->addSeparator();

	action = new KAction(i18n("Volume Slider"), this);
	volumeSlider = new QSlider(toolBar);
	volumeSlider->setFocusPolicy(Qt::NoFocus);
	volumeSlider->setOrientation(Qt::Horizontal);
	volumeSlider->setRange(0, 100);
	volumeSlider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	volumeSlider->setToolTip(action->text());
	volumeSlider->setValue(KGlobal::config()->group("MediaObject").readEntry("Volume", 100));
	connect(volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged(int)));
	backend->setVolume(volumeSlider->value());
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
	navigationMenu->addAction(
		collection->addAction("controls_long_skip_backward", longSkipBackwardAction));

	shortSkipBackwardAction = new KAction(KIcon("media-skip-backward"),
		i18nc("submenu of 'Skip'", "Skip %1s Backward", shortSkipDuration), this);
	shortSkipBackwardAction->setShortcut(Qt::Key_Left);
	connect(shortSkipBackwardAction, SIGNAL(triggered(bool)), this, SLOT(shortSkipBackward()));
	navigationMenu->addAction(
		collection->addAction("controls_skip_backward", shortSkipBackwardAction));

	shortSkipForwardAction = new KAction(KIcon("media-skip-forward"),
		i18nc("submenu of 'Skip'", "Skip %1s Forward", shortSkipDuration), this);
	shortSkipForwardAction->setShortcut(Qt::Key_Right);
	connect(shortSkipForwardAction, SIGNAL(triggered(bool)), this, SLOT(shortSkipForward()));
	navigationMenu->addAction(
		collection->addAction("controls_skip_forward", shortSkipForwardAction));

	longSkipForwardAction = new KAction(KIcon("media-skip-forward"),
		i18nc("submenu of 'Skip'", "Skip %1s Forward", longSkipDuration), this);
	longSkipForwardAction->setShortcut(Qt::SHIFT + Qt::Key_Right);
	connect(longSkipForwardAction, SIGNAL(triggered(bool)), this, SLOT(longSkipForward()));
	navigationMenu->addAction(
		collection->addAction("controls_long_skip_forward", longSkipForwardAction));
	menu->addMenu(navigationMenu);

	jumpToPositionAction = new KAction(KIcon("go-jump"), i18n("Jump to Position"), this);
	jumpToPositionAction->setShortcut(Qt::CTRL + Qt::Key_J);
	connect(jumpToPositionAction, SIGNAL(triggered(bool)), this, SLOT(jumpToPosition()));
	menu->addAction(collection->addAction("controls_jump_to_position", jumpToPositionAction));
	menu->addSeparator();

	toolBar->addAction(KIcon("player-time"), i18n("Seek Slider"))->setEnabled(false);

	action = new KAction(i18n("Seek Slider"), this);
	seekSlider = new SeekSlider(toolBar);
	seekSlider->setFocusPolicy(Qt::NoFocus);
	seekSlider->setOrientation(Qt::Horizontal);
	seekSlider->setToolTip(action->text());
	connect(seekSlider, SIGNAL(valueChanged(int)), backend, SLOT(seek(int)));
	action->setDefaultWidget(seekSlider);
	toolBar->addAction(collection->addAction("controls_position_slider", action));

	titleMenu = new KMenu(i18n("Title"), this);
	titleGroup = new QActionGroup(this);
	connect(titleGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(currentTitleChanged(QAction*)));
	menu->addMenu(titleMenu);

	chapterMenu = new KMenu(i18n("Chapter"), this);
	chapterGroup = new QActionGroup(this);
	connect(chapterGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(currentChapterChanged(QAction*)));
	menu->addMenu(chapterMenu);

	angleMenu = new KMenu(i18n("Angle"), this);
	angleGroup = new QActionGroup(this);
	connect(angleGroup, SIGNAL(triggered(QAction*)), this,
		SLOT(currentAngleChanged(QAction*)));
	menu->addMenu(angleMenu);

	action = new KAction(i18n("Switch between elapsed and remaining time display"), this);
	timeButton = new QPushButton(toolBar);
	timeButton->setFocusPolicy(Qt::NoFocus);
	timeButton->setToolTip(action->text());
	showElapsedTime = true;
	connect(timeButton, SIGNAL(clicked(bool)), this, SLOT(timeButtonClicked()));
	action->setDefaultWidget(timeButton);
	toolBar->addAction(collection->addAction("controls_time_button", action));

	QTimer *timer = new QTimer(this);
	timer->start(50000);
	connect(timer, SIGNAL(timeout()), this, SLOT(checkScreenSaver()));

	playbackChanged(false);
	seekableChanged(false);
	totalTimeChanged(0);
	currentTimeChanged(0);
	metadataChanged(); // FIXME
	audioChannelsChanged(QStringList(), -1);
	subtitlesChanged(QStringList(), -1);
	dvdPlaybackChanged(false);
	titlesChanged(0, -1);
	chaptersChanged(0, -1);
	anglesChanged(0, -1);
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
		"*.anim6 *.anim7 *.anim8 *.anim9 *.animj *.asf *.asx *.au *.avi *.divx *.dv "
		"*.flac *.flc *.fli *.flv *.it *.m15 *.m2t *.m3u *.m3u8 *.m4a *.m4b *.m4v *.med "
		"*.mka *.mkv *.mng *.mod *.moov *.mov *.mp+ *.mp2 *.mp3 *.mp4 *.mpc *.mpe *.mpeg "
		"*.mpg *.mpga *.mpp *.mtm *.nsv *.oga *.ogg *.ogm *.ogv *.ogx *.pls *.qt *.qtl "
		"*.qtvr *.ra *.ram *.rax *.rm *.rmj *.rmm *.rms *.rmvb *.rmx *.rv *.rvx *.s3m "
		"*.shn *.snd *.spx *.spx *.stm *.tta *.ult *.uni *.vlc *.vob *.voc *.wav *.wax "
		"*.wma *.wmv *.wmx *.wv *.wvp *.wvx *.xm *.xspf "
		// manual entries
		"*.kaffeine *.iso|") + i18nc("file filter", "Supported Media Files") +
		"\n*|" + i18nc("file filter", "All Files");
}

void MediaWidget::play(const KUrl &url)
{
	backend->playUrl(url);
}

void MediaWidget::playAudioCd()
{
	QList<Solid::Device> devices =
		Solid::Device::listFromQuery("OpticalDisc.availableContent & 'Audio'");
	QString deviceName;

	if (!devices.isEmpty()) {
		Solid::Block *block = devices.first().as<Solid::Block>();

		if (block != NULL) {
			deviceName = block->device();
		}
	}

	backend->playAudioCd(deviceName);
}

void MediaWidget::playVideoCd()
{
	QList<Solid::Device> devices = Solid::Device::listFromQuery(
		"OpticalDisc.availableContent & 'VideoCd|SuperVideoCd'");
	QString deviceName;

	if (!devices.isEmpty()) {
		Solid::Block *block = devices.first().as<Solid::Block>();

		if (block != NULL) {
			deviceName = block->device();
		}
	}

	backend->playVideoCd(deviceName);
}

void MediaWidget::playDvd()
{
	QList<Solid::Device> devices =
		Solid::Device::listFromQuery("OpticalDisc.availableContent & 'VideoDvd'");
	QString deviceName;

	if (!devices.isEmpty()) {
		Solid::Block *block = devices.first().as<Solid::Block>();

		if (block != NULL) {
			deviceName = block->device();
		}
	}

	backend->playDvd(deviceName);
}

OsdWidget *MediaWidget::getOsdWidget()
{
	return osdWidget;
}

void MediaWidget::playDvb(const QString &channelName)
{
	if (dvbFeed != NULL) {
		delete dvbFeed;
	}

	dvbFeed = new DvbFeed(this);
	backend->playDvb(dvbFeed->getUrl());

	if (!channelName.isEmpty()) {
		osdWidget->showText(channelName, 2500);
	}

	emit changeCaption(channelName);
}

void MediaWidget::writeDvbData(const QByteArray &data)
{
	dvbFeed->writeData(data);
}

void MediaWidget::updateDvbAudioChannels(const QStringList &audioChannels, int currentAudioChannel)
{
	dvbFeed->audioChannels = audioChannels;

	if (!audioChannels.isEmpty()) {
		audioChannelsReady = false;
		audioChannelBox->clear();
		audioChannelBox->addItems(audioChannels);
		audioChannelBox->setCurrentIndex(currentAudioChannel);
		audioChannelBox->setEnabled(audioChannelBox->count() > 1);
		audioChannelsReady = true;
	} else {
		audioChannelsChanged(backend->getAudioChannels(),
			backend->getCurrentAudioChannel());
	}
}

void MediaWidget::updateDvbSubtitles(const QStringList &subtitles, int currentSubtitle)
{
	dvbFeed->subtitles = subtitles;

	if (!subtitles.isEmpty()) {
		subtitlesReady = false;
		subtitleBox->clear();
		subtitleBox->addItem(textSubtitlesOff);
		subtitleBox->addItems(subtitles);
		subtitleBox->setCurrentIndex(currentSubtitle + 1);
		subtitleBox->setEnabled(subtitleBox->count() > 1);
		subtitlesReady = true;
	} else {
		subtitlesChanged(backend->getSubtitles(), backend->getCurrentSubtitle());
	}
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
	shortSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward",
		duration));
	shortSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", duration));
}

void MediaWidget::setLongSkipDuration(int duration)
{
	longSkipDuration = duration;
	longSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward", duration));
	longSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", duration));
}

bool MediaWidget::isPlaying() const
{
	return backend->isPlaying();
}

bool MediaWidget::isPaused() const
{
	return actionPlayPause->isChecked();
}

int MediaWidget::getVolume() const
{
	return volumeSlider->value();
}

int MediaWidget::getPosition() const
{
	return backend->getCurrentTime();
}

void MediaWidget::play()
{
	// FIXME not the best behaviour

	if (dvbFeed == NULL) {
		emit playlistPlay();
	}
}

void MediaWidget::togglePause()
{
	actionPlayPause->trigger();
}

void MediaWidget::setPosition(int position)
{
	backend->seek(position);
}

void MediaWidget::setVolume(int volume)
{
	// QSlider ensures that the value is within the range
	volumeSlider->setValue(volume);
}

void MediaWidget::toggleMuted()
{
	muteAction->trigger();
}

void MediaWidget::previous()
{
	if (dvbFeed != NULL) {
		emit previousDvbChannel();
	} else if (!backend->playPreviousTitle()) {
		emit playlistPrevious();
	}
}

void MediaWidget::next()
{
	if (dvbFeed != NULL) {
		emit nextDvbChannel();
	} else if (!backend->playNextTitle()) {
		emit playlistNext();
	}
}

void MediaWidget::stop()
{
	backend->stop();
	osdWidget->showText(i18nc("osd", "Stopped"), 1500);
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

void MediaWidget::playbackChanged(bool playing)
{
	if (playing) {
		actionPlayPause->setText(textPause);
		actionPlayPause->setIcon(iconPause);
		actionPlayPause->setCheckable(true);
		actionPrevious->setEnabled(true);
		actionStop->setEnabled(true);
		actionNext->setEnabled(true);
		timeButton->setEnabled(true);

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
		timeButton->setEnabled(false);
	}
}

void MediaWidget::seekableChanged(bool seekable)
{
	if (seekable) {
		seekSlider->setEnabled(true);
		navigationMenu->setEnabled(true);
		jumpToPositionAction->setEnabled(true);
	} else {
		seekSlider->setEnabled(false);
		navigationMenu->setEnabled(false);
		jumpToPositionAction->setEnabled(false);
	}
}

void MediaWidget::totalTimeChanged(int totalTime)
{
	seekSlider->setRange(0, totalTime);
}

void MediaWidget::currentTimeChanged(int currentTime)
{
	seekSlider->setValue(currentTime);
	updateTimeButton(currentTime);
}

void MediaWidget::metadataChanged()
{
/*
	if (dvbFeed != NULL) {
		return;
	}

	QString caption;

	if (backend->isPlaying()) {
		// FIXME include artist?
		QStringList strings; // FIXME = backend->metaData(Phonon::TitleMetaData);

		if (!strings.isEmpty() && !strings.at(0).isEmpty()) {
			caption = strings.at(0);
		} else {
			// caption = KUrl(backend->currentSource().url()).fileName();
			// FIXME
		}
	}

	if (!caption.isEmpty()) {
		osdWidget->showText(caption, 2500);
	}

	emit changeCaption(caption);
*/
}

void MediaWidget::audioChannelsChanged(const QStringList &audioChannels, int currentAudioChannel)
{
	if ((dvbFeed == NULL) || dvbFeed->audioChannels.isEmpty()) {
		audioChannelsReady = false;
		audioChannelBox->clear();
		audioChannelBox->addItems(audioChannels);
		audioChannelBox->setCurrentIndex(currentAudioChannel);
		audioChannelBox->setEnabled(audioChannelBox->count() > 1);
		audioChannelsReady = true;
	}
}

void MediaWidget::setCurrentAudioChannel(int currentAudioChannel)
{
	if ((dvbFeed == NULL) || dvbFeed->audioChannels.isEmpty()) {
		audioChannelBox->setCurrentIndex(currentAudioChannel);
	}
}

void MediaWidget::subtitlesChanged(const QStringList &subtitles, int currentSubtitle)
{
	if ((dvbFeed == NULL) || dvbFeed->subtitles.isEmpty()) {
		subtitlesReady = false;
		subtitleBox->clear();
		subtitleBox->addItem(textSubtitlesOff);
		subtitleBox->addItems(subtitles);
		subtitleBox->setCurrentIndex(currentSubtitle + 1);
		subtitleBox->setEnabled(subtitleBox->count() > 1);
		subtitlesReady = true;
	} else {
		if (!subtitles.isEmpty()) {
			backend->setCurrentSubtitle(0);
		}
	}
}

void MediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	if ((dvbFeed == NULL) || dvbFeed->subtitles.isEmpty()) {
		subtitleBox->setCurrentIndex(currentSubtitle + 1);
	}
}

void MediaWidget::dvdPlaybackChanged(bool playingDvd)
{
	// FIXME dvd menu button

	if (playingDvd) {
		titleMenu->setVisible(true);
		chapterMenu->setVisible(true);
		angleMenu->setVisible(true);
	} else {
		titleMenu->setVisible(false);
		chapterMenu->setVisible(false);
		angleMenu->setVisible(false);
	}
}

void MediaWidget::titlesChanged(int titleCount, int currentTitle)
{
	if (titleCount > 1) {
		QList<QAction *> actions = titleGroup->actions();

		if (actions.count() < titleCount) {
			int i = actions.count();
			actions.clear();

			for (; i < titleCount; ++i) {
				QAction *action = titleGroup->addAction(QString::number(i + 1));
				action->setCheckable(true);
				titleMenu->addAction(action);
			}

			actions = titleGroup->actions();
		}

		for (int i = 0; i < actions.size(); ++i) {
			actions.at(i)->setVisible(i < titleCount);
		}

		setCurrentTitle(currentTitle);
		titleMenu->setEnabled(true);
	} else {
		titleMenu->setEnabled(false);
	}
}

void MediaWidget::setCurrentTitle(int currentTitle)
{
	if ((currentTitle >= 1) && (currentTitle <= titleGroup->actions().count())) {
		titleGroup->actions().at(currentTitle - 1)->setChecked(true);
	} else if (titleGroup->checkedAction() != NULL) {
		titleGroup->checkedAction()->setChecked(false);
	}
}

void MediaWidget::chaptersChanged(int chapterCount, int currentChapter)
{
	if (chapterCount > 1) {
		QList<QAction *> actions = chapterGroup->actions();

		if (actions.count() < chapterCount) {
			int i = actions.count();
			actions.clear();

			for (; i < chapterCount; ++i) {
				QAction *action = chapterGroup->addAction(QString::number(i + 1));
				action->setCheckable(true);
				chapterMenu->addAction(action);
			}

			actions = chapterGroup->actions();
		}

		for (int i = 0; i < actions.size(); ++i) {
			actions.at(i)->setVisible(i < chapterCount);
		}

		setCurrentChapter(currentChapter);
		chapterMenu->setEnabled(true);
	} else {
		chapterMenu->setEnabled(false);
	}
}

void MediaWidget::setCurrentChapter(int currentChapter)
{
	if ((currentChapter >= 1) && (currentChapter <= chapterGroup->actions().count())) {
		chapterGroup->actions().at(currentChapter - 1)->setChecked(true);
	} else if (chapterGroup->checkedAction() != NULL) {
		chapterGroup->checkedAction()->setChecked(false);
	}
}

void MediaWidget::anglesChanged(int angleCount, int currentAngle)
{
	if (angleCount > 1) {
		QList<QAction *> actions = angleGroup->actions();

		if (actions.count() < angleCount) {
			int i = actions.count();
			actions.clear();

			for (; i < angleCount; ++i) {
				QAction *action = angleGroup->addAction(QString::number(i + 1));
				action->setCheckable(true);
				angleMenu->addAction(action);
			}

			actions = angleGroup->actions();
		}

		for (int i = 0; i < actions.size(); ++i) {
			actions.at(i)->setVisible(i < angleCount);
		}

		setCurrentAngle(currentAngle);
		angleMenu->setEnabled(true);
	} else {
		angleMenu->setEnabled(false);
	}
}

void MediaWidget::setCurrentAngle(int currentAngle)
{
	if ((currentAngle >= 1) && (currentAngle <= angleGroup->actions().count())) {
		angleGroup->actions().at(currentAngle - 1)->setChecked(true);
	} else if (angleGroup->checkedAction() != NULL) {
		angleGroup->checkedAction()->setChecked(false);
	}
}

void MediaWidget::dvbPlaybackFinished()
{
	if ((dvbFeed != NULL) && !dvbFeed->ignoreStop) {
		delete dvbFeed;
		dvbFeed = NULL;
		audioChannelsChanged(QStringList(), -1);
		subtitlesChanged(QStringList(), -1);
		emit dvbStopped();
	}
}

void MediaWidget::playbackFinished()
{
	if ((dvbFeed != NULL) && dvbFeed->timeShiftActive) {
		dvbFeed->ignoreStop = true;
		emit startDvbTimeShift();
		dvbFeed->ignoreStop = false;
	} else {
		emit playlistNext();
	}
}

void MediaWidget::checkScreenSaver()
{
	if (backend->isPlaying() && !isPaused()) {
		// FIXME check whether there's video or not
		// FIXME DPMS
		QDBusInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
			"org.freedesktop.ScreenSaver").
			call(QDBus::NoBlock, "SimulateUserActivity");
	}
}

void MediaWidget::mutedChanged()
{
	isMuted = !isMuted;
	backend->setMuted(isMuted);

	if (isMuted) {
		muteAction->setIcon(mutedIcon);
		osdWidget->showText(i18nc("osd", "Mute On"), 1500);
	} else {
		muteAction->setIcon(unmutedIcon);
		osdWidget->showText(i18nc("osd", "Mute Off"), 1500);
	}
}

void MediaWidget::volumeChanged(int volume)
{
	backend->setVolume(volume);
	osdWidget->showText(i18nc("osd", "Volume: %1%", volume), 1500);
}

void MediaWidget::aspectRatioChanged(QAction *action)
{
	bool ok;
	unsigned int aspectRatio_ = action->data().toInt(&ok);

	if (ok && aspectRatio_ <= XineMediaWidget::AspectRatioWidget) {
		backend->setAspectRatio(static_cast<XineMediaWidget::AspectRatio>(aspectRatio_));
		return;
	}

	kError() << "internal error" << action->data();
}

void MediaWidget::autoResizeTriggered(QAction *action)
{
	foreach (QAction *autoResizeAction, action->actionGroup()->actions()) {
		autoResizeAction->setChecked(autoResizeAction == action);
	}

	bool ok;
	unsigned int autoResizeFactor_ = action->data().toInt(&ok);

	if (ok && (autoResizeFactor_ <= 2)) {
		autoResizeFactor = autoResizeFactor_;

		if (autoResizeFactor > 0) {
			emit resizeToVideo(autoResizeFactor);
		}

		return;
	}

	kError() << "internal error" << action->data();
}

void MediaWidget::pausedChanged(bool paused)
{
	if (backend->isPlaying()) {
		if (paused) {
			actionPlayPause->setIcon(iconPlay);
			actionPlayPause->setText(textPlay);
			osdWidget->showText(i18nc("osd", "Paused"), 1500);
			backend->setPaused(true);

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
				backend->setPaused(false);
			}
		}
	} else {
		emit playlistPlay();
	}
}

void MediaWidget::timeButtonClicked()
{
	showElapsedTime = !showElapsedTime;
	updateTimeButton(backend->getCurrentTime());
}

void MediaWidget::longSkipBackward()
{
	int time = backend->getCurrentTime() - 1000 * longSkipDuration;

	if (time < 0) {
		time = 0;
	}

	backend->seek(time);
}

void MediaWidget::shortSkipBackward()
{
	int time = backend->getCurrentTime() - 1000 * shortSkipDuration;

	if (time < 0) {
		time = 0;
	}

	backend->seek(time);
}

void MediaWidget::shortSkipForward()
{
	backend->seek(backend->getCurrentTime() + 1000 * shortSkipDuration);
}

void MediaWidget::longSkipForward()
{
	backend->seek(backend->getCurrentTime() + 1000 * longSkipDuration);
}

void MediaWidget::jumpToPosition()
{
	KDialog *dialog = new JumpToPositionDialog(this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void MediaWidget::currentAudioChannelChanged(int currentAudioChannel)
{
	if (audioChannelsReady) {
		if ((dvbFeed == NULL) || dvbFeed->audioChannels.isEmpty()) {
			backend->setCurrentAudioChannel(currentAudioChannel);
		} else {
			emit changeDvbAudioChannel(currentAudioChannel);
		}
	}
}

void MediaWidget::currentSubtitleChanged(int currentSubtitle)
{
	if (subtitlesReady) {
		if ((dvbFeed == NULL) || dvbFeed->subtitles.isEmpty()) {
			backend->setCurrentSubtitle(currentSubtitle - 1);
		} else {
			emit changeDvbSubtitle(currentSubtitle - 1);
		}
	}
}

void MediaWidget::currentTitleChanged(QAction *action)
{
	backend->setCurrentTitle(titleGroup->actions().indexOf(action) + 1);
}

void MediaWidget::currentChapterChanged(QAction *action)
{
	backend->setCurrentChapter(chapterGroup->actions().indexOf(action) + 1);
}

void MediaWidget::currentAngleChanged(QAction *action)
{
	backend->setCurrentAngle(angleGroup->actions().indexOf(action) + 1);
}

void MediaWidget::updateTimeButton(int currentTime)
{
	if (showElapsedTime) {
		timeButton->setText(' ' + QTime(0, 0).addMSecs(currentTime).toString());
	} else {
		int remainingTime = backend->getTotalTime() - currentTime;
		timeButton->setText('-' + QTime(0, 0).addMSecs(remainingTime).toString());
	}
}

void MediaWidget::contextMenuEvent(QContextMenuEvent *event)
{
	menu->popup(event->globalPos());
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	Q_UNUSED(event)
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
	qint64 time = backend->getCurrentTime() - (25 * shortSkipDuration * event->delta()) / 3;

	if (time < 0) {
		time = 0;
	}

	backend->seek(time);
}
