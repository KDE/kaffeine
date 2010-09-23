/*
 * dvbchannel.h
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <QSharedData>
#include <QString>
#include "dvbtransponder.h"

class DvbChannelBase
{
public:
	DvbChannelBase() : number(-1), networkId(-1), transportStreamId(-1), pmtPid(-1),
		audioPid(-1), hasVideo(false), isScrambled(false) { }
	~DvbChannelBase() { }

	void readChannel(QDataStream &stream);

	int getServiceId() const
	{
		if (pmtSection.size() < 5) {
			return -1;
		}

		return ((static_cast<unsigned char>(pmtSection.at(3)) << 8) |
			static_cast<unsigned char>(pmtSection.at(4)));
	}

	void setServiceId(int serviceId)
	{
		if (pmtSection.size() < 5) {
			return;
		}

		pmtSection[3] = (serviceId >> 8);
		pmtSection[4] = (serviceId & 0xff);
	}

	QString name;
	int number;

	QString source;
	DvbTransponder transponder;
	int networkId; // may be -1 (not present); ATSC meaning: source id
	int transportStreamId;
	int pmtPid;

	QByteArray pmtSection;
	int audioPid; // may be -1 (not present)
	bool hasVideo;
	bool isScrambled;
};

class DvbChannel : public DvbChannelBase, public QSharedData
{
public:
	DvbChannel() { }
	explicit DvbChannel(const DvbChannelBase &channel) : DvbChannelBase(channel) { }
	~DvbChannel() { }

	DvbChannel &operator=(const DvbChannel &other)
	{
		DvbChannelBase *base = this;
		*base = other;
		return (*this);
	}
};

Q_DECLARE_TYPEINFO(QSharedDataPointer<DvbChannel>, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QExplicitlySharedDataPointer<const DvbChannel>, Q_MOVABLE_TYPE);

#endif /* DVBCHANNEL_H */
