/*
 * dvbliveview_p.h
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBLIVEVIEW_P_H
#define DVBLIVEVIEW_P_H

#include <QFile>
#include "../osdwidget.h"
#include "dvbepg.h"
#include "dvbsi.h"

class MediaWidget;

class DvbOsd : public OsdObject
{
public:
	enum OsdLevel {
		Off,
		ShortOsd,
		LongOsd
	};

	DvbOsd() : level(Off) { }
	~DvbOsd() { }

	void init(OsdLevel level_, const QString &channelName_,
		const QList<DvbSharedEpgEntry> &epgEntries);

	OsdLevel level;

private:
	QPixmap paintOsd(QRect &rect, const QFont &font, Qt::LayoutDirection direction);

	QString channelName;
	DvbEpgEntry firstEntry;
	DvbEpgEntry secondEntry;
};

class DvbLiveViewInternal : public DvbPidFilter
{
public:
	DvbLiveViewInternal() : mediaWidget(NULL) { }
	~DvbLiveViewInternal() { }

	MediaWidget *mediaWidget;
	DvbPmtFilter pmtFilter;
	QByteArray pmtSectionData;
	DvbSectionGenerator patGenerator;
	DvbSectionGenerator pmtGenerator;
	QByteArray buffer;
	QFile timeShiftFile;
	DvbOsd dvbOsd;

private:
	void processData(const char data[188]);
};

#endif /* DVBLIVEVIEW_P_H */
