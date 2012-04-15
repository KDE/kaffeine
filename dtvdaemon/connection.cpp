/*
 * connection.cpp
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

#include "connection.h"

#include <QLocalSocket>
#include <QtEndian>
#include "log.h"

Connection::Connection(QLocalSocket *socket_) : QObject(socket_), socket(socket_),
	packetCommand(0), packetLength(0)
{
	if (socket->state() != QLocalSocket::ConnectedState) {
		socket->deleteLater();
		return;
	}

	Log("Connection::Connection: opened connection") << quintptr(this);
	connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
	connect(socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
}

Connection::~Connection()
{
	Log("Connection::~Connection: closed connection") << quintptr(this);
}

void Connection::checkIdle(bool *idle)
{
	*idle = false;
}

void Connection::readyRead()
{
	while (true) {
		if (packetCommand == 0) {
			if (socket->bytesAvailable() < 8) {
				break;
			}

			union {
				struct {
					quint32 command;
					quint32 length;
				} info;

				char data[8];
			} u;

			memset(&u.info, 0, sizeof(u.info));
			socket->read(u.data, 8);
			quint32 command = qFromLittleEndian(u.info.command);
			quint32 length = qFromLittleEndian(u.info.length);

			if ((command == 0) || (length > (1024 * 1024))) {
				Log("Connection::readyRead: invalid packet");
				socket->abort();
				return;
			}

			packetCommand = command;
			packetLength = length;
		} else {
			if (socket->bytesAvailable() < packetLength) {
				break;
			}

			handlePacket();
			packetCommand = 0;
			packetLength = 0;
		}
	}
}

void Connection::handlePacket()
{
	bool ok = false;

	switch (packetCommand) {
	case GetVersion: {
		QString version = QLatin1String("1.2.2"); // FIXME
		writeHeader(GetVersionReply, sizeForString(version));
		writeString(version);
		ok = true;
		break;
	    }
	}

	if (!ok) {
		Log("Connection::handlePacket: invalid packet") << packetCommand;
	} else if (packetLength != 0) {
		Log("Connection::handlePacket: packet has wrong size") << packetCommand;
	}

	while (packetLength > 0) {
		char buffer[4096];
		int size = qMin(packetLength, int(sizeof(buffer)));
		int bytesRead = int(socket->read(buffer, size));

		if (bytesRead <= 0) {
			Log("Connection::handlePacket: cannot empty buffer");
			break;
		}

		packetLength -= bytesRead;
	}
}

void Connection::writeHeader(quint32 command, quint32 length)
{
	union {
		struct {
			quint32 command;
			quint32 length;
		} info;

		char data[8];
	} u;

	u.info.command = qToLittleEndian(command);
	u.info.length = qToLittleEndian(length);
	socket->write(u.data, 8);
}

int Connection::sizeForString(const QString &string)
{
	return (6 + 2 * string.size());
}

void Connection::writeString(const QString &string)
{
	union {
		struct {
			quint32 size;
			quint16 byteOrderMark;
		} info;

		char data[6];
	} u;

	u.info.size = qToLittleEndian(string.size());
	u.info.byteOrderMark = 0xfeff;
	socket->write(u.data, 6);
	socket->write(reinterpret_cast<const char *>(string.constData()), 2 * string.size());
}
