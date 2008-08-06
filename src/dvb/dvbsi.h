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
	DvbSectionData(const char *data_, int size_) : length(-1), size(size_), data(data_) { }
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
	bool checkSize(int size_) const
	{
		return size >= size_;
	}

	unsigned char at(int index) const
	{
		return data[index];
	}

	DvbSectionData subArray(int index) const
	{
		return DvbSectionData(data + index, size - index);
	}

	DvbSectionData subArray(int index, int size) const
	{
		return DvbSectionData(data + index, size);
	}

	int length;

private:
	int size;
	const char *data;
};

class DvbSection : public DvbSectionData
{
public:
	explicit DvbSection(const DvbSectionData &data) : DvbSectionData(data)
	{
		if (checkSize(3)) {
			length = sectionLength();

			if (!checkSize(length)) {
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
		if (checkSize(2)) {
			length = descriptorLength();

			if (!checkSize(length)) {
				length = -1;
			}
		}
	}

	~DvbDescriptor() { }

	int descriptorTag() const
	{
		return at(0);
	}

	DvbDescriptor next() const
	{
		return DvbDescriptor(subArray(length));
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

class DvbServiceDescriptor : public DvbDescriptor
{
public:
	explicit DvbServiceDescriptor(const DvbDescriptor &descriptor) : DvbDescriptor(descriptor)
	{
		if (length < 5) {
			length = -1;
		} else {
			providerNameLength = at(3);

			if (length < (5 + providerNameLength)) {
				length = -1;
			} else {
				serviceNameLength = at(4 + providerNameLength);

				if (length < (5 + providerNameLength + serviceNameLength)) {
					length = -1;
				}
			}
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

class DvbSatelliteDescriptor : public DvbDescriptor
{
public:
	enum ModulationSystem
	{
		ModulationDvbS,
		ModulationDvbS2
	};

	explicit DvbSatelliteDescriptor(const DvbDescriptor &descriptor) : DvbDescriptor(descriptor)
	{
		if (length < 13) {
			length = -1;
		}
	}

	~DvbSatelliteDescriptor() { }

	ModulationSystem modulationSystem() const
	{
		if ((at(8) & 0x04) == 0) {
			return ModulationDvbS;
		} else {
			return ModulationDvbS2;
		}
	}

	DvbSTransponder *createDvbSTransponder() const
	{
		Q_ASSERT(modulationSystem() == ModulationDvbS);

		DvbSTransponder *transponder = new DvbSTransponder;
		transponder->frequency = frequency();
		transponder->polarization = polarization();
		transponder->symbolRate = symbolRate();
		transponder->fecRate = fecRate();

		return transponder;
	}

private:
	int frequency() const
	{
		unsigned int bcd = (at(2) << 24) | (at(3) << 16) | (at(4) << 8) | at(5);

		return bcdToInt(bcd, 10);
	}

	DvbSTransponder::Polarization polarization() const
	{
		switch ((at(8) >> 5) & ((1 << 2) - 1)) {
		case 0:
			return DvbSTransponder::Horizontal;
		case 1:
			return DvbSTransponder::Vertical;
		case 2:
			return DvbSTransponder::CircularLeft;
		default:
			// to make the compiler happy - the only possible value left is 3
			return DvbSTransponder::CircularRight;
		}
	}

	int symbolRate() const
	{
		unsigned int bcd = (at(9) << 20) | (at(10) << 12) | (at(11) << 4) | (at(12) >> 4);

		return bcdToInt(bcd, 100);
	}

	DvbTransponderBase::FecRate fecRate() const
	{
		switch (at(12) & ((1 << 4) - 1)) {
		case 1:
			return DvbTransponderBase::Fec1_2;
		case 2:
			return DvbTransponderBase::Fec2_3;
		case 3:
			return DvbTransponderBase::Fec3_4;
		case 4:
			return DvbTransponderBase::Fec5_6;
		case 5:
			return DvbTransponderBase::Fec7_8;
		case 6:
			return DvbTransponderBase::Fec8_9;
		case 8:
			return DvbTransponderBase::Fec4_5;
		default:
			// this includes rates like 3/5 and 9/10
			return DvbTransponderBase::FecAuto;
		}
	}
};

class DvbPatSectionEntry : public DvbSectionData
{
public:
	explicit DvbPatSectionEntry(const DvbSectionData &data) : DvbSectionData(data)
	{
		if (checkSize(4)) {
			length = 4;
		}
	}

	~DvbPatSectionEntry() { }

	int programNumber() const
	{
		return (at(0) << 8) | at(1);
	}

	int pid() const
	{
		return ((at(2) << 8) | at(3)) & ((1 << 13) - 1);
	}

	DvbPatSectionEntry next() const
	{
		return DvbPatSectionEntry(subArray(4));
	}
};

class DvbPmtSectionEntry : public DvbSectionData
{
public:
	explicit DvbPmtSectionEntry(const DvbSectionData &data) : DvbSectionData(data)
	{
		if (checkSize(5)) {
			length = entryLength();

			if (!checkSize(length)) {
				length = -1;
			}
		}
	}

	~DvbPmtSectionEntry() { }

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

	DvbPmtSectionEntry next() const
	{
		return DvbPmtSectionEntry(subArray(length));
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

class DvbSdtSectionEntry : public DvbSectionData
{
public:
	explicit DvbSdtSectionEntry(const DvbSectionData &data) : DvbSectionData(data)
	{
		if (checkSize(5)) {
			length = entryLength();

			if (!checkSize(length)) {
				length = -1;
			}
		}
	}

	~DvbSdtSectionEntry() { }

	int serviceId() const
	{
		return (at(0) << 8) | at(1);
	}

	bool isScrambled() const
	{
		return (at(3) & (1 << 4));
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(5, length - 5));
	}

	DvbSdtSectionEntry next() const
	{
		return DvbSdtSectionEntry(subArray(length));
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

class DvbNitSectionEntry : public DvbSectionData
{
public:
	explicit DvbNitSectionEntry(const DvbSectionData &data) : DvbSectionData(data)
	{
		if (checkSize(6)) {
			length = entryLength();

			if (!checkSize(length)) {
				length = -1;
			}
		}
	}

	~DvbNitSectionEntry() { }

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(6, length - 6));
	}

	DvbNitSectionEntry next() const
	{
		return DvbNitSectionEntry(subArray(length));
	}

private:
	/*
	 * the meaning of this field differs from the standard
	 * it returns the length of the /whole/ entry,
	 * not only the number of bytes immediately following this field
	 */

	int entryLength() const
	{
		return (((at(4) << 8) | at(5)) & ((1 << 12) - 1)) + 6;
	}
};

class DvbPatSection : public DvbStandardSection
{
public:
	explicit DvbPatSection(const DvbStandardSection &section) : DvbStandardSection(section)
	{
		if (length < 12) {
			length = -1;
		} else if (tableId() != 0x0) {
			length = -1;
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

	int programNumber() const
	{
		return (at(3) << 8) | at(4);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(subArray(12, programDescriptorsLength));
	}

	DvbPmtSectionEntry entries() const
	{
		return DvbPmtSectionEntry(subArray(12 + programDescriptorsLength,
			length - 16 - programDescriptorsLength));
	}

private:
	int programInfoLength() const
	{
		return ((at(10) << 8) | at(11)) & ((1 << 12) - 1);
	}

	int programDescriptorsLength;
};

class DvbSdtSection : public DvbStandardSection
{
public:
	DvbSdtSection(const DvbSectionData &data, int tableId_) : DvbStandardSection(data)
	{
		if (length < 15) {
			length = -1;
		} else if (tableId() != tableId_) {
			length = -1;
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
	DvbNitSection(const DvbSectionData &data, int tableId_) : DvbStandardSection(data)
	{
		if (length < 16) {
			length = -1;
		} else if (tableId() != tableId_) {
			length = -1;
		} else {
			networkInfoLength = networkDescriptorsLength();

			if (length < (networkInfoLength + 16)) {
				length = -1;
			} else {
				nitEntriesLength = transportStreamLoopLength();

				if (length < (networkInfoLength + nitEntriesLength + 16)) {
					length = -1;
				}
			}
		}
	}

	~DvbNitSection() { }

	DvbNitSectionEntry entries() const
	{
		return DvbNitSectionEntry(subArray(12 + networkInfoLength, nitEntriesLength));
	}

private:
	int networkDescriptorsLength() const
	{
		return ((at(8) << 8) | at(9)) & ((1 << 12) - 1);
	}

	int transportStreamLoopLength() const
	{
		int index = 10 + networkInfoLength;
		return ((at(index) << 8) | at(index + 1)) & ((1 << 12) - 1);
	}

	int networkInfoLength;
	int nitEntriesLength;
};

#endif /* DVBSI_H */
