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

#include <KFileDialog>
#include <KInputDialog>
#include <KLocalizedString>
#include <KToolBar>

#include "manager.h"
#include "mediawidget.h"
#include "kaffeine.h"

KCmdLineOptions Kaffeine::cmdLineOptions()
{
	// FIXME add options
	return KCmdLineOptions();
}

Kaffeine::Kaffeine()
{
	// FIXME workaround
	setAttribute(Qt::WA_DeleteOnClose, false);

	manager = new Manager(this);
	mediaWidget = manager->getMediaWidget();
	setCentralWidget(manager);
	setupGUI(ToolBar | Keys | Create);

	// workaround broken size restoring
	bool maximized = KConfig().group("Workarounds").readEntry("Maximized", false);
	if (maximized) {
		showMaximized();
	}
	setAutoSaveSettings();

	show();
}

Kaffeine::~Kaffeine()
{
	// workaround broken size restoring
	KConfig().group("Workarounds").writeEntry("Maximized", isMaximized());
}

void Kaffeine::parseArgs()
{
	// FIXME implement
}

void Kaffeine::actionFullscreen()
{
	manager->toggleFullscreen();
}

void Kaffeine::actionOpen()
{
	// FIXME do we want to be able to open several files at once or not?
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Open file"));
	if (url.isValid()) {
		manager->activate(Manager::tabPlayer);
		manager->addRecentUrl(url);
		mediaWidget->play(url);
	}
}

void Kaffeine::actionOpenUrl()
{
	KUrl url(KInputDialog::getText(i18n("Open URL"), i18n("Enter a URL:")));
	if (url.isValid()) {
		manager->activate(Manager::tabPlayer);
		manager->addRecentUrl(url);
		mediaWidget->play(url);
	}
}

void Kaffeine::actionOpenRecent(const KUrl &url)
{
	manager->activate(Manager::tabPlayer);
	mediaWidget->play(url);
}

void Kaffeine::actionOpenAudioCd()
{
	manager->activate(Manager::tabAudioCd);
	mediaWidget->playAudioCd();
}

void Kaffeine::actionOpenVideoCd()
{
	manager->activate(Manager::tabPlayer);
	mediaWidget->playVideoCd();
}

void Kaffeine::actionOpenDvd()
{
	manager->activate(Manager::tabPlayer);
	mediaWidget->playDvd();
}

void Kaffeine::actionPlayPause(bool paused)
{
	if (manager->isPlaying()) {
		mediaWidget->setPaused(paused);
	} else {
		// FIXME do some special actions - play playlist, ask for input ...
	}
}
