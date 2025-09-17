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

#include "log.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KToolBar>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QDBusInterface>
#include <QEvent>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QStringListModel>
#include <QTimeEdit>
#include <QTimer>
#include <QWidgetAction>
#include <QActionGroup>

#include <Solid/Block>
#include <Solid/Device>
#include <KWindowSystem>

#include "backend-vlc/vlcmediawidget.h"
#include "configuration.h"
#include "mediawidget.h"
#include "mediawidget_p.h"
#include "osdwidget.h"

// Needs to be included before X11 headers, which define some things like "Bool"
#include "moc_mediawidget.cpp"

#include <X11/extensions/scrnsaver.h>

MediaWidget::MediaWidget(QMenu *menu_, QToolBar *toolBar, KActionCollection *collection,
	QWidget *parent) : QWidget(parent), menu(menu_), displayMode(NormalMode),
	autoResizeFactor(0), blockBackendUpdates(false), muted(false),
	screenSaverSuspended(false), showElapsedTime(true)
{
	dummySource.reset(new MediaSource());
	source = dummySource.data();

	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

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
	actionPrevious->setIcon(QIcon::fromTheme(QLatin1String("media-skip-backward"), QIcon(":media-skip-backward")));
	actionPrevious->setText(i18n("Previous"));
	actionPrevious->setShortcut(Qt::Key_PageUp);
	connect(actionPrevious, SIGNAL(triggered()), this, SLOT(previous()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_previous"), actionPrevious));
	menu->addAction(actionPrevious);

	actionPlayPause = new QWidgetAction(this);
	actionPlayPause->setShortcut(Qt::Key_Space);
	textPlay = i18n("Play");
	textPause = i18n("Pause");
	iconPlay = QIcon::fromTheme(QLatin1String("media-playback-start"), QIcon(":media-playback-start"));
	iconPause = QIcon::fromTheme(QLatin1String("media-playback-pause"), QIcon(":media-playback-pause"));
	connect(actionPlayPause, SIGNAL(triggered(bool)), this, SLOT(pausedChanged(bool)));
	toolBar->addAction(collection->addAction(QLatin1String("controls_play_pause"), actionPlayPause));
	menu->addAction(actionPlayPause);

	actionStop = new QWidgetAction(this);
	actionStop->setIcon(QIcon::fromTheme(QLatin1String("media-playback-stop"), QIcon(":media-playback-stop")));
	actionStop->setText(i18n("Stop"));
	actionStop->setShortcut(Qt::Key_Backspace);
	connect(actionStop, SIGNAL(triggered()), this, SLOT(stop()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_stop"), actionStop));
	menu->addAction(actionStop);

	actionNext = new QWidgetAction(this);
	actionNext->setIcon(QIcon::fromTheme(QLatin1String("media-skip-forward"), QIcon(":media-skip-forward")));
	actionNext->setText(i18n("Next"));
	actionNext->setShortcut(Qt::Key_PageDown);
	connect(actionNext, SIGNAL(triggered()), this, SLOT(next()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_next"), actionNext));
	menu->addAction(actionNext);
	menu->addSeparator();

	fullScreenAction = new QWidgetAction(this);
	fullScreenAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen"), QIcon(":view-fullscreen")));
	fullScreenAction->setText(i18nc("'Playback' menu", "Full Screen Mode"));
	fullScreenAction->setShortcut(Qt::Key_F);
	connect(fullScreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));
	menu->addAction(collection->addAction(QLatin1String("view_fullscreen"), fullScreenAction));

	minimalModeAction = new QWidgetAction(this);
	minimalModeAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen"), QIcon(":view-fullscreen")));
	minimalModeAction->setText(i18nc("'Playback' menu", "Minimal Mode"));
	minimalModeAction->setShortcut(Qt::Key_Period);
	connect(minimalModeAction, SIGNAL(triggered()), this, SLOT(toggleMinimalMode()));
	menu->addAction(collection->addAction(QLatin1String("view_minimal_mode"), minimalModeAction));

	audioStreamBox = new QComboBox(toolBar);
	connect(audioStreamBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(currentAudioStreamChanged(int)));
	toolBar->addWidget(audioStreamBox);

	audioStreamModel = new QStringListModel(toolBar);
	audioStreamBox->setModel(audioStreamModel);

	QMenu *subtitleMenu = new QMenu(i18nc("'Subtitle' menu", "Subtitle"), this);
	subtitleBox = new QComboBox(this);
	QWidgetAction *action = new QWidgetAction(this);
	action->setDefaultWidget(subtitleBox);
	textSubtitlesOff = i18nc("subtitle selection entry", "off");
	connect(subtitleBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(currentSubtitleChanged(int)));
	subtitleModel = new QStringListModel(toolBar);
	subtitleBox->setModel(subtitleModel);
	subtitleMenu->addAction(action);
	menu->addMenu(subtitleMenu);
	action = new QWidgetAction(this);
	action->setText(i18nc("'Subtitle' menu", "Add subtitle file"));
	action->setIcon(QIcon::fromTheme(QLatin1String("application-x-subrip"), QIcon(":application-x-subrip")));
	connect(action, &QWidgetAction::triggered, this, &MediaWidget::openSubtitle);
	subtitleMenu->addAction(action);

	menu->addMenu(subtitleMenu);

	QMenu *audioMenu = new QMenu(i18nc("'Playback' menu", "Audio"), this);
	action = new QWidgetAction(this);
	action->setIcon(QIcon::fromTheme(QLatin1String("audio-card"), QIcon(":audio-card")));
	action->setText(i18nc("'Audio' menu", "Audio Device"));

	audioDevMenu = new QMenu(i18nc("'Playback' menu", "Audio Device"), audioMenu);
	action = new QWidgetAction(this);
	connect(audioDevMenu, &QMenu::aboutToShow, this, &MediaWidget::getAudioDevices);
	audioMenu->addMenu(audioDevMenu);

	action = new QWidgetAction(this);
	action->setIcon(QIcon::fromTheme(QLatin1String("audio-volume-high"), QIcon(":audio-volume-high")));
	action->setText(i18nc("'Audio' menu", "Increase Volume"));
	action->setShortcut(Qt::Key_Plus);
	connect(action, SIGNAL(triggered()), this, SLOT(increaseVolume()));
	audioMenu->addAction(collection->addAction(QLatin1String("controls_increase_volume"), action));

	action = new QWidgetAction(this);
	action->setIcon(QIcon::fromTheme(QLatin1String("audio-volume-low"), QIcon(":audio-volume-low")));
	action->setText(i18nc("'Audio' menu", "Decrease Volume"));
	action->setShortcut(Qt::Key_Minus);
	connect(action, SIGNAL(triggered()), this, SLOT(decreaseVolume()));
	audioMenu->addAction(collection->addAction(QLatin1String("controls_decrease_volume"), action));

	muteAction = new QWidgetAction(this);
	muteAction->setText(i18nc("'Audio' menu", "Mute Volume"));
	mutedIcon = QIcon::fromTheme(QLatin1String("audio-volume-muted"), QIcon(":audio-volume-muted"));
	unmutedIcon = QIcon::fromTheme(QLatin1String("audio-volume-medium"), QIcon(":audio-volume-medium"));
	muteAction->setIcon(unmutedIcon);
	muteAction->setShortcut(Qt::Key_M);
	connect(muteAction, SIGNAL(triggered()), this, SLOT(mutedChanged()));
	toolBar->addAction(collection->addAction(QLatin1String("controls_mute_volume"), muteAction));
	audioMenu->addAction(muteAction);
	menu->addMenu(audioMenu);

	QMenu *videoMenu = new QMenu(i18nc("'Playback' menu", "Video"), this);
	menu->addMenu(videoMenu);
	menu->addSeparator();

	QMenu *deinterlaceMenu = new QMenu(i18nc("'Video' menu", "Deinterlace"), this);
	deinterlaceMenu->setIcon(QIcon::fromTheme(QLatin1String("format-justify-center"), QIcon(":format-justify-center")));
	QActionGroup *deinterlaceGroup = new QActionGroup(this);
	connect(deinterlaceMenu, SIGNAL(triggered(QAction*)),
		this, SLOT(deinterlacingChanged(QAction*)));
	videoMenu->addMenu(deinterlaceMenu);

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "disabled"));
	action->setCheckable(true);
	action->setShortcut(Qt::Key_D);
	action->setData(DeinterlaceDisabled);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_disabled"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "discard"));
	action->setCheckable(true);
	action->setData(DeinterlaceDiscard);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_discard"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "bob"));
	action->setCheckable(true);
	action->setData(DeinterlaceBob);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_bob"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "linear"));
	action->setCheckable(true);
	action->setData(DeinterlaceLinear);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_linear"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "yadif"));
	action->setCheckable(true);
	action->setData(DeinterlaceYadif);
	action->setShortcut(Qt::Key_I);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_yadif"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "yadif2x"));
	action->setCheckable(true);
	action->setData(DeinterlaceYadif2x);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_yadif2x"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "phosphor"));
	action->setCheckable(true);
	action->setData(DeinterlacePhosphor);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_phosphor"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "x"));
	action->setCheckable(true);
	action->setData(DeinterlaceX);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_x"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "mean"));
	action->setCheckable(true);
	action->setData(DeinterlaceMean);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_mean"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "blend"));
	action->setCheckable(true);
	action->setData(DeinterlaceBlend);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_blend"), action));

	action = new QWidgetAction(deinterlaceGroup);
	action->setText(i18nc("'Deinterlace' menu", "Inverse telecine"));
	action->setCheckable(true);
	action->setData(DeinterlaceIvtc);
	deinterlaceMenu->addAction(collection->addAction(QLatin1String("interlace_ivtc"), action));

	deinterlaceMode =
		KSharedConfig::openConfig()->group("MediaObject").readEntry("Deinterlace", 0);

	if (deinterlaceMode <= DeinterlaceIvtc) {
		for (int i = 0; i < deinterlaceGroup->actions().size(); i++) {
			if (deinterlaceGroup->actions().at(i)->data().toInt() == deinterlaceMode) {
				deinterlaceGroup->actions().at(i)->setChecked(true);
				break;
			}
		}
	} else {
		deinterlaceGroup->actions().at(0)->setChecked(true);
	}
	backend->setDeinterlacing(static_cast<DeinterlaceMode>(deinterlaceMode));

	QMenu *aspectMenu = new QMenu(i18nc("'Video' menu", "Aspect Ratio"), this);
	QActionGroup *aspectGroup = new QActionGroup(this);
	connect(aspectGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(aspectRatioChanged(QAction*)));
	videoMenu->addMenu(aspectMenu);

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "Automatic"));
	action->setCheckable(true);
	action->setData(AspectRatioAuto);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_auto"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "1:1"));
	action->setCheckable(true);
	action->setData(AspectRatio1_1);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_1_1"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "4:3"));
	action->setCheckable(true);
	action->setData(AspectRatio4_3);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_4_3"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "5:4"));
	action->setCheckable(true);
	action->setData(AspectRatio5_4);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_5_4"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "16:9"));
	action->setCheckable(true);
	action->setData(AspectRatio16_9);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_16_9"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "16:10"));
	action->setCheckable(true);
	action->setData(AspectRatio16_10);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_16_10"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "2.21:1"));
	action->setCheckable(true);
	action->setData(AspectRatio221_100);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_221_100"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "2.35:1"));
	action->setCheckable(true);
	action->setData(AspectRatio235_100);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_235_100"), action));

	action = new QWidgetAction(aspectGroup);
	action->setText(i18nc("'Aspect Ratio' menu", "2.39:1"));
	action->setCheckable(true);
	action->setData(AspectRatio239_100);
	aspectMenu->addAction(collection->addAction(QLatin1String("controls_aspect_239_100"), action));

	QMenu *autoResizeMenu = new QMenu(i18n("Video size"), this);
	QActionGroup *autoResizeGroup = new QActionGroup(this);
	// we need an event even if you select the currently selected item
	autoResizeGroup->setExclusive(false);
	connect(autoResizeGroup, SIGNAL(triggered(QAction*)),
		this, SLOT(autoResizeTriggered(QAction*)));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "Automatic"));
	action->setCheckable(true);
	action->setData(0);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_off"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "25%"));
	action->setCheckable(true);
	action->setData(25);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "50%"));
	action->setCheckable(true);
	action->setData(50);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));


	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "75%"));
	action->setCheckable(true);
	action->setData(75);

	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));
	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "Original Size"));
	action->setCheckable(true);
	action->setData(100);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_original"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "150%"));
	action->setCheckable(true);
	action->setData(150);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "200%"));
	action->setCheckable(true);
	action->setData(200);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "250%"));
	action->setCheckable(true);
	action->setData(250);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));

	action = new QWidgetAction(autoResizeGroup);
	action->setText(i18nc("Video size", "300%"));
	action->setCheckable(true);
	action->setData(300);
	autoResizeMenu->addAction(collection->addAction(QLatin1String("controls_autoresize_double"), action));

	autoResizeFactor =
		KSharedConfig::openConfig()->group("MediaObject").readEntry("AutoResizeFactor", 0);

	if (autoResizeFactor <= 300) {
		for (int i = 0; i < autoResizeGroup->actions().size(); i++) {
			if (autoResizeGroup->actions().at(i)->data().toInt() == autoResizeFactor) {
				autoResizeGroup->actions().at(i)->setChecked(true);
				break;
			}
		}
	} else {
		autoResizeGroup->actions().at(0)->setChecked(true);
	}
	setVideoSize();

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
	jumpToPositionAction->setIcon(QIcon::fromTheme(QLatin1String("go-jump"), QIcon(":go-jump")));
	jumpToPositionAction->setText(i18nc("@action:inmenu", "Jump to Position..."));
	jumpToPositionAction->setShortcut(Qt::CTRL | Qt::Key_J);
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
	longSkipBackwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-backward"), QIcon(":media-skip-backward")));
	// xgettext:no-c-format
	longSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward", longSkipDuration));
	longSkipBackwardAction->setShortcut(Qt::SHIFT | Qt::Key_Left);
	connect(longSkipBackwardAction, SIGNAL(triggered()), this, SLOT(longSkipBackward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_long_skip_backward"), longSkipBackwardAction));

	shortSkipBackwardAction = new QWidgetAction(this);
	shortSkipBackwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-backward"), QIcon(":media-skip-backward")));
	// xgettext:no-c-format
	shortSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward", shortSkipDuration));
	shortSkipBackwardAction->setShortcut(Qt::Key_Left);
	connect(shortSkipBackwardAction, SIGNAL(triggered()), this, SLOT(shortSkipBackward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_skip_backward"), shortSkipBackwardAction));

	shortSkipForwardAction = new QWidgetAction(this);
	shortSkipForwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-forward"), QIcon(":media-skip-forward")));
	// xgettext:no-c-format
	shortSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", shortSkipDuration));
	shortSkipForwardAction->setShortcut(Qt::Key_Right);
	connect(shortSkipForwardAction, SIGNAL(triggered()), this, SLOT(shortSkipForward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_skip_forward"), shortSkipForwardAction));

	longSkipForwardAction = new QWidgetAction(this);
	longSkipForwardAction->setIcon(QIcon::fromTheme(QLatin1String("media-skip-forward"), QIcon(":media-skip-forward")));
	// xgettext:no-c-format
	longSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward", longSkipDuration));
	longSkipForwardAction->setShortcut(Qt::SHIFT | Qt::Key_Right);
	connect(longSkipForwardAction, SIGNAL(triggered()), this, SLOT(longSkipForward()));
	navigationMenu->addAction(
		collection->addAction(QLatin1String("controls_long_skip_forward"), longSkipForwardAction));

	toolBar->addAction(QIcon::fromTheme(QLatin1String("player-time"), QIcon(":player-time")), i18n("Seek Slider"))->setEnabled(false);

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
	menuAction->setIcon(QIcon::fromTheme(QLatin1String("media-optical-video"), QIcon(":media-optical-video")));
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

	// Set the play/pause icons accordingly
	playbackStatusChanged();
}

MediaWidget::~MediaWidget()
{
	// Ensure that the screen saver will be disabled
	checkScreenSaver(true);

	KSharedConfig::openConfig()->group("MediaObject").writeEntry("Volume", volumeSlider->value());
	KSharedConfig::openConfig()->group("MediaObject").writeEntry("Deinterlace", deinterlaceMode);
	KSharedConfig::openConfig()->group("MediaObject").writeEntry("AutoResizeFactor", autoResizeFactor);
}

QString MediaWidget::extensionFilter()
{
	return i18n(
		"Supported Media Files ("
		// generated from kaffeine.desktop's mime types
		"*.3ga *.3gp *.3gpp *.669 *.ac3 *.aif *.aiff *.anim1 *.anim2 *.anim3 *.anim4 "
		"*.anim5 *.anim6 *.anim7 *.anim8 *.anim9 *.animj *.asf *.asx *.au *.avf *.avi "
		"*.bdm *.bdmv *.clpi *.cpi *.divx *.dv *.f4a *.f4b *.f4v *.flac *.flc *.fli *.flv "
		"*.it *.lrv *.m15 *.m2t *.m2ts *.m3u *.m3u8 *.m4a *.m4b *.m4v *.med *.mka *.mkv "
		"*.mng *.mod *.moov *.mov *.mp+ *.mp2 *.mp3 *.mp4 *.mpc *.mpe *.mpeg *.mpg *.mpga "
		"*.mpl *.mpls *.mpp *.mtm *.mts *.nsv *.oga *.ogg *.ogm *.ogv *.ogx *.opus *.pls "
		"*.qt *.qtl *.qtvr *.ra *.ram *.rax *.rm *.rmj *.rmm *.rms *.rmvb *.rmx *.rp *.rv "
		"*.rvx *.s3m *.shn *.snd *.spl *.stm *.swf *.ts *.tta *.ult *.uni *.vdr *.vlc "
		"*.vob *.voc *.wav *.wax *.webm *.wma *.wmv *.wmx *.wv *.wvp *.wvx *.xm *.xspf "
		// manual entries
		"*.kaffeine *.iso);;"
		"All Files (*)");
}

QList<KFileFilter> MediaWidget::extensionFilters() {
	QList<KFileFilter> l = {
		KFileFilter { QStringLiteral("Media files"),
			QStringList(), QStringList{
			QStringLiteral("x-content/video-vcd"),
			QStringLiteral("x-content/video-svcd"),
			QStringLiteral("x-content/video-dvd"),
			QStringLiteral("x-content/audio-player"),
			QStringLiteral("x-content/audio-cdda"),
			QStringLiteral("video/x-ogm+ogg"),
			QStringLiteral("video/x-nsv"),
			QStringLiteral("video/x-ms-wmv"),
			QStringLiteral("video/x-mng"),
			QStringLiteral("video/x-matroska"),
			QStringLiteral("video/x-flv"),
			QStringLiteral("video/x-flic"),
			QStringLiteral("video/x-anim"),
			QStringLiteral("video/webm"),
			QStringLiteral("video/vnd.rn-realvideo"),
			QStringLiteral("video/vnd.avi"),
			QStringLiteral("video/quicktime"),
			QStringLiteral("video/ogg"),
			QStringLiteral("video/mpeg"),
			QStringLiteral("video/mp4"),
			QStringLiteral("video/mp2t"),
			QStringLiteral("video/dv"),
			QStringLiteral("inode/directory"),
			QStringLiteral("image/vnd.rn-realpix"),
			QStringLiteral("audio/x-xm"),
			QStringLiteral("audio/x-wavpack"),
			QStringLiteral("audio/x-vorbis+ogg"),
			QStringLiteral("audio/x-voc"),
			QStringLiteral("audio/x-tta"),
			QStringLiteral("audio/x-stm"),
			QStringLiteral("audio/x-scpls"),
			QStringLiteral("audio/x-s3m"),
			QStringLiteral("audio/x-pn-realaudio-plugin"),
			QStringLiteral("audio/x-musepack"),
			QStringLiteral("audio/x-ms-wma"),
			QStringLiteral("audio/x-ms-asx"),
			QStringLiteral("audio/x-mpegurl"),
			QStringLiteral("audio/x-mod"),
			QStringLiteral("audio/x-matroska"),
			QStringLiteral("audio/x-m4b"),
			QStringLiteral("audio/x-it"),
			QStringLiteral("audio/x-aiff"),
			QStringLiteral("audio/webm"),
			QStringLiteral("audio/vnd.wave"),
			QStringLiteral("audio/vnd.rn-realaudio"),
			QStringLiteral("audio/ogg"),
			QStringLiteral("audio/mpeg"),
			QStringLiteral("audio/mp4"),
			QStringLiteral("audio/flac"),
			QStringLiteral("audio/basic"),
			QStringLiteral("audio/ac3"),
			QStringLiteral("application/xspf+xml"),
			QStringLiteral("application/x-shorten"),
			QStringLiteral("application/x-quicktime-media-link"),
			QStringLiteral("application/x-matroska"),
			QStringLiteral("application/vnd.rn-realmedia"),
			QStringLiteral("application/vnd.ms-asf"),
			QStringLiteral("application/vnd.adobe.flash.movie"),
			QStringLiteral("application/ram"),
			QStringLiteral("application/ogg")
			} },
		// manual entries
		KFileFilter { QStringLiteral("Iso files"), QStringList { QStringLiteral("*.iso") }, QStringList() },
		KFileFilter { QStringLiteral("Kaffeine files"), QStringList { QStringLiteral("*.kaffeine") }, QStringList() },
		KFileFilter { QStringLiteral("All files"), QStringList { QStringLiteral("(*)") }, QStringList() }
	};
	return l;
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
			fullScreenAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen"), QIcon(":view-fullscreen")));
			fullScreenAction->setText(i18nc("'Playback' menu", "Full Screen Mode"));
			break;
		case FullScreenMode:
		case FullScreenReturnToMinimalMode:
			fullScreenAction->setIcon(QIcon::fromTheme(QLatin1String("view-restore"), QIcon(":view-restore")));
			fullScreenAction->setText(i18nc("'Playback' menu",
				"Exit Full Screen Mode"));
			break;
		}

		switch (displayMode) {
		case NormalMode:
		case FullScreenMode:
		case FullScreenReturnToMinimalMode:
			minimalModeAction->setIcon(QIcon::fromTheme(QLatin1String("view-restore"), QIcon(":view-restore")));
			minimalModeAction->setText(i18nc("'Playback' menu", "Minimal Mode"));
			break;
		case MinimalMode:
			minimalModeAction->setIcon(QIcon::fromTheme(QLatin1String("view-fullscreen"), QIcon(":view-fullscreen")));
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

void MediaWidget::openSubtitle()
{
	QString fname = QFileDialog::getOpenFileName(this,
			i18nc("@title:window", "Open subtitle"),".",
			i18n("Subtitles (*.cdg *.idx *.srt " \
					"*.sub *.utf *.ass " \
					"*.ssa *.aqt " \
					"*.jss *.psb " \
					"*.rt *.smi *.txt " \
					"*.smil *.stl *.usf " \
					"*.dks *.pjs *.mpl2 *.mks " \
					"*.vtt *.ttml *.dfxp"));

	setSubtitle(QUrl::fromLocalFile(fname));
}

void MediaWidget::setSubtitle(QUrl url)
{
	if (!url.isValid()) {
		return;
	}

	backend->setExternalSubtitle(url);
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

void MediaWidget::setVolumeUnderMouse(int volume)
{
	if (volume == 100 || !volume)
		osdWidget->showText(i18nc("osd", "Volume: %1%", volume), 1500);
	else
		setVolume(volume);
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
	source->playbackStatusChanged(Idle);
}

void MediaWidget::setAudioCard()
{
	QAction *action = qobject_cast<QAction *>(sender());
	backend->setAudioDevice(action->data().toString());
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

void MediaWidget::checkScreenSaver(bool noDisable)
{
	bool suspendScreenSaver = false;

	if (!noDisable) {
		switch (backend->getPlaybackStatus()) {
		case Idle:
		case Paused:
			break;
		case Playing:
			suspendScreenSaver = isVisible();
			break;
		}
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

void MediaWidget::deinterlacingChanged(QAction *action)
{
	bool ok;
	const char *mode;

	deinterlaceMode = action->data().toInt(&ok);

	switch (deinterlaceMode) {
	case DeinterlaceDiscard:
		mode = "discard";
		break;
	case DeinterlaceBob:
		mode = "bob";
		break;
	case DeinterlaceLinear:
		mode = "linear";
		break;
	case DeinterlaceYadif:
		mode = "yadif";
		break;
	case DeinterlaceYadif2x:
		mode = "yadif2x";
		break;
	case DeinterlacePhosphor:
		mode = "phosphor";
		break;
	case DeinterlaceX:
		mode = "x";
		break;
	case DeinterlaceMean:
		mode = "mean";
		break;
	case DeinterlaceBlend:
		mode = "blend";
		break;
	case DeinterlaceIvtc:
		mode = "ivtc";
		break;
	case DeinterlaceDisabled:
	default:
		mode = "disabled";
	}

	backend->setDeinterlacing(static_cast<DeinterlaceMode>(deinterlaceMode));

	osdWidget->showText(i18nc("osd message", "Deinterlace %1", mode), 1500);
}

void MediaWidget::aspectRatioChanged(QAction *action)
{
	bool ok;
	unsigned int aspectRatio_ = action->data().toInt(&ok);

	if (aspectRatio_ <= AspectRatio239_100) {
		backend->setAspectRatio(static_cast<AspectRatio>(aspectRatio_));
		setVideoSize();

		return;
	}

	qCWarning(logMediaWidget, "internal error");
}


void MediaWidget::setAspectRatio(MediaWidget::AspectRatio aspectRatio)
{
	backend->setAspectRatio(aspectRatio);
}


void MediaWidget::setVideoSize()
{
	float scale = autoResizeFactor / 100.0;

	if (scale > 3.4 || scale < 0)
		scale = 0;

	backend->resizeToVideo(scale);
}

void MediaWidget::autoResizeTriggered(QAction *action)
{
	foreach (QAction *autoResizeAction, action->actionGroup()->actions()) {
		autoResizeAction->setChecked(autoResizeAction == action);
	}

	bool ok = false;
	autoResizeFactor = action->data().toInt(&ok);

	if (ok)
		setVideoSize();
	else
		qCWarning(logMediaWidget, "internal error");
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
	audioStreamBox->view()->setMinimumWidth(audioStreamBox->view()->sizeHintForColumn(0));

}

void MediaWidget::currentSubtitleChanged(int currentSubtitle)
{
	if (blockBackendUpdates)
		return;

	if (source->overrideSubtitles()) {
		source->setCurrentSubtitle(currentSubtitle - 1);
		return;
	}

	source->setCurrentSubtitle(currentSubtitle - 1 - backend->getSubtitles().size());

	backend->setCurrentSubtitle(currentSubtitle);
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
        // xgettext:no-c-format
	shortSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward",
		shortSkipDuration));
	// xgettext:no-c-format
	shortSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward",
		shortSkipDuration));
}

void MediaWidget::longSkipDurationChanged(int longSkipDuration)
{
        // xgettext:no-c-format
	longSkipBackwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Backward",
		longSkipDuration));
	// xgettext:no-c-format
	longSkipForwardAction->setText(i18nc("submenu of 'Skip'", "Skip %1s Forward",
		longSkipDuration));
}

void MediaWidget::getAudioDevices()
{
	foreach(QAction *action, audioDevMenu->actions()) {
		audioDevMenu->removeAction(action);
	}

	foreach(const QString &device, backend->getAudioDevices()) {
		QAction *action = new QWidgetAction(this);
		action->setText(device);
		action->setData(device);
		connect(action, SIGNAL(triggered()), this, SLOT(setAudioCard()));
		audioDevMenu->addAction(action);
	}
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
	int currentIndex = 0;

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
		currentIndex = (backend->getCurrentSubtitle());
		int currentSourceIndex = source->getCurrentSubtitle();

		if (currentSourceIndex >= 0) {
			currentIndex = (currentSourceIndex + backend->getSubtitles().size() + 1);
		}
	}

	blockBackendUpdates = true;

	if (subtitleModel->stringList() != items) {
		subtitleModel->setStringList(items);
	}

	if (currentIndex < 0)
		currentIndex = 0;

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

	// If the player backend doesn't implement currentTime and/or
	// totalTime, the source can implement such logic
	source->validateCurrentTotalTime(currentTime, totalTime);

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
	timeButton->setEnabled(seekable);
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

bool MediaWidget::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::ShortcutOverride:
		{
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		int key = keyEvent->key();

		if (backend->hasDvdMenu()) {
			switch (key){
			case Qt::Key_Return:
			case Qt::Key_Up:
			case Qt::Key_Down:
			case Qt::Key_Left:
			case Qt::Key_Right:
				backend->dvdNavigate(key);
				event->accept();
				return true;
			}
		}
		break;
		}
	default:
		break;
	}

	return QWidget::event(event);
}

void MediaWidget::keyPressEvent(QKeyEvent *event)
{
	int key = event->key();

	if ((key >= Qt::Key_0) && (key <= Qt::Key_9)) {
		event->accept();
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
	int y = event->angleDelta().y();
	int delta = (y < 0) ? -1 : 1;

	setVolumeUnderMouse(getVolume() + delta);
}

void MediaWidget::playbackFinished()
{
	if (source->getType() == MediaSource::Url)
		emit playlistNext();

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

	QString osdText = caption;
	if (backend->hasDvdMenu())
		osdText += "\nUse keys to navigate at DVD menu";

	if (!caption.isEmpty()) {
		osdWidget->showText(osdText, 2500);
	}

	emit changeCaption(caption);
}

void MediaWidget::dvdMenuChanged()
{
	bool hasDvdMenu = backend->hasDvdMenu();

	menuAction->setEnabled(hasDvdMenu);
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
	setVideoSize();
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

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);

	timeEdit->setFocus();

	QVBoxLayout *mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(widget);
	mainLayout->addWidget(buttonBox);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
}

JumpToPositionDialog::~JumpToPositionDialog()
{
}

void JumpToPositionDialog::accept()
{
	mediaWidget->setPosition(QTime(0, 0, 0).msecsTo(timeEdit->time()));
	QDialog::accept();
}

void SeekSlider::mousePressEvent(QMouseEvent *event)
{
	int buttons = style()->styleHint(QStyle::SH_Slider_AbsoluteSetButtons);
	Qt::MouseButton button = static_cast<Qt::MouseButton>(buttons & (~(buttons - 1)));
	QMouseEvent modifiedEvent(event->type(), event->pos(), event->globalPosition(), button,
		event->buttons() ^ event->button() ^ button, event->modifiers());
	QSlider::mousePressEvent(&modifiedEvent);
}

void SeekSlider::wheelEvent(QWheelEvent *event)
{
	int delta = (event->angleDelta().y() < 0) ? -1 : 1;
	int new_value = value() + delta * maximum() / 100;

	event->accept();
	setValue(new_value);
}
