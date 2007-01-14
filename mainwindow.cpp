/*
 * mainwindow.cpp
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
#include <KLocalizedString>
#include <KStandardAction>
#include <KToolBar>

#include "mainwindow.h"
#include "mainwindow.moc"

MainWindow::MainWindow(Kaffeine *kaffeine_) : kaffeine(kaffeine_), currentState(stateAll)
{
	/*
	 * initialise gui elements
	 */

	KAction *action;

	action = KStandardAction::open(kaffeine, SLOT(actionOpen()), actionCollection());
	addAction("file_open", stateAlways, action);

	action = KStandardAction::quit(this, SLOT(close()), actionCollection());
	addAction("file_quit", stateAlways, action);

	action = new KAction(KIcon("player_start"), i18n("Previous"), actionCollection());
	addAction("controls_previous", stateFlags(statePrevNext) | statePlaying, action);

	controlsPlayPause = new KAction(KIcon("player_play"), i18n("Play"), actionCollection());
	connect(controlsPlayPause, SIGNAL(triggered(bool)), this, SLOT(actionPlayPause()));
	addAction("controls_play_pause", stateAlways, controlsPlayPause);

	action = new KAction(KIcon("player_stop"), i18n("Stop"), actionCollection());
	connect(action, SIGNAL(triggered(bool)), kaffeine, SLOT(actionStop()));
	addAction("controls_stop", statePlaying, action);

	action = new KAction(KIcon("player_end"), i18n("Next"), actionCollection());
	addAction("controls_next", statePrevNext, action);

	action = new KAction(i18n("Volume"), actionCollection());
	controlsVolume = new QSlider(Qt::Horizontal, this);
	controlsVolume->setMinimum(0);
	controlsVolume->setMaximum(100);
	connect(controlsVolume, SIGNAL(valueChanged(int)), kaffeine, SLOT(actionVolume(int)));
	action->setDefaultWidget(controlsVolume);
	addAction("controls_volume", stateAlways, action);

	action = new KAction(i18n("Position"), actionCollection());
	controlsPosition = new QSlider(Qt::Horizontal, this);
	controlsPosition->setMinimum(0);
	controlsPosition->setMaximum(65536);
	connect(controlsPosition, SIGNAL(valueChanged(int)), kaffeine, SLOT(actionPosition(int)));
	action->setDefaultWidget(controlsPosition);
	addAction("position_slider", statePlaying, action);

	setState(stateAlways);

	createGUI();

	// FIXME workaround
	addToolBar(Qt::BottomToolBarArea, toolBar("main_controls_toolbar"));
	addToolBar(Qt::BottomToolBarArea, toolBar("position_slider_toolbar"));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setPosition(int position)
{
	controlsPosition->setValue(position);
}

void MainWindow::setVolume(int volume)
{
	controlsVolume->setValue(volume);
}

void MainWindow::actionPlayPause()
{
	if (currentState.testFlag(statePlaying))
		kaffeine->actionPause(controlsPlayPause->isChecked());
	else
		kaffeine->actionPlay();
}

void MainWindow::addAction(const QString &name, stateFlags flags, KAction *action)
{
	actionCollection()->addAction(name, action);
	if (flags != stateAlways)
		actionList.append(qMakePair(flags, action));
}

void MainWindow::setState(stateFlags newState)
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
