/*
 * dvbtransponder.h
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

#ifndef DVBTRANSPONDER_H
#define DVBTRANSPONDER_H

#include <string.h>

class QDataStream;
class QString;
class DvbTransponder;

class DvbTransponderBase
{
public:
	enum TransmissionType {
		Invalid = 5,
		DvbC = 0,
		DvbS = 1,
		DvbS2 = 4,
		DvbT = 2,
		Atsc = 3
	};

	enum FecRate {
		FecNone = 0,
		Fec1_2 = 1,
		Fec1_3 = 10,
		Fec1_4 = 11,
		Fec2_3 = 2,
		Fec2_5 = 12,
		Fec3_4 = 3,
		Fec3_5 = 13,
		Fec4_5 = 4,
		Fec5_6 = 5,
		Fec6_7 = 6,
		Fec7_8 = 7,
		Fec8_9 = 8,
		Fec9_10 = 14,
		FecAuto = 9
	};

private:
	TransmissionType transmissionType : 8;
};

class DvbCTransponder : public DvbTransponderBase
{
public:
	enum Modulation {
		Qam16 = 0,
		Qam32 = 1,
		Qam64 = 2,
		Qam128 = 3,
		Qam256 = 4,
		ModulationAuto = 5
	};

	void readTransponder(QDataStream &stream);
	bool fromString(const QString &string);
	QString toString() const;
	bool corresponds(const DvbTransponder &transponder) const;

	Modulation modulation : 8;
	FecRate fecRate : 8;
	int frequency; // Hz
	int symbolRate; // symbols per second
};

class DvbSTransponder : public DvbTransponderBase
{
public:
	enum Polarization {
		Horizontal = 0,
		Vertical = 1,
		CircularLeft = 2,
		CircularRight = 3
	};

	void readTransponder(QDataStream &stream);
	bool fromString(const QString &string);
	QString toString() const;
	bool corresponds(const DvbTransponder &transponder) const;

	Polarization polarization : 8;
	FecRate fecRate : 8;
	int frequency; // kHz
	int symbolRate; // symbols per second
};

class DvbS2Transponder : public DvbSTransponder
{
public:
	enum Modulation {
		Qpsk = 0,
		Psk8 = 1,
		Apsk16 = 2,
		Apsk32 = 3,
		ModulationAuto = 4
	};

	enum RollOff {
		RollOff20 = 0,
		RollOff25 = 1,
		RollOff35 = 2,
		RollOffAuto = 3
	};

	void readTransponder(QDataStream &stream);
	bool fromString(const QString &string);
	QString toString() const;
	bool corresponds(const DvbTransponder &transponder) const;

	Modulation modulation : 8;
	RollOff rollOff : 8;
};

class DvbTTransponder : public DvbTransponderBase
{
public:
	enum Bandwidth {
		Bandwidth6MHz = 0,
		Bandwidth7MHz = 1,
		Bandwidth8MHz = 2,
		BandwidthAuto = 3
	};

	enum Modulation {
		Qpsk = 0,
		Qam16 = 1,
		Qam64 = 2,
		ModulationAuto = 3
	};

	enum TransmissionMode {
		TransmissionMode2k = 0,
		TransmissionMode4k = 3,
		TransmissionMode8k = 1,
		TransmissionModeAuto = 2
	};

	enum GuardInterval {
		GuardInterval1_4 = 0,
		GuardInterval1_8 = 1,
		GuardInterval1_16 = 2,
		GuardInterval1_32 = 3,
		GuardIntervalAuto = 4
	};

	enum Hierarchy {
		HierarchyNone = 0,
		Hierarchy1 = 1,
		Hierarchy2 = 2,
		Hierarchy4 = 3,
		HierarchyAuto = 4
	};

	void readTransponder(QDataStream &stream);
	bool fromString(const QString &string);
	QString toString() const;
	bool corresponds(const DvbTransponder &transponder) const;

	Bandwidth bandwidth : 8;
	Modulation modulation : 8;
	FecRate fecRateHigh : 8; // high priority stream
	FecRate fecRateLow : 8; // low priority stream
	TransmissionMode transmissionMode : 8;
	GuardInterval guardInterval : 8;
	Hierarchy hierarchy : 8;
	int frequency; // Hz
};

class AtscTransponder : public DvbTransponderBase
{
public:
	enum Modulation {
		Qam64 = 0,
		Qam256 = 1,
		Vsb8 = 2,
		Vsb16 = 3,
		ModulationAuto = 4
	};

	void readTransponder(QDataStream &stream);
	bool fromString(const QString &string);
	QString toString() const;
	bool corresponds(const DvbTransponder &transponder) const;

	Modulation modulation : 8;
	int frequency; // Hz
};

class DvbTransponder
{
public:
	DvbTransponder()
	{
		data.transmissionType = DvbTransponderBase::Invalid;
	}

	explicit DvbTransponder(DvbTransponderBase::TransmissionType transmissionType_)
	{
		memset(&data, 0, sizeof(data));
		data.transmissionType = transmissionType_;
	}

	~DvbTransponder() { }

	DvbTransponderBase::TransmissionType getTransmissionType() const
	{
		return data.transmissionType;
	}

	bool isValid() const
	{
		return (data.transmissionType != DvbTransponderBase::Invalid);
	}

	static DvbTransponder fromString(const QString &string); // linuxtv scan file format
	QString toString() const; // linuxtv scan file format

	/*
	 * corresponding in this context means that both tuning parameters will lead to the same
	 * transponder; note the tuning parameters don't have to be equal, it's sufficient that
	 * they can't coexist at the same time (for example the frequency difference between two
	 * channels in the same network has to be big enough because of bandwidth)
	 */

	bool corresponds(const DvbTransponder &transponder) const;

	template<class T> const T *as() const
	{
		if (data.transmissionType == transmissionTypeFor(static_cast<T *>(NULL))) {
			return reinterpret_cast<const T *>(&data);
		}

		return NULL;
	}

	template<class T> T *as()
	{
		if (data.transmissionType == transmissionTypeFor(static_cast<T *>(NULL))) {
			return reinterpret_cast<T *>(&data);
		}

		return NULL;
	}

private:
	DvbTransponderBase::TransmissionType transmissionTypeFor(const DvbCTransponder *) const
	{
		return DvbTransponderBase::DvbC;
	}

	DvbTransponderBase::TransmissionType transmissionTypeFor(const DvbSTransponder *) const
	{
		return DvbTransponderBase::DvbS;
	}

	DvbTransponderBase::TransmissionType transmissionTypeFor(const DvbS2Transponder *) const
	{
		return DvbTransponderBase::DvbS2;
	}

	DvbTransponderBase::TransmissionType transmissionTypeFor(const DvbTTransponder *) const
	{
		return DvbTransponderBase::DvbT;
	}

	DvbTransponderBase::TransmissionType transmissionTypeFor(const AtscTransponder *) const
	{
		return DvbTransponderBase::Atsc;
	}

	union {
		DvbTransponderBase::TransmissionType transmissionType : 8;
		DvbCTransponder dvbCTransponder;
		DvbSTransponder dvbSTransponder;
		DvbS2Transponder dvbS2Transponder;
		DvbTTransponder dvbTTransponder;
		AtscTransponder atscTransponder;
	} data;
};

#endif /* DVBTRANSPONDER_H */
