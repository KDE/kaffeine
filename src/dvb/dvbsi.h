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

#include "dvbchannel.h"
#include "dvbdevice.h"

class DvbSectionData;

class DvbSectionFilter : public QObject, public DvbPidFilter
{
	Q_OBJECT
public:
	DvbSectionFilter() : continuityCounter(-1), bufferValid(false) { }
	~DvbSectionFilter() { }

	void resetFilter()
	{
		buffer.clear();
		continuityCounter = -1;
		bufferValid = false;
	}

signals:
	void sectionFound(const DvbSectionData &data);

private:
	void processData(const char data[188]);

	void appendData(const char *data, int length);

	QByteArray buffer;
	int continuityCounter;
	bool bufferValid;
};

class DvbSectionData
{
	friend class DvbSiText;
public:
	DvbSectionData(const char *data_, int size_) : size(size_), length(0), data(data_) { }
	~DvbSectionData() { }

	bool isEmpty() const
	{
		return size == 0;
	}

	bool isValid() const
	{
		return length > 0;
	}

protected:
	unsigned char at(int index) const
	{
		return data[index];
	}

	DvbSectionData genericNext() const
	{
		return DvbSectionData(data + length, size - length);
	}

	DvbSectionData subArray(int index, int size) const
	{
		return DvbSectionData(data + index, size);
	}

	int size;
	int length;

private:
	const char *data;
};

