/*
 * dbusobjects.cpp
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

#include "dbusobjects.h"

#include <QDBusMetaType>
#include <KAboutData>
#include <KApplication>

QDBusArgument &operator<<(QDBusArgument &argument, const MprisVersionStruct &versionStruct)
{
	argument.beginStructure();
	argument << versionStruct.major << versionStruct.minor;
	argument.endStructure();
	return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MprisVersionStruct &versionStruct)
{
	argument.beginStructure();
	argument >> versionStruct.major >> versionStruct.minor;
	argument.endStructure();
	return argument;
}

MprisRootObject::MprisRootObject(QObject *parent) : QObject(parent)
{
	 qDBusRegisterMetaType<MprisVersionStruct>();
}

MprisRootObject::~MprisRootObject()
{
}

QString MprisRootObject::Identity()
{
	const KAboutData *aboutData = KGlobal::mainComponent().aboutData();
	return aboutData->programName() + ' ' + aboutData->version();
}

void MprisRootObject::Quit()
{
	kapp->quit();
}

MprisVersionStruct MprisRootObject::MprisVersion()
{
	MprisVersionStruct versionStruct;
	versionStruct.major = 1;
	versionStruct.minor = 0;
	return versionStruct;
}
