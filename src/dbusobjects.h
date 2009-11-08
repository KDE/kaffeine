/*
 * dbusobjects.h
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DBUSOBJECTS_H
#define DBUSOBJECTS_H

#include <QMetaType>

struct MprisVersionStruct;

class MprisRootObject : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.MediaPlayer")
public:
	explicit MprisRootObject(QObject *parent);
	~MprisRootObject();

public slots:
	QString Identity();
	void Quit();
	MprisVersionStruct MprisVersion();
};

struct MprisVersionStruct
{
	quint16 major;
	quint16 minor;
};

Q_DECLARE_METATYPE(MprisVersionStruct)

#endif /* DBUSOBJECTS_H */
