/*
 * main.cpp
 *
 * Copyright (C) 2012 Christoph Pfister <christophpfister@gmail.com>
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

#include <QCoreApplication>
#include <QDir>
#include <unistd.h>
#include "dtvdaemon.h"
#include "log.h"

int main(int argc, char *argv[])
{
	QCoreApplication application(argc, argv);

	QDir::home().mkpath(QLatin1String(".local/share/dtvdaemon"));
	QFile file(QDir::homePath() + QLatin1String("/.local/share/dtvdaemon/lockfile"));

	if (!file.open(QIODevice::WriteOnly)) {
		Log("main: cannot open") << file.fileName();
		return 1;
	}

	if (lockf(file.handle(), F_TLOCK, 0) != 0) {
		// cannot lock file
		return 0;
	}

	// closing the file or terminating the application releases the lock
	DtvDaemon daemon(&file);
	return application.exec();
}
