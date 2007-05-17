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

	virtual ~DvbTransponder() { }

	TransmissionType getTransmissionType() const
	{
		return transmissionType;
	}

	QString getSource() const
	{
		return source;
	}

protected:
	DvbTransponder(TransmissionType transmissionType_,
		const QString &source_) : transmissionType(transmissionType_),
		source(source_) { }

private:
	Q_DISABLE_COPY(DvbTransponder)

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

	DvbCTransponder(const QString &source_, int frequency_,
		ModulationType modulationType_, int symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbC, source_),
		frequency(frequency_), modulationType(modulationType_),
		symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbCTransponder() { }

	/*
	 * frequency (Hz)
	 */

	int getFrequency() const
	{
		return frequency;
	}

	void setFrequency(int value)
	{
		frequency = value;
	}

	/*
	 * modulation type
	 */

	ModulationType getModulationType() const
	{
		return modulationType;
	}

	void setModulationType(ModulationType value)
	{
		modulationType = value;
	}

	/*
	 * symbol rate (symbols per second)
	 */

	int getSymbolRate() const
	{
		return symbolRate;
	}

	void setSymbolRate(int value)
	{
		symbolRate = value;
	}

	/*
	 * FEC rate
	 */

	FecRate getFecRate() const
	{
		return fecRate;
	}

	void setFecRate(FecRate value)
	{
		fecRate = value;
	}

private:
	int frequency;
	ModulationType modulationType;
	int symbolRate;
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
		int frequency_, int symbolRate_, FecRate fecRate_) :
		DvbTransponder(DvbS, source_),  polarization(polarization_),
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

	void setPolarization(Polarization value)
	{
		polarization = value;
	}

	/*
	 * frequency (kHz)
	 */

	int getFrequency() const
	{
		return frequency;
	}

	void setFrequency(int value)
	{
		frequency = value;
	}

	/*
	 * symbol rate (symbols per second)
	 */

	int getSymbolRate() const
	{
		return symbolRate;
	}

	void setSymbolRate(int value)
	{
		symbolRate = value;
	}

	/*
	 * FEC rate
	 */

	FecRate getFecRate() const
	{
		return fecRate;
	}

	void setFecRate(FecRate value)
	{
		fecRate = value;
	}

private:
	Polarization polarization;
	int frequency;
	int symbolRate;
	FecRate fecRate;
};

class DvbChannel
{
public:
	DvbChannel(DvbTransponder *transponder_, int serviceId_, int videoPid_)
		: transponder(transponder_), serviceId(serviceId_),
		videoPid(videoPid_) { }

	~DvbChannel()
	{
		delete transponder;
	}

	/*
	 * transponder (owned by DvbChannel)
	 */

	DvbTransponder *getTransponder() const
	{
		return transponder;
	}

	void setTransponder(DvbTransponder *value)
	{
		delete transponder;
		transponder = value;
	}

	/*
	 * service id
	 */

	int getServiceId() const
	{
		return serviceId;
	}

	void setServiceId(int value)
	{
		serviceId = value;
	}

	/*
	 * video pid
	 */

	int getVideoPid() const
	{
		return videoPid;
	}

	void setVideoPid(int value)
	{
		videoPid = value;
	}

private:
	Q_DISABLE_COPY(DvbChannel)

	DvbTransponder *transponder;
	int serviceId;
	int videoPid;
};

#endif /* DVBCHANNEL_H */
