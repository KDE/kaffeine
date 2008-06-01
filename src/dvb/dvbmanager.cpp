/*
 * dvbmanager.cpp
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

#include "dvbmanager.h"

#include <KGlobal>
#include <KLocale>
#include "dvbchannel.h"
#include "dvbdevice.h"

DvbManager::DvbManager(QObject *parent) : QObject(parent)
{
	// FIXME
	scanFilesDate = QDate(2008, 4, 26);

	// FIXME
	sourceList << "Astra-19.2E";

	// FIXME
	channelModel = new DvbChannelModel(this);
	QList<DvbChannel> list;
	DvbChannel channel;
	channel.name = "sample";
	channel.number = 1;
	channel.source = "Astra19.2E";
	channel.networkId = 1;
	channel.transportStreamId = 1079;
	channel.serviceId = 28006;
	channel.videoPid = 110;
	channel.audioPid = 120;
	channel.setTransponder(DvbSharedTransponder(new DvbSTransponder(DvbSTransponder::Horizontal, 11953000, 27500000, DvbSTransponder::FecAuto)));
	list.append(channel);
	channel.name = "channel";
	channel.number = 2;
	list.append(channel);
	channel.name = "test";
	channel.number = 3;
	list.append(channel);
	channelModel->setList(list);

	deviceManager = new DvbDeviceManager(this);
}

DvbManager::~DvbManager()
{
}

QString DvbManager::getScanFilesDate() const
{
	return KGlobal::locale()->formatDate(scanFilesDate, KLocale::ShortDate);
}

QList<DvbDevice *> DvbManager::getDeviceList() const
{
	return deviceManager->getDeviceList();
}

DvbSharedConfig DvbManager::getConfig(const QString &source) const
{
	// FIXME
	return DvbSharedConfig(new DvbSConfig("Astra-19.2E"));
}

QList<DvbSharedTransponder> DvbManager::getTransponderList(const QString &source) const
{
	// FIXME
	QList<DvbSharedTransponder> transponderList;
	transponderList.append(DvbSharedTransponder(new DvbSTransponder(DvbSTransponder::Horizontal, 11836000, 27500000, DvbSTransponder::Fec3_4)));
	transponderList.append(DvbSharedTransponder(new DvbSTransponder(DvbSTransponder::Vertical, 12551500, 22000000, DvbSTransponder::Fec5_6)));
	return transponderList;
}
