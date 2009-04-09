/*
 * dvbchannel.cpp
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dvbchannel.h"

#include <QDataStream>
#include <QStringList>
#include <QTextStream>
#include <KLocalizedString>

template<> DvbTransponderBase::FecRate DvbTransponderBase::maxEnumValue()
{
	return DvbTransponderBase::FecRateMax;
}

template<> DvbCTransponder::Modulation DvbTransponderBase::maxEnumValue()
{
	return DvbCTransponder::ModulationMax;
}

template<> DvbSTransponder::Polarization DvbTransponderBase::maxEnumValue()
{
	return DvbSTransponder::PolarizationMax;
}

template<> DvbTTransponder::Bandwidth DvbTransponderBase::maxEnumValue()
{
	return DvbTTransponder::BandwidthMax;
}

template<> DvbTTransponder::Modulation DvbTransponderBase::maxEnumValue()
{
	return DvbTTransponder::ModulationMax;
}

template<> DvbTTransponder::TransmissionMode DvbTransponderBase::maxEnumValue()
{
	return DvbTTransponder::TransmissionModeMax;
}

template<> DvbTTransponder::GuardInterval DvbTransponderBase::maxEnumValue()
{
	return DvbTTransponder::GuardIntervalMax;
}

template<> DvbTTransponder::Hierarchy DvbTransponderBase::maxEnumValue()
{
	return DvbTTransponder::HierarchyMax;
}

template<> AtscTransponder::Modulation DvbTransponderBase::maxEnumValue()
{
	return AtscTransponder::ModulationMax;
}

static const char *dvbFecRateTable[] = {
	/* FecNone = 0 */ "NONE",
	/* Fec1_2 = 1  */ "1/2",
	/* Fec2_3 = 2  */ "2/3",
	/* Fec3_4 = 3  */ "3/4",
	/* Fec4_5 = 4  */ "4/5",
	/* Fec5_6 = 5  */ "5/6",
	/* Fec6_7 = 6  */ "6/7",
	/* Fec7_8 = 7  */ "7/8",
	/* Fec8_9 = 8  */ "8/9",
	/* FecAuto = 9 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbTransponderBase::FecRate>()
{
	QStringList strings;
	strings.append(/* FecNone = 0 */ "NONE");
	strings.append(/* Fec1_2 = 1  */ "1/2");
	strings.append(/* Fec2_3 = 2  */ "2/3");
	strings.append(/* Fec3_4 = 3  */ "3/4");
	strings.append(/* Fec4_5 = 4  */ "4/5");
	strings.append(/* Fec5_6 = 5  */ "5/6");
	strings.append(/* Fec6_7 = 6  */ "6/7");
	strings.append(/* Fec7_8 = 7  */ "7/8");
	strings.append(/* Fec8_9 = 8  */ "8/9");
	strings.append(/* FecAuto = 9 */ "AUTO");
	return strings;
}

static const char *dvbCModulationTable[] = {
	/* Qam16 = 0          */ "QAM16",
	/* Qam32 = 1          */ "QAM32",
	/* Qam64 = 2          */ "QAM64",
	/* Qam128 = 3         */ "QAM128",
	/* Qam256 = 4         */ "QAM256",
	/* ModulationAuto = 5 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbCTransponder::Modulation>()
{
	QStringList strings;
	strings.append(/* Qam16 = 0          */ "QAM16");
	strings.append(/* Qam32 = 1          */ "QAM32");
	strings.append(/* Qam64 = 2          */ "QAM64");
	strings.append(/* Qam128 = 3         */ "QAM128");
	strings.append(/* Qam256 = 4         */ "QAM256");
	strings.append(/* ModulationAuto = 5 */ "AUTO");
	return strings;
}

