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
#include <QTreeView>
#include <kfilewidget.h>
#include "../mediawidget.h"
#include "playlistmodel.h"

PlaylistTab::PlaylistTab(MediaWidget *mediaWidget_) : mediaWidget(mediaWidget_), currentTrack(0)
{
	QBoxLayout *widgetLayout = new QHBoxLayout(this);
	widgetLayout->setMargin(0);

	QSplitter *horizontalSplitter = new QSplitter(this);
	widgetLayout->addWidget(horizontalSplitter);

	new KFileWidget(KUrl(), horizontalSplitter);

	QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, horizontalSplitter);

	playlistModel = new PlaylistModel(this);
	connect(mediaWidget, SIGNAL(playlistNext()), this, SLOT(nextTrack()));
	connect(mediaWidget, SIGNAL(playlistPrevious()), this, SLOT(previousTrack()));

	playlistView = new QTreeView(this);
	playlistView->setAcceptDrops(true);
	playlistView->setAlternatingRowColors(true);
	playlistView->setDragDropMode(QAbstractItemView::DragDrop);
	playlistView->setDragEnabled(true);
	playlistView->setDropIndicatorShown(true);
	playlistView->setRootIsDecorated(false);
	playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	playlistView->setModel(playlistModel);
	connect(playlistView, SIGNAL(doubleClicked(QModelIndex)), // FIXME use activated(...) ?
		this, SLOT(playTrack(QModelIndex)));

	verticalSplitter->addWidget(playlistView);

	QWidget *mediaContainer = new QWidget(verticalSplitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);
	verticalSplitter->setStretchFactor(1, 1);
	horizontalSplitter->setStretchFactor(1, 1);
}

PlaylistTab::~PlaylistTab()
{
}

void PlaylistTab::playUrl(const KUrl &url)
{
	int track = playlistModel->trackCount();
	playlistModel->appendUrl(url);
	playTrack(track);
}

void PlaylistTab::playUrls(const QList<KUrl> &urls)
{
	int track = playlistModel->trackCount();
	playlistModel->appendUrls(urls);
	playTrack(track);
}

void PlaylistTab::previousTrack()
{
	if ((currentTrack - 1) >= 0) {
		playTrack(currentTrack - 1);
	}
}

void PlaylistTab::nextTrack()
{
	if ((currentTrack + 1) < playlistModel->trackCount()) {
		playTrack(currentTrack + 1);
	}
}

void PlaylistTab::playTrack(const QModelIndex &index)
{
	if (!index.isValid() || (index.row() >= playlistModel->trackCount())) {
		return;
	}

	playTrack(index.row());
}

void PlaylistTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
}

void PlaylistTab::playTrack(int track)
{
	currentTrack = track;
	mediaWidget->play(playlistModel->getTrack(currentTrack));
	// FIXME update playlistView currentTrack display
}
