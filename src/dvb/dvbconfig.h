/*
 * dvbconfig.h
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

#ifndef DVBCONFIG_H
#define DVBCONFIG_H

#include <QSharedData>
#include <QString>

class DvbConfigBase : public QSharedData
{
public:
	enum TransmissionType
	{
		DvbC = 0,
		DvbS = 1,
		DvbT = 2,
		Atsc = 3,
		TransmissionTypeMax = Atsc
	};

	DvbConfigBase(TransmissionType transmissionType_) : transmissionType(transmissionType_) { }
	~DvbConfigBase() { }

	TransmissionType getTransmissionType() const
	{
		return transmissionType;
	}

	QString name;
	QString scanSource;
	int timeout; // tuning timeout (ms)

	// only used for DVB-S

	enum Configuration
	{
		DiseqcSwitch = 0,
		UsalsRotor = 1,
		PositionsRotor = 2,
		ConfigurationMax = PositionsRotor
	};

	Configuration configuration;
	int lnbNumber;         // corresponds to diseqc switch position (0 = first sat etc)
			       // or to rotor position (0 = first position etc)
	int lowBandFrequency;  // kHz (C-band multipoint: horizontal)
	int switchFrequency;   // kHz (0 == only low band or C-band multipoint)
	int highBandFrequency; // kHz (C-band multipoint: vertical)

private:
	TransmissionType transmissionType;
};

class DvbConfig : public QSharedDataPointer<DvbConfigBase>
{
public:
	explicit DvbConfig(DvbConfigBase *config = NULL) :
		QSharedDataPointer<DvbConfigBase>(config) { }
	~DvbConfig() { }
};

Q_DECLARE_TYPEINFO(DvbConfig, Q_MOVABLE_TYPE);

#endif /* DVBCONFIG_H */
