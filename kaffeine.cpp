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

#include <KFileDialog>
#include <KLocalizedString>
#include <KToolBar>

#include "mediawidget.h"
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

	manager = new Manager(this);
	mediaWidget = manager->getMediaWidget();
	setCentralWidget(manager);
	setupGUI(ToolBar | Keys | Save | Create);

	// FIXME workaround
	addToolBar(Qt::BottomToolBarArea, toolBar("main_controls"));
	addToolBar(Qt::BottomToolBarArea, toolBar("position_slider"));
	addToolBar(Qt::LeftToolBarArea, toolBar("tab_manager"));

	show();
}

Kaffeine::~Kaffeine()
{
}

void Kaffeine::parseArgs()
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
	if (manager->isPlaying())
		mediaWidget->togglePause(paused);
	else
		// FIXME do some special actions - play playlist, ask for input ...
		mediaWidget->play();
}
