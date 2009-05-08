/*
 * playlistmodel.h
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

#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <KUrl>

class MediaWidget;

class PlaylistTrack
{
public:
	explicit PlaylistTrack(const KUrl &url_);
	~PlaylistTrack();

	KUrl getUrl() const;
	QString getTitle() const;

	int index; // only used for sorting

private:
	KUrl url;
	QString title;
};

class PlaylistModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	PlaylistModel(MediaWidget *mediaWidget_, QObject *parent);
	~PlaylistModel();

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	Qt::DropActions supportedDropActions() const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	QStringList mimeTypes() const;
	QMimeData *mimeData(const QModelIndexList &indexes) const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
	bool removeRows(int row, int count, const QModelIndex &parent);
	void sort(int column, Qt::SortOrder order);

	QList<PlaylistTrack> getPlaylist() const;
	void setPlaylist(const QList<PlaylistTrack> &tracks_);

public slots:
	void appendUrls(const QList<KUrl> &urls, bool enqueue = true);
	void playPreviousTrack();
	void playCurrentTrack();
	void playNextTrack();
	void playTrack(const QModelIndex &index);
	void repeatPlaylist(bool repeat_);
	void shufflePlaylist();
	void clearPlaylist();

private:
	static QList<PlaylistTrack> processUrls(const QList<KUrl> &urls);

	void playTrack(int track);

	MediaWidget *mediaWidget;
	QList<PlaylistTrack> tracks;
	int currentTrack;
	bool repeat;
};

#endif /* PLAYLISTMODEL_H */
