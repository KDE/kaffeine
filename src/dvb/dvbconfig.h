/*
 * dvbconfig.h
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

#ifndef DVBCONFIG_H
#define DVBCONFIG_H

#include <QSharedData>
#include <QString>

class DvbCConfig;
class DvbSConfig;
class DvbTConfig;
class AtscConfig;
class DvbSTransponder;

class DvbConfigBase : public QSharedData
{
public:
	enum TransmissionType
	{
		DvbC = 0,
		DvbS = 1,
		DvbT = 2,
		Atsc = 3
	};

	DvbConfigBase() : timeout(1500) { }
	virtual ~DvbConfigBase() { }

	virtual TransmissionType getTransmissionType() const = 0;

	virtual const DvbCConfig *getDvbCConfig() const
	{
		return NULL;
	}

	virtual const DvbSConfig *getDvbSConfig() const
	{
		return NULL;
	}

	virtual const DvbTConfig *getDvbTConfig() const
	{
		return NULL;
	}

	virtual const AtscConfig *getAtscConfig() const
	{
		return NULL;
	}

	QString name;
	QString scanSource;
	int timeout; // tuning timeout (ms)
};

class DvbCConfig : public DvbConfigBase
{
public:
	TransmissionType getTransmissionType() const
	{
		return DvbC;
	}

	const DvbCConfig *getDvbCConfig() const
	{
		return this;
	}
};

class DvbSConfig : public DvbConfigBase
{
public:
	DvbSConfig(int lnbNumber_) : lnbNumber(lnbNumber_), lowBandFrequency(9750000),
		switchFrequency(11700000), highBandFrequency(10600000) { }
	~DvbSConfig() { }

	TransmissionType getTransmissionType() const
	{
		return DvbS;
	}

	const DvbSConfig *getDvbSConfig() const
	{
		return this;
	}

	int lnbNumber;         // corresponds to diseqc switch position (0 = first sat etc)
	int lowBandFrequency;  // kHz
	int switchFrequency;   // kHz (0 == only low band)
	int highBandFrequency; // kHz
};

class DvbTConfig : public DvbConfigBase
{
public:
	TransmissionType getTransmissionType() const
	{
		return DvbT;
	}

	const DvbTConfig *getDvbTConfig() const
	{
		return this;
	}
};

class AtscConfig : public DvbConfigBase
{
public:
	TransmissionType getTransmissionType() const
	{
		return Atsc;
	}

	const AtscConfig *getAtscConfig() const
	{
		return this;
	}
};

class DvbConfig : public QExplicitlySharedDataPointer<const DvbConfigBase>
{
public:
	explicit DvbConfig(const DvbConfigBase *config = NULL) :
		QExplicitlySharedDataPointer<const DvbConfigBase>(config) { }
	~DvbConfig() { }
};

#endif /* DVBCONFIG_H */
