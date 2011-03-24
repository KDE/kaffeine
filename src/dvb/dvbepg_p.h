/*
 * dvbepg_p.h
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBEPG_P_H
#define DVBEPG_P_H

#include "dvbbackenddevice.h"
#include "dvbepg.h"

class DvbEpgFilter : public QSharedData, public DvbSectionFilter
{
public:
	DvbEpgFilter(DvbManager *manager, DvbDevice *device_, const DvbSharedChannel &channel);
	~DvbEpgFilter();

	DvbDevice *device;
	QString source;
	DvbTransponder transponder;

private:
	Q_DISABLE_COPY(DvbEpgFilter)
	static QTime bcdToTime(int bcd);

	void processSection(const char *data, int size);

	DvbChannelModel *channelModel;
	DvbEpgModel *epgModel;
};

class AtscEpgMgtFilter : public DvbSectionFilter
{
public:
	explicit AtscEpgMgtFilter(AtscEpgFilter *epgFilter_) : epgFilter(epgFilter_) { }
	~AtscEpgMgtFilter() { }

private:
	Q_DISABLE_COPY(AtscEpgMgtFilter)
	void processSection(const char *data, int size);

	AtscEpgFilter *epgFilter;
};

class AtscEpgEitFilter : public DvbSectionFilter
{
public:
	explicit AtscEpgEitFilter(AtscEpgFilter *epgFilter_) : epgFilter(epgFilter_) { }
	~AtscEpgEitFilter() { }

private:
	Q_DISABLE_COPY(AtscEpgEitFilter)
	void processSection(const char *data, int size);

	AtscEpgFilter *epgFilter;
};

class AtscEpgEttFilter : public DvbSectionFilter
{
public:
	explicit AtscEpgEttFilter(AtscEpgFilter *epgFilter_) : epgFilter(epgFilter_) { }
	~AtscEpgEttFilter() { }

private:
	Q_DISABLE_COPY(AtscEpgEttFilter)
	void processSection(const char *data, int size);

	AtscEpgFilter *epgFilter;
};

class AtscEpgFilter : public QSharedData
{
	friend class AtscEpgMgtFilter;
	friend class AtscEpgEitFilter;
	friend class AtscEpgEttFilter;
public:
	AtscEpgFilter(DvbManager *manager, DvbDevice *device_, const DvbSharedChannel &channel);
	~AtscEpgFilter();

	DvbDevice *device;
	QString source;
	DvbTransponder transponder;

private:
	Q_DISABLE_COPY(AtscEpgFilter)
	void processMgtSection(const char *data, int size);
	void processEitSection(const char *data, int size);
	void processEttSection(const char *data, int size);

	DvbChannelModel *channelModel;
	DvbEpgModel *epgModel;
	AtscEpgMgtFilter mgtFilter;
	AtscEpgEitFilter eitFilter;
	AtscEpgEttFilter ettFilter;
	QList<int> eitPids;
	QList<int> ettPids;
	QMap<quint32, DvbSharedEpgEntry> epgEntries;
};

#endif /* DVBEPG_P_H */
