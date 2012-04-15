/*
 * datetimeedit.cpp
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

#include "datetimeedit.h"

#include <KGlobal>
#include <KLocale>

static QString localQtDateFormat()
{
	QString dateFormat = KGlobal::locale()->dateFormatShort();

	for (int i = 0; (i + 1) < dateFormat.size(); ++i) {
		if (dateFormat.at(i) != QLatin1Char('%')) {
			continue;
		}

		switch (dateFormat.at(i + 1).unicode()) {
		case 'Y':
			dateFormat.replace(i, 2, QLatin1String("yyyy"));
			break;
		case 'y':
			dateFormat.replace(i, 2, QLatin1String("yy"));
			break;
		case 'n':
			dateFormat.replace(i, 2, QLatin1Char('M'));
			break;
		case 'm':
			dateFormat.replace(i, 2, QLatin1String("MM"));
			break;
		case 'e':
			dateFormat.replace(i, 2, QLatin1Char('d'));
			break;
		case 'd':
			dateFormat.replace(i, 2, QLatin1String("dd"));
			break;
		case 'b':
			dateFormat.replace(i, 2, QLatin1String("MMM"));
			break;
		case 'B':
			dateFormat.replace(i, 2, QLatin1String("MMMM"));
			break;
		case 'a':
			dateFormat.replace(i, 2, QLatin1String("ddd"));
			break;
		case 'A':
			dateFormat.replace(i, 2, QLatin1String("dddd"));
			break;
		}
	}

	return dateFormat;
}

static QString localQtTimeFormat(bool showSeconds, bool duration)
{
	QString timeFormat = KGlobal::locale()->timeFormat();

	for (int i = 0; (i + 1) < timeFormat.size(); ++i) {
		if (timeFormat.at(i) != QLatin1Char('%')) {
			continue;
		}

		bool strip = false;

		switch (timeFormat.at(i + 1).unicode()) {
		case 'H':
		case 'I':
			timeFormat.replace(i, 2, QLatin1String("hh"));
			break;
		case 'k':
		case 'l':
			timeFormat.replace(i, 2, QLatin1Char('h'));
			break;
		case 'M':
			timeFormat.replace(i, 2, QLatin1String("mm"));
			break;
		case 'S':
			if (showSeconds) {
				timeFormat.replace(i, 2, QLatin1String("ss"));
			} else {
				strip = true;
			}

			break;
		case 'p':
			if (!duration) {
				timeFormat.replace(i, 2, QLatin1String("AP"));
			} else {
				strip = true;
			}

			break;
		}

		if (strip) {
			int beginRemove = i;
			int endRemove = i + 2;

			while ((beginRemove > 0) && timeFormat.at(beginRemove - 1).isSpace()) {
				--beginRemove;
			}

			if ((beginRemove > 0) && timeFormat.at(beginRemove - 1).isPunct() &&
			    (timeFormat.at(beginRemove - 1) != QLatin1Char('%'))) {
				--beginRemove;

				while ((beginRemove > 0) &&
				       timeFormat.at(beginRemove - 1).isSpace()) {
					--beginRemove;
				}
			}

			if (beginRemove == 0) {
				while ((endRemove < timeFormat.size()) &&
				       timeFormat.at(endRemove).isSpace()) {
					++endRemove;
				}

				if ((endRemove < timeFormat.size()) &&
				    timeFormat.at(endRemove).isPunct() &&
				    (timeFormat.at(endRemove) != QLatin1Char('%'))) {
					++endRemove;

					while ((endRemove < timeFormat.size()) &&
					       timeFormat.at(endRemove).isSpace()) {
						++endRemove;
					}
				}
			}

			timeFormat.remove(beginRemove, endRemove - beginRemove);
			i = beginRemove - 1;
		}
	}

	return timeFormat;
}

DateTimeEdit::DateTimeEdit(QWidget *parent) : QDateTimeEdit(parent)
{
	setDisplayFormat(localQtDateFormat() + QLatin1Char(' ') + localQtTimeFormat(false, false));
}

DateTimeEdit::~DateTimeEdit()
{
}

DurationEdit::DurationEdit(QWidget *parent) : QTimeEdit(parent)
{
	setDisplayFormat(localQtTimeFormat(false, true));
}

DurationEdit::~DurationEdit()
{
}
