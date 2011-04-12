/*
 * dvbsi.h
 *
 * Copyright (C) 2008-2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBSI_H
#define DVBSI_H

#include <QPair>
#include "dvbbackenddevice.h"

class DvbPmtSection;

class DvbSectionData
{
public:
	bool isValid() const
	{
		return (length > 0);
	}

	unsigned char at(int index) const
	{
		return data[index];
	}

	const char *getData() const
	{
		return data;
	}

	int getLength() const
	{
		return length;
	}

	QByteArray toByteArray() const
	{
		return QByteArray(data, length);
	}

protected:
	DvbSectionData() { }
	~DvbSectionData() { }

	void initSectionData()
	{
		data = NULL;
		length = 0;
		size = 0;
	}

	void initSectionData(const char *data_, int length_, int size_)
	{
		data = data_;
		length = length_;
		size = size_;
	}

	int getSize() const
	{
		return size;
	}

private:
	const char *data;
	int length;
	int size;
};

class DvbSection : public DvbSectionData
{
public:
	int getSectionLength() const
	{
		return getLength();
	}

	int tableId() const
	{
		return at(0);
	}

	bool isStandardSection() const
	{
		return (at(1) & 0x80) != 0;
	}

protected:
	DvbSection() { }
	~DvbSection() { }

	void initSection(const char *data, int size);

private:
	Q_DISABLE_COPY(DvbSection)
};

class DvbStandardSection : public DvbSection
{
public:
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

	static int verifyCrc32(const char *data, int size);
	static const unsigned int crc32Table[];

protected:
	DvbStandardSection() { }
	~DvbStandardSection() { }

	void initStandardSection(const char *data, int size);

private:
	Q_DISABLE_COPY(DvbStandardSection)
};

class DvbSiText
{
public:
	static QString convertText(const char *data, int size);
	static void setOverride6937(bool override);

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
		Iso8859_13	= 12,
		Iso8859_14	= 13,
		Iso8859_15	= 14,
		Gb2312		= 15,
		Big5		= 16,
		Utf_8		= 17,
		EncodingTypeMax	= 17
	};

	static QTextCodec *codecTable[EncodingTypeMax + 1];
	static bool override6937;
};

class DvbDescriptor : public DvbSectionData
{
public:
	DvbDescriptor(const char *data, int size)
	{
		initDescriptor(data, size);
	}

	~DvbDescriptor() { }

	int descriptorTag() const
	{
		return at(0);
	}

	void advance()
	{
		initDescriptor(getData() + getLength(), getSize() - getLength());
	}

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
	void initDescriptor(const char *data, int size);
};

// ATSC "Multiple String Structure".  See A/65C Section 6.10
class AtscPsipText
{
public:
	static QString convertText(const char *data, int size);

private:
	static QString interpretTextData(const char *data, unsigned int len,
					 unsigned int mode);
};

// ATSC Huffman compressed string support, conforming to A/65C Annex C
class AtscHuffmanString
{
public:
	static QString convertText(const char *data_, int size, int table);
private:
	AtscHuffmanString(const char *data_, int size, int table);
	~AtscHuffmanString();
	bool hasBits();
	unsigned char getBit();
	unsigned char getByte();
	void decompress();

	const char *data;
	int bitsLeft;

	QString result;
	const unsigned short *offsets;
	const unsigned char *tableBase;

	static const unsigned short Huffman1Offsets[128];
	static const unsigned char Huffman1Tables[];

	static const unsigned short Huffman2Offsets[128];
	static const unsigned char Huffman2Tables[];
};

class DvbPmtFilter : public QObject, public DvbSectionFilter
{
	Q_OBJECT
public:
	DvbPmtFilter() : programNumber(-1) { }
	~DvbPmtFilter() { }

	void setProgramNumber(int programNumber_)
	{
		programNumber = programNumber_;
	}

signals:
	void pmtSectionChanged(const QByteArray &pmtSectionData);

private:
	void processSection(const char *data, int size);

	int programNumber;
	QByteArray lastPmtSectionData;
};

class DvbSectionGenerator
{
public:
	DvbSectionGenerator() : versionNumber(0), continuityCounter(0) { }
	~DvbSectionGenerator() { }

	void initPat(int transportStreamId, int programNumber, int pmtPid);
	void initPmt(int pmtPid, const DvbPmtSection &section, const QList<int> &pids);

	void reset()
	{
		packets.clear();
		versionNumber = 0;
		continuityCounter = 0;
	}

	QByteArray generatePackets();

private:
	char *startSection(int sectionLength);
	void endSection(int sectionLength, int pid);

	QByteArray packets;
	int versionNumber;
	int continuityCounter;
};

class DvbPmtParser
{
public:
	explicit DvbPmtParser(const DvbPmtSection &section);
	~DvbPmtParser() { }

	int videoPid;
	QList<QPair<int, QString> > audioPids; // QString = language code (may be empty)
	QList<QPair<int, QString> > subtitlePids; // QString = language code
	int teletextPid;
};

class AtscEitSectionEntry : public DvbSectionData
{
public:
	AtscEitSectionEntry(const char *data, int size)
	{
		initEitSectionEntry(data, size);
	}

	~AtscEitSectionEntry() { }

	void advance()
	{
		initEitSectionEntry(getData() + getLength(), getSize() - getLength());
	}

	int eventId() const
	{
		return ((at(0) & 0x3f) << 8) | at(1);
	}

	int startTime() const
	{
		return (at(2) << 24) | (at(3) << 16) | (at(4) << 8) | at(5);
	}

	int duration() const
	{
		return ((at(6) & 0xf) << 16) | (at(7) << 8) | at(8);
	}

	QString title() const
	{
		return AtscPsipText::convertText(getData() + 10, titleLength);
	}

private:
	void initEitSectionEntry(const char *data, int size);

	int titleLength;
};

// everything below this line is automatically generated

class DvbLanguageDescriptor : public DvbDescriptor
{
public:
	explicit DvbLanguageDescriptor(const DvbDescriptor &descriptor);
	~DvbLanguageDescriptor() { }

	int languageCode1() const
	{
		return at(2);
	}

	int languageCode2() const
	{
		return at(3);
	}

	int languageCode3() const
	{
		return at(4);
	}

private:
	Q_DISABLE_COPY(DvbLanguageDescriptor)
};

class DvbSubtitleDescriptor : public DvbDescriptor
{
public:
	explicit DvbSubtitleDescriptor(const DvbDescriptor &descriptor);
	~DvbSubtitleDescriptor() { }

	int languageCode1() const
	{
		return at(2);
	}

	int languageCode2() const
	{
		return at(3);
	}

	int languageCode3() const
	{
		return at(4);
	}

	int subtitleType() const
	{
		return at(5);
	}

private:
	Q_DISABLE_COPY(DvbSubtitleDescriptor)
};

class DvbServiceDescriptor : public DvbDescriptor
{
public:
	explicit DvbServiceDescriptor(const DvbDescriptor &descriptor);
	~DvbServiceDescriptor() { }

	QString providerName() const
	{
		return DvbSiText::convertText(getData() + 4, providerNameLength);
	}

	QString serviceName() const
	{
		return DvbSiText::convertText(getData() + 5 + providerNameLength, serviceNameLength);
	}

private:
	Q_DISABLE_COPY(DvbServiceDescriptor)

	int providerNameLength;
	int serviceNameLength;
};

class DvbShortEventDescriptor : public DvbDescriptor
{
public:
	explicit DvbShortEventDescriptor(const DvbDescriptor &descriptor);
	~DvbShortEventDescriptor() { }

	QString eventName() const
	{
		return DvbSiText::convertText(getData() + 6, eventNameLength);
	}

	QString text() const
	{
		return DvbSiText::convertText(getData() + 7 + eventNameLength, textLength);
	}

private:
	Q_DISABLE_COPY(DvbShortEventDescriptor)

	int eventNameLength;
	int textLength;
};

class DvbExtendedEventDescriptor : public DvbDescriptor
{
public:
	explicit DvbExtendedEventDescriptor(const DvbDescriptor &descriptor);
	~DvbExtendedEventDescriptor() { }

	QString text() const
	{
		return DvbSiText::convertText(getData() + 8 + itemsLength, textLength);
	}

private:
	Q_DISABLE_COPY(DvbExtendedEventDescriptor)

	int itemsLength;
	int textLength;
};

class DvbCableDescriptor : public DvbDescriptor
{
public:
	explicit DvbCableDescriptor(const DvbDescriptor &descriptor);
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

private:
	Q_DISABLE_COPY(DvbCableDescriptor)
};

class DvbSatelliteDescriptor : public DvbDescriptor
{
public:
	explicit DvbSatelliteDescriptor(const DvbDescriptor &descriptor);
	~DvbSatelliteDescriptor() { }

	int frequency() const
	{
		return (at(2) << 24) | (at(3) << 16) | (at(4) << 8) | at(5);
	}

	int polarization() const
	{
		return ((at(8) & 0x7f) >> 5);
	}

	int rollOff() const
	{
		return ((at(8) & 0x1f) >> 3);
	}

	bool isDvbS2() const
	{
		return ((at(8) & 0x4) != 0);
	}

	int modulation() const
	{
		return (at(8) & 0x3);
	}

	int symbolRate() const
	{
		return (at(9) << 20) | (at(10) << 12) | (at(11) << 4) | (at(12) >> 4);
	}

	int fecRate() const
	{
		return (at(12) & 0xf);
	}

private:
	Q_DISABLE_COPY(DvbSatelliteDescriptor)
};

class DvbTerrestrialDescriptor : public DvbDescriptor
{
public:
	explicit DvbTerrestrialDescriptor(const DvbDescriptor &descriptor);
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

	int fecRateHigh() const
	{
		return (at(7) & 0x7);
	}

	int fecRateLow() const
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

private:
	Q_DISABLE_COPY(DvbTerrestrialDescriptor)
};

class AtscChannelNameDescriptor : public DvbDescriptor
{
public:
	explicit AtscChannelNameDescriptor(const DvbDescriptor &descriptor);
	~AtscChannelNameDescriptor() { }

	QString name() const
	{
		return AtscPsipText::convertText(getData() + 2, getLength() - 2);
	}

private:
	Q_DISABLE_COPY(AtscChannelNameDescriptor)
};

class DvbPatSectionEntry : public DvbSectionData
{
public:
	DvbPatSectionEntry(const char *data, int size)
	{
		initPatSectionEntry(data, size);
	}

	~DvbPatSectionEntry() { }

	void advance()
	{
		initPatSectionEntry(getData() + getLength(), getSize() - getLength());
	}

	int programNumber() const
	{
		return (at(0) << 8) | at(1);
	}

	int pid() const
	{
		return ((at(2) & 0x1f) << 8) | at(3);
	}

private:
	void initPatSectionEntry(const char *data, int size);
};

class DvbPmtSectionEntry : public DvbSectionData
{
public:
	DvbPmtSectionEntry(const char *data, int size)
	{
		initPmtSectionEntry(data, size);
	}

	~DvbPmtSectionEntry() { }

	void advance()
	{
		initPmtSectionEntry(getData() + getLength(), getSize() - getLength());
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
		return DvbDescriptor(getData() + 5, getLength() - 5);
	}

private:
	void initPmtSectionEntry(const char *data, int size);
};

class DvbSdtSectionEntry : public DvbSectionData
{
public:
	DvbSdtSectionEntry(const char *data, int size)
	{
		initSdtSectionEntry(data, size);
	}

	~DvbSdtSectionEntry() { }

	void advance()
	{
		initSdtSectionEntry(getData() + getLength(), getSize() - getLength());
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
		return DvbDescriptor(getData() + 5, getLength() - 5);
	}

private:
	void initSdtSectionEntry(const char *data, int size);
};

class DvbEitSectionEntry : public DvbSectionData
{
public:
	DvbEitSectionEntry(const char *data, int size)
	{
		initEitSectionEntry(data, size);
	}

	~DvbEitSectionEntry() { }

	void advance()
	{
		initEitSectionEntry(getData() + getLength(), getSize() - getLength());
	}

	int startDate() const
	{
		return (at(2) << 8) | at(3);
	}

	int startTime() const
	{
		return (at(4) << 16) | (at(5) << 8) | at(6);
	}

	int duration() const
	{
		return (at(7) << 16) | (at(8) << 8) | at(9);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(getData() + 12, getLength() - 12);
	}

private:
	void initEitSectionEntry(const char *data, int size);
};

class DvbNitSectionEntry : public DvbSectionData
{
public:
	DvbNitSectionEntry(const char *data, int size)
	{
		initNitSectionEntry(data, size);
	}

	~DvbNitSectionEntry() { }

	void advance()
	{
		initNitSectionEntry(getData() + getLength(), getSize() - getLength());
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(getData() + 6, getLength() - 6);
	}

private:
	void initNitSectionEntry(const char *data, int size);
};

class AtscMgtSectionEntry : public DvbSectionData
{
public:
	AtscMgtSectionEntry(const char *data, int size)
	{
		initMgtSectionEntry(data, size);
	}

	~AtscMgtSectionEntry() { }

	void advance()
	{
		initMgtSectionEntry(getData() + getLength(), getSize() - getLength());
	}

	int tableType() const
	{
		return (at(0) << 8) | at(1);
	}

	int pid() const
	{
		return ((at(2) & 0x1f) << 8) | at(3);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(getData() + 11, getLength() - 11);
	}

private:
	void initMgtSectionEntry(const char *data, int size);
};

class AtscVctSectionEntry : public DvbSectionData
{
public:
	AtscVctSectionEntry(const char *data, int size)
	{
		initVctSectionEntry(data, size);
	}

	~AtscVctSectionEntry() { }

	void advance()
	{
		initVctSectionEntry(getData() + getLength(), getSize() - getLength());
	}

	int shortName1() const
	{
		return (at(0) << 8) | at(1);
	}

	int shortName2() const
	{
		return (at(2) << 8) | at(3);
	}

	int shortName3() const
	{
		return (at(4) << 8) | at(5);
	}

	int shortName4() const
	{
		return (at(6) << 8) | at(7);
	}

	int shortName5() const
	{
		return (at(8) << 8) | at(9);
	}

	int shortName6() const
	{
		return (at(10) << 8) | at(11);
	}

	int shortName7() const
	{
		return (at(12) << 8) | at(13);
	}

	int majorNumber() const
	{
		return ((at(14) & 0xf) << 6) | (at(15) >> 2);
	}

	int minorNumber() const
	{
		return ((at(15) & 0x3) << 8) | at(16);
	}

	int programNumber() const
	{
		return (at(24) << 8) | at(25);
	}

	bool isScrambled() const
	{
		return ((at(26) & 0x20) != 0);
	}

	int sourceId() const
	{
		return (at(28) << 8) | at(29);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(getData() + 32, getLength() - 32);
	}

private:
	void initVctSectionEntry(const char *data, int size);
};

class DvbPatSection : public DvbStandardSection
{
public:
	DvbPatSection(const char *data, int size)
	{
		initPatSection(data, size);
	}

	explicit DvbPatSection(const QByteArray &byteArray)
	{
		initPatSection(byteArray.constData(), byteArray.size());
	}

	~DvbPatSection() { }

	int transportStreamId() const
	{
		return (at(3) << 8) | at(4);
	}

	DvbPatSectionEntry entries() const
	{
		return DvbPatSectionEntry(getData() + 8, getLength() - 12);
	}

private:
	Q_DISABLE_COPY(DvbPatSection)
	void initPatSection(const char *data, int size);
};

class DvbPmtSection : public DvbStandardSection
{
public:
	DvbPmtSection(const char *data, int size)
	{
		initPmtSection(data, size);
	}

	explicit DvbPmtSection(const QByteArray &byteArray)
	{
		initPmtSection(byteArray.constData(), byteArray.size());
	}

	~DvbPmtSection() { }

	int programNumber() const
	{
		return (at(3) << 8) | at(4);
	}

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(getData() + 12, descriptorsLength);
	}

	DvbPmtSectionEntry entries() const
	{
		return DvbPmtSectionEntry(getData() + 12 + descriptorsLength, getLength() - (16 + descriptorsLength));
	}

private:
	Q_DISABLE_COPY(DvbPmtSection)
	void initPmtSection(const char *data, int size);

	int descriptorsLength;
};

class DvbSdtSection : public DvbStandardSection
{
public:
	DvbSdtSection(const char *data, int size)
	{
		initSdtSection(data, size);
	}

	explicit DvbSdtSection(const QByteArray &byteArray)
	{
		initSdtSection(byteArray.constData(), byteArray.size());
	}

	~DvbSdtSection() { }

	int originalNetworkId() const
	{
		return (at(8) << 8) | at(9);
	}

	DvbSdtSectionEntry entries() const
	{
		return DvbSdtSectionEntry(getData() + 11, getLength() - 15);
	}

private:
	Q_DISABLE_COPY(DvbSdtSection)
	void initSdtSection(const char *data, int size);
};

class DvbEitSection : public DvbStandardSection
{
public:
	DvbEitSection(const char *data, int size)
	{
		initEitSection(data, size);
	}

	explicit DvbEitSection(const QByteArray &byteArray)
	{
		initEitSection(byteArray.constData(), byteArray.size());
	}

	~DvbEitSection() { }

	int serviceId() const
	{
		return (at(3) << 8) | at(4);
	}

	int transportStreamId() const
	{
		return (at(8) << 8) | at(9);
	}

	int originalNetworkId() const
	{
		return (at(10) << 8) | at(11);
	}

	DvbEitSectionEntry entries() const
	{
		return DvbEitSectionEntry(getData() + 14, getLength() - 18);
	}

private:
	Q_DISABLE_COPY(DvbEitSection)
	void initEitSection(const char *data, int size);
};

class DvbNitSection : public DvbStandardSection
{
public:
	DvbNitSection(const char *data, int size)
	{
		initNitSection(data, size);
	}

	explicit DvbNitSection(const QByteArray &byteArray)
	{
		initNitSection(byteArray.constData(), byteArray.size());
	}

	~DvbNitSection() { }

	DvbDescriptor descriptors() const
	{
		return DvbDescriptor(getData() + 10, descriptorsLength);
	}

	DvbNitSectionEntry entries() const
	{
		return DvbNitSectionEntry(getData() + 12 + descriptorsLength, entriesLength);
	}

private:
	Q_DISABLE_COPY(DvbNitSection)
	void initNitSection(const char *data, int size);

	int descriptorsLength;
	int entriesLength;
};

class AtscMgtSection : public DvbStandardSection
{
public:
	AtscMgtSection(const char *data, int size)
	{
		initMgtSection(data, size);
	}

	explicit AtscMgtSection(const QByteArray &byteArray)
	{
		initMgtSection(byteArray.constData(), byteArray.size());
	}

	~AtscMgtSection() { }

	int entryCount() const
	{
		return (at(9) << 8) | at(10);
	}

	AtscMgtSectionEntry entries() const
	{
		return AtscMgtSectionEntry(getData() + 11, getLength() - 15);
	}

private:
	Q_DISABLE_COPY(AtscMgtSection)
	void initMgtSection(const char *data, int size);
};

class AtscVctSection : public DvbStandardSection
{
public:
	AtscVctSection(const char *data, int size)
	{
		initVctSection(data, size);
	}

	explicit AtscVctSection(const QByteArray &byteArray)
	{
		initVctSection(byteArray.constData(), byteArray.size());
	}

	~AtscVctSection() { }

	int entryCount() const
	{
		return at(9);
	}

	AtscVctSectionEntry entries() const
	{
		return AtscVctSectionEntry(getData() + 10, getLength() - 14);
	}

private:
	Q_DISABLE_COPY(AtscVctSection)
	void initVctSection(const char *data, int size);
};

class AtscEitSection : public DvbStandardSection
{
public:
	AtscEitSection(const char *data, int size)
	{
		initEitSection(data, size);
	}

	explicit AtscEitSection(const QByteArray &byteArray)
	{
		initEitSection(byteArray.constData(), byteArray.size());
	}

	~AtscEitSection() { }

	int sourceId() const
	{
		return (at(3) << 8) | at(4);
	}

	int entryCount() const
	{
		return at(9);
	}

	AtscEitSectionEntry entries() const
	{
		return AtscEitSectionEntry(getData() + 10, getLength() - 14);
	}

private:
	Q_DISABLE_COPY(AtscEitSection)
	void initEitSection(const char *data, int size);
};

class AtscEttSection : public DvbStandardSection
{
public:
	AtscEttSection(const char *data, int size)
	{
		initEttSection(data, size);
	}

	explicit AtscEttSection(const QByteArray &byteArray)
	{
		initEttSection(byteArray.constData(), byteArray.size());
	}

	~AtscEttSection() { }

	int sourceId() const
	{
		return (at(9) << 8) | at(10);
	}

	int eventId() const
	{
		return (at(11) << 6) | (at(12) >> 2);
	}

	int messageType() const
	{
		return (at(12) & 0x3);
	}

	QString text() const
	{
		return AtscPsipText::convertText(getData() + 13, getLength() - 17);
	}

private:
	Q_DISABLE_COPY(AtscEttSection)
	void initEttSection(const char *data, int size);
};

#endif /* DVBSI_H */
