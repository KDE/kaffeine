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
#include <QSharedData>
#include <QString>
#include <KPageDialog>

class DvbSConfig;
class DvbSTransponder;
class DvbTab;

class DvbConfig : public QSharedData
{
public:
	virtual ~DvbConfig() { }

	virtual const DvbSConfig *getDvbSConfig() const
	{
		return NULL;
	}

	const QString source;

	/*
	 * returns the tuning timeout (in ms)
	 */

	int getTimeout() const
	{
		// FIXME hack
		return 1500;
	}

protected:
	DvbConfig(const QString &source_) : source(source_) { }

private:
	Q_DISABLE_COPY(DvbConfig);
};

class DvbSConfig : public DvbConfig
{
public:
	DvbSConfig(const QString &source_) : DvbConfig(source_) { }
	~DvbSConfig() { }

	const DvbSConfig *getDvbSConfig() const
	{
		return this;
	}

	/*
	 * determines the correct parameters for the lnb
	 * first return value: intermediate frequency (in Khz)
	 * second return value: band (true = high band, false = low band)
	 */

	QPair<int, bool> getParameters(const DvbSTransponder *transponder) const;

	/*
	 * returns the diseqc switch position (0 = first sat, 1 = second sat etc)
	 */

	int getSwitchPos() const
	{
		// FIXME hack
		return 0;
	}
};

// you can't modify it anyway; explicit sharing just makes implementation easier
typedef QExplicitlySharedDataPointer<DvbConfig> DvbSharedConfig;

class DvbConfigDialog : public KPageDialog
{
public:
	explicit DvbConfigDialog(DvbTab *dvbTab_);
	~DvbConfigDialog();
};

#endif /* DVBCONFIG_H */
