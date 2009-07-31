/*
 * playlisttab.h
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

#ifndef PLAYLISTTAB_H
#define PLAYLISTTAB_H

#include <QTreeView>
#include "../tabbase.h"

class KActionCollection;
class KMenu;
class KUrl;
class MediaWidget;
class Playlist;
class PlaylistBrowserModel;
class PlaylistBrowserView;
class PlaylistModel;

class PlaylistView : public QTreeView
{
	Q_OBJECT
public:
	explicit PlaylistView(QWidget *parent);
	~PlaylistView();

public slots:
	void removeSelectedRows();

protected:
	void contextMenuEvent(QContextMenuEvent *event);
	void keyPressEvent(QKeyEvent *event);
};

class PlaylistBrowserModel : public QAbstractListModel
{
	Q_OBJECT
public:
	PlaylistBrowserModel(PlaylistModel *playlistModel_, QObject *parent);
	~PlaylistBrowserModel();

	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);

	void append(Playlist *playlist);
	Playlist *getPlaylist(int row) const;

private slots:
	void setCurrentPlaylist(Playlist *playlist);

private:
	PlaylistModel *playlistModel;
	QList<Playlist *> playlists;
	int currentPlaylist;
};

class PlaylistTab : public TabBase
{
	Q_OBJECT
public:
	PlaylistTab(KMenu *menu, KActionCollection *collection, MediaWidget *mediaWidget_);
	~PlaylistTab();

	void playUrls(const QList<KUrl> &urls);

private slots:
	void newPlaylist();
	void renamePlaylist();
	void removePlaylist();
	void savePlaylist();
	void saveAsPlaylist();
	void playlistActivated(const QModelIndex &index);

private:
	void activate();
	void savePlaylist(bool askName);

	MediaWidget *mediaWidget;
	QLayout *mediaLayout;
	PlaylistBrowserModel *playlistBrowserModel;
	PlaylistBrowserView *playlistBrowserView;
	PlaylistModel *playlistModel;
	PlaylistView *playlistView;
};

#endif /* PLAYLISTTAB_H */
