/*
 * shareddata.h
 *
 * Copyright (C) 2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef SHAREDDATA_H
#define SHAREDDATA_H

#include <QSharedData>

class SharedData : public QSharedData
{
public:
	SharedData() { }
	~SharedData() { }

	SharedData &operator=(const SharedData &)
	{
		return *this;
	}
};

template<class T> class ExplicitlySharedDataPointer
{
public:
	explicit ExplicitlySharedDataPointer(T *data_ = NULL) : data(data_) { }
	~ExplicitlySharedDataPointer() { }

	bool isValid() const
	{
		return (data.constData() != NULL);
	}

	const T *constData() const
	{
		return data.constData();
	}

	const T &operator*() const
	{
		return *data;
	}

	const T *operator->() const
	{
		return data.constData();
	}

	bool operator==(const ExplicitlySharedDataPointer<T> &other) const
	{
		return (data.constData() == other.data.constData());
	}

	bool operator!=(const ExplicitlySharedDataPointer<T> &other) const
	{
		return (data.constData() != other.data.constData());
	}

	bool operator<(const ExplicitlySharedDataPointer<T> &other) const
	{
		return (data.constData() < other.data.constData());
	}

	friend uint qHash(const ExplicitlySharedDataPointer<T> &pointer)
	{
		return qHash(pointer.data.constData());
	}

private:
	QExplicitlySharedDataPointer<T> data;
};

#endif /* SHAREDDATA_H */
