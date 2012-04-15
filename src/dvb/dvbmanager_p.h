/*
 * dvbmanager_p.h
 *
 * Copyright (C) 2008-2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBMANAGER_P_H
#define DVBMANAGER_P_H

#include <QTextStream>

class DvbScanData
{
public:
	explicit DvbScanData(const QByteArray &data_);
	~DvbScanData();

	bool checkEnd() const;
	const char *getLine() const;
	const char *readLine();
	QDate readDate();

private:
	QByteArray data;
	char *begin;
	char *pos;
	const char *end;
};

class DvbDeviceConfigReader : public QTextStream
{
public:
	explicit DvbDeviceConfigReader(QIODevice *device) : QTextStream(device), valid(true)
	{
		setCodec("UTF-8");
	}

	~DvbDeviceConfigReader() { }

	bool isValid() const
	{
		return valid;
	}

	template<typename T> T readEnum(const QString &entry, T maxValue)
	{
		int value = readInt(entry);

		if (value > maxValue) {
			valid = false;
		}

		return static_cast<T>(value);
	}

	int readInt(const QString &entry)
	{
		QString string = readString(entry);

		if (string.isEmpty()) {
			valid = false;
			return -1;
		}

		bool ok;
		int value = string.toInt(&ok);

		if (!ok || (value < 0)) {
			valid = false;
		}

		return value;
	}

	QString readString(const QString &entry)
	{
		QString line = readLine();

		if (!line.startsWith(entry + QLatin1Char('='))) {
			valid = false;
			return QString();
		}

		return line.remove(0, entry.size() + 1);
	}

private:
	bool valid;
};

class DvbDeviceConfigWriter : public QTextStream
{
public:
	explicit DvbDeviceConfigWriter(QIODevice *device) : QTextStream(device)
	{
		setCodec("UTF-8");
	}

	~DvbDeviceConfigWriter() { }

	void write(const QString &string)
	{
		*this << string << '\n';
	}

	void write(const QString &entry, int value)
	{
		*this << entry << '=' << value << '\n';
	}

	void write(const QString &entry, const QString &string)
	{
		*this << entry << '=' << string << '\n';
	}
};

#endif /* DVBMANAGER_P_H */
