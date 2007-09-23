/*
 * dvbconfig.h
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

#ifndef DVBCONFIG_H
#define DVBCONFIG_H

#include <QPair>
#include <QString>

#include "dvbchannel.h"

class DvbConfig
{
public:
	virtual ~DvbConfig() { }

	QString source;

	/*
	 * returns the tuning timeout (in ms)
	 */

	int getTimeout() const
	{
		// FIXME hack
		return 1500;
	}

protected:
	Q_DISABLE_COPY(DvbConfig)

	DvbConfig(const QString &source_) : source(source_) { }
};

class DvbSConfig : public DvbConfig
{
public:
	DvbSConfig(const QString &source_) : DvbConfig(source_) { }
	~DvbSConfig() { }

	/*
	 * determines the correct parameters for the lnb
	 * first return value: intermediate frequency (in Khz)
	 * second return value: band (true = high band, false = low band)
	 */

	QPair<int, bool> getParameters(const DvbSTransponder *transponder) const
	{
		// FIXME hack
		return QPair<int, bool>(transponder->frequency - 10600000, true);
	}

	/*
	 * returns the diseqc switch position (0 = first sat, 1 = second sat etc)
	 */

	int getSwitchPos() const
	{
		// FIXME hack
		return 0;
	}
};

#endif /* DVBCONFIG_H */
