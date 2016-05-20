/*
 * mediawidget.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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
#include <QLabel>
#include <QPushButton>
#include <QStringListModel>
#include <QMimeData>
#include <QTimeEdit>
#include <QTimer>
#include <QX11Info>
#include <Solid/Block>
#include <Solid/Device>
#include <QWidgetAction>
#include <KActionCollection>
#include <KComboBox>
#include <KLocalizedString>
#include <QMenu>
#include <KToolBar>
#include <X11/extensions/scrnsaver.h>
#include <KConfigGroup>
#include <KSharedConfig>
#include "backend-vlc/vlcmediawidget.h"
#include "configuration.h"
#include "log.h"
#include "osdwidget.h"

MediaWidget::MediaWidget(QMenu *menu_, QToolBar *toolBar, KActionCollection *collection,
	QWidget *parent) : QWidget(parent), menu(menu_), displayMode(NormalMode),
	automaticResize(ResizeOff), blockBackendUpdates(false), muted(false),
	screenSaverSuspended(false), showElapsedTime(true)
{
	dummySource.reset(new MediaSource());
	source = dummySource.data();

	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);

	QPalette palette = QWidget::palette();
	palette.setColor(backgroundRole(), Qt::black);
	setPalette(palette);
	setAutoFillBackground(true);

	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);

	backend = VlcMediaWidget::createVlcMediaWidget(this);

	if (backend == NULL) {
		backend = new DummyMediaWidget(this);
	}

	backend->connectToMediaWidget(this);
	layout->addWidget(backend);
	osdWidget = new OsdWidget(this);

	actionPrevious = new QWidgetAction(this);
	actionPrevious->setIcon(QIcon::fromTheme(QLatin1String("media-skip-backward")));
	actionPrevious->setText(i18n("Previous"));
	actionPrevious->setShortcut(QKeySequence(Qt::Key_PageUp, Qt::Key_MediaPrevious));
	connect(actionPrevious, SIGNAL(triggered()), this, SLOT(previous()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_previous"), actionPrevious));
	menu->addAction(actionPrevious);

	actionPlayPause = new QWidgetAction(this);
	actionPlayPause->setShortcut(QKeySequence(Qt::Key_Space, Qt::Key_MediaPlay));
	textPlay = i18n("Play");
	textPause = i18n("Pause");
	iconPlay = QIcon::fromTheme(QLatin1String("media-playback-start"));
	iconPause = QIcon::fromTheme(QLatin1String("media-playback-pause"));
	connect(actionPlayPause, SIGNAL(triggered(bool)), this, SLOT(pausedChanged(bool)));
	toolBar->addAction(collection->addAction(QLatin1String("controls_play_pause"), actionPlayPause));
	menu->addAction(actionPlayPause);

	actionStop = new QWidgetAction(this);
	actionStop->setIcon(QIcon::fromTheme(QLatin1String("media-playback-stop")));
	actionStop->setText(i18n("Stop"));
	actionStop->setShortcut(QKeySequence(Qt::Key_Backspace, Qt::Key_MediaStop));
	connect(actionStop, SIGNAL(triggered()), this, SLOT(stop()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_stop"), actionStop));
	menu->addAction(actionStop);

	actionNext = new QWidgetAction(this);
	actionNext->setIcon(QIcon::fromTheme(QLatin1String("media-skip-forward")));
	actionNext->setText(i18n("Next"));
	actionNext->setShortcut(QKeySequence(Qt::Key_PageDown, Qt::Key_MediaNext));
	connect(actionNext, SIGNAL(triggered()), this, SLOT(next()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_next"), actionNext));
	menu->addAction(actionNext);
	menu->addSeparator();

	fullScreenAction = new QWidgetAction(this);
	fullScreenAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen")));
	fullScreenAction->setText(i18nc("'Playback' menu", "Full Screen Mode"));
	fullScreenAction->setShortcut(Qt::Key_F);
	connect(fullScreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));
	menu->addAction(collection->addAction(QLatin1String("view_fullscreen"), fullScreenAction));

	minimalModeAction = new QWidgetAction(this);
	minimalModeAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen")));
	minimalModeAction->setText(i18nc("'Playback' menu", "Minimal Mode"));
	minimalModeAction->setShortcut(Qt::Key_Period);
	connect(minimalModeAction, SIGNAL(triggered()), this, SLOT(toggleMinimalMode()));
	menu->addAction(collection->addAction(QLatin1String("view_minimal_mode"), minimalModeAction));

	audioStreamBox = new KComboBox(toolBar);
	connect(audioStreamBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(currentAudioStreamChanged(int)));
	toolBar->addWidget(audioStreamBox);

	audioStreamModel = new QStringListModel(toolBar);
	audioStreamBox->setModel(audioStreamModel);

	subtitleBox = new KComboBox(toolBar);
	textSubtitlesOff = i18nc("subtitle selection entry", "off");
	connect(subtitleBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(currentSubtitleChanged(int)));
	toolBar->addWidget(subtitleBox);

	subtitleModel = new QStringListModel(toolBar);
	subtitleBox->setModel(subtitleModel);

	QMenu *audioMenu = new QMenu(i18nc("'Playback' menu", "Audio"), this);

	QWidgetAction *action = new QWidgetAction(this);
	action->setIcon(QIcon::fromTheme(QLatin1String("audio-volume-high")));
	action->setText(i18nc("'Audio' menu", "Increase Volume"));
	action->setShortcut(QKeySequence(Qt::Key_Plus, Qt::Key_VolumeUp));
	connect(action, SIGNAL(triggered()), this, SLOT(increaseVolume()));
	audioMenu->addAction(collection->addAction(QLatin1String("controls_increase_volume"), action));

	action = new QWidgetAction(this);
	action->setIcon(QIcon::fromTheme(QLatin1String("audio-volume-low")));
	action->setText(i18nc("'Audio' menu", "Decrease Volume"));
	action->setShortcut(QKeySequence(Qt::Key_Minus, Qt::Key_VolumeDown));
	connect(action, SIGNAL(triggered()), this, SLOT(decreaseVolume()));
	audioMenu->addAction(collection->addAction(QLatin1String("controls_decrease_volume"), action));

	muteAction = new QWidgetAction(this);
	muteAction->setText(i18nc("'Audio' menu", "Mute Volume"));
	mutedIcon = QIcon::fromTheme(QLatin1String("audio-volume-muted"));
	unmutedIcon = QIcon::fromTheme(QLatin1String("audio-volume-medium"));
	muteAction->setIcon(unmutedIcon);
	muteAction->setShortcut(QKeySequence(Qt::Key_M, Qt::Key_VolumeMute));
	connect(muteAction, SIGNAL(triggered()), this, SLOT(mutedChanged()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_mute_volume"), muteAction));
	audioMenu->addAction(muteAction);
	menu->addMenu(audioMenu);

	QMenu *videoMenu = new QMenu(i18nc("'Playback' menu", "Video"), this);
	menu->addMenu(videoMenu);
	menu->addSeparator();

	deinterlaceAction = new QWidgetAction(this);
	deinterlaceAction->setIcon(QIcon::fromTheme(QLatin1String("format-justify-center")));
	deinterlaceAction->setText(i18nc("'Video' menu", "Deinterlace"));
	deinterlaceAction->setCheckable(true);
	deinterlaceAction->setChecked(
		KSharedConfig::openConfig()->group("MediaObject").readEntry("Deinterlace", true));
	deinterlaceAction->setShortcut(Qt::Key_I);
	connect(deinterlaceAction, SIGNAL(toggled(bool)), this, SLOT(deinterlacingChanged(bool)));
	backend->setDeinterlacing(deinterlaceAction->isChecked());
	videoMenu->addAction(collection->addAction(QLatin1String("controls_deinterlace"), deinterlaceAction));

	QMenu *aspectMenu = new QMenu(i18nc("'Video' menu", "Aspect Ratio"), this);
	QActionGroup *aspectGroup = new QActionGroup(this);
	connect(aspectGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(aspectRatioChanged(QAction*)));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "Automatic"));
	action->setCheckable(true);
	action->setChecked(true);
	action->setData(AspectRatioAuto);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_auto"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "Fit to Window"));
	action->setCheckable(true);
	action->setData(AspectRatioWidget);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_widget"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "4:3"));
	action->setCheckable(true);
	action->setData(AspectRatio4_3);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_4_3"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "16:9"));
	action->setCheckable(true);
	action->setData(AspectRatio16_9);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_16_9"), action));
	videoMenu->addMenu(aspectMenu);

	QMenu *autoResizeMenu = new QMenu(i18n("Automatic Resize"), this);
	QActionGroup *autoResizeGroup = new QActionGroup(this);
	// we need an event even if you select the currently selected item
	autoResizeGroup->setExclusive(false);
	connect(autoResizeGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(autoResizeTriggered(QAction*)));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("automatic resize", "Off"));
	action->setCheckable(true);
	action->setData(0);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_off"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("automatic resize", "Original Size"));
	action->setCheckable(true);
	action->setData(1);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_original"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("automatic resize", "Double Size"));
	action->setCheckable(true);
	action->setData(2);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));

	int autoResizeFactor =
		KSharedConfig::openConfig()->group("MediaObject").readEntry("AutoResizeFactor", 0);

	switch (autoResizeFactor) {
	case 1:
		automaticResize = OriginalSize;
		autoResizeGroup->actions().at(1)->setChecked(true);
		break;
	case 2:
		automaticResize = DoubleSize;
		autoResizeGroup->actions().at(2)->setChecked(true);
		break;
	default:
		automaticResize = ResizeOff;
		autoResizeGroup->actions().at(0)->setChecked(true);
		break;
	}

	videoMenu->addMenu(autoResizeMenu);

	action = new QWidgetAction(this);
	action->setText(i18n("Volume Slider"));
	volumeSlider = new QSlider(toolBar);
	volumeSlider->setFocusPolicy(Qt::NoFocus);
	volumeSlider->setOrientation(Qt::Horizontal);
	volumeSlider->setRange(0, 100);
	volumeSlider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	volumeSlider->setToolTip(action->text());
	volumeSlider->setValue(KSharedConfig::openConfig()->group("MediaObject").readEntry("Volume", 100));
	connect(volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged(int)));
	backend->setVolume(volumeSlider->value());
	action->setDefaultWidget(volumeSlider);
	toolBar->addAction(collection->addAction(QLatin1String("controls_volume_slider"), action));

	jumpToPositionAction = new QWidgetAction(this);
	jumpToPositionAction->setIcon(QIcon::fromTheme(QLatin1String("go-jump")));
	jumpToPositionAction->setText(i18nc("@action:inmenu", "Jump to Position..."));
	jumpToPositionAction->setShortcut(Qt::CTRL + Qt::Key_J);
	connect(jumpToPositionAction, SIGNAL(triggered()), this, SLOT(jumpToPosition()));
	menu->addAction(collection->addAction(QLatin1String("controls_jump_to_position"), jumpToPositionAction));

	navigationMenu = new QMenu(i18nc("playback menu", "Skip"), this);
	menu->addMenu(navigationMenu);
	menu->addSeparator();

	int shortSkipDuration = Configuration::instance()->getShortSkipDuration();
	int longSkipDuration = Configuration::instance()->getLongSkipDuration();
	connect(Configuration::instance(), SIGNAL(shortSkipDurationChanged(int)),
		this, SLOT(shortSkipDurationChanged(int)));
	connect(Configuration::instance(), SIGNAL(longSkipDurationChanged(int)),
		this, SLOT(longSkipDurationChanged(int)));

	longSkipBackwardAction = new QWidgetAction(this);
	longSkipBackwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-backward")));
	longSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward", longSkipDuration));
	longSkipBackwardAction->setShortcut(Qt::SHIFT + Qt::Key_Left);
	connect(longSkipBackwardAction, SIGNAL(triggered()), this, SLOT(longSkipBackward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_long_skip_backward"), longSkipBackwardAction));

	shortSkipBackwardAction = new QWidgetAction(this);
	shortSkipBackwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-backward")));
	shortSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward", shortSkipDuration));
	shortSkipBackwardAction->setShortcut(Qt::Key_Left);
	connect(shortSkipBackwardAction, SIGNAL(triggered()), this, SLOT(shortSkipBackward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_skip_backward"), shortSkipBackwardAction));

	shortSkipForwardAction = new QWidgetAction(this);
	shortSkipForwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-forward")));
	shortSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", shortSkipDuration));
	shortSkipForwardAction->setShortcut(Qt::Key_Right);
	connect(shortSkipForwardAction, SIGNAL(triggered()), this, SLOT(shortSkipForward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_skip_forward"), shortSkipForwardAction));

	longSkipForwardAction = new QWidgetAction(this);
	longSkipForwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-forward")));
	longSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", longSkipDuration));
	longSkipForwardAction->setShortcut(Qt::SHIFT + Qt::Key_Right);
	connect(longSkipForwardAction, SIGNAL(triggered()), this, SLOT(longSkipForward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_long_skip_forward"), longSkipForwardAction));

	toolBar->addAction(QIcon::fromTheme(QLatin1String("player-time")), i18n("Seek Slider"))->setEnabled(false);

	action = new QWidgetAction(this);
	action->setText(i18n("Seek Slider"));
	seekSlider = new SeekSlider(toolBar);
	seekSlider->setFocusPolicy(Qt::NoFocus);
	seekSlider->setOrientation(Qt::Horizontal);
	seekSlider->setToolTip(action->text());
	connect(seekSlider, SIGNAL(valueChanged(int)), this, SLOT(seek(int)));
	action->setDefaultWidget(seekSlider);
	toolBar->addAction(collection->addAction(QLatin1String("controls_position_slider"), action));

	menuAction = new QWidgetAction(this);
	menuAction->setIcon(QIcon::fromTheme(QLatin1String("media-optical-video")));
	menuAction->setText(i18nc("dvd navigation", "DVD Menu"));
	connect(menuAction, SIGNAL(triggered()), this, SLOT(toggleMenu()));
	menu->addAction(collection->addAction(QLatin1String("controls_toggle_menu"), menuAction));

	titleMenu = new QMenu(i18nc("dvd navigation", "Title"), this);
	titleGroup = new QActionGroup(this);
	connect(titleGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(currentTitleChanged(QAction*)));
	menu->addMenu(titleMenu);

	chapterMenu = new QMenu(i18nc("dvd navigation", "Chapter"), this);
	chapterGroup = new QActionGroup(this);
	connect(chapterGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(currentChapterChanged(QAction*)));
	menu->addMenu(chapterMenu);

	angleMenu = new QMenu(i18nc("dvd navigation", "Angle"), this);
	angleGroup = new QActionGroup(this);
	connect(angleGroup, SIGNAL(triggered(QAction*)), this,
		SLOT(currentAngleChanged(QAction*)));
	menu->addMenu(angleMenu);

	action = new QWidgetAction(this);
	action->setText(i18n("Switch between elapsed and remaining time display"));
	timeButton = new QPushButton(toolBar);
	timeButton->setFocusPolicy(Qt::NoFocus);
	timeButton->setToolTip(action->text());
	connect(timeButton, SIGNAL(clicked(bool)), this, SLOT(timeButtonClicked()));
	action->setDefaultWidget(timeButton);
	toolBar->addAction(collection->addAction(QLatin1String("controls_time_button"), action));

	QTimer *timer = new QTimer(this);
	timer->start(50000);
	connect(timer, SIGNAL(timeout()), this, SLOT(checkScreenSaver()));
}

MediaWidget::~MediaWidget()
{
	KSharedConfig::openConfig()->group("MediaObject").writeEntry("Volume", volumeSlider->value());
	KSharedConfig::openConfig()->group("MediaObject").writeEntry("Deinterlace",
		deinterlaceAction->isChecked());

	int autoResizeFactor = 0;

	switch (automaticResize) {
	case ResizeOff:
		autoResizeFactor = 0;
		break;
	case OriginalSize:
		autoResizeFactor = 1;
		break;
	case DoubleSize:
		autoResizeFactor = 2;
		break;
	}

	KSharedConfig::openConfig()->group("MediaObject").writeEntry("AutoResizeFactor", autoResizeFactor);
}

QString MediaWidget::extensionFilter()
{
	return QLatin1String(
		// generated from kaffeine.desktop's mime types
		"*.669 *.aac *.ac3 *.aif *.aifc *.aiff *.anim1 *.anim2 *.anim3 *.anim4 *.anim5 "
		"*.anim6 *.anim7 *.anim8 *.anim9 *.animj *.asf *.asx *.au *.avi *.bdm *.bdmv "
		"*.clpi *.cpi *.divx *.dv *.f4a *.f4b *.f4v *.flac *.flc *.fli *.flv *.it *.m15 "
		"*.m2t *.m2ts *.m3u *.m3u8 *.m4a *.m4b *.m4v *.med *.mka *.mkv *.mng *.mod *.moov "
		"*.mov *.mp+ *.mp2 *.mp3 *.mp4 *.mpc *.mpe *.mpeg *.mpg *.mpga *.mpl *.mpls *.mpp "
		"*.mtm *.mts *.nsv *.oga *.ogg *.ogm *.ogv *.ogx *.pls *.qt *.qtl *.qtvr *.ra "
		"*.ram *.rax *.rm *.rmj *.rmm *.rms *.rmvb *.rmx *.rv *.rvx *.s3m *.shn *.snd "
		"*.spx *.stm *.ts *.tta *.ult *.uni *.vlc *.vob *.voc *.wav *.wax *.wma *.wmv "
		"*.wmx *.wv *.wvp *.wvx *.xm *.xspf "
		// manual entries
		"*.kaffeine *.iso|") + i18nc("file filter", "Supported Media Files") +
		QLatin1String("\n*|") + i18nc("file filter", "All Files");
}

MediaWidget::DisplayMode MediaWidget::getDisplayMode() const
{
	return displayMode;
}

void MediaWidget::setDisplayMode(DisplayMode displayMode_)
{
	if (displayMode != displayMode_) {
		displayMode = displayMode_;

		switch (displayMode) {
		case NormalMode:
		case MinimalMode:
			fullScreenAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen")));
			fullScreenAction->setText(i18nc("'Playback' menu", "Full Screen Mode"));
			break;
		case FullScreenMode:
		case FullScreenReturnToMinimalMode:
			fullScreenAction->setIcon(QIcon::fromTheme(QLatin1String("view-restore")));
			fullScreenAction->setText(i18nc("'Playback' menu",
				"Exit Full Screen Mode"));
			break;
		}

		switch (displayMode) {
		case NormalMode:
		case FullScreenMode:
		case FullScreenReturnToMinimalMode:
			minimalModeAction->setIcon(QIcon::fromTheme(QLatin1String("view-restore")));
			minimalModeAction->setText(i18nc("'Playback' menu", "Minimal Mode"));
			break;
		case MinimalMode:
			minimalModeAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen")));
			minimalModeAction->setText(i18nc("'Playback' menu", "Exit Minimal Mode"));
			break;
		}

		emit displayModeChanged();
	}
}

void MediaWidget::play(MediaSource *source_)
{
	if (source != source_) {
		source->playbackStatusChanged(Idle);
		source = source_;

		if (source == NULL) {
			source = dummySource.data();
		}
	}

	source->setMediaWidget(this);
	backend->play(*source);
}

void MediaWidget::mediaSourceDestroyed(MediaSource *mediaSource)
{
	if (source == mediaSource) {
		source = dummySource.data();
	}
}

void MediaWidget::play(const QUrl &url, const QUrl &subtitleUrl)
{
	// FIXME mem-leak
	play(new MediaSourceUrl(url, subtitleUrl));
}

void MediaWidget::playAudioCd(const QString &device)
{
	QUrl devicePath;

	if (!device.isEmpty()) {
		devicePath = QUrl::fromLocalFile(device);
	} else {
		QList<Solid::Device> devices =
			Solid::Device::listFromQuery(QLatin1String("OpticalDisc.availableContent & 'Audio'"));

		if (!devices.isEmpty()) {
			Solid::Block *block = devices.first().as<Solid::Block>();

			if (block != NULL) {
				devicePath = QUrl::fromLocalFile(block->device());
			}
		}
	}

	// FIXME mem-leak
	play(new MediaSourceAudioCd(devicePath));
}

void MediaWidget::playVideoCd(const QString &device)
{
	QUrl devicePath;

	if (!device.isEmpty()) {
		devicePath = QUrl::fromLocalFile(device);
	} else {
		QList<Solid::Device> devices = Solid::Device::listFromQuery(
			QLatin1String("OpticalDisc.availableContent & 'VideoCd|SuperVideoCd'"));

		if (!devices.isEmpty()) {
			Solid::Block *block = devices.first().as<Solid::Block>();

			if (block != NULL) {
				devicePath = QUrl::fromLocalFile(block->device());
			}
		}
	}

	// FIXME mem-leak
	play(new MediaSourceVideoCd(devicePath));
}

void MediaWidget::playDvd(const QString &device)
{
	QUrl devicePath;

	if (!device.isEmpty()) {
		devicePath = QUrl::fromLocalFile(device);
	} else {
		QList<Solid::Device> devices =
			Solid::Device::listFromQuery(QLatin1String("OpticalDisc.availableContent & 'VideoDvd'"));

		if (!devices.isEmpty()) {
			Solid::Block *block = devices.first().as<Solid::Block>();

			if (block != NULL) {
				devicePath = QUrl::fromLocalFile(block->device());
			}
		}
	}

	// FIXME mem-leak
	play(new MediaSourceDvd(devicePath));
}

OsdWidget *MediaWidget::getOsdWidget()
{
	return osdWidget;
}

MediaWidget::PlaybackStatus MediaWidget::getPlaybackStatus() const
{
	return backend->getPlaybackStatus();
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
	source->replay();
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
	if (source->getType() == MediaSource::Url)
		emit playlistPrevious();
	source->previous();
}

void MediaWidget::next()
{
	if (source->getType() == MediaSource::Url)
		emit playlistNext();
	source->next();
}

void MediaWidget::stop()
{
	switch (backend->getPlaybackStatus()) {
	case Idle:
		break;
	case Playing:
	case Paused:
		osdWidget->showText(i18nc("osd", "Stopped"), 1500);
		break;
	}

	backend->stop();
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

void MediaWidget::checkScreenSaver()
{
	bool suspendScreenSaver = false;

	switch (backend->getPlaybackStatus()) {
	case Idle:
	case Paused:
		break;
	case Playing:
		suspendScreenSaver = isVisible();
		break;
	}

	if (suspendScreenSaver) {
		// KDE - Inhibit doesn't inhibit "lock screen after inactivity"
		QDBusInterface(QLatin1String("org.freedesktop.ScreenSaver"), QLatin1String("/ScreenSaver"),
			QLatin1String("org.freedesktop.ScreenSaver")).call(QDBus::NoBlock,
			QLatin1String("SimulateUserActivity"));

		// GNOME - Inhibit doesn't inhibit power management functions
		QDBusInterface(QLatin1String("org.gnome.ScreenSaver"), QLatin1String("/"),
			       QLatin1String("org.gnome.ScreenSaver")).
			call(QDBus::NoBlock, QLatin1String("SimulateUserActivity"));
	}

	if (screenSaverSuspended != suspendScreenSaver) {
		// X11 - needed if none of the above applications is running
		screenSaverSuspended = suspendScreenSaver;
		XScreenSaverSuspend(QX11Info::display(), suspendScreenSaver);
	}
}

void MediaWidget::mutedChanged()
{
	muted = !muted;
	backend->setMuted(muted);

	if (muted) {
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

void MediaWidget::toggleFullScreen()
{
	switch (displayMode) {
	case NormalMode:
		setDisplayMode(FullScreenMode);
		break;
	case FullScreenMode:
		setDisplayMode(NormalMode);
		break;
	case FullScreenReturnToMinimalMode:
		setDisplayMode(MinimalMode);
		break;
	case MinimalMode:
		setDisplayMode(FullScreenReturnToMinimalMode);
		break;
	}
}

void MediaWidget::toggleMinimalMode()
{
	switch (displayMode) {
	case NormalMode:
	case FullScreenMode:
	case FullScreenReturnToMinimalMode:
		setDisplayMode(MinimalMode);
		break;
	case MinimalMode:
		setDisplayMode(NormalMode);
		break;
	}
}

void MediaWidget::seek(int position)
{
	if (blockBackendUpdates) {
		return;
	}

	backend->seek(position);
}

void MediaWidget::deinterlacingChanged(bool deinterlacing)
{
	backend->setDeinterlacing(deinterlacing);

	if (deinterlacing) {
		osdWidget->showText(i18nc("osd message", "Deinterlacing On"), 1500);
	} else {
		osdWidget->showText(i18nc("osd message", "Deinterlacing Off"), 1500);
	}
}

void MediaWidget::aspectRatioChanged(QAction *action)
{
	bool ok;
	unsigned int aspectRatio_ = action->data().toInt(&ok);

	if (ok && aspectRatio_ <= AspectRatioWidget) {
		backend->setAspectRatio(static_cast<AspectRatio>(aspectRatio_));
		return;
	}

	Log("MediaWidget::aspectRatioChanged: internal error");
}

void MediaWidget::autoResizeTriggered(QAction *action)
{
	foreach (QAction *autoResizeAction, action->actionGroup()->actions()) {
		autoResizeAction->setChecked(autoResizeAction == action);
	}

	bool ok = false;
	int autoResizeFactor = action->data().toInt(&ok);

	if (ok) {
		switch (autoResizeFactor) {
		case 0:
			automaticResize = ResizeOff;
			return;
		case 1:
			automaticResize = OriginalSize;
			emit resizeToVideo(automaticResize);
			return;
		case 2:
			automaticResize = DoubleSize;
			emit resizeToVideo(automaticResize);
			return;
		}
	}

	Log("MediaWidget::autoResizeTriggered: internal error");
}

void MediaWidget::pausedChanged(bool paused)
{
	switch (backend->getPlaybackStatus()) {
	case Idle:
		source->replay();
		break;
	case Playing:
	case Paused:
		backend->setPaused(paused);
		break;
	}
}

void MediaWidget::timeButtonClicked()
{
	showElapsedTime = !showElapsedTime;
	currentTotalTimeChanged();
}

void MediaWidget::longSkipBackward()
{
	int longSkipDuration = Configuration::instance()->getLongSkipDuration();
	int currentTime = (backend->getCurrentTime() - 1000 * longSkipDuration);

	if (currentTime < 0) {
		currentTime = 0;
	}

	backend->seek(currentTime);
}

void MediaWidget::shortSkipBackward()
{
	int shortSkipDuration = Configuration::instance()->getShortSkipDuration();
	int currentTime = (backend->getCurrentTime() - 1000 * shortSkipDuration);

	if (currentTime < 0) {
		currentTime = 0;
	}

	backend->seek(currentTime);
}

void MediaWidget::shortSkipForward()
{
	int shortSkipDuration = Configuration::instance()->getShortSkipDuration();
	backend->seek(backend->getCurrentTime() + 1000 * shortSkipDuration);
}

void MediaWidget::longSkipForward()
{
	int longSkipDuration = Configuration::instance()->getLongSkipDuration();
	backend->seek(backend->getCurrentTime() + 1000 * longSkipDuration);
}

void MediaWidget::jumpToPosition()
{
	QDialog *dialog = new JumpToPositionDialog(this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void MediaWidget::currentAudioStreamChanged(int currentAudioStream)
{
	if (!blockBackendUpdates) {
		if (source->overrideAudioStreams()) {
			source->setCurrentAudioStream(currentAudioStream);
			return;
		}

		source->setCurrentAudioStream(currentAudioStream -
			backend->getAudioStreams().size());

		if (currentAudioStream >= backend->getAudioStreams().size()) {
			currentAudioStream = -1;
		}

		if (backend->getCurrentAudioStream() != currentAudioStream) {
			backend->setCurrentAudioStream(currentAudioStream);
		}
	}
}

void MediaWidget::currentSubtitleChanged(int currentSubtitle)
{
	if (!blockBackendUpdates) {
		--currentSubtitle;

		if (source->overrideSubtitles()) {
			source->setCurrentSubtitle(currentSubtitle);
			return;
		}

		source->setCurrentSubtitle(currentSubtitle - backend->getSubtitles().size());

		if (currentSubtitle >= backend->getSubtitles().size()) {
			currentSubtitle = -1;
		}

		if (backend->getCurrentSubtitle() != currentSubtitle) {
			backend->setCurrentSubtitle(currentSubtitle);
		}
	}
}

void MediaWidget::toggleMenu()
{
	backend->showDvdMenu();
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

void MediaWidget::shortSkipDurationChanged(int shortSkipDuration)
{
	shortSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward",
		shortSkipDuration));
	shortSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward",
		shortSkipDuration));
}

void MediaWidget::longSkipDurationChanged(int longSkipDuration)
{
	longSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward",
		longSkipDuration));
	longSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward",
		longSkipDuration));
}

void MediaWidget::audioStreamsChanged()
{
	QStringList items;
	int currentIndex;

	if (source->overrideAudioStreams()) {
		items = source->getAudioStreams();
		currentIndex = source->getCurrentAudioStream();
	} else {
		items = backend->getAudioStreams();
		currentIndex = backend->getCurrentAudioStream();
	}

	blockBackendUpdates = true;

	if (audioStreamModel->stringList() != items) {
		audioStreamModel->setStringList(items);
	}

	audioStreamBox->setCurrentIndex(currentIndex);
	audioStreamBox->setEnabled(items.size() > 1);
	blockBackendUpdates = false;
}

void MediaWidget::subtitlesChanged()
{
	QStringList items(textSubtitlesOff);
	int currentIndex;

	if (source->overrideSubtitles()) {
		items += source->getSubtitles();
		currentIndex = (source->getCurrentSubtitle() + 1);

		// automatically choose appropriate subtitle
		int selectedSubtitle = -1;

		if (currentIndex > 0) {
			selectedSubtitle = (backend->getSubtitles().size() - 1);
		}

		if (backend->getCurrentSubtitle() != selectedSubtitle) {
			backend->setCurrentSubtitle(selectedSubtitle);
		}
	} else {
		items += backend->getSubtitles();
		items += source->getSubtitles();
		currentIndex = (backend->getCurrentSubtitle() + 1);
		int currentSourceIndex = source->getCurrentSubtitle();

		if (currentSourceIndex >= 0) {
			currentIndex = (currentSourceIndex + backend->getSubtitles().size() + 1);
		}
	}

	blockBackendUpdates = true;

	if (subtitleModel->stringList() != items) {
		subtitleModel->setStringList(items);
	}

	subtitleBox->setCurrentIndex(currentIndex);
	subtitleBox->setEnabled(items.size() > 1);
	blockBackendUpdates = false;
}

void MediaWidget::currentTotalTimeChanged()
{
	int currentTime = backend->getCurrentTime();
	int totalTime = backend->getTotalTime();
	source->trackLengthChanged(totalTime);

	if (source->getType() == MediaSource::Url)
		emit playlistTrackLengthChanged(totalTime);

	if (source->hideCurrentTotalTime()) {
		currentTime = 0;
		totalTime = 0;
	}

	blockBackendUpdates = true;
	seekSlider->setRange(0, totalTime);
	seekSlider->setValue(currentTime);

	if (showElapsedTime) {
		timeButton->setText(QLatin1Char(' ') + QTime(0, 0).addMSecs(currentTime).toString());
	} else {
		int remainingTime = (totalTime - currentTime);
		timeButton->setText(QLatin1Char('-') + QTime(0, 0).addMSecs(remainingTime).toString());
	}

	blockBackendUpdates = false;
}

void MediaWidget::seekableChanged()
{
	bool seekable = (backend->isSeekable() && !source->hideCurrentTotalTime());
	seekSlider->setEnabled(seekable);
	navigationMenu->setEnabled(seekable);
	jumpToPositionAction->setEnabled(seekable);
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
		emit playlistUrlsDropped(mimeData->urls());
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
	int shortSkipDuration = Configuration::instance()->getShortSkipDuration();
	int currentTime =
		(backend->getCurrentTime() - ((25 * shortSkipDuration * event->delta()) / 3));

	if (currentTime < 0) {
		currentTime = 0;
	}

	backend->seek(currentTime);
}

void MediaWidget::playbackFinished()
{
	source->playbackFinished();
}

void MediaWidget::playbackStatusChanged()
{
	source->playbackStatusChanged(backend->getPlaybackStatus());
	bool playing = true;

	switch (backend->getPlaybackStatus()) {
	case Idle:
		actionPlayPause->setIcon(iconPlay);
		actionPlayPause->setText(textPlay);
		playing = false;
		break;
	case Playing:
		actionPlayPause->setIcon(iconPause);
		actionPlayPause->setText(textPause);
		osdWidget->showText(i18nc("osd", "Playing"), 1500);
		break;
	case Paused:
		actionPlayPause->setIcon(iconPlay);
		actionPlayPause->setText(textPlay);
		osdWidget->showText(i18nc("osd", "Paused"), 1500);
		break;
	}

	actionPlayPause->setCheckable(playing);
	actionPrevious->setEnabled(playing);
	actionStop->setEnabled(playing);
	actionNext->setEnabled(playing);
	timeButton->setEnabled(playing);
}

void MediaWidget::metadataChanged()
{
	QMap<MediaWidget::MetadataType, QString> metadata = backend->getMetadata();
	source->metadataChanged(metadata);

	if (source->getType() == MediaSource::Url)
		emit playlistTrackMetadataChanged(metadata);

	if (source->overrideCaption()) {
		emit changeCaption(source->getDefaultCaption());
		return;
	}

	QString caption = metadata.value(Title);
	QString artist = metadata.value(Artist);

	if (!caption.isEmpty() && !artist.isEmpty()) {
		caption += QLatin1Char(' ');
	}

	if (!artist.isEmpty()) {
		caption += QLatin1Char('(');
		caption += artist;
		caption += QLatin1Char(')');
	}

	if (caption.isEmpty()) {
		caption = source->getDefaultCaption();
	}

	if (!caption.isEmpty()) {
		osdWidget->showText(caption, 2500);
	}

	emit changeCaption(caption);
}

void MediaWidget::dvdMenuChanged()
{
	menuAction->setEnabled(backend->hasDvdMenu());
}

void MediaWidget::titlesChanged()
{
	int titleCount = backend->getTitleCount();
	int currentTitle = backend->getCurrentTitle();

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

		if ((currentTitle >= 1) && (currentTitle <= titleGroup->actions().count())) {
			titleGroup->actions().at(currentTitle - 1)->setChecked(true);
		} else if (titleGroup->checkedAction() != NULL) {
			titleGroup->checkedAction()->setChecked(false);
		}

		titleMenu->setEnabled(true);
	} else {
		titleMenu->setEnabled(false);
	}
}

void MediaWidget::chaptersChanged()
{
	int chapterCount = backend->getChapterCount();
	int currentChapter = backend->getCurrentChapter();

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

		if ((currentChapter >= 1) && (currentChapter <= chapterGroup->actions().count())) {
			chapterGroup->actions().at(currentChapter - 1)->setChecked(true);
		} else if (chapterGroup->checkedAction() != NULL) {
			chapterGroup->checkedAction()->setChecked(false);
		}

		chapterMenu->setEnabled(true);
	} else {
		chapterMenu->setEnabled(false);
	}
}

void MediaWidget::anglesChanged()
{
	int angleCount = backend->getAngleCount();
	int currentAngle = backend->getCurrentAngle();

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

		if ((currentAngle >= 1) && (currentAngle <= angleGroup->actions().count())) {
			angleGroup->actions().at(currentAngle - 1)->setChecked(true);
		} else if (angleGroup->checkedAction() != NULL) {
			angleGroup->checkedAction()->setChecked(false);
		}

		angleMenu->setEnabled(true);
	} else {
		angleMenu->setEnabled(false);
	}
}

void MediaWidget::videoSizeChanged()
{
	if (automaticResize != ResizeOff) {
		emit resizeToVideo(automaticResize);
	}
}

JumpToPositionDialog::JumpToPositionDialog(MediaWidget *mediaWidget_) : QDialog(mediaWidget_),
	mediaWidget(mediaWidget_)
{
	setWindowTitle(i18nc("@title:window", "Jump to Position"));

	QWidget *widget = new QWidget(this);
	QBoxLayout *layout = new QVBoxLayout(widget);

	layout->addWidget(new QLabel(i18n("Enter a position:")));

	timeEdit = new QTimeEdit(this);
	timeEdit->setDisplayFormat(QLatin1String("hh:mm:ss"));
	timeEdit->setTime(QTime().addMSecs(mediaWidget->getPosition()));
	layout->addWidget(timeEdit);

	timeEdit->setFocus();

	QVBoxLayout *mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(widget);
}

JumpToPositionDialog::~JumpToPositionDialog()
{
}

void JumpToPositionDialog::accept()
{
	mediaWidget->setPosition(QTime().msecsTo(timeEdit->time()));
	QDialog::accept();
}

void SeekSlider::mousePressEvent(QMouseEvent *event)
{
	int buttons = style()->styleHint(QStyle::SH_Slider_AbsoluteSetButtons);
	Qt::MouseButton button = static_cast<Qt::MouseButton>(buttons & (~(buttons - 1)));
	QMouseEvent modifiedEvent(event->type(), event->pos(), event->globalPos(), button,
		event->buttons() ^ event->button() ^ button, event->modifiers());
	QSlider::mousePressEvent(&modifiedEvent);
}