static const char *dvbSPolarizationTable[] = {
	/* Horizontal = 0    */ "H",
	/* Vertical = 1      */ "V",
	/* CircularLeft = 2  */ "H",
	/* CircularRight = 3 */ "V"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbSTransponder::Polarization>()
{
	QStringList strings;
	strings.append(/* Horizontal = 0    */ i18n("Horizontal"));
	strings.append(/* Vertical = 1      */ i18n("Vertical"));
	strings.append(/* CircularLeft = 2  */ i18n("Circular left"));
	strings.append(/* CircularRight = 3 */ i18n("Circular right"));
	return strings;
}

static const char *dvbTBandwidthTable[] = {
	/* Bandwidth6MHz = 0 */ "6MHz",
	/* Bandwidth7MHz = 1 */ "7MHz",
	/* Bandwidth8MHz = 2 */ "8MHz",
	/* BandwidthAuto = 3 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbTTransponder::Bandwidth>()
{
	QStringList strings;
	strings.append(/* Bandwidth6MHz = 0 */ "6MHz");
	strings.append(/* Bandwidth7MHz = 1 */ "7MHz");
	strings.append(/* Bandwidth8MHz = 2 */ "8MHz");
	strings.append(/* BandwidthAuto = 3 */ "AUTO");
	return strings;
}

static const char *dvbTModulationTable[] = {
	/* Qpsk = 0           */ "QPSK",
	/* Qam16 = 1          */ "QAM16",
	/* Qam64 = 2          */ "QAM64",
	/* ModulationAuto = 3 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbTTransponder::Modulation>()
{
	QStringList strings;
	strings.append(/* Qpsk = 0           */ "QPSK");
	strings.append(/* Qam16 = 1          */ "QAM16");
	strings.append(/* Qam64 = 2          */ "QAM64");
	strings.append(/* ModulationAuto = 3 */ "AUTO");
	return strings;
}

static const char *dvbTTransmissionModeTable[] = {
	/* TransmissionMode2k = 0   */ "2k",
	/* TransmissionMode8k = 1   */ "8k",
	/* TransmissionModeAuto = 2 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbTTransponder::TransmissionMode>()
{
	QStringList strings;
	strings.append(/* TransmissionMode2k = 0   */ "2k");
	strings.append(/* TransmissionMode8k = 1   */ "8k");
	strings.append(/* TransmissionModeAuto = 2 */ "AUTO");
	return strings;
}

static const char *dvbTGuardIntervalTable[] = {
	/* GuardInterval1_4 = 0  */ "1/4",
	/* GuardInterval1_8 = 1  */ "1/8",
	/* GuardInterval1_16 = 2 */ "1/16",
	/* GuardInterval1_32 = 3 */ "1/32",
	/* GuardIntervalAuto = 4 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbTTransponder::GuardInterval>()
{
	QStringList strings;
	strings.append(/* GuardInterval1_4 = 0  */ "1/4");
	strings.append(/* GuardInterval1_8 = 1  */ "1/8");
	strings.append(/* GuardInterval1_16 = 2 */ "1/16");
	strings.append(/* GuardInterval1_32 = 3 */ "1/32");
	strings.append(/* GuardIntervalAuto = 4 */ "AUTO");
	return strings;
}

static const char *dvbTHierarchyTable[] = {
	/* HierarchyNone = 0 */ "NONE",
	/* Hierarchy1 = 1    */ "1",
	/* Hierarchy2 = 2    */ "2",
	/* Hierarchy4 = 3    */ "4",
	/* HierarchyAuto = 4 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<DvbTTransponder::Hierarchy>()
{
	QStringList strings;
	strings.append(/* HierarchyNone = 0 */ "NONE");
	strings.append(/* Hierarchy1 = 1    */ "1");
	strings.append(/* Hierarchy2 = 2    */ "2");
	strings.append(/* Hierarchy4 = 3    */ "4");
	strings.append(/* HierarchyAuto = 4 */ "AUTO");
	return strings;
}

static const char *atscModulationTable[] = {
	/* Qam64 = 0          */ "QAM64",
	/* Qam256 = 1         */ "QAM256",
	/* Vsb8 = 2           */ "8VSB",
	/* Vsb16 = 3          */ "16VSB",
	/* ModulationAuto = 4 */ "AUTO"
};

template<> QStringList DvbTransponderBase::displayStrings<AtscTransponder::Modulation>()
{
	QStringList strings;
	strings.append(/* Qam64 = 0          */ "QAM64");
	strings.append(/* Qam256 = 1         */ "QAM256");
	strings.append(/* Vsb8 = 2           */ "VSB8");
	strings.append(/* Vsb16 = 3          */ "VSB16");
	strings.append(/* ModulationAuto = 4 */ "AUTO");
	return strings;
}

template<class T> static QDataStream &operator>>(QDataStream &stream, T &value)
{
	int intValue;
	stream >> intValue;

	if ((intValue < 0) || (intValue > DvbTransponderBase::maxEnumValue<T>())) {
		stream.setStatus(QDataStream::ReadCorruptData);
	} else {
		value = static_cast<T>(intValue);
	}

	return stream;
}

