/*
 * log.h
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

#ifndef LOG_H
#define LOG_H

#include <QString>

class log
{
public:
	log(const char *message)
	{
		begin(message);
	}

	~log()
	{
		end();
	}

	static QString getLog()
	{
		if (buffer != NULL) {
			return *buffer;
		}

		return QString();
	}

	log &operator<<(qint32 value)
	{
		append(qint64(value));
		return (*this);
	}

	log &operator<<(quint32 value)
	{
		append(quint64(value));
		return (*this);
	}

	log &operator<<(qint64 value)
	{
		append(value);
		return (*this);
	}

	log &operator<<(quint64 value)
	{
		append(value);
		return (*this);
	}

private:
	Q_DISABLE_COPY(log)

	static void begin(const char *message);
	static void append(qint64 value);
	static void append(quint64 value);
	static void end();

	static QString *buffer;
	static int position;
};

#endif /* LOG_H */
