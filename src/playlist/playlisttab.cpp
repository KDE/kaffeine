/*
 * playlisttab.cpp
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#include "playlisttab.h"

#include <QBoxLayout>
#include <QLabel>
#include <QSplitter>
#include "../mediawidget.h"

// FIXME stubby :)

PlaylistTab::PlaylistTab(MediaWidget *mediaWidget_) : mediaWidget(mediaWidget_)
{
	QBoxLayout *widgetLayout = new QHBoxLayout(this);
	widgetLayout->setMargin(0);

	QSplitter *horizontalSplitter = new QSplitter(this);
	widgetLayout->addWidget(horizontalSplitter);

	QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, horizontalSplitter);

	new QLabel("Hi, i'm the file browser", verticalSplitter);
	new QLabel("Hi, i'm the playlist browser", verticalSplitter);

	verticalSplitter = new QSplitter(Qt::Vertical, horizontalSplitter);

	new QLabel("Hi, i'm the playlist", verticalSplitter);

	QWidget *mediaContainer = new QWidget(verticalSplitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);

	verticalSplitter->setStretchFactor(0, 0);
	verticalSplitter->setStretchFactor(1, 1);
}

PlaylistTab::~PlaylistTab()
{
}

void PlaylistTab::addUrl(const KUrl &url)
{
	// FIXME
}

void PlaylistTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
}
