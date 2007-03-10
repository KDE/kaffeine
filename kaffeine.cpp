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

#include "kaffeine.h"
#include "kaffeine.moc"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	// FIXME add options
	KCmdLineLastOption
};

Kaffeine::Kaffeine()
{
	// FIXME workaround
	setAttribute(Qt::WA_DeleteOnClose, false);

	mediaWidget = new MediaWidget(this);
	actionManager = new ActionManager(this, mediaWidget);

	// FIXME
	connect(mediaWidget, SIGNAL(newState(MediaState)), this, SLOT(newMediaState(MediaState)));

	/*
	 * initialise gui elements
	 */

	setupGUI();

	// FIXME workaround
	addToolBar(Qt::BottomToolBarArea, toolBar("main_controls"));
	addToolBar(Qt::BottomToolBarArea, toolBar("position_slider"));
	addToolBar(Qt::LeftToolBarArea, toolBar("tab_manager"));

	KToolBar *tabBar = new KToolBar("xyzTab manager", this, Qt::LeftToolBarArea);
	tabManager = new TabManager(this, tabBar, mediaWidget);

	setCentralWidget(tabManager);

	show();
}

Kaffeine::~Kaffeine()
{
	delete actionManager;
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
		mediaWidget->play(url);
}

void Kaffeine::actionPlayPause(bool paused)
{
	if (actionManager->isPlaying())
		mediaWidget->togglePause(paused);
	else
		// FIXME do some special actions - play playlist, ask for input ...
		mediaWidget->play();
}

void Kaffeine::newMediaState(MediaState status)
{
	switch (status) {
		case MediaPlaying:
			actionManager->play();
			break;
		case MediaPaused:
			break;
		case MediaStopped:
			actionManager->stop();
			break;
		default:
			break;
	}
}
