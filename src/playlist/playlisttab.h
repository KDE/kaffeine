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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef PLAYLISTTAB_H
#define PLAYLISTTAB_H

#include "../kaffeine.h"

class QTreeView;
class PlaylistModel;
class PlaylistView;

class PlaylistTab : public TabBase
{
public:
	explicit PlaylistTab(MediaWidget *mediaWidget_);
	~PlaylistTab();

	void playUrls(const QList<KUrl> &urls);

private:
	void activate();

	MediaWidget *mediaWidget;
	QLayout *mediaLayout;
	PlaylistModel *playlistModel;
	PlaylistView *playlistView;
};

#endif /* PLAYLISTTAB_H */
