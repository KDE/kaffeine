/*
 * log.h
 *
 * Copyright (C) 2017 Mauro Carvalho Chehab <mchehab+kde@kernel.org>
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

#include <KLocalizedString>

#include <QDebug>
#if QT_VERSION < 0x050500
# define qInfo qDebug
#endif

#include <QLoggingCategory>

// Log categories. Should match the ones at mainwindow.cpp

Q_DECLARE_LOGGING_CATEGORY(logCam)
Q_DECLARE_LOGGING_CATEGORY(logDev)
Q_DECLARE_LOGGING_CATEGORY(logDvb)
Q_DECLARE_LOGGING_CATEGORY(logDvbSi)
Q_DECLARE_LOGGING_CATEGORY(logEpg)

Q_DECLARE_LOGGING_CATEGORY(logConfig)
Q_DECLARE_LOGGING_CATEGORY(logMediaWidget)
Q_DECLARE_LOGGING_CATEGORY(logPlaylist)
Q_DECLARE_LOGGING_CATEGORY(logSql)
