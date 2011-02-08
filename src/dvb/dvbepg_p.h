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
#include "dvbtransponder.h"

class DvbEpgEnsureNoPendingOperation
{
public:
	explicit DvbEpgEnsureNoPendingOperation(bool *hasPendingOperation_) :
		hasPendingOperation(hasPendingOperation_)
	{
		if (*hasPendingOperation) {
			printFatalErrorMessage();
		}

		*hasPendingOperation = true;
	}

	~DvbEpgEnsureNoPendingOperation()
	{
		*hasPendingOperation = false;
	}

private:
	void printFatalErrorMessage();

	bool *hasPendingOperation;
};

class DvbEpgLessThan
{
public:
	DvbEpgLessThan() { }
	~DvbEpgLessThan() { }

	bool operator()(const DvbEpgEntry &x, const DvbEpgEntry &y) const
	{
		if (x.channelName != y.channelName) {
			return (x.channelName < y.channelName);
		}

		if (x.begin != y.begin) {
			return (x.begin < y.begin);
		}

		if (x.duration != y.duration) {
			return (x.duration < y.duration);
		}

		if (x.title != y.title) {
			return (x.title < y.title);
		}

		if (x.subheading != y.subheading) {
			return (x.subheading < y.subheading);
		}

		return (x.details < y.details);
	}

	bool operator()(const DvbEpgEntry &x, const QString &channelName) const
	{
		return (x.channelName < channelName);
	}

	bool operator()(const QString &channelName, const DvbEpgEntry &y) const
	{
		return (channelName < y.channelName);
	}
};

class DvbEitEntry
{
public:
	DvbEitEntry() : networkId(0), transportStreamId(0), serviceId(0) { }
	explicit DvbEitEntry(const DvbChannel *channel);
	~DvbEitEntry() { }

	bool operator==(const DvbEitEntry &other) const
	{
		return ((source == other.source) && (networkId == other.networkId) &&
			(transportStreamId == other.transportStreamId) &&
			(serviceId == other.serviceId));
	}

	QString source;
	int networkId;
	int transportStreamId;
	int serviceId;
};

class DvbEpgFilter : public DvbSectionFilter
{
public:
	DvbEpgFilter(DvbEpg *epg_, DvbDevice *device_, const DvbChannel *channel);
	~DvbEpgFilter();

	DvbEpg *epg;
	DvbDevice *device;
	QString source;
	DvbTransponder transponder;

private:
	Q_DISABLE_COPY(DvbEpgFilter)
	static QTime bcdToTime(int bcd);

	void processSection(const char *data, int size, int crc);
};

class AtscEitEntry
{
public:
	AtscEitEntry() : sourceId(0) { }
	explicit AtscEitEntry(const DvbChannel *channel);
	~AtscEitEntry() { }

	bool operator==(const AtscEitEntry &other) const
	{
		return ((source == other.source) && (sourceId == other.sourceId));
	}

	QString source;
	int sourceId;
};

class AtscEpgMgtFilter : public DvbSectionFilter
{
public:
	explicit AtscEpgMgtFilter(AtscEpgFilter *epgFilter_) : epgFilter(epgFilter_) { }
	~AtscEpgMgtFilter() { }

private:
	void processSection(const char *data, int size, int crc);

	AtscEpgFilter *epgFilter;
};

class AtscEpgEitFilter : public DvbSectionFilter
{
public:
	explicit AtscEpgEitFilter(AtscEpgFilter *epgFilter_) : epgFilter(epgFilter_) { }
	~AtscEpgEitFilter() { }

private:
	void processSection(const char *data, int size, int crc);

	AtscEpgFilter *epgFilter;
};

class AtscEpgEttFilter : public DvbSectionFilter
{
public:
	explicit AtscEpgEttFilter(AtscEpgFilter *epgFilter_) : epgFilter(epgFilter_) { }
	~AtscEpgEttFilter() { }

private:
	void processSection(const char *data, int size, int crc);

	AtscEpgFilter *epgFilter;
};

class AtscEpgFilter
{
	friend class AtscEpgMgtFilter;
	friend class AtscEpgEitFilter;
	friend class AtscEpgEttFilter;
public:
	AtscEpgFilter(DvbEpg *epg_, DvbDevice *device_, const DvbChannel *channel);
	~AtscEpgFilter();

	DvbEpg *epg;
	DvbDevice *device;
	QString source;
	DvbTransponder transponder;

private:
	void processMgtSection(const char *data, int size, int crc);
	void processEitSection(const char *data, int size, int crc);
	void processEttSection(const char *data, int size, int crc);

	AtscEpgMgtFilter mgtFilter;
	AtscEpgEitFilter eitFilter;
	AtscEpgEttFilter ettFilter;
	QList<int> eitPids;
	QList<int> ettPids;
	QHash<quint32, DvbEpgEntry> epgEntryMapping; // TODO use less memory
};

#endif /* DVBEPG_P_H */
