/*
 * dvbchannel.h
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <config-kaffeine.h>

#include <QString>

class DvbTransponder
{
public:
	enum TransmissionType
	{
		DvbC,
		DvbS,
		DvbT,
		Atsc
	};

	enum FecRate
	{
		FecInvalid,
		Fec1_2,
		Fec2_3,
		Fec3_4,
		Fec4_5,
		Fec5_6,
		Fec6_7,
		Fec7_8,
		Fec8_9,
		FecAuto
	};

	DvbTransponder(TransmissionType transmissionType_,
		const QString &source_) : transmissionType(transmissionType_),
		source(source_) { }
	virtual ~DvbTransponder() { }

	TransmissionType getTransmissionType() const
	{
		return transmissionType;
	}

	QString getSource() const
	{
		return source;
	}

private:
	TransmissionType transmissionType;
	QString source;
};

class DvbCTransponder : public DvbTransponder
{
public:
	enum ModulationType
	{
		Qam64,
		Qam256
	};

	DvbCTransponder(const QString &source_, uint frequency_,
		ModulationType modulationType_, uint symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbC, source_),
		frequency(frequency_), modulationType(modulationType_),
		symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbCTransponder() { }

	/*
	 * frequency (Hz)
	 */
	uint getFrequency() const
	{
		return frequency;
	}

	/*
	 * modulation type
	 */
	ModulationType getModulationType() const
	{
		return modulationType;
	}

	/*
	 * symbol rate (symbols per second)
	 */
	uint getSymbolRate() const
	{
		return symbolRate;
	}

	/*
	 * FEC rate
	 */
	FecRate getFecRate() const
	{
		return fecRate;
	}

private:
	uint frequency;
	ModulationType modulationType;
	uint symbolRate;
	FecRate fecRate;
};

class DvbSTransponder : public DvbTransponder
{
public:
	enum Polarization
	{
		Horizontal,
		Vertical
	};

	DvbSTransponder(const QString &source_, Polarization polarization_,
		uint frequency_, uint symbolRate_, FecRate fecRate_) :
		DvbTransponder(DvbS, source_), polarization(polarization_),
		frequency(frequency_), symbolRate(symbolRate_),
		fecRate(fecRate_) { }
	~DvbSTransponder() { }

	/*
	 * polarization
	 */
	Polarization getPolarization() const
	{
		return polarization;
	}

	/*
	 * frequency (kHz)
	 */
	uint getFrequency() const
	{
		return frequency;
	}

	/*
	 * symbol rate (symbols per second)
	 */
	uint getSymbolRate() const
	{
		return symbolRate;
	}

	/*
	 * FEC rate
	 */
	FecRate getFecRate() const
	{
		return fecRate;
	}

private:
	Polarization polarization;
	uint frequency;
	uint symbolRate;
	FecRate fecRate;
};

class DvbChannel
{
public:
	DvbChannel();
	~DvbChannel();
};

#endif /* DVBCHANNEL_H */
