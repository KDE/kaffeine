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

#include "dvbbackenddevice.h"

class DvbConfigBase : public QSharedData
{
public:
	enum TransmissionType
	{
		DvbC = 0,
		DvbS = 1,
		DvbT = 2,
		Atsc = 3,
		IsdbT = 4,
		TransmissionTypeMax = IsdbT
	};

	explicit DvbConfigBase(TransmissionType transmissionType_) : transmissionType(transmissionType_) { }
	~DvbConfigBase() { }

	TransmissionType getTransmissionType() const
	{
		return transmissionType;
	}

	QString name;
	QString scanSource;
	int timeout; // tuning timeout (ms)
	int numberOfTuners;

	// only used for Satellite

	enum Configuration
	{
		DiseqcSwitch = 0,
		UsalsRotor = 1,
		PositionsRotor = 2,
		NoDiseqc = 3,
		ConfigurationMax = NoDiseqc
	};

	Configuration configuration;

	struct lnbSat currentLnb;
	int lnbNumber;         // corresponds to diseqc switch position (0 = first sat etc)
			       // or to rotor position (0 = first position etc)
	int bpf;		// Bandpass Frequency for SCR/Unicable

	int latitude, longitude; // degrees - needed for Usals rotor
	int higherVoltage;	// Use a higher voltage to compensate cable loss
				// Qt::PartiallyChecked: don't use the ioctl
				// Qt::Unchecked: normal voltage (13V/18V)
				// Qt::Checked: higher voltages (typically: 14V/19V)

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
