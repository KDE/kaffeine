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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "playlisttab.h"

#include <QBoxLayout>
#include <QKeyEvent>
#include <QListView>
#include <QSplitter>
#include <QToolButton>
#include <QTreeView>
#include <KAction>
#include <KActionCollection>
#include <kfilewidget.h>
#include <KLocalizedString>
#include <KMenu>
#include "../mediawidget.h"
#include "playlistmodel.h"

class Playlist
{
public:
	explicit Playlist(const QString &name_) : name(name_) { }
	~Playlist() { }

	QString getName() const
	{
		return name;
	}

	void setName(const QString &name_)
	{
		name = name_;
	}

	QList<PlaylistTrack> getTracks() const
	{
		return tracks;
	}

	void setTracks(const QList<PlaylistTrack> &tracks_)
	{
		tracks = tracks_;
	}

private:
	QString name;
	QList<PlaylistTrack> tracks;
};

class PlaylistBrowserModel : public QAbstractListModel
{
public:
	explicit PlaylistBrowserModel(QObject *parent) : QAbstractListModel(parent) { }
	~PlaylistBrowserModel() { }

	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool setData(const QModelIndex &index, const QVariant &value, int role);

	void append(Playlist *playlist);
	Playlist *getPlaylist(int row) const;

private:
	QList<Playlist *> playlists;
};

int PlaylistBrowserModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return playlists.size();
}

QVariant PlaylistBrowserModel::data(const QModelIndex &index, int role) const
{
	if (role != Qt::DisplayRole) {
		return QVariant();
	}

	return playlists.at(index.row())->getName();
}

Qt::ItemFlags PlaylistBrowserModel::flags(const QModelIndex &index) const
{
	return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool PlaylistBrowserModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (parent.isValid()) {
		return false;
	}

	QList<Playlist *>::iterator beginIt = playlists.begin() + row;
	QList<Playlist *>::iterator endIt = beginIt + count;

	beginRemoveRows(QModelIndex(), row, row + count - 1);
	qDeleteAll(beginIt, endIt);
	playlists.erase(beginIt, endIt);
	endRemoveRows();

	return true;
}

bool PlaylistBrowserModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (role == Qt::EditRole) {
		playlists.at(index.row())->setName(value.toString());
		emit dataChanged(index, index);
		return true;
	}

	return false;
}

void PlaylistBrowserModel::append(Playlist *playlist)
{
	beginInsertRows(QModelIndex(), playlists.size(), playlists.size());
	playlists.append(playlist);
	endInsertRows();
}

Playlist *PlaylistBrowserModel::getPlaylist(int row) const
{
	return playlists.at(row);
}

class PlaylistBrowserView : public QListView
{
public:
	explicit PlaylistBrowserView(QWidget *parent) : QListView(parent) { }
	~PlaylistBrowserView() { }

protected:
	void keyPressEvent(QKeyEvent *event);
};

void PlaylistBrowserView::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		QModelIndexList selectedRows = selectionModel()->selectedRows();
		qSort(selectedRows);

		for (int i = selectedRows.size() - 1; i >= 0; --i) {
			// FIXME compress
			model()->removeRows(selectedRows.at(i).row(), 1);
		}

		return;
	}

	QListView::keyPressEvent(event);
}

class PlaylistView : public QTreeView
{
public:
	explicit PlaylistView(QWidget *parent) : QTreeView(parent) { }
	~PlaylistView() { }

protected:
	void keyPressEvent(QKeyEvent *event);
};

void PlaylistView::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		QModelIndexList selectedRows = selectionModel()->selectedRows();
		qSort(selectedRows);

		for (int i = selectedRows.size() - 1; i >= 0; --i) {
			// FIXME compress
			model()->removeRows(selectedRows.at(i).row(), 1);
		}

		return;
	}

	QTreeView::keyPressEvent(event);
}

