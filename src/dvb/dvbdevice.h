/*
 * dvbdevice.h
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

#ifndef DVBDEVICE_H
#define DVBDEVICE_H

#include <QString>

namespace Solid
{
class DvbInterface;
};

class DvbDevice
{
public:
	DvbDevice(int adapter_, int index_);
	~DvbDevice() { }

	int getAdapter() const
	{
		return adapter;
	}

	int getIndex() const
	{
		return index;
	}

	void componentAdded(const DvbInterface *component);
	void componentRemoved(const DvbInterface *component);

private:
	int adapter;
	int index;

	bool failure;

	QString caPath;
	QString demuxPath;
	QString dvrPath;
	QString frontendPath;
};

#endif /* DVBDEVICE_H */
