/*
 * log.cpp
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

#include "log.h"

#include <QTextCodec>
#include <QTime>
#include <stdio.h>

void log::begin(const char *message)
{
	if (buffer == NULL) {
		buffer = new QString();
		buffer->reserve(8176);
	}

	if (buffer->size() > (8176 - 1024)) {
		buffer->remove(0, buffer->indexOf(QLatin1Char('\n'), 1023) + 1);
	}

	position = buffer->size();
	buffer->append(QTime::currentTime().toString(Qt::ISODate));
	buffer->append(QLatin1Char(' '));
	buffer->append(QLatin1String(message));
}

void log::append(qint64 value)
{
	buffer->append(QLatin1Char(' '));
	buffer->append(QString::number(value));
}

void log::append(quint64 value)
{
	buffer->append(QLatin1Char(' '));
	buffer->append(QString::number(value));
}

void log::end()
{
	buffer->append(QLatin1Char('\n'));
	fprintf(stderr, "%s", QTextCodec::codecForLocale()->fromUnicode(
		buffer->constData() + position, buffer->size() - position).constData());
}

QString *log::buffer = NULL;
int log::position = 0;
