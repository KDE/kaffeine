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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>

class KUrl;

class PlaylistModel : public QAbstractTableModel
{
public:
	explicit PlaylistModel(QObject *parent);
	~PlaylistModel();

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	Qt::DropActions supportedDropActions() const;

	void appendUrl(const KUrl &url);
	void appendUrls(const QList<KUrl> &urls);

	KUrl getTrack(int number) const;
	int trackCount() const; // FIXME use rowCount() instead?

private:
	QList<KUrl> tracks;
};

#endif /* PLAYLISTMODEL_H */
