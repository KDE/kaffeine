/*
 * kaffeine.cpp
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

#include <config.h>

#include <QSlider>

#include <KAction>
#include <KActionCollection>
#include <KFileDialog>
#include <KLocalizedString>
#include <KStandardAction>
#include <KToolBar>

#include "maintabs.h"
#include "kaffeine.h"

#include "kaffeine.moc"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	// FIXME add options
	KCmdLineLastOption
};

Kaffeine::Kaffeine() : currentState(stateAll)
{
	// FIXME workaround
	setAttribute(Qt::WA_DeleteOnClose, false);

	player = new MediaWidget(this);

	connect(player, SIGNAL(newState(MediaState)), this, SLOT(newMediaState(MediaState)));

	/*
	 * initialise gui elements
	 */

	KAction *action;

	action = KStandardAction::open(this, SLOT(actionOpen()), actionCollection());
	addAction("file_open", stateAlways, action);

	action = KStandardAction::quit(this, SLOT(close()), actionCollection());
	addAction("file_quit", stateAlways, action);

	action = new KAction(KIcon("player_start"), i18n("Previous"), actionCollection());
	addAction("controls_previous", stateFlags(statePrevNext) | statePlaying, action);

	controlsPlayPause = new KAction(KIcon("player_play"), i18n("Play"), actionCollection());
	connect(controlsPlayPause, SIGNAL(triggered(bool)), this, SLOT(actionPlayPause()));
	addAction("controls_play_pause", stateAlways, controlsPlayPause);

	action = new KAction(KIcon("player_stop"), i18n("Stop"), actionCollection());
	connect(action, SIGNAL(triggered(bool)), player, SLOT(stop()));
	addAction("controls_stop", statePlaying, action);

	action = new KAction(KIcon("player_end"), i18n("Next"), actionCollection());
	addAction("controls_next", statePrevNext, action);

	action = new KAction(actionCollection());
	QSlider *slider = new QSlider(Qt::Horizontal, this);
	slider->setFocusPolicy(Qt::NoFocus);
	slider->setMinimum(0);
	slider->setMaximum(100);
	slider->setSingleStep(1);
	slider->setPageStep(10);
	connect(player, SIGNAL(volumeChanged(int)), slider, SLOT(setValue(int)));
	connect(slider, SIGNAL(valueChanged(int)), player, SLOT(setVolume(int)));
	action->setDefaultWidget(slider);
	addAction("controls_volume", stateAlways, action);

	action = new KAction(actionCollection());
	slider = new QSlider(Qt::Horizontal, this);
	slider->setFocusPolicy(Qt::NoFocus);
	slider->setMinimum(0);
	slider->setMaximum(65536);
	slider->setSingleStep(64);
	slider->setPageStep(4096);
	connect(player, SIGNAL(positionChanged(int)), slider, SLOT(setValue(int)));
	connect(slider, SIGNAL(valueChanged(int)), player, SLOT(setPosition(int)));
	action->setDefaultWidget(slider);
	addAction("position_slider", statePlaying, action);

	setState(stateAlways);

	createGUI();

	// FIXME workaround
	addToolBar(Qt::BottomToolBarArea, toolBar("main_controls_toolbar"));
	addToolBar(Qt::BottomToolBarArea, toolBar("position_slider_toolbar"));

	KToolBar *tabBar = new KToolBar("Tab manager", this, Qt::LeftToolBarArea);
	tabManager = new TabManager(this, tabBar);
	tabManager->addTab(i18n("Start"), new StartTab(tabManager));
	tabManager->addTab(i18n("Player"), new PlayerTab(tabManager, player));

	setCentralWidget(tabManager);

	show();
}

Kaffeine::~Kaffeine()
{
}

void Kaffeine::updateArgs()
{
	// FIXME implement
}

void Kaffeine::actionOpen()
{
	// FIXME do we want to be able to open several files at once or not?
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Open file"));
	if (url.isValid())
		player->play(url);
}

void Kaffeine::actionPlayPause()
{
	if (currentState.testFlag(statePlaying))
		player->togglePause(controlsPlayPause->isChecked());
	else
		// FIXME do some special actions - play playlist, ask for input ...
		player->play();
}

void Kaffeine::newMediaState(MediaState status)
{
	switch (status) {
		case MediaPlaying:
			play();
			break;
		case MediaPaused:
			break;
		case MediaStopped:
			stop();
			break;
		default:
			break;
	}
}

void Kaffeine::addAction(const QString &name, stateFlags flags, KAction *action)
{
	actionCollection()->addAction(name, action);
	if (flags != stateAlways)
		actionList.append(qMakePair(flags, action));
}

void Kaffeine::setState(stateFlags newState)
{
	if (currentState == newState)
		return;

	stateFlags changedFlags = currentState ^ newState;

	// starting / stopping playback is special
	if (changedFlags.testFlag(statePlaying))
		if (newState.testFlag(statePlaying)) {
			controlsPlayPause->setIcon(KIcon("player_pause"));
			controlsPlayPause->setText(i18n("Pause"));
			controlsPlayPause->setCheckable(true);
		} else {
			controlsPlayPause->setIcon(KIcon("player_play"));
			controlsPlayPause->setText(i18n("Play"));
			controlsPlayPause->setCheckable(false);
		}

	QPair<stateFlags, KAction *> action;
	foreach (action, actionList)
		action.second->setEnabled((action.first & newState) != stateAlways);

	currentState = newState;
}
