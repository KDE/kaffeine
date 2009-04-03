/*
 * dvbchannel.cpp
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
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

#include <QTextStream>

static const char *dvbFecRateTable[DvbTransponderBase::FecRateMax + 2] = {
	/* FecNone = 0 */ "NONE",
	/* Fec1_2 = 1  */ "1/2",
	/* Fec2_3 = 2  */ "2/3",
	/* Fec3_4 = 3  */ "3/4",
	/* Fec4_5 = 4  */ "4/5",
	/* Fec5_6 = 5  */ "5/6",
	/* Fec6_7 = 6  */ "6/7",
	/* Fec7_8 = 7  */ "7/8",
	/* Fec8_9 = 8  */ "8/9",
	/* FecAuto = 9 */ "AUTO",
	NULL };

static const char *dvbCModulationTable[DvbCTransponder::ModulationMax + 2] = {
	/* Qam16 = 0          */ "QAM16",
	/* Qam32 = 1          */ "QAM32",
	/* Qam64 = 2          */ "QAM64",
	/* Qam128 = 3         */ "QAM128",
	/* Qam256 = 4         */ "QAM256",
	/* ModulationAuto = 5 */ "AUTO",
	NULL };

static const char *dvbSPolarizationTable[DvbSTransponder::PolarizationMax + 2] = {
	/* Horizontal = 0    */ "H",
	/* Vertical = 1      */ "V",
	/* CircularLeft = 2  */ "H",
	/* CircularRight = 3 */ "V",
	NULL };

static const char *dvbTBandwidthTable[DvbTTransponder::BandwidthMax + 2] = {
	/* Bandwidth6Mhz = 0 */ "6MHz",
	/* Bandwidth7Mhz = 1 */ "7MHz",
	/* Bandwidth8Mhz = 2 */ "8MHz",
	/* BandwidthAuto = 3 */ "AUTO",
	NULL };

static const char *dvbTModulationTable[DvbTTransponder::ModulationMax + 2] = {
	/* Qpsk = 0           */ "QPSK",
	/* Qam16 = 1          */ "QAM16",
	/* Qam64 = 2          */ "QAM64",
	/* ModulationAuto = 3 */ "AUTO",
	NULL };

static const char *dvbTTransmissionModeTable[DvbTTransponder::TransmissionModeMax + 2] = {
	/* TransmissionMode2k = 0   */ "2k",
	/* TransmissionMode8k = 1   */ "8k",
	/* TransmissionModeAuto = 2 */ "AUTO",
	NULL };

static const char *dvbTGuardIntervalTable[DvbTTransponder::GuardIntervalMax + 2] = {
	/* GuardInterval1_4 = 0  */ "1/4",
	/* GuardInterval1_8 = 1  */ "1/8",
	/* GuardInterval1_16 = 2 */ "1/16",
	/* GuardInterval1_32 = 3 */ "1/32",
	/* GuardIntervalAuto = 4 */ "AUTO",
	NULL };

static const char *dvbTHierarchyTable[DvbTTransponder::HierarchyMax + 2] = {
	/* HierarchyNone = 0 */ "NONE",
	/* Hierarchy1 = 1    */ "1",
	/* Hierarchy2 = 2    */ "2",
	/* Hierarchy4 = 3    */ "4",
	/* HierarchyAuto = 4 */ "AUTO",
	NULL };

static const char *atscModulationTable[AtscTransponder::ModulationMax + 2] = {
	/* Qam64 = 0          */ "QAM64",
	/* Qam256 = 1         */ "QAM256",
	/* Vsb8 = 2           */ "8VSB",
	/* Vsb16 = 3          */ "16VSB",
	/* ModulationAuto = 4 */ "AUTO",
	NULL };

class DvbChannelStringReader
{
public:
	DvbChannelStringReader(const QString &string_) : string(string_), valid(true)
	{
		stream.setString(&string);
		stream.setIntegerBase(10);
	}

	~DvbChannelStringReader() { }

	bool isValid() const
	{
		return valid;
	}

	void readInt(int &value, bool allowNegativeOne = false)
	{
		stream >> value;

		if (allowNegativeOne) {
			if (value < -1) {
				valid = false;
			}
		} else {
			if (value < 0) {
				valid = false;
			}
		}
	}

	template<class T> void readEnum(T &value, const char **table)
	{
		QString string;
		stream >> string;

		for (int i = 0; table[i] != NULL; ++i) {
			if (string == table[i]) {
				value = static_cast<T>(i);
				return;
			}
		}

		valid = false;
	}

