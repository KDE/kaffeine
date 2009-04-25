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

#include <KLocalizedString>
#include <KUrl>

PlaylistModel::PlaylistModel(QObject *parent) : QAbstractTableModel(parent)
{
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
	if (!index.isValid() || (role != Qt::DisplayRole) || (index.row() >= tracks.size())) {
		return QVariant();
	}

	switch (index.column()) {
	case 0:
		return index.row() + 1;
	case 1:
		return tracks.at(index.row()).fileName();
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

void PlaylistModel::appendUrl(const KUrl &url)
{
	beginInsertRows(QModelIndex(), tracks.size(), tracks.size());
	tracks.append(url);
	endInsertRows();
}

void PlaylistModel::appendUrls(const QList<KUrl> &urls)
{
	beginInsertRows(QModelIndex(), tracks.size(), tracks.size() + urls.size() - 1);
	tracks += urls;
	endInsertRows();
}

KUrl PlaylistModel::getTrack(int number) const
{
	return tracks.at(number);
}

int PlaylistModel::trackCount() const
{
	return tracks.size();
}
