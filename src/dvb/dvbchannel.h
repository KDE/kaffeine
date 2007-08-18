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

#include "dvbdevice.h"

class DvbTransponder
{
public:
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

	const DvbDevice::TransmissionType transmissionType;

protected:
	DvbTransponder(DvbDevice::TransmissionType transmissionType_) :
		transmissionType(transmissionType_) { }
};

class DvbCTransponder : public DvbTransponder
{
public:
	enum ModulationType
	{
		Qam64,
		Qam256
	};

	DvbCTransponder(int frequency_, ModulationType modulationType_, int symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbDevice::DvbC), frequency(frequency_),
		modulationType(modulationType_), symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbCTransponder() { }

	/*
	 * frequency (Hz)
	 */

	int frequency;

	/*
	 * modulation type
	 */

	ModulationType modulationType;

	/*
	 * symbol rate (symbols per second)
	 */

	int symbolRate;

	/*
	 * FEC rate
	 */

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

	DvbSTransponder(Polarization polarization_, int frequency_, int symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbDevice::DvbS),  polarization(polarization_),
		frequency(frequency_), symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbSTransponder() { }

	/*
	 * polarization
	 */

	Polarization polarization;

	/*
	 * frequency (kHz)
	 */

	int frequency;

	/*
	 * symbol rate (symbols per second)
	 */

	int symbolRate;

	/*
	 * FEC rate
	 */

	FecRate fecRate;
};

class DvbChannel
{
public:
	DvbChannel(QString source_, DvbTransponder *transponder_, int serviceId_, int videoPid_) :
		source(source_), serviceId(serviceId_), videoPid(videoPid_),
		transponder(transponder_) { }

	~DvbChannel()
	{
		delete transponder;
	}

	/*
	 * source
	 */

	QString source;

	/*
	 * transponder (owned by DvbChannel)
	 */

	const DvbTransponder *getTransponder() const
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

	int serviceId;

	/*
	 * video pid
	 */

	int videoPid;

private:
	Q_DISABLE_COPY(DvbChannel)

	DvbTransponder *transponder;
};

#endif /* DVBCHANNEL_H */
