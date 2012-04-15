/*
 * dvbtransponder.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbtransponder.h"

#include <QDataStream>
#include <QTextStream>

static const char *enumToLinuxtv(DvbTransponderBase::FecRate fecRate)
{
	switch (fecRate) {
	case DvbTransponderBase::FecNone: return "NONE";
	case DvbTransponderBase::Fec1_2: return "1/2";
	case DvbTransponderBase::Fec1_3: return "1/3";
	case DvbTransponderBase::Fec1_4: return "1/4";
	case DvbTransponderBase::Fec2_3: return "2/3";
	case DvbTransponderBase::Fec2_5: return "2/5";
	case DvbTransponderBase::Fec3_4: return "3/4";
	case DvbTransponderBase::Fec3_5: return "3/5";
	case DvbTransponderBase::Fec4_5: return "4/5";
	case DvbTransponderBase::Fec5_6: return "5/6";
	case DvbTransponderBase::Fec6_7: return "6/7";
	case DvbTransponderBase::Fec7_8: return "7/8";
	case DvbTransponderBase::Fec8_9: return "8/9";
	case DvbTransponderBase::Fec9_10: return "9/10";
	case DvbTransponderBase::FecAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbCTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbCTransponder::Qam16: return "QAM16";
	case DvbCTransponder::Qam32: return "QAM32";
	case DvbCTransponder::Qam64: return "QAM64";
	case DvbCTransponder::Qam128: return "QAM128";
	case DvbCTransponder::Qam256: return "QAM256";
	case DvbCTransponder::ModulationAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbSTransponder::Polarization polarization)
{
	switch (polarization) {
	case DvbSTransponder::Horizontal: return "H";
	case DvbSTransponder::Vertical: return "V";
	case DvbSTransponder::CircularLeft: return "L";
	case DvbSTransponder::CircularRight: return "R";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbS2Transponder::Modulation modulation)
{
	switch (modulation) {
	case DvbS2Transponder::Qpsk: return "QPSK";
	case DvbS2Transponder::Psk8: return "8PSK";
	case DvbS2Transponder::Apsk16: return "16APSK";
	case DvbS2Transponder::Apsk32: return "32APSK";
	case DvbS2Transponder::ModulationAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbS2Transponder::RollOff rollOff)
{
	switch (rollOff) {
	case DvbS2Transponder::RollOff20: return "20";
	case DvbS2Transponder::RollOff25: return "25";
	case DvbS2Transponder::RollOff35: return "35";
	case DvbS2Transponder::RollOffAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbTTransponder::Bandwidth6MHz: return "6MHz";
	case DvbTTransponder::Bandwidth7MHz: return "7MHz";
	case DvbTTransponder::Bandwidth8MHz: return "8MHz";
	case DvbTTransponder::BandwidthAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbTTransponder::Qpsk: return "QPSK";
	case DvbTTransponder::Qam16: return "QAM16";
	case DvbTTransponder::Qam64: return "QAM64";
	case DvbTTransponder::ModulationAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbTTransponder::TransmissionMode transmissionMode)
{
	switch (transmissionMode) {
	case DvbTTransponder::TransmissionMode2k: return "2k";
	case DvbTTransponder::TransmissionMode4k: return "4k";
	case DvbTTransponder::TransmissionMode8k: return "8k";
	case DvbTTransponder::TransmissionModeAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbTTransponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case DvbTTransponder::GuardInterval1_4: return "1/4";
	case DvbTTransponder::GuardInterval1_8: return "1/8";
	case DvbTTransponder::GuardInterval1_16: return "1/16";
	case DvbTTransponder::GuardInterval1_32: return "1/32";
	case DvbTTransponder::GuardIntervalAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(DvbTTransponder::Hierarchy hierarchy)
{
	switch (hierarchy) {
	case DvbTTransponder::HierarchyNone: return "NONE";
	case DvbTTransponder::Hierarchy1: return "1";
	case DvbTTransponder::Hierarchy2: return "2";
	case DvbTTransponder::Hierarchy4: return "4";
	case DvbTTransponder::HierarchyAuto: return "AUTO";
	}

	return NULL;
}

static const char *enumToLinuxtv(AtscTransponder::Modulation modulation)
{
	switch (modulation) {
	case AtscTransponder::Qam64: return "QAM64";
	case AtscTransponder::Qam256: return "QAM256";
	case AtscTransponder::Vsb8: return "8VSB";
	case AtscTransponder::Vsb16: return "16VSB";
	case AtscTransponder::ModulationAuto: return "AUTO";
	}

	return NULL;
}

template<class T> static T readEnum(QDataStream &stream)
{
	int intValue;
	stream >> intValue;
	T value = static_cast<T>(intValue);

	if ((value != intValue) || (enumToLinuxtv(value) == NULL)) {
		stream.setStatus(QDataStream::ReadCorruptData);
	}

	return value;
}

class DvbChannelStringReader
{
public:
	explicit DvbChannelStringReader(const QString &string_) : string(string_)
	{
		stream.setString(&string);
		stream.setIntegerBase(10);
	}

	~DvbChannelStringReader() { }

	bool isValid() const
	{
		return (stream.status() == QTextStream::Ok);
	}

	void readInt(int &value)
	{
		stream >> value;
	}

	template<class T> T readEnum()
	{
		QString token;
		stream >> token;

		for (int i = 0;; ++i) {
			T value = static_cast<T>(i);

			if (value != i) {
				break;
			}

			const char *entry = enumToLinuxtv(value);

			if (entry == NULL) {
				break;
			}

			if (token == QLatin1String(entry)) {
				return value;
			}
		}

		stream.setStatus(QTextStream::ReadCorruptData);
		return T();
	}

	void checkChar(const QChar &value)
	{
		QString token;
		stream >> token;

		if (token != value) {
			stream.setStatus(QTextStream::ReadCorruptData);
		}
	}

	void checkString(const QString &value)
	{
		QString token;
		stream >> token;

		if (token != value) {
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

	template<class T> void writeEnum(T value)
	{
		stream << enumToLinuxtv(value) << ' ';
	}

	void writeChar(const QChar &value)
	{
		stream << value << ' ';
	}

	void writeString(const QString &value)
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
	modulation = readEnum<Modulation>(stream);
	fecRate = readEnum<FecRate>(stream);
}

bool DvbCTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar(QLatin1Char('C'));
	reader.readInt(frequency);
	reader.readInt(symbolRate);
	fecRate = reader.readEnum<FecRate>();
	modulation = reader.readEnum<Modulation>();
	return reader.isValid();
}

QString DvbCTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar(QLatin1Char('C'));
	writer.writeInt(frequency);
	writer.writeInt(symbolRate);
	writer.writeEnum(fecRate);
	writer.writeEnum(modulation);
	return writer.getString();
}

bool DvbCTransponder::corresponds(const DvbTransponder &transponder) const
{
	const DvbCTransponder *dvbCTransponder = transponder.as<DvbCTransponder>();

	return ((dvbCTransponder != NULL) &&
		(qAbs(dvbCTransponder->frequency - frequency) <= 2000000));
}

void DvbSTransponder::readTransponder(QDataStream &stream)
{
	polarization = readEnum<Polarization>(stream);
	stream >> frequency;
	stream >> symbolRate;
	fecRate = readEnum<FecRate>(stream);
}

bool DvbSTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar(QLatin1Char('S'));
	reader.readInt(frequency);
	polarization = reader.readEnum<Polarization>();
	reader.readInt(symbolRate);
	fecRate = reader.readEnum<FecRate>();
	return reader.isValid();
}

QString DvbSTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar(QLatin1Char('S'));
	writer.writeInt(frequency);
	writer.writeEnum(polarization);
	writer.writeInt(symbolRate);
	writer.writeEnum(fecRate);
	return writer.getString();
}

bool DvbSTransponder::corresponds(const DvbTransponder &transponder) const
{
	const DvbSTransponder *dvbSTransponder = transponder.as<DvbSTransponder>();

	return ((dvbSTransponder != NULL) &&
		(dvbSTransponder->polarization == polarization) &&
		(qAbs(dvbSTransponder->frequency - frequency) <= 2000));
}

void DvbS2Transponder::readTransponder(QDataStream &stream)
{
	polarization = readEnum<Polarization>(stream);
	stream >> frequency;
	stream >> symbolRate;
	fecRate = readEnum<FecRate>(stream);
	modulation = readEnum<Modulation>(stream);
	rollOff = readEnum<RollOff>(stream);
}

bool DvbS2Transponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkString(QLatin1String("S2"));
	reader.readInt(frequency);
	polarization = reader.readEnum<Polarization>();
	reader.readInt(symbolRate);
	fecRate = reader.readEnum<FecRate>();
	rollOff = reader.readEnum<RollOff>();
	modulation = reader.readEnum<Modulation>();
	return reader.isValid();
}

QString DvbS2Transponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeString(QLatin1String("S2"));
	writer.writeInt(frequency);
	writer.writeEnum(polarization);
	writer.writeInt(symbolRate);
	writer.writeEnum(fecRate);
	writer.writeEnum(rollOff);
	writer.writeEnum(modulation);
	return writer.getString();
}

bool DvbS2Transponder::corresponds(const DvbTransponder &transponder) const
{
	const DvbS2Transponder *dvbS2Transponder = transponder.as<DvbS2Transponder>();

	return ((dvbS2Transponder != NULL) &&
		(dvbS2Transponder->polarization == polarization) &&
		(qAbs(dvbS2Transponder->frequency - frequency) <= 2000));
}

void DvbTTransponder::readTransponder(QDataStream &stream)
{
	stream >> frequency;
	bandwidth = readEnum<Bandwidth>(stream);
	modulation = readEnum<Modulation>(stream);
	fecRateHigh = readEnum<FecRate>(stream);
	fecRateLow = readEnum<FecRate>(stream);
	transmissionMode = readEnum<TransmissionMode>(stream);
	guardInterval = readEnum<GuardInterval>(stream);
	hierarchy = readEnum<Hierarchy>(stream);
}

bool DvbTTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar(QLatin1Char('T'));
	reader.readInt(frequency);
	bandwidth = reader.readEnum<Bandwidth>();
	fecRateHigh = reader.readEnum<FecRate>();
	fecRateLow = reader.readEnum<FecRate>();
	modulation = reader.readEnum<Modulation>();
	transmissionMode = reader.readEnum<TransmissionMode>();
	guardInterval = reader.readEnum<GuardInterval>();
	hierarchy = reader.readEnum<Hierarchy>();
	return reader.isValid();
}

QString DvbTTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar(QLatin1Char('T'));
	writer.writeInt(frequency);
	writer.writeEnum(bandwidth);
	writer.writeEnum(fecRateHigh);
	writer.writeEnum(fecRateLow);
	writer.writeEnum(modulation);
	writer.writeEnum(transmissionMode);
	writer.writeEnum(guardInterval);
	writer.writeEnum(hierarchy);
	return writer.getString();
}

bool DvbTTransponder::corresponds(const DvbTransponder &transponder) const
{
	const DvbTTransponder *dvbTTransponder = transponder.as<DvbTTransponder>();

	return ((dvbTTransponder != NULL) &&
		(qAbs(dvbTTransponder->frequency - frequency) <= 2000000));
}

void AtscTransponder::readTransponder(QDataStream &stream)
{
	stream >> frequency;
	modulation = readEnum<Modulation>(stream);
}

bool AtscTransponder::fromString(const QString &string)
{
	DvbChannelStringReader reader(string);
	reader.checkChar(QLatin1Char('A'));
	reader.readInt(frequency);
	modulation = reader.readEnum<Modulation>();
	return reader.isValid();
}

QString AtscTransponder::toString() const
{
	DvbChannelStringWriter writer;
	writer.writeChar(QLatin1Char('A'));
	writer.writeInt(frequency);
	writer.writeEnum(modulation);
	return writer.getString();
}

bool AtscTransponder::corresponds(const DvbTransponder &transponder) const
{
	const AtscTransponder *atscTransponder = transponder.as<AtscTransponder>();

	return ((atscTransponder != NULL) &&
		(qAbs(atscTransponder->frequency - frequency) <= 2000000));
}

bool DvbTransponder::corresponds(const DvbTransponder &transponder) const
{
	switch (data.transmissionType) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
		return as<DvbCTransponder>()->corresponds(transponder);
	case DvbTransponderBase::DvbS:
		return as<DvbSTransponder>()->corresponds(transponder);
	case DvbTransponderBase::DvbS2:
		return as<DvbS2Transponder>()->corresponds(transponder);
	case DvbTransponderBase::DvbT:
		return as<DvbTTransponder>()->corresponds(transponder);
	case DvbTransponderBase::Atsc:
		return as<AtscTransponder>()->corresponds(transponder);
	}

	return false;
}

QString DvbTransponder::toString() const
{
	switch (data.transmissionType) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
		return as<DvbCTransponder>()->toString();
	case DvbTransponderBase::DvbS:
		return as<DvbSTransponder>()->toString();
	case DvbTransponderBase::DvbS2:
		return as<DvbS2Transponder>()->toString();
	case DvbTransponderBase::DvbT:
		return as<DvbTTransponder>()->toString();
	case DvbTransponderBase::Atsc:
		return as<AtscTransponder>()->toString();
	}

	return QString();
}

DvbTransponder DvbTransponder::fromString(const QString &string)
{
	if (string.size() >= 2) {
		switch (string.at(0).unicode()) {
		case 'C': {
			DvbTransponder transponder(DvbTransponderBase::DvbC);

			if (transponder.as<DvbCTransponder>()->fromString(string)) {
				return transponder;
			}

			break;
		    }
		case 'S':
			if (string.at(1) != QLatin1Char('2')) {
				DvbTransponder transponder(DvbTransponderBase::DvbS);

				if (transponder.as<DvbSTransponder>()->fromString(string)) {
					return transponder;
				}
			} else {
				DvbTransponder transponder(DvbTransponderBase::DvbS2);

				if (transponder.as<DvbS2Transponder>()->fromString(string)) {
					return transponder;
				}
			}

			break;
		case 'T': {
			DvbTransponder transponder(DvbTransponderBase::DvbT);

			if (transponder.as<DvbTTransponder>()->fromString(string)) {
				return transponder;
			}

			break;
		    }
		case 'A': {
			DvbTransponder transponder(DvbTransponderBase::Atsc);

			if (transponder.as<AtscTransponder>()->fromString(string)) {
				return transponder;
			}

			break;
		    }
		}
	}

	return DvbTransponder();
}
