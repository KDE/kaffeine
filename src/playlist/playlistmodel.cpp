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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "playlistmodel.h"

#include <QMimeData>
#include <KLocalizedString>
#include <KUrl>
#include "../mediawidget.h"

class PlaylistTrack
{
public:
	explicit PlaylistTrack(const KUrl &url_);
	~PlaylistTrack() { }

	KUrl getUrl() const
	{
		return url;
	}

	QString getTitle() const
	{
		return title;
	}

	bool operator<(const PlaylistTrack &other) const
	{
		if (title != other.title) {
			return title.localeAwareCompare(other.title) < 0;
		}

		return index < other.index;
	}

	int index; // only used for sorting

private:
	KUrl url;
	QString title;
};

PlaylistTrack::PlaylistTrack(const KUrl &url_) : url(url_)
{
	title = url.fileName();
}

PlaylistModel::PlaylistModel(MediaWidget *mediaWidget_, QObject *parent) :
	QAbstractTableModel(parent), mediaWidget(mediaWidget_), currentTrack(-1)
{
	setSupportedDragActions(Qt::MoveAction);

	connect(mediaWidget, SIGNAL(playlistPrevious()), this, SLOT(playPreviousTrack()));
	connect(mediaWidget, SIGNAL(playlistPlay()), this, SLOT(playCurrentTrack()));
	connect(mediaWidget, SIGNAL(playlistNext()), this, SLOT(playNextTrack()));
	connect(mediaWidget, SIGNAL(playlistUrlsDropped(QList<KUrl>)),
		this, SLOT(appendUrls(QList<KUrl>)));
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

	return tracks.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || (index.row() >= tracks.size())) {
		return QVariant();
	}

	if (role == Qt::DecorationRole) {
		if ((index.row() == currentTrack) && (index.column() == 0)) {
			return KIcon("media-playback-start");
		}
	} else if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0:
			return index.row() + 1;
		case 1:
			return tracks.at(index.row()).getTitle();
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
		urls.append(tracks.at(row).getUrl());
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
		beginRow = tracks.size();
	}

	if (!data->hasUrls()) {
		return false;
	}

	QList<KUrl> urls = KUrl::List::fromMimeData(data);

	beginInsertRows(QModelIndex(), beginRow, beginRow + urls.size() - 1);

	for (int i = 0; i < urls.size(); ++i) {
		tracks.insert(beginRow + i, PlaylistTrack(urls[i]));
	}

	endInsertRows();

	if (beginRow <= currentTrack) {
		currentTrack += urls.size();
	}

	return true;
}

void PlaylistModel::appendUrl(const KUrl &url)
{
	bool startPlayback = tracks.isEmpty();

	beginInsertRows(QModelIndex(), tracks.size(), tracks.size());
	tracks.append(PlaylistTrack(url));
	endInsertRows();

	if (startPlayback) {
		playTrack(0);
	}
}

void PlaylistModel::appendUrls(const QList<KUrl> &urls)
{
	bool startPlayback = tracks.isEmpty();

	beginInsertRows(QModelIndex(), tracks.size(), tracks.size() + urls.size() - 1);

	foreach (const KUrl &url, urls) {
		tracks.append(PlaylistTrack(url));
	}

	endInsertRows();

	if (startPlayback) {
		playTrack(0);
	}
}

bool PlaylistModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (parent.isValid()) {
		return false;
	}

	int end = row + count;

	beginRemoveRows(QModelIndex(), row, end - 1);
	tracks.erase(tracks.begin() + row, tracks.begin() + end);
	endRemoveRows();

	if (row <= currentTrack) {
		if (end <= currentTrack) {
			currentTrack -= count;
		} else {
			playTrack(-1);
		}
	}

	return true;
}

void PlaylistModel::sort(int column, Qt::SortOrder order)
{
	if (column != 1) {
		return;
	}

	emit layoutAboutToBeChanged();

	for (int i = 0; i < tracks.size(); ++i) {
		tracks[i].index = i;
	}

	if (order == Qt::AscendingOrder) {
		qSort(tracks);
	} else {
		qSort(tracks.begin(), tracks.end(), qGreater<PlaylistTrack>());
	}

	QMap<int, int> mapping;

	for (int i = 0; i < tracks.size(); ++i) {
		mapping.insert(tracks.at(i).index, i);
	}

	QModelIndexList oldIndexes = persistentIndexList();
	QModelIndexList newIndexes;

	foreach (const QModelIndex &oldIndex, oldIndexes) {
		newIndexes.append(index(mapping.value(oldIndex.row()), oldIndex.column()));
	}

	changePersistentIndexList(oldIndexes, newIndexes);

	if (currentTrack != -1) {
		currentTrack = mapping.value(currentTrack);
	}

	emit layoutChanged();
}

void PlaylistModel::playPreviousTrack()
{
	playTrack(currentTrack - 1);
}

void PlaylistModel::playCurrentTrack()
{
	playTrack(currentTrack);
}

void PlaylistModel::playNextTrack()
{
	playTrack(currentTrack + 1);
}

void PlaylistModel::playTrack(const QModelIndex &index)
{
	playTrack(index.row());
}

void PlaylistModel::playTrack(int track)
{
	if ((track < 0) || (track >= tracks.size())) {
		track = -1;
	}

	if (track != currentTrack) {
		int oldTrack = currentTrack;
		currentTrack = track;

		if (oldTrack != -1) {
			emit dataChanged(index(oldTrack, 0), index(oldTrack, 0));
		}

		if (track != -1) {
			emit dataChanged(index(track, 0), index(track, 0));
		}
	}

	if (currentTrack != -1) {
		mediaWidget->play(tracks.at(currentTrack).getUrl());
	} else {
		mediaWidget->stop();
	}
}