class DvbChannelStringReader
{
public:
	DvbChannelStringReader(const QString &string_) : string(string_)
	{
		stream.setString(&string);
		stream.setIntegerBase(10);
	}

	~DvbChannelStringReader() { }

	bool isValid() const
	{
		return stream.status() == QTextStream::Ok;
	}

	void readInt(int &value)
	{
		stream >> value;
	}

	template<class T> void readEnum(T &value, const char **table)
	{
		QString string;
		stream >> string;

		for (int i = 0; i < DvbTransponderBase::maxEnumValue<T>(); ++i) {
			if (string == table[i]) {
				value = static_cast<T>(i);
				return;
			}
		}

		stream.setStatus(QTextStream::ReadCorruptData);
	}

	void checkChar(QChar value)
	{
		QString string;
		stream >> string;

		if (string != value) {
			stream.setStatus(QTextStream::ReadCorruptData);
		}
	}

private:
	QString string;
	QTextStream stream;
};

class DvbChannelStringWriter
{
public:
	DvbChannelStringWriter()
	{
		stream.setString(&string);
	}

	~DvbChannelStringWriter() { }

	QString getString()
	{
		string.chop(1);
		return string;
	}

	void writeInt(int value)
	{
		stream << value << ' ';
	}

	void writeEnum(int value, const char **table)
	{
		stream << table[value] << ' ';
	}

	void writeChar(QChar value)
	{
		stream << value << ' ';
	}

private:
	QString string;
	QTextStream stream;
};

void DvbCTransponder::readTransponder(QDataStream &stream)
{
	stream >> frequency;
	stream >> symbolRate;
	stream >> modulation;
	stream >> fecRate;
}

void DvbCTransponder::writeTransponder(QDataStream &stream) const
{
	stream << frequency;
	stream << symbolRate;
	stream << modulation;
	stream << fecRate;
}

bool DvbCTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar('C');
	reader.readInt(frequency);
	reader.readInt(symbolRate);
	reader.readEnum(fecRate, dvbFecRateTable);
	reader.readEnum(modulation, dvbCModulationTable);
	return reader.isValid();
}

QString DvbCTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar('C');
	writer.writeInt(frequency);
	writer.writeInt(symbolRate);
	writer.writeEnum(fecRate, dvbFecRateTable);
	writer.writeEnum(modulation, dvbCModulationTable);
	return writer.getString();
}

bool DvbCTransponder::corresponds(const DvbTransponder &transponder) const
{
	const DvbCTransponder *dvbCTransponder = transponder->getDvbCTransponder();

	return (dvbCTransponder != NULL) &&
	       (qAbs(dvbCTransponder->frequency - frequency) <= 2000000);
}

void DvbSTransponder::readTransponder(QDataStream &stream)
{
	stream >> frequency;
	stream >> polarization;
	stream >> symbolRate;
	stream >> fecRate;
}

void DvbSTransponder::writeTransponder(QDataStream &stream) const
{
	stream << frequency;
	stream << polarization;
	stream << symbolRate;
	stream << fecRate;
}

bool DvbSTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar('S');
	reader.readInt(frequency);
	reader.readEnum(polarization, dvbSPolarizationTable);
	reader.readInt(symbolRate);
	reader.readEnum(fecRate, dvbFecRateTable);
	return reader.isValid();
}

QString DvbSTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar('S');
	writer.writeInt(frequency);
	writer.writeEnum(polarization, dvbSPolarizationTable);
	writer.writeInt(symbolRate);
	writer.writeEnum(fecRate, dvbFecRateTable);
	return writer.getString();
}

bool DvbSTransponder::corresponds(const DvbTransponder &transponder) const
{
	const DvbSTransponder *dvbSTransponder = transponder->getDvbSTransponder();

	return (dvbSTransponder != NULL) &&
	       (dvbSTransponder->polarization == polarization) &&
	       (qAbs(dvbSTransponder->frequency - frequency) <= 2000);
}

void DvbTTransponder::readTransponder(QDataStream &stream)
{
	stream >> frequency;
	stream >> bandwidth;
	stream >> modulation;
	stream >> fecRateHigh;
	stream >> fecRateLow;
	stream >> transmissionMode;
	stream >> guardInterval;
	stream >> hierarchy;
}

