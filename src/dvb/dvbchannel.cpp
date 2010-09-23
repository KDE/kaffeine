/*
 * dvbchannel.cpp
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

#include "dvbchannel.h"

#include <QDataStream>
#include <QTextStream>

void DvbChannelBase::readChannel(QDataStream &stream)
{
	int type;
	stream >> type;

	stream >> name;
	stream >> number;

	stream >> source;

	switch (type) {
	case DvbTransponderBase::DvbC: {
		DvbCTransponder dvbCTransponder;
		dvbCTransponder.readTransponder(stream);
		transponder = DvbTransponder(dvbCTransponder);
		break;
	    }
	case DvbTransponderBase::DvbS: {
		DvbSTransponder dvbSTransponder;
		dvbSTransponder.readTransponder(stream);
		transponder = DvbTransponder(dvbSTransponder);
		break;
	    }
	case DvbTransponderBase::DvbS2: {
		DvbS2Transponder dvbS2Transponder;
		dvbS2Transponder.readTransponder(stream);
		transponder = DvbTransponder(dvbS2Transponder);
		break;
	    }
	case DvbTransponderBase::DvbT: {
		DvbTTransponder dvbTTransponder;
		dvbTTransponder.readTransponder(stream);
		transponder = DvbTransponder(dvbTTransponder);
		break;
	    }
	case DvbTransponderBase::Atsc: {
		AtscTransponder atscTransponder;
		atscTransponder.readTransponder(stream);
		transponder = DvbTransponder(atscTransponder);
		break;
	    }
	default:
		stream.setStatus(QDataStream::ReadCorruptData);
		return;
	}

	stream >> networkId;
	stream >> transportStreamId;
	int serviceId;
	stream >> serviceId;
	stream >> pmtPid;

	stream >> pmtSection;
	int videoPid;
	stream >> videoPid;
	stream >> audioPid;

	int flags;
	stream >> flags;
	hasVideo = (videoPid >= 0);
	isScrambled = (flags & 0x1) != 0;
}
