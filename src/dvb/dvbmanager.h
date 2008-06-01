/*
 * dvbmanager.h
 *
 * Copyright (C) 2008 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBMANAGER_H
#define DVBMANAGER_H

#include <QDate>
#include <QStringList>
#include "dvbchannel.h"
#include "dvbconfig.h"

class DvbDevice;
class DvbDeviceManager;

class DvbManager : public QObject
{
public:
	explicit DvbManager(QObject *parent);
	~DvbManager();

	/*
	 * returns the formatted (short) date of the last scan file update
	 */

	QString getScanFilesDate() const;

	QStringList getSourceList() const
	{
		return sourceList;
	}

	DvbChannelModel *getChannelModel() const
	{
		return channelModel;
	}

	QList<DvbDevice *> getDeviceList() const;

	DvbSharedConfig getConfig(const QString &source) const;

	QList<DvbSharedTransponder> getTransponderList(const QString &source) const;

private:
	QDate scanFilesDate;
	QStringList sourceList;
	DvbChannelModel *channelModel;
	DvbDeviceManager *deviceManager;
};

#endif /* DVBMANAGER_H */
