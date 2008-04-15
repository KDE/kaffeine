/*
 * dvbtab.h
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#include <QDate>
#include <QMap>
#include "../manager.h"

class QModelIndex;
namespace Solid
{
class Device;
};
class DvbChannel;
class DvbChannelModel;
class DvbDevice;
class DvbStream;

class DvbTab : public TabBase
{
	Q_OBJECT
public:
	DvbTab(Manager *manager_);
	~DvbTab();

	/*
	 * returns the date of the last scan file update
	 */

	QDate getScanFilesDate() const
	{
		// FIXME
		return QDate(2008, 3, 10);
	}

	QStringList getSourceList() const
	{
		// FIXME
		return QStringList();
	}

	DvbChannelModel *getChannelModel() const
	{
		return channelModel;
	}

	DvbDevice *getLiveDevice() const
	{
		return liveDevice;
	}

	const DvbChannel *getLiveChannel() const
	{
		return liveChannel;
	}

public slots:
	void configureChannels();
	void configureDvb();

private slots:
	void componentAdded(const QString &udi);
	void componentRemoved(const QString &udi);
	void playLive(const QModelIndex &index);
	void livePaused(bool paused);
	void liveStopped();

private:
	void activate();

	void customEvent(QEvent *event);
	void componentAdded(const Solid::Device &component);

	QLayout *mediaLayout;
	QMap<int, DvbDevice *> devices;

	DvbChannelModel *channelModel;

	DvbDevice *liveDevice;
	const DvbChannel *liveChannel;

	// FIXME - just a demo hack
	DvbStream *dvbStream;
};

#endif /* DVBTAB_H */
