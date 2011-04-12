/*
 * playlistmodel.h
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

#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <QTime>
#include "../mediawidget.h"

class PlaylistTrack
{
public:
	PlaylistTrack() : trackNumber(-1), currentSubtitle(-1) { }
	~PlaylistTrack() { }

	KUrl url;
	QString title;
	QString artist;
	QString album;
	int trackNumber;
	QTime length;
	QList<KUrl> subtitles;
	int currentSubtitle;
};

class Playlist
{
public:
	Playlist() : currentTrack(-1) { }
	~Playlist() { }

	const PlaylistTrack &at(int index) const
	{
		return tracks.at(index);
	}

	enum Format {
		Invalid,
		Kaffeine, // read-only
		M3U,
		PLS,
		XSPF
	};

	bool load(const KUrl &url_, Format format);
	bool save(Format format) const;

	KUrl url;
	QString title;
	QList<PlaylistTrack> tracks;
	int currentTrack;

private:
	void appendTrack(PlaylistTrack &track);
	KUrl fromFileOrUrl(const QString &fileOrUrl) const;
	KUrl fromRelativeUrl(const QString &trackUrlString) const;
	QString toFileOrUrl(const KUrl &trackUrl) const;
	QString toRelativeUrl(const KUrl &trackUrl) const;

	bool loadKaffeinePlaylist(QIODevice *device);
	bool loadM3UPlaylist(QIODevice *device);
	void saveM3UPlaylist(QIODevice *device) const;
	bool loadPLSPlaylist(QIODevice *device);
	void savePLSPlaylist(QIODevice *device) const;
	bool loadXSPFPlaylist(QIODevice *device);
	void saveXSPFPlaylist(QIODevice *device) const;
};

class PlaylistModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	PlaylistModel(Playlist *visiblePlaylist_, QObject *parent);
	~PlaylistModel();

	void setVisiblePlaylist(Playlist *visiblePlaylist_);
	Playlist *getVisiblePlaylist() const;

	void appendUrls(Playlist *playlist, const QList<KUrl> &urls, bool playImmediately);
	void removeRows(Playlist *playlist, int row, int count);
	void setCurrentTrack(Playlist *playlist, int track);
	void updateTrackLength(Playlist *playlist, int length);
	void updateTrackMetadata(Playlist *playlist,
		const QMap<MediaWidget::MetadataType, QString> &metadata);

public slots:
	void clearVisiblePlaylist();

signals:
	void appendPlaylist(Playlist *playlist, bool playImmediately);
	void playTrack(Playlist *playlist, int track);

private:
	void insertUrls(Playlist *playlist, int row, const QList<KUrl> &urls,
		bool playImmediately);

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	bool removeRows(int row, int count, const QModelIndex &parent);
	void sort(int column, Qt::SortOrder order);

	Qt::ItemFlags flags(const QModelIndex &index) const;
	QStringList mimeTypes() const;
	Qt::DropActions supportedDropActions() const;
	QMimeData *mimeData(const QModelIndexList &indexes) const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
		const QModelIndex &parent);

	Playlist *visiblePlaylist;
};

#endif /* PLAYLISTMODEL_H */
