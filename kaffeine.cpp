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
#include <KLocalizedString>

#include "kaffeine.h"
#include "mainwindow.h"

#include "kaffeine.moc"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	// FIXME add options
	KCmdLineLastOption
};

Kaffeine::Kaffeine()
{
	mainWindow = new MainWindow(this);
	player = new MediaWidget(mainWindow);

	connect(player, SIGNAL(newState(MediaState)), this, SLOT(newMediaState(MediaState)));
	connect(player, SIGNAL(positionChanged(int)), mainWindow, SLOT(setPosition(int)));

	mainWindow->setCentralWidget(player);

	mainWindow->show();
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
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), mainWindow, i18n("Open file"));
	if (url.isValid())
		player->play(url);
}

void Kaffeine::actionPlay()
{
	// FIXME do some special actions - play playlist, ask for input ...
	player->play();
}

void Kaffeine::actionPause(bool paused)
{
	player->togglePause(paused);
}

void Kaffeine::actionStop()
{
	player->stop();
}

void Kaffeine::actionVolume(int volume)
{
	player->setVolume(volume);
}

void Kaffeine::actionPosition(int position)
{
	player->setPosition(position);
}

void Kaffeine::newMediaState(MediaState status)
{
	switch (status) {
		case MediaPlaying:
			mainWindow->play();
			break;
		case MediaPaused:
			break;
		case MediaStopped:
			mainWindow->stop();
			break;
		default:
			break;
	}
}
