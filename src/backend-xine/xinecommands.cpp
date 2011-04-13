/*
 * xinecommands.cpp
 *
 * Copyright (C) 2010-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "xinecommands.h"

#include <errno.h>
#include <unistd.h>
#include "../log.h"

XinePipeReader::XinePipeReader(int fd_, QObject *parent) : fd(fd_),
	notifier(fd, QSocketNotifier::Read)
{
	QObject::connect(&notifier, SIGNAL(activated(int)), parent, SLOT(readyRead()));

	buffer.resize(4080);
	bufferPosition = 0;
	bufferSize = 0;
	currentData = NULL;
	currentSize = -1;
}

XinePipeReader::~XinePipeReader()
{
}

void XinePipeReader::readyRead()
{
	while (true) {
		int bytesRead = read(fd, buffer.data() + bufferSize, buffer.size() - bufferSize);

		if ((bytesRead < 0) && (errno == EINTR)) {
			continue;
		}

		if (bytesRead > 0) {
			bufferSize += bytesRead;

			if (bufferSize == buffer.size()) {
				buffer.resize(buffer.size() + 4096);
				continue;
			}
		}

		break;
	}

	currentSize = -1;
}

int XinePipeReader::nextCommand()
{
	qint32 size = 5;

	if ((bufferSize - bufferPosition) >= 4) {
		memcpy(&size, buffer.constData() + bufferPosition, 4);

		if (size < 5) {
			Log("XinePipeReader::nextCommand: size too small");
			size = 5;
		}
	}

	if ((bufferSize - bufferPosition) < size) {
		if (bufferPosition == bufferSize) {
			if (buffer.size() != 4080) {
				buffer.clear();
				buffer.resize(4080);
			}

			bufferPosition = 0;
			bufferSize = 0;
		} else if (bufferPosition > 0) {
			memmove(buffer.data(), buffer.constData() + bufferPosition,
				bufferSize - bufferPosition);
			bufferSize -= bufferPosition;
			bufferPosition = 0;

			if (buffer.size() < size) {
				buffer.resize(4080 + ((size - 4080 + 4095) & (~4095)));
			} else if ((buffer.size() > 4080) && (size <= 4080)) {
				buffer.resize(4080);
			}
		}

		currentSize = -1;
		return -1;
	}

	currentData = buffer.constData() + bufferPosition + 5;
	currentSize = size - 5;
	bufferPosition += size;
	return *(currentData - 1);
}

bool XinePipeReader::isValid() const
{
	return (currentSize == 0);
}

bool XinePipeReader::readBool()
{
	bool value = false;

	if (currentSize >= 1) {
		value = (*currentData != 0);
		++currentData;
		--currentSize;
	} else {
		currentSize = -1;
	}

	return value;
}

qint8 XinePipeReader::readChar()
{
	qint8 value = 0;

	if (currentSize >= static_cast<int>(sizeof(value))) {
		memcpy(&value, currentData, sizeof(value));
		currentData += sizeof(value);
		currentSize -= sizeof(value);
	} else {
		currentSize = -1;
	}

	return value;
}

qint16 XinePipeReader::readShort()
{
	qint16 value = 0;

	if (currentSize >= static_cast<int>(sizeof(value))) {
		memcpy(&value, currentData, sizeof(value));
		currentData += sizeof(value);
		currentSize -= sizeof(value);
	} else {
		currentSize = -1;
	}

	return value;
}

qint32 XinePipeReader::readInt()
{
	qint32 value = 0;

	if (currentSize >= static_cast<int>(sizeof(value))) {
		memcpy(&value, currentData, sizeof(value));
		currentData += sizeof(value);
		currentSize -= sizeof(value);
	} else {
		currentSize = -1;
	}

	return value;
}

qint64 XinePipeReader::readLongLong()
{
	qint64 value = 0;

	if (currentSize >= static_cast<int>(sizeof(value))) {
		memcpy(&value, currentData, sizeof(value));
		currentData += sizeof(value);
		currentSize -= sizeof(value);
	} else {
		currentSize = -1;
	}

	return value;
}

QByteArray XinePipeReader::readByteArray()
{
	QByteArray byteArray;

	if (currentSize >= 0) {
		byteArray.resize(currentSize);
		memcpy(byteArray.data(), currentData, currentSize);
		currentSize = 0;
	}

	return byteArray;
}

QString XinePipeReader::readString()
{
	QString string;

	if ((currentSize >= 0) && ((currentSize & 0x01) == 0)) {
		string.resize(currentSize / 2);
		memcpy(reinterpret_cast<char *>(string.data()), currentData, currentSize);
		currentSize = 0;
	} else {
		currentSize = -1;
	}

	return string;
}

void XinePipeWriterBase::write(qint8 command, const char *firstArg, unsigned int firstArgSize,
	const char *secondArg, unsigned int secondArgSize)
{
	Q_UNUSED(command)
	Q_UNUSED(firstArg)
	Q_UNUSED(firstArgSize)
	Q_UNUSED(secondArg)
	Q_UNUSED(secondArgSize)
}

XinePipeWriter::XinePipeWriter(int fd_, QObject *parent) : QObject(parent), fd(fd_),
	notifier(fd, QSocketNotifier::Write)
{
	notifier.setEnabled(false);
	connect(&notifier, SIGNAL(activated(int)), this, SLOT(readyWrite()));

	buffer.resize(4080);
	bufferPosition = 0;
}

XinePipeWriter::~XinePipeWriter()
{
}

void XinePipeWriter::readyWrite()
{
	if (flush()) {
		notifier.setEnabled(false);
	}
}

void XinePipeWriter::write(qint8 command, const char *firstArg, unsigned int firstArgSize,
	const char *secondArg, unsigned int secondArgSize)
{
	if ((firstArgSize >= 0x100000) || (secondArgSize >= 0x100000)) {
		Log("XinePipeWriter::write: monstrous big write") << command;
		firstArgSize = 0;
		secondArgSize = 0;
	}

	qint32 size = 5 + firstArgSize + secondArgSize;

	if (size > (buffer.size() - bufferPosition)) {
		buffer.resize(4080 + ((bufferPosition + size - 4080 + 4095) & (~4095)));
	}

	char *data = buffer.data() + bufferPosition;
	memcpy(data, &size, 4);
	data[4] = command;
	memcpy(data + 5, firstArg, firstArgSize);
	memcpy(data + 5 + firstArgSize, secondArg, secondArgSize);
	bufferPosition += size;

	if (!flush()) {
		notifier.setEnabled(true);
	}
}

bool XinePipeWriter::flush()
{
	int bytesWritten;

	do {
		bytesWritten = ::write(fd, buffer.constData(), bufferPosition);
	} while ((bytesWritten < 0) && (errno == EINTR));

	if (bytesWritten > 0) {
		if (bytesWritten == bufferPosition) {
			if (buffer.size() != 4080) {
				buffer.clear();
				buffer.resize(4080);
			}

			bufferPosition = 0;
			return true;
		}

		memmove(buffer.data(), buffer.constData() + bytesWritten,
			bufferPosition - bytesWritten);
		bufferPosition -= bytesWritten;
	}

	return false;
}