	void checkChar(QChar value)
	{
		QString string;
		stream >> string;

		if (string != value) {
			valid = false;
		}
	}

private:
	QString string;
	QTextStream stream;
	bool valid;
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

int DvbLineReader::readInt(bool allowEmpty)
{
	QString string = readString();

	if (string.isEmpty()) {
		if (!allowEmpty) {
			valid = false;
		}

		return -1;
	}

	bool ok;
	int value = string.toInt(&ok);

	if (!ok || (value < 0)) {
		valid = false;
	}

	return value;
}

QString DvbLineReader::readString()
{
	int begin = pos;
	int end = pos;

	while ((end < line.size()) && (line[end] != '|')) {
		++end;
	}

	pos = end + 1;

	if (end > begin) {
		return line.mid(begin, end - begin);
	} else {
		return QString();
	}
}

QString DvbLineWriter::getLine()
{
	int index = line.size() - 1;
	Q_ASSERT(index >= 0);
	line[index] = '\n';
	return line;
}

void DvbLineWriter::writeInt(int value)
{
	if (value >= 0) {
		line.append(QString::number(value));
	}

	line.append('|');
}

void DvbLineWriter::writeString(const QString &string)
{
	line.append(string);
	line.append('|');
}

DvbCTransponder *DvbLineReader::readDvbCTransponder()
{
	DvbCTransponder *transponder = new DvbCTransponder;

	transponder->frequency = readInt();
	transponder->symbolRate = readInt();
	transponder->modulation = readEnum(DvbCTransponder::ModulationMax);
	transponder->fecRate = readEnum(DvbCTransponder::FecRateMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
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

void DvbLineWriter::writeTransponder(const DvbCTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->symbolRate);
	writeInt(transponder->modulation);
	writeInt(transponder->fecRate);
}

DvbSTransponder *DvbLineReader::readDvbSTransponder()
{
	DvbSTransponder *transponder = new DvbSTransponder;

	transponder->frequency = readInt();
	transponder->polarization = readEnum(DvbSTransponder::PolarizationMax);
	transponder->symbolRate = readInt();
	transponder->fecRate = readEnum(DvbSTransponder::FecRateMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
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

void DvbLineWriter::writeTransponder(const DvbSTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->polarization);
	writeInt(transponder->symbolRate);
	writeInt(transponder->fecRate);
}

DvbTTransponder *DvbLineReader::readDvbTTransponder()
{
	DvbTTransponder *transponder = new DvbTTransponder;

	transponder->frequency = readInt();
	transponder->bandwidth = readEnum(DvbTTransponder::BandwidthMax);
	transponder->modulation = readEnum(DvbTTransponder::ModulationMax);
	transponder->fecRateHigh = readEnum(DvbTTransponder::FecRateMax);
	transponder->fecRateLow = readEnum(DvbTTransponder::FecRateMax);
	transponder->transmissionMode = readEnum(DvbTTransponder::TransmissionModeMax);
	transponder->guardInterval = readEnum(DvbTTransponder::GuardIntervalMax);
	transponder->hierarchy = readEnum(DvbTTransponder::HierarchyMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
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

void DvbLineWriter::writeTransponder(const DvbTTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->bandwidth);
	writeInt(transponder->modulation);
	writeInt(transponder->fecRateHigh);
	writeInt(transponder->fecRateLow);
	writeInt(transponder->transmissionMode);
	writeInt(transponder->guardInterval);
	writeInt(transponder->hierarchy);
}

AtscTransponder *DvbLineReader::readAtscTransponder()
{
	AtscTransponder *transponder = new AtscTransponder;

	transponder->frequency = readInt();
	transponder->modulation = readEnum(AtscTransponder::ModulationMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
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

void DvbLineWriter::writeTransponder(const AtscTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->modulation);
}

DvbChannel *DvbLineReader::readChannel()
{
	DvbChannel *channel = new DvbChannel;

	int type = readInt();

	channel->name = readString();
	channel->number = readInt();

	channel->source = readString();
	channel->networkId = readInt(true);
	channel->transportStreamId = readInt();
	channel->serviceId = readInt();

	channel->pmtPid = readInt();
	channel->videoPid = readInt(true);
	channel->audioPid = readInt(true);

	channel->scrambled = (readInt() & 0x1) != 0;

	if (!isValid()) {
		delete channel;
		return NULL;
	}

	DvbTransponderBase *transponder = NULL;

	switch (type) {
	case DvbTransponderBase::DvbC:
		transponder = readDvbCTransponder();
		break;
	case DvbTransponderBase::DvbS:
		transponder = readDvbSTransponder();
		break;
	case DvbTransponderBase::DvbT:
		transponder = readDvbTTransponder();
		break;
	case DvbTransponderBase::Atsc:
		transponder = readAtscTransponder();
		break;
	}

	if (transponder == NULL) {
		delete channel;
		return NULL;
	}

	channel->transponder = DvbTransponder(transponder);

	return channel;
}

void DvbLineWriter::writeChannel(const DvbChannel *channel)
{
	int type = channel->transponder->getTransmissionType();

	writeInt(type);

	writeString(channel->name);
	writeInt(channel->number);

	writeString(channel->source);
	writeInt(channel->networkId);
	writeInt(channel->transportStreamId);
	writeInt(channel->serviceId);

	writeInt(channel->pmtPid);
	writeInt(channel->videoPid);
	writeInt(channel->audioPid);

	writeInt(channel->scrambled ? 1 : 0);

	switch (type) {
	case DvbTransponderBase::DvbC:
		writeTransponder(channel->transponder->getDvbCTransponder());
		break;
	case DvbTransponderBase::DvbS:
		writeTransponder(channel->transponder->getDvbSTransponder());
		break;
	case DvbTransponderBase::DvbT:
		writeTransponder(channel->transponder->getDvbTTransponder());
		break;
	case DvbTransponderBase::Atsc:
		writeTransponder(channel->transponder->getAtscTransponder());
		break;
	}
}