class DvbSection : public DvbSectionData
{
public:
	explicit DvbSection(const DvbSectionData &data) : DvbSectionData(data)
	{
		if (size < 3) {
			length = 0;
			return;
		}

		length = sectionLength() + 3;

		if (size < length) {
			length = 0;
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

	int sectionLength() const
	{
		return ((at(1) & 0xf) << 8) | at(2);
	}
};

class DvbStandardSection : public DvbSection
{
public:
	explicit DvbStandardSection(const DvbSectionData &data) : DvbSection(data)
	{
		if (length < 12) {
			length = 0;
		} else if (!isStandardSection()) {
			length = 0;
		} else if (!verifyCrc32()) {
			length = 0;
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

class DvbSiText
{
public:
	static QString convertText(const DvbSectionData &text);

private:
	enum TextEncoding
	{
		Iso6937		=  0,
		Iso8859_1	=  1,
		Iso8859_2	=  2,
		Iso8859_3	=  3,
		Iso8859_4	=  4,
		Iso8859_5	=  5,
		Iso8859_6	=  6,
		Iso8859_7	=  7,
		Iso8859_8	=  8,
		Iso8859_9	=  9,
		Iso8859_10	= 10,
		Iso8859_11	= 11,
		Iso8859_12	= 12,
		Iso8859_13	= 13,
		Iso8859_14	= 14,
		Iso8859_15	= 15,
		Gb2312		= 16,
		Big5		= 17,
		Utf_8		= 18,

		EncodingTypeMax	= 18
	};

	DvbSiText() { }
	~DvbSiText() { }

	static QTextCodec *codecTable[EncodingTypeMax + 1];
};

class DvbDescriptor : public DvbSectionData
{
public:
	explicit DvbDescriptor(const DvbSectionData &data) : DvbSectionData(data)
	{
		if (size < 2) {
			length = 0;
			return;
		}

		length = descriptorLength() + 2;

		if (size < length) {
			length = 0;
		}
	}

	~DvbDescriptor() { }

	int descriptorTag() const
	{
		return at(0);
	}

	int descriptorLength() const
	{
		return at(1);
	}

	DvbDescriptor next() const
	{
		return DvbDescriptor(genericNext());
	}

protected:
	static int bcdToInt(unsigned int bcd, int multiplier)
	{
		int value = 0;

		while (bcd != 0) {
			value += (bcd & 0xf) * multiplier;
			multiplier *= 10;
			bcd >>= 4;
		}

		return value;
	}
};

// everything below this line is automatically generated

class DvbServiceDescriptor : public DvbDescriptor
{
public:
	explicit DvbServiceDescriptor(const DvbSectionData &data_) : DvbDescriptor(data_)
	{
		if (length < 5) {
			length = 0;
			return;
		}

		providerNameLength = at(3);

		if (length < (5 + providerNameLength)) {
			length = 0;
			return;
		}

		serviceNameLength = at(4 + providerNameLength);

		if (length < (5 + providerNameLength + serviceNameLength)) {
			length = 0;
			return;
		}
	}

	~DvbServiceDescriptor() { }

	QString providerName() const
	{
		return DvbSiText::convertText(subArray(4, providerNameLength));
	}

	QString serviceName() const
	{
		return DvbSiText::convertText(subArray(5 + providerNameLength, serviceNameLength));
	}

private:
	int providerNameLength;
	int serviceNameLength;
};

class DvbCableDescriptor : public DvbDescriptor
{
public:
	explicit DvbCableDescriptor(const DvbSectionData &data_) : DvbDescriptor(data_)
	{
		if (length < 13) {
			length = 0;
			return;
		}
	}

	~DvbCableDescriptor() { }

	int frequency() const
	{
		return (at(2) << 24) | (at(3) << 16) | (at(4) << 8) | at(5);
	}

	int modulation() const
	{
		return at(8);
	}

	int symbolRate() const
	{
		return (at(9) << 20) | (at(10) << 12) | (at(11) << 4) | (at(12) >> 4);
	}

	int fecRate() const
	{
		return (at(12) & 0xf);
	}
};

class DvbSatelliteDescriptor : public DvbDescriptor
{
public:
	explicit DvbSatelliteDescriptor(const DvbSectionData &data_) : DvbDescriptor(data_)
	{
		if (length < 13) {
			length = 0;
			return;
		}
	}

	~DvbSatelliteDescriptor() { }

	int frequency() const
	{
		return (at(2) << 24) | (at(3) << 16) | (at(4) << 8) | at(5);
	}

	int polarization() const
	{
		return ((at(8) & 0x7f) >> 5);
	}

	bool isDvbS2() const
	{
		return ((at(8) & 0x4) != 0);
	}

	int symbolRate() const
	{
		return (at(9) << 20) | (at(10) << 12) | (at(11) << 4) | (at(12) >> 4);
	}

	int fecRate() const
	{
		return (at(12) & 0xf);
	}
};

class DvbTerrestrialDescriptor : public DvbDescriptor
{
public:
	explicit DvbTerrestrialDescriptor(const DvbSectionData &data_) : DvbDescriptor(data_)
	{
		if (length < 13) {
			length = 0;
			return;
		}
	}

	~DvbTerrestrialDescriptor() { }

	int frequency() const
	{
		return (at(2) << 24) | (at(3) << 16) | (at(4) << 8) | at(5);
	}

	int bandwidth() const
	{
		return (at(6) >> 5);
	}

	int constellation() const
	{
		return (at(7) >> 6);
	}

	int hierarchy() const
	{
		return ((at(7) & 0x3f) >> 3);
	}

	int fecHighRate() const
	{
		return (at(7) & 0x7);
	}

	int fecLowRate() const
	{
		return (at(8) >> 5);
	}

	int guardInterval() const
	{
		return ((at(8) & 0x1f) >> 3);
	}

	int transmissionMode() const
	{
		return ((at(8) & 0x7) >> 1);
	}
};

class DvbPatSectionEntry : public DvbSectionData
{
public:
	explicit DvbPatSectionEntry(const DvbSectionData &data_) : DvbSectionData(data_)
	{
		if (size < 4) {
			length = 0;
			return;
		}

		length = 4;
	}

	~DvbPatSectionEntry() { }

	DvbPatSectionEntry next() const
	{
		return DvbPatSectionEntry(genericNext());
	}

	int programNumber() const
	{
		return (at(0) << 8) | at(1);
	}

	int pid() const
	{
		return ((at(2) & 0x1f) << 8) | at(3);
	}
};

class DvbPmtSectionEntry : public DvbSectionData
{
public:
	explicit DvbPmtSectionEntry(const DvbSectionData &data_) : DvbSectionData(data_)
	{
		if (size < 5) {
			length = 0;
			return;
		}

		length = (((at(3) & 0xf) << 8) | at(4)) + 5;

		if (size < length) {
			length = 0;
			return;
		}
	}

	~DvbPmtSectionEntry() { }

	DvbPmtSectionEntry next() const
	{
		return DvbPmtSectionEntry(genericNext());
	}

	int streamType() const
	{
		return at(0);
	}

	int pid() const
	{
		return ((at(1) & 0x1f) << 8) | at(2);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(5, length - 5));
	}
};

class DvbSdtSectionEntry : public DvbSectionData
{
public:
	explicit DvbSdtSectionEntry(const DvbSectionData &data_) : DvbSectionData(data_)
	{
		if (size < 5) {
			length = 0;
			return;
		}

		length = (((at(3) & 0xf) << 8) | at(4)) + 5;

		if (size < length) {
			length = 0;
			return;
		}
	}

	~DvbSdtSectionEntry() { }

	DvbSdtSectionEntry next() const
	{
		return DvbSdtSectionEntry(genericNext());
	}

	int serviceId() const
	{
		return (at(0) << 8) | at(1);
	}

	bool isScrambled() const
	{
		return ((at(3) & 0x10) != 0);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(5, length - 5));
	}
};

class DvbNitSectionEntry : public DvbSectionData
{
public:
	explicit DvbNitSectionEntry(const DvbSectionData &data_) : DvbSectionData(data_)
	{
		if (size < 6) {
			length = 0;
			return;
		}

		length = (((at(4) & 0xf) << 8) | at(5)) + 6;

		if (size < length) {
			length = 0;
			return;
		}
	}

	~DvbNitSectionEntry() { }

	DvbNitSectionEntry next() const
	{
		return DvbNitSectionEntry(genericNext());
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(6, length - 6));
	}
};

class DvbPatSection : public DvbStandardSection
{
public:
	explicit DvbPatSection(const DvbSectionData &data_) : DvbStandardSection(data_)
	{
		if (length < 12) {
			length = 0;
			return;
		}
	}

	~DvbPatSection() { }

	DvbPatSectionEntry entries() const
	{
		return DvbPatSectionEntry(subArray(8, length - 12));
	}
};

class DvbPmtSection : public DvbStandardSection
{
public:
	explicit DvbPmtSection(const DvbSectionData &data_) : DvbStandardSection(data_)
	{
		if (length < 16) {
			length = 0;
			return;
		}

		descriptorsLength = ((at(10) & 0xf) << 8) | at(11);

		if (length < (16 + descriptorsLength)) {
			length = 0;
			return;
		}
	}

	~DvbPmtSection() { }

	int programNumber() const
	{
		return (at(3) << 8) | at(4);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(12, descriptorsLength));
	}

	DvbPmtSectionEntry entries() const
	{
		return DvbPmtSectionEntry(subArray(12 + descriptorsLength, length - (16 + descriptorsLength)));
	}

private:
	int descriptorsLength;
};

class DvbSdtSection : public DvbStandardSection
{
public:
	explicit DvbSdtSection(const DvbSectionData &data_) : DvbStandardSection(data_)
	{
		if (length < 15) {
			length = 0;
			return;
		}
	}

	~DvbSdtSection() { }

	int transportStreamId() const
	{
		return (at(3) << 8) | at(4);
	}

	int originalNetworkId() const
	{
		return (at(8) << 8) | at(9);
	}

	DvbSdtSectionEntry entries() const
	{
		return DvbSdtSectionEntry(subArray(11, length - 15));
	}
};

class DvbNitSection : public DvbStandardSection
{
public:
	explicit DvbNitSection(const DvbSectionData &data_) : DvbStandardSection(data_)
	{
		if (length < 16) {
			length = 0;
			return;
		}

		descriptorsLength = ((at(8) & 0xf) << 8) | at(9);

		if (length < (16 + descriptorsLength)) {
			length = 0;
			return;
		}

		entriesLength = ((at(10 + descriptorsLength) & 0xf) << 8) | at(11 + descriptorsLength);

		if (length < (16 + descriptorsLength + entriesLength)) {
			length = 0;
			return;
		}
	}

	~DvbNitSection() { }

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(10, descriptorsLength));
	}

	DvbNitSectionEntry entries() const
	{
		return DvbNitSectionEntry(subArray(12 + descriptorsLength, entriesLength));
	}

private:
	int descriptorsLength;
	int entriesLength;
};

#endif /* DVBSI_H */
