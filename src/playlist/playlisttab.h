/*
 * playlisttab.h
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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
#include "../mediawidget.h"
#include "../tabbase.h"

class QSplitter;
class Playlist;
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
	PlaylistBrowserModel(PlaylistModel *playlistModel_, Playlist *temporaryPlaylist,
		QObject *parent);
	~PlaylistBrowserModel();

	void append(Playlist *playlist);
	Playlist *getPlaylist(int row) const;
	void setCurrentPlaylist(Playlist *playlist);
	Playlist *getCurrentPlaylist() const;
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());

signals:
	void playTrack(Playlist *playlist, int track);

private:
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);

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

	void appendToCurrentPlaylist(const QList<KUrl> &urls, bool playImmediately);
	void appendToVisiblePlaylist(const QList<KUrl> &urls, bool playImmediately);
	void removeTrack(int row);
	void setRandom(bool random);
	void setRepeat(bool repeat);

	int getCurrentTrack() const;
	int getTrackCount() const;
	bool getRandom() const;
	bool getRepeat() const;

private slots:
	void createFileWidget();
	void newPlaylist();
	void renamePlaylist();
	void removePlaylist();
	void savePlaylist();
	void savePlaylistAs();
	void addSubtitle();
	void playlistActivated(const QModelIndex &index);
	void playPreviousTrack();
	void playCurrentTrack();
	void playNextTrack();
	void playTrack(Playlist *playlist, int track);
	void playTrack(const QModelIndex &index);
	void appendUrls(const QList<KUrl> &urls);
	void appendPlaylist(Playlist *playlist, bool playImmediately);
	void updateTrackLength(int length);
	void updateTrackMetadata(const QMap<MediaWidget::MetadataType, QString> &metadata);

private:
	static QString subtitleExtensionFilter(); // usable for KFileDialog::setFilter()
	void activate();
	void savePlaylist(bool askName);

	MediaWidget *mediaWidget;
	QLayout *mediaLayout;
	QSplitter *fileWidgetSplitter;
	PlaylistBrowserModel *playlistBrowserModel;
	PlaylistBrowserView *playlistBrowserView;
	PlaylistModel *playlistModel;
	PlaylistView *playlistView;
	KAction *randomAction;
	KAction *repeatAction;
};

#endif /* PLAYLISTTAB_H */