void DvbTTransponder::writeTransponder(QDataStream &stream) const
{
	stream << frequency;
	stream << bandwidth;
	stream << modulation;
	stream << fecRateHigh;
	stream << fecRateLow;
	stream << transmissionMode;
	stream << guardInterval;
	stream << hierarchy;
}

bool DvbTTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar('T');
	reader.readInt(frequency);
	reader.readEnum(bandwidth, dvbTBandwidthTable);
	reader.readEnum(fecRateHigh, dvbFecRateTable);
	reader.readEnum(fecRateLow, dvbFecRateTable);
	reader.readEnum(modulation, dvbTModulationTable);
	reader.readEnum(transmissionMode, dvbTTransmissionModeTable);
	reader.readEnum(guardInterval, dvbTGuardIntervalTable);
	reader.readEnum(hierarchy, dvbTHierarchyTable);
	return reader.isValid();
}

QString DvbTTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar('T');
	writer.writeInt(frequency);
	writer.writeEnum(bandwidth, dvbTBandwidthTable);
	writer.writeEnum(fecRateHigh, dvbFecRateTable);
	writer.writeEnum(fecRateLow, dvbFecRateTable);
	writer.writeEnum(modulation, dvbTModulationTable);
	writer.writeEnum(transmissionMode, dvbTTransmissionModeTable);
	writer.writeEnum(guardInterval, dvbTGuardIntervalTable);
	writer.writeEnum(hierarchy, dvbTHierarchyTable);
	return writer.getString();
}

bool DvbTTransponder::corresponds(const DvbTransponder &transponder) const
{
	const DvbTTransponder *dvbTTransponder = transponder->getDvbTTransponder();

	return (dvbTTransponder != NULL) &&
	       (qAbs(dvbTTransponder->frequency - frequency) <= 2000000);
}

void AtscTransponder::readTransponder(QDataStream &stream)
{
	stream >> frequency;
	stream >> modulation;
}

void AtscTransponder::writeTransponder(QDataStream &stream) const
{
	stream << frequency;
	stream << modulation;
}

bool AtscTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar('A');
	reader.readInt(frequency);
	reader.readEnum(modulation, atscModulationTable);
	return reader.isValid();
}

QString AtscTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar('A');
	writer.writeInt(frequency);
	writer.writeEnum(modulation, atscModulationTable);
	return writer.getString();
}

bool AtscTransponder::corresponds(const DvbTransponder &transponder) const
{
	const AtscTransponder *atscTransponder = transponder->getAtscTransponder();

	return (atscTransponder != NULL) &&
	       (qAbs(atscTransponder->frequency - frequency) <= 2000000);
}

void DvbChannelBase::readChannel(QDataStream &stream)
{
	int type;
	stream >> type;

	DvbTransponderBase *transponderBase = NULL;

	switch (type) {
	case DvbTransponderBase::DvbC:
		transponderBase = new DvbCTransponder();
		break;
	case DvbTransponderBase::DvbS:
		transponderBase = new DvbSTransponder();
		break;
	case DvbTransponderBase::DvbT:
		transponderBase = new DvbTTransponder();
		break;
	case DvbTransponderBase::Atsc:
		transponderBase = new AtscTransponder();
		break;
	}

	if (transponderBase == NULL) {
		stream.setStatus(QDataStream::ReadCorruptData);
		return;
	}

	stream >> name;
	stream >> number;

	stream >> source;
	transponderBase->readTransponder(stream);
	transponder = DvbTransponder(transponderBase);
	stream >> networkId;
	stream >> transportStreamId;
	stream >> serviceId;
	stream >> pmtPid;

	stream >> pmtSection;
	stream >> videoPid;
	stream >> audioPid;

	int flags;
	stream >> flags;
	scrambled = (flags & 0x1) != 0;
}

void DvbChannelBase::writeChannel(QDataStream &stream) const
{
	stream << transponder->getTransmissionType();

	stream << name;
	stream << number;

	stream << source;
	transponder->writeTransponder(stream);
	stream << networkId;
	stream << transportStreamId;
	stream << serviceId;
	stream << pmtPid;

	stream << pmtSection;
	stream << videoPid;
	stream << audioPid;
	stream << (scrambled ? 1 : 0);
}
