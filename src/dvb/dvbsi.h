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

class DvbSectionData
{
public:
	DvbSectionData(const char *data_, int size_) : length(-1), data(data_), size(size_) { }
	~DvbSectionData() { }

	bool isEmpty() const
	{
		return size == 0;
	}

	bool isValid() const
	{
		return length >= 0;
	}

protected:
	bool checkLength() const
	{
		return size >= length;
	}

	unsigned char at(int index) const
	{
		return data[index];
	}

	DvbSectionData subArray(int index, int size) const
	{
		return DvbSectionData(data + index, size);
	}

	DvbSectionData nextData() const
	{
		return DvbSectionData(data + length, size - length);
	}

	int length;

private:
	const char *data;
	int size;
};

class DvbSectionFilter : public DvbPidFilter
{
public:
	DvbSectionFilter() : continuityCounter(0), bufferValid(false) { }
	~DvbSectionFilter() { }

protected:
	virtual void processSection(const DvbSectionData &data) = 0;

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
	void processSection(const DvbSectionData &data);

	int versionNumber;
};

class DvbPmtFilter : public DvbSectionFilter
{
public:
	DvbPmtFilter() : versionNumber(-1) { }
	~DvbPmtFilter() { }

private:
	void processSection(const DvbSectionData &data);

	int versionNumber;
};

class DvbDescriptor : public DvbSectionData
{
public:
	DvbDescriptor(const DvbSectionData &data) : DvbSectionData(data)
	{
		length = 2;

		if (!checkLength()) {
			length = -1;
		} else {
			length = descriptorLength();

			if (!checkLength()) {
				length = -1;
			}
		}
	}

	~DvbDescriptor() { }

	DvbDescriptor next() const
	{
		return DvbDescriptor(nextData());
	}

	int descriptorTag() const
	{
		return at(0);
	}

private:
	/*
	 * the meaning of this field differs from the standard
	 * it returns the length of the /whole/ descriptor,
	 * not only the number of bytes immediately following this field
	 */

	int descriptorLength() const
	{
		return at(1) + 2;
	}
};

class DvbPatEntry : public DvbSectionData
{
public:
	DvbPatEntry(const DvbSectionData &data) : DvbSectionData(data)
	{
		length = 4;

		if (!checkLength()) {
			length = -1;
		}
	}

	~DvbPatEntry() { }

	DvbPatEntry next() const
	{
		return DvbPatEntry(nextData());
	}

	int programNumber() const
	{
		return (at(0) << 8) | at(1);
	}

	int pid() const
	{
		return ((at(2) << 8) | at(3)) & ((1 << 13) - 1);
	}
};

class DvbPmtEntry : public DvbSectionData
{
public:
	DvbPmtEntry(const DvbSectionData &data) : DvbSectionData(data)
	{
		length = 5;

		if (!checkLength()) {
			length = -1;
		} else {
			length = entryLength();

			if (!checkLength()) {
				length = -1;
			}
		}
	}

	~DvbPmtEntry() { }

	DvbPmtEntry next() const
	{
		return DvbPmtEntry(nextData());
	}

	int streamType() const
	{
		return at(0);
	}

	int pid() const
	{
		return ((at(1) << 8) | at(2)) & ((1 << 13) - 1);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(5, length - 5));
	}

private:
	/*
	 * the meaning of this field differs from the standard
	 * it returns the length of the /whole/ entry,
	 * not only the number of bytes immediately following this field
	 */

	int entryLength() const
	{
		return (((at(3) << 8) | at(4)) & ((1 << 12) - 1)) + 5;
	}
};

class DvbSection : public DvbSectionData
{
public:
	explicit DvbSection(const DvbSectionData &data) : DvbSectionData(data)
	{
		length = 3;

		if (!checkLength()) {
			length = -1;
		} else {
			length = sectionLength();

			if (!checkLength()) {
				length = -1;
			}
		}
	}

	~DvbSection() { }

	int tableId() const
	{
		return at(0);
	}

	bool isStandardSection() const
	{
		return (at(1) & 0x80) != 0;
	}

private:
	/*
	 * the meaning of this field differs from the standard
	 * it returns the length of the /whole/ section,
	 * not only the number of bytes immediately following this field
	 */

	int sectionLength() const
	{
		return (((at(1) << 8) | at(2)) & ((1 << 12) - 1)) + 3;
	}
};

class DvbStandardSection : public DvbSection
{
public:
	explicit DvbStandardSection(const DvbSectionData &data) : DvbSection(data)
	{
		if (length < 12) {
			length = -1;
		} else if (!isStandardSection()) {
			length = -1;
		} else if (!verifyCrc32()) {
			length = -1;
		}
	}

	~DvbStandardSection() { }

	int versionNumber() const
	{
		return (at(5) >> 1) & ((1 << 5) - 1);
	}

	bool currentNextIndicator() const
	{
		return (at(5) & 0x01) != 0;
	}

	int sectionNumber() const
	{
		return at(6);
	}

	int lastSectionNumber() const
	{
		return at(7);
	}

private:
	bool verifyCrc32() const;

	static const unsigned int crc32Table[];
};

class DvbPatSection : public DvbStandardSection
{
public:
	explicit DvbPatSection(const DvbSectionData &data) : DvbStandardSection(data)
	{
		if (length < 12) {
			length = -1;
		} else if (tableId() != 0x0) {
			length = -1;
		}
	}

	~DvbPatSection() { }

	DvbPatEntry entries() const
	{
		return DvbPatEntry(subArray(8, length - 12));
	}
};

class DvbPmtSection : public DvbStandardSection
{
public:
	explicit DvbPmtSection(const DvbSectionData &data) : DvbStandardSection(data)
	{
		if (length < 16) {
			length = -1;
		} else if (tableId() != 0x2) {
			length = -1;
		} else {
			programDescriptorsLength = programInfoLength();

			if (length < (programDescriptorsLength + 16)) {
				length = -1;
			}
		}
	}

	~DvbPmtSection() { }

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(12, programDescriptorsLength));
	}

	DvbPmtEntry entries() const
	{
		return DvbPmtEntry(subArray(12 + programDescriptorsLength,
			length - 16 - programDescriptorsLength));
	}

private:
	int programInfoLength() const
	{
		return ((at(10) << 8) | at(11)) & ((1 << 12) - 1);
	}

	int programDescriptorsLength;
};

#endif /* DVBSI_H */
