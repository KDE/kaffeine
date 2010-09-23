/*
 * dvbsifilter.cpp
 *
 * Copyright (C) 2008-2010 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbsifilter.h"

#include <KDebug>
#include "dvbsi.h"

// FIXME some debug messages may be printed too often

void DvbSectionFilter::processData(const char data[188])
{
	if ((data[3] & 0x10) == 0) {
		// no payload
		kDebug() << "no payload";
		return;
	}

	unsigned char continuity = (data[3] & 0x0f);

	if (bufferValid) {
		// check continuity
		if (continuity == continuityCounter) {
			// duplicate packets
			kDebug() << "duplicate packets";
			return;
		}

		if (continuity != ((continuityCounter + 1) & 0x0f)) {
			// discontinuity
			kDebug() << "discontinuity";
		}
	}

	continuityCounter = continuity;

	bool sectionStart = ((data[1] & 0x40) != 0);
	const char *payload;
	int payloadLength;

	if ((data[3] & 0x20) == 0) {
		// adaptation field not present
		payload = data + 4;
		payloadLength = 188 - 4;
	} else {
		// adaptation field present
		unsigned char length = data[4];

		if (length > 182) {
			kDebug() << "no payload or corrupt";
			return;
		}

		payload = data + 5 + length;
		payloadLength = 188 - 5 - length;
	}

	// be careful that playloadLength is > 0 at this point

	if (sectionStart) {
		unsigned char pointer = payload[0];

		if (pointer >= payloadLength) {
			kDebug() << "invalid pointer";
			pointer = payloadLength - 1;
		}

		if (bufferValid) {
			appendData(payload + 1, pointer);
			processSections(true);
			// be aware that the filter might have been reset
			// (buffer cleared, bufferValid set to false)
			// in this case don't poison the filter with new data
			// --> check bufferValid before calling appendData()
		} else {
			bufferValid = true;
		}

		payload += pointer + 1;
		payloadLength -= pointer + 1;
	}

	if (bufferValid) {
		appendData(payload, payloadLength);
		processSections();
	}
}

void DvbSectionFilter::appendData(const char *data, int length)
{
	int size = buffer.size();
	buffer.resize(size + length);
	memcpy(buffer.data() + size, data, length);
}

void DvbSectionFilter::processSections(bool force)
{
	while (buffer.size() >= 3) {
		int tableId = static_cast<unsigned char>(buffer.at(0));

		if (tableId == 0xff) {
			// padding
			buffer.clear();
			break;
		}

		int sectionLength = (((static_cast<unsigned char>(buffer.at(1)) & 0x0f) << 8) |
				     static_cast<unsigned char>(buffer.at(2))) + 3;

		if (sectionLength <= buffer.size()) {
			QByteArray section = buffer.left(sectionLength);
			buffer.remove(0, sectionLength);
			processSection(section);
			continue;
		}

		if (force) {
			kDebug() << "short section";
			QByteArray section = buffer;
			buffer.clear();
			processSection(section);
		}

		break;
	}

	if (force && !buffer.isEmpty()) {
		int tableId = static_cast<unsigned char>(buffer.at(0));

		if (tableId != 0xff) {
			kDebug() << "stray data";
		}

		buffer.clear();
	}
}

void DvbPmtFilter::processSection(const QByteArray &data)
{
	DvbPmtSection pmtSection(data);

	if (!pmtSection.isValid() || (pmtSection.tableId() != 0x2) ||
	    (pmtSection.programNumber() != programNumber) ||
	    (pmtSection.versionNumber() == versionNumber)) {
		return;
	}

	versionNumber = pmtSection.versionNumber();
	emit pmtSectionChanged(pmtSection);
}