PlaylistTab::PlaylistTab(KMenu *menu, KActionCollection *collection, MediaWidget *mediaWidget_) :
	mediaWidget(mediaWidget_)
{
	playlistModel = new PlaylistModel(mediaWidget, this);

	KAction *repeatAction = new KAction(KIcon("media-playlist-repeat"),
					    i18nc("playlist menu", "Repeat"), this);
	repeatAction->setCheckable(true);
	connect(repeatAction, SIGNAL(triggered(bool)), playlistModel, SLOT(repeatPlaylist(bool)));
	menu->addAction(collection->addAction("playlist_repeat", repeatAction));

	KAction *shuffleAction = new KAction(KIcon("media-playlist-shuffle"),
					     i18nc("playlist menu", "Shuffle"), this);
	connect(shuffleAction, SIGNAL(triggered(bool)), playlistModel, SLOT(shufflePlaylist()));
	menu->addAction(collection->addAction("playlist_shuffle", shuffleAction));

	KAction *clearAction = new KAction(KIcon("edit-clear-list"),
					   i18nc("remove all items from a list", "Clear"), this);
	connect(clearAction, SIGNAL(triggered(bool)), playlistModel, SLOT(clearPlaylist()));
	menu->addAction(collection->addAction("playlist_clear", clearAction));

	menu->addSeparator();

	KAction *newAction = new KAction(KIcon("list-add"),
					 i18nc("add a new item to a list", "New"), this);
	connect(newAction, SIGNAL(triggered(bool)), this, SLOT(newPlaylist()));
	menu->addAction(collection->addAction("playlist_new", newAction));

	KAction *removeAction = new KAction(KIcon("edit-delete"),
					    i18nc("remove an item from a list", "Remove"), this);
	connect(removeAction, SIGNAL(triggered(bool)), this, SLOT(removePlaylist()));
	menu->addAction(collection->addAction("playlist_remove", removeAction));

	QBoxLayout *widgetLayout = new QHBoxLayout(this);
	widgetLayout->setMargin(0);

	QSplitter *horizontalSplitter = new QSplitter(this);
	widgetLayout->addWidget(horizontalSplitter);

	QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, horizontalSplitter);

	QWidget *widget = new QWidget(verticalSplitter);
	QBoxLayout *sideLayout = new QVBoxLayout(widget);
	sideLayout->setMargin(0);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QToolButton *toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(newAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(removeAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	boxLayout->addStretch();
	sideLayout->addLayout(boxLayout);

	currentPlaylist = -1;
	playlistBrowserModel = new PlaylistBrowserModel(this);

	playlistBrowserView = new PlaylistBrowserView(widget);
	playlistBrowserView->setModel(playlistBrowserModel);
	connect(playlistBrowserView, SIGNAL(activated(QModelIndex)),
		this, SLOT(playlistActivated(QModelIndex)));
	sideLayout->addWidget(playlistBrowserView);

	KFileWidget *fileWidget = new KFileWidget(KUrl(), verticalSplitter);
	fileWidget->setFilter(MediaWidget::extensionFilter());
	fileWidget->setMode(KFile::Files | KFile::ExistingOnly);
	verticalSplitter->setStretchFactor(1, 1);

	verticalSplitter = new QSplitter(Qt::Vertical, horizontalSplitter);

	widget = new QWidget(verticalSplitter);
	sideLayout = new QVBoxLayout(widget);
	sideLayout->setMargin(0);

	boxLayout = new QHBoxLayout();

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(repeatAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(shuffleAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(clearAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	boxLayout->addStretch();
	sideLayout->addLayout(boxLayout);

	playlistView = new PlaylistView(widget);
	playlistView->setAlternatingRowColors(true);
	playlistView->setDragDropMode(QAbstractItemView::DragDrop);
	playlistView->setRootIsDecorated(false);
	playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	playlistView->setModel(playlistModel);
	playlistView->sortByColumn(0, Qt::AscendingOrder);
	playlistView->setSortingEnabled(true);
	connect(playlistView, SIGNAL(activated(QModelIndex)),
		playlistModel, SLOT(playTrack(QModelIndex)));
	sideLayout->addWidget(playlistView);

	QWidget *mediaContainer = new QWidget(verticalSplitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);

	verticalSplitter->setStretchFactor(1, 1);
	horizontalSplitter->setStretchFactor(1, 1);
}

PlaylistTab::~PlaylistTab()
{
}

void PlaylistTab::playUrls(const QList<KUrl> &urls)
{
	playlistModel->appendUrls(urls, false);
}

void PlaylistTab::newPlaylist()
{
	Playlist *playlist = new Playlist("Unnamed Playlist"); // FIXME
	playlistBrowserModel->append(playlist);
}

void PlaylistTab::removePlaylist()
{
	QModelIndexList selectedRows = playlistBrowserView->selectionModel()->selectedRows();
	qSort(selectedRows);

	for (int i = selectedRows.size() - 1; i >= 0; --i) {
		// FIXME compress
		playlistBrowserModel->removeRows(selectedRows.at(i).row(), 1);
	}
}

void PlaylistTab::playlistActivated(const QModelIndex &index)
{
	int newPlaylist = index.row();

	if (newPlaylist != currentPlaylist) {
		if (currentPlaylist != -1) {
			playlistBrowserModel->getPlaylist(currentPlaylist)->setTracks(playlistModel->getPlaylist());
		}

		playlistModel->setPlaylist(playlistBrowserModel->getPlaylist(index.row())->getTracks());
		currentPlaylist = newPlaylist;
	}
}

void PlaylistTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
}
