/*
 * dvbsifilter.h
 *
 * Copyright (C) 2008-2010 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBSIFILTER_H
#define DVBSIFILTER_H

#include "dvbdevice.h"

class DvbSectionFilter : public DvbPidFilter
{
public:
	DvbSectionFilter() : continuityCounter(-1), bufferValid(false) { }
	~DvbSectionFilter() { }

	void resetFilter()
	{
		buffer.clear();
		continuityCounter = -1;
		bufferValid = false;
	}

protected:
	virtual void processSection(const QByteArray &data) = 0;

private:
	void processData(const char data[188]);

	void appendData(const char *data, int length);
	void processSections(bool force = false);

	QByteArray buffer;
	int continuityCounter;
	bool bufferValid;
};

class DvbPmtFilter : public QObject, public DvbSectionFilter
{
	Q_OBJECT
public:
	DvbPmtFilter() : programNumber(-1), versionNumber(-1) { }
	~DvbPmtFilter() { }

	void reset()
	{
		DvbSectionFilter::resetFilter();
		programNumber = -1;
		versionNumber = -1;
	}

	void setProgramNumber(int programNumber_)
	{
		programNumber = programNumber_;
	}

signals:
	void pmtSectionChanged(const DvbPmtSection &section);

private:
	void processSection(const QByteArray &data);

	int programNumber;
	int versionNumber;
};

#endif /* DVBSIFILTER_H */
