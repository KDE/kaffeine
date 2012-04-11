/*
 * connection.h
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

#ifndef CONNECTION_H
#define CONNECTION_H

#include <QObject>

class QLocalSocket;

class Connection : public QObject
{
	Q_OBJECT
public:
	explicit Connection(QLocalSocket *socket_);
	~Connection();

	enum Request {
		GetVersion = 0x67c60000
	};

	enum Reply {
		GetVersionReply = 0x58e70000
	};

private slots:
	void checkIdle(bool *idle);
	void readyRead();

private:
	void handlePacket();
	void writeHeader(quint32 command, quint32 length);

	int sizeForString(const QString &string);
	void writeString(const QString &string);

	QLocalSocket *socket;
	int packetCommand;
	int packetLength;
};

#endif /* CONNECTION_H */
