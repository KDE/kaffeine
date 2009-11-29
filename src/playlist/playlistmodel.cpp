/*
 * playlistmodel.cpp
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

#include "playlistmodel.h"

#include <QDir>
#include <QMimeData>
#include <KLocalizedString>
#include "../mediawidget.h"

PlaylistTrack::PlaylistTrack(const KUrl &url_) : url(url_)
{
	title = url.fileName();
}

PlaylistTrack::~PlaylistTrack()
{
}

KUrl PlaylistTrack::getUrl() const
{
	return url;
}

QString PlaylistTrack::getTitle() const
{
	return title;
}

PlaylistModel::PlaylistModel(MediaWidget *mediaWidget_, Playlist *playlist, QObject *parent) :
	QAbstractTableModel(parent), mediaWidget(mediaWidget_), random(false), repeat(false)
{
	setSupportedDragActions(Qt::MoveAction);

	connect(mediaWidget, SIGNAL(playlistPrevious()), this, SLOT(playPreviousTrack()));
	connect(mediaWidget, SIGNAL(playlistPlay()), this, SLOT(playCurrentTrack()));
	connect(mediaWidget, SIGNAL(playlistNext()), this, SLOT(playNextTrack()));
	connect(mediaWidget, SIGNAL(playlistUrlsDropped(QList<KUrl>)),
		this, SLOT(appendUrls(QList<KUrl>)));

	visiblePlaylist = playlist;
	activePlaylist = playlist;
}

PlaylistModel::~PlaylistModel()
{
}

int PlaylistModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 2;
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return visiblePlaylist->tracks.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || (index.row() >= visiblePlaylist->tracks.size())) {
		return QVariant();
	}

	if (role == Qt::DecorationRole) {
		if ((index.row() == visiblePlaylist->currentTrack) && (index.column() == 0)) {
			return KIcon("arrow-right");
		}
	} else if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0:
			return index.row() + 1;
		case 1:
			return visiblePlaylist->tracks.at(index.row()).getTitle();
		}
	}

	return QVariant();
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18n("Number");
	case 1:
		return i18n("Title");
	}

	return QVariant();
}

Qt::DropActions PlaylistModel::supportedDropActions() const
{
	return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);

	if (index.isValid()) {
		return defaultFlags | Qt::ItemIsDragEnabled;
	} else {
		return defaultFlags | Qt::ItemIsDropEnabled;
	}
}

QStringList PlaylistModel::mimeTypes() const
{
	return QStringList("text/uri-list");
}

QMimeData *PlaylistModel::mimeData(const QModelIndexList &indexes) const
{
	QList<int> rows;

	foreach (const QModelIndex &index, indexes) {
		if (index.isValid() && (index.column() == 0)) {
			rows.append(index.row());
		}
	}

	qSort(rows);

	KUrl::List urls;

	foreach (int row, rows) {
		urls.append(visiblePlaylist->tracks.at(row).getUrl());
	}

	QMimeData *mimeData = new QMimeData();
	urls.populateMimeData(mimeData);
	return mimeData;
}

bool PlaylistModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int, const QModelIndex &parent)
{
	if (action == Qt::IgnoreAction) {
		return true;
	}

	int beginRow = 0;

	if (row != -1) {
		beginRow = row;
	} else if (parent.isValid()) {
		beginRow = parent.row();
	} else {
		beginRow = visiblePlaylist->tracks.size();
	}

	if (!data->hasUrls()) {
		return false;
	}

	QList<PlaylistTrack> newTracks = processUrls(KUrl::List::fromMimeData(data));

	if (newTracks.size() < 1) {
		return false;
	}

	beginInsertRows(QModelIndex(), beginRow, beginRow + newTracks.size() - 1);

	for (int i = 0; i < newTracks.size(); ++i) {
		visiblePlaylist->tracks.insert(beginRow + i, newTracks.at(i));
	}

	endInsertRows();

	if (visiblePlaylist->currentTrack >= beginRow) {
		visiblePlaylist->currentTrack += newTracks.size();
	}

	return true;
}

void PlaylistModel::appendUrls(const QList<KUrl> &urls, bool enqueue)
{
	QList<PlaylistTrack> newTracks = processUrls(urls);

	if (newTracks.size() < 1) {
		return;
	}

	int oldTracksSize = visiblePlaylist->tracks.size();

	beginInsertRows(QModelIndex(), visiblePlaylist->tracks.size(), visiblePlaylist->tracks.size() + newTracks.size() - 1);
	visiblePlaylist->tracks += newTracks;
	endInsertRows();

	if ((oldTracksSize == 0) || !enqueue) {
		playTrack(visiblePlaylist, oldTracksSize);
	}
}

bool PlaylistModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (parent.isValid()) {
		return false;
	}

	int end = row + count;

	beginRemoveRows(QModelIndex(), row, end - 1);
	visiblePlaylist->tracks.erase(visiblePlaylist->tracks.begin() + row, visiblePlaylist->tracks.begin() + end);
	endRemoveRows();

	if (visiblePlaylist->currentTrack >= row) {
		if (visiblePlaylist->currentTrack >= end) {
			visiblePlaylist->currentTrack -= count;
		} else {
			playTrack(visiblePlaylist, -1);
		}
	}

	return true;
}

static bool playlistTitleLessThan(const PlaylistTrack &x, const PlaylistTrack &y)
{
	if (x.getTitle() != y.getTitle()) {
		return x.getTitle().localeAwareCompare(y.getTitle()) < 0;
	}

	return x.index < y.index;
}

static bool playlistTitleGreater(const PlaylistTrack &x, const PlaylistTrack &y)
{
	if (x.getTitle() != y.getTitle()) {
		return x.getTitle().localeAwareCompare(y.getTitle()) > 0;
	}

	return x.index > y.index;
}

void PlaylistModel::sort(int column, Qt::SortOrder order)
{
	if ((visiblePlaylist->tracks.size() < 2) || (column != 1)) {
		return;
	}

	emit layoutAboutToBeChanged();

	for (int i = 0; i < visiblePlaylist->tracks.size(); ++i) {
		visiblePlaylist->tracks[i].index = i;
	}

	if (order == Qt::AscendingOrder) {
		qSort(visiblePlaylist->tracks.begin(), visiblePlaylist->tracks.end(), playlistTitleLessThan);
	} else {
		qSort(visiblePlaylist->tracks.begin(), visiblePlaylist->tracks.end(), playlistTitleGreater);
	}

	QMap<int, int> mapping;

	for (int i = 0; i < visiblePlaylist->tracks.size(); ++i) {
		mapping.insert(visiblePlaylist->tracks.at(i).index, i);
	}

	QModelIndexList oldIndexes = persistentIndexList();
	QModelIndexList newIndexes;

	foreach (const QModelIndex &oldIndex, oldIndexes) {
		newIndexes.append(index(mapping.value(oldIndex.row()), oldIndex.column()));
	}

	changePersistentIndexList(oldIndexes, newIndexes);

	if (visiblePlaylist->currentTrack != -1) {
		visiblePlaylist->currentTrack = mapping.value(visiblePlaylist->currentTrack);
	}

	emit layoutChanged();
}

void PlaylistModel::setPlaylist(Playlist *playlist)
{
	if (visiblePlaylist != playlist) {
		visiblePlaylist = playlist;
		reset();
	}
}

void PlaylistModel::setCurrentPlaylist(Playlist *playlist)
{
	if (visiblePlaylist != playlist) {
		visiblePlaylist = playlist;
		reset();
	}

	if (activePlaylist != playlist) {
		playTrack(playlist, -1);
	}
}

bool PlaylistModel::getRandom() const
{
	return random;
}

bool PlaylistModel::getRepeat() const
{
	return repeat;
}

void PlaylistModel::playPreviousTrack()
{
	playTrack(activePlaylist, activePlaylist->currentTrack - 1);
}

void PlaylistModel::playCurrentTrack()
{
	if (activePlaylist->currentTrack != -1) {
		playTrack(activePlaylist, activePlaylist->currentTrack);
	} else {
		playTrack(visiblePlaylist, 0);
	}
}

void PlaylistModel::playNextTrack()
{
	if (random) {
		int newTrack;

		if (activePlaylist->currentTrack < 0) {
			newTrack = qrand() % activePlaylist->tracks.size();
		} else {
			newTrack = qrand() % (activePlaylist->tracks.size() - 1);

			if (newTrack >= activePlaylist->currentTrack) {
				++newTrack;
			}
		}

		playTrack(activePlaylist, newTrack);
	} else if (repeat &&
		   ((activePlaylist->currentTrack + 1) >= activePlaylist->tracks.size())) {
		playTrack(activePlaylist, 0);
	} else {
		playTrack(activePlaylist, activePlaylist->currentTrack + 1);
	}
}

void PlaylistModel::playTrack(const QModelIndex &index)
{
	playTrack(visiblePlaylist, index.row());
}

void PlaylistModel::setRandom(bool random_)
{
	random = random_;
}

void PlaylistModel::setRepeat(bool repeat_)
{
	repeat = repeat_;
}

void PlaylistModel::clearPlaylist()
{
	visiblePlaylist->tracks.clear();

	if (visiblePlaylist->currentTrack != -1) {
		playTrack(visiblePlaylist, -1);
	}

	reset();
}

QList<PlaylistTrack> PlaylistModel::processUrls(const QList<KUrl> &urls)
{
	QList<PlaylistTrack> newTracks;

	foreach (const KUrl &url, urls) {
		QString localFile = url.toLocalFile();

		if (!localFile.isEmpty() && QFileInfo(localFile).isDir()) {
			QDir dir(localFile);

			QString extensionFilter = MediaWidget::extensionFilter();
			extensionFilter.truncate(extensionFilter.indexOf('|'));

			QStringList entries = dir.entryList(extensionFilter.split(' '), QDir::Files,
							    QDir::Name | QDir::LocaleAware);

			foreach (const QString &entry, entries) {
				newTracks.append(PlaylistTrack(KUrl::fromLocalFile(dir.filePath(entry))));
			}
		} else {
			newTracks.append(PlaylistTrack(url));
		}
	}

	return newTracks;
}

void PlaylistModel::playTrack(Playlist *playlist, int track)
{
	Playlist *oldPlaylist = activePlaylist;
	activePlaylist = playlist;

	if ((track < 0) || (track >= activePlaylist->tracks.size())) {
		track = -1;
	}

	if ((oldPlaylist == visiblePlaylist) && (visiblePlaylist->currentTrack != -1)) {
		QModelIndex modelIndex = index(visiblePlaylist->currentTrack, 0);
		emit dataChanged(modelIndex, modelIndex);
	}

	oldPlaylist->currentTrack = -1;
	activePlaylist->currentTrack = track;

	if ((activePlaylist == visiblePlaylist) && (track != -1)) {
		QModelIndex modelIndex = index(track, 0);
		emit dataChanged(modelIndex, modelIndex);
	}

	if (track != -1) {
		mediaWidget->play(activePlaylist->tracks.at(track).getUrl());
		emit currentPlaylistChanged(activePlaylist);
	} else {
		mediaWidget->stop();
		emit currentPlaylistChanged(NULL);
	}
}
