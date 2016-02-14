/*
 * dtvdaemon.cpp
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

#include "dtvdaemon.h"

#include <QCoreApplication>
#include <QDir>
#include "connection.h"
#include "log.h"

DtvDaemon::DtvDaemon(QFile *lockfile_) : lockfile(lockfile_)
{
	Log("DtvDaemon::DtvDaemon: started");
	startTimer(54000);

	QString path = QDir::homePath() + QLatin1String("/.local/share/dtvdaemon/socket");
	QLocalServer::removeServer(path);

	if (!server.listen(path)) {
		Log("DtvDaemon::DtvDaemon: cannot listen on") << path << server.errorString();
	}

	connect(&server, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

DtvDaemon::~DtvDaemon()
{
	Log("DtvDaemon::~DtvDaemon: stopped");
}

void DtvDaemon::newConnection()
{
	while (true) {
		QLocalSocket *socket = server.nextPendingConnection();

		if (socket == NULL) {
			break;
		}

		Connection *connection = new Connection(socket);
		connect(this, SIGNAL(checkIdle(bool*)), connection, SLOT(checkIdle(bool*)));
	}
}

void DtvDaemon::timerEvent(QTimerEvent *)
{
	bool idle = true;
	emit checkIdle(&idle);

	if (idle) {
		lockfile->close();
		server.close();
		QCoreApplication::exit();
	}
}
