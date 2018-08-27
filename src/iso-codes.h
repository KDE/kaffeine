/*
 * iso-codes.h
 *
 * Copyright (C) 2017 Mauro Carvalho Chehab <mchehab+samsung@kernel.org>
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
 */

#ifndef ISO_CODES_H
#define ISO_CODES_H

class QString;

namespace IsoCodes
{
	bool getLanguage(const QString &code, QString *language);
	bool getCountry(const QString &code, QString *country);
}

#endif
