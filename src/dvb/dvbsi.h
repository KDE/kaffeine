/*
 * dvbsi.h
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

#ifndef DVBSI_H
#define DVBSI_H

#include "dvbdevice.h"

class DvbUnsignedByteArray
{
public:
	DvbUnsignedByteArray(const QByteArray &data_, int pos_) : data(data_), pos(pos_) { }
	~DvbUnsignedByteArray() { }

	int size() const
	{
		return data.size() - pos;
	}

	unsigned int at(int index) const
	{
		return static_cast<unsigned char> (data.at(pos + index));
	}

private:
	QByteArray data;
	int pos;
};

class DvbSection;
class DvbPublicSection;

class DvbSectionFilter : public DvbPidFilter
{
public:
	DvbSectionFilter() : continuityCounter(0), bufferValid(false) { }
	~DvbSectionFilter() { }

protected:
	virtual void processSection(const DvbSection &section) = 0;

private:
	void processData(const char data[188]);

	void appendData(const char *data, int length);

	QByteArray buffer;
	int continuityCounter;
	bool bufferValid;
};

class DvbPatFilter : public DvbSectionFilter
{
public:
	DvbPatFilter() : versionNumber(-1) { }
	~DvbPatFilter() { }

private:
	void processSection(const DvbSection &section);

	int versionNumber;
};

class DvbSection
{
public:
	explicit DvbSection(const DvbUnsignedByteArray &data_) : data(data_) { }
	~DvbSection() { }

	bool isValid() const
	{
		if (data.size() < 3) {
			return false;
		}

		return data.size() >= sectionLength();
	}

	int tableId() const
	{
		return data.at(0);
	}

	bool isPublicSection() const
	{
		return (data.at(1) & 0x80) != 0;
	}

	/*
	 * the meaning of this field differs from the standard
	 * it returns the length of the /whole/ section,
	 * not only the number of bytes immediately following this field
	 */

	int sectionLength() const
	{
		return (((data.at(1) << 8) | data.at(2)) & ((1 << 12) - 1)) + 3;
	}

protected:
	DvbUnsignedByteArray data;
};

class DvbPublicSection : public DvbSection
{
public:
	/*
	 * precondition: the section must be valid and isPublicSection() should return true
	 */

	explicit DvbPublicSection(const DvbSection &section) : DvbSection(section) { }
	~DvbPublicSection() { }

	bool isValid() const
	{
		return sectionLength() >= 12;
	}

	int tableIdExtension() const
	{
		return (data.at(3) << 8) | data.at(4);
	}

	int versionNumber() const
	{
		return (data.at(5) >> 1) & ((1 << 5) - 1);
	}

	bool currentNextIndicator() const
	{
		return (data.at(5) & 0x01) != 0;
	}

	int sectionNumber() const
	{
		return data.at(6);
	}

	int lastSectionNumber() const
	{
		return data.at(7);
	}

	bool verifyCrc32() const;

private:
	static const unsigned int crc32Table[];
};

#endif /* DVBSI_H */
