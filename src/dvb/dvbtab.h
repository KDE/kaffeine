/*
 * dvbtab.h
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBTAB_H
#define DVBTAB_H

#include "../kaffeine.h"
#include "dvbchannel.h"

class QModelIndex;
class DvbDevice;
class DvbManager;
class DvbStream;

class DvbTab : public TabBase
{
	Q_OBJECT
public:
	explicit DvbTab(MediaWidget *mediaWidget_);
	~DvbTab();

	DvbManager *getDvbManager() const
	{
		return dvbManager;
	}

	DvbDevice *getLiveDevice() const;
	DvbSharedChannel getLiveChannel() const;

public slots:
	void configureChannels();
	void configureDvb();

private slots:
	void playLive(const QModelIndex &index);
	void liveStopped();

private:
	void activate();

	MediaWidget *mediaWidget;
	DvbManager *dvbManager;
	QLayout *mediaLayout;

	// FIXME - just a demo hack
	DvbStream *liveStream;
};

#endif /* DVBTAB_H */
