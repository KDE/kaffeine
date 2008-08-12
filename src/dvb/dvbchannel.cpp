/*
 * dvbchannel.cpp
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dvbchannel.h"

int DvbLineReader::readInt(bool allowEmpty)
{
	QString string = readString();

	if (string.isEmpty()) {
		if (!allowEmpty) {
			valid = false;
		}

		return -1;
	}

	bool ok;
	int value = string.toInt(&ok);

	if (!ok || (value < 0)) {
		valid = false;
	}

	return value;
}

QString DvbLineReader::readString()
{
	int begin = pos;
	int end = pos;

	while ((end < line.size()) && (line[end] != '|')) {
		++end;
	}

	pos = end + 1;

	if (end > begin) {
		return line.mid(begin, end - begin);
	} else {
		return QString();
	}
}

QString DvbLineWriter::getLine()
{
	int index = line.size() - 1;
	Q_ASSERT(index >= 0);
	line[index] = '\n';
	return line;
}

void DvbLineWriter::writeInt(int value)
{
	if (value >= 0) {
		line.append(QString::number(value));
	}

	line.append('|');
}

void DvbLineWriter::writeString(const QString &string)
{
	line.append(string);
	line.append('|');
}

DvbCTransponder *DvbLineReader::readDvbCTransponder()
{
	DvbCTransponder *transponder = new DvbCTransponder;

	transponder->frequency = readInt();
	transponder->symbolRate = readInt();
	transponder->modulation = readEnum(DvbCTransponder::ModulationMax);
	transponder->fecRate = readEnum(DvbCTransponder::FecRateMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

void DvbLineWriter::writeTransponder(const DvbCTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->symbolRate);
	writeInt(transponder->modulation);
	writeInt(transponder->fecRate);
}

DvbSTransponder *DvbLineReader::readDvbSTransponder()
{
	DvbSTransponder *transponder = new DvbSTransponder;

	transponder->frequency = readInt();
	transponder->polarization = readEnum(DvbSTransponder::PolarizationMax);
	transponder->symbolRate = readInt();
	transponder->fecRate = readEnum(DvbSTransponder::FecRateMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

void DvbLineWriter::writeTransponder(const DvbSTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->polarization);
	writeInt(transponder->symbolRate);
	writeInt(transponder->fecRate);
}

DvbTTransponder *DvbLineReader::readDvbTTransponder()
{
	DvbTTransponder *transponder = new DvbTTransponder;

	transponder->frequency = readInt();
	transponder->bandwidth = readEnum(DvbTTransponder::BandwidthMax);
	transponder->modulation = readEnum(DvbTTransponder::ModulationMax);
	transponder->fecRateHigh = readEnum(DvbTTransponder::FecRateMax);
	transponder->fecRateLow = readEnum(DvbTTransponder::FecRateMax);
	transponder->transmissionMode = readEnum(DvbTTransponder::TransmissionModeMax);
	transponder->guardInterval = readEnum(DvbTTransponder::GuardIntervalMax);
	transponder->hierarchy = readEnum(DvbTTransponder::HierarchyMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

void DvbLineWriter::writeTransponder(const DvbTTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->bandwidth);
	writeInt(transponder->modulation);
	writeInt(transponder->fecRateHigh);
	writeInt(transponder->fecRateLow);
	writeInt(transponder->transmissionMode);
	writeInt(transponder->guardInterval);
	writeInt(transponder->hierarchy);
}

AtscTransponder *DvbLineReader::readAtscTransponder()
{
	AtscTransponder *transponder = new AtscTransponder;

	transponder->frequency = readInt();
	transponder->modulation = readEnum(AtscTransponder::ModulationMax);

	if (!isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

void DvbLineWriter::writeTransponder(const AtscTransponder *transponder)
{
	writeInt(transponder->frequency);
	writeInt(transponder->modulation);
}

DvbChannel *DvbLineReader::readChannel()
{
	DvbChannel *channel = new DvbChannel;

	int type = readInt();

	channel->name = readString();
	channel->number = readInt();

	channel->source = readString();
	channel->networkId = readInt(true);
	channel->transportStreamId = readInt(true);
	channel->serviceId = readInt();

	channel->pmtPid = readInt();
	channel->videoPid = readInt(true);
	channel->audioPid = readInt(true);

	channel->scrambled = (readInt() & 0x1) != 0;

	if (!isValid()) {
		delete channel;
		return NULL;
	}

	DvbTransponderBase *transponder = NULL;

	switch (type) {
	case DvbTransponderBase::DvbC:
		transponder = readDvbCTransponder();
		break;
	case DvbTransponderBase::DvbS:
		transponder = readDvbSTransponder();
		break;
	case DvbTransponderBase::DvbT:
		transponder = readDvbTTransponder();
		break;
	case DvbTransponderBase::Atsc:
		transponder = readAtscTransponder();
		break;
	}

	if (transponder == NULL) {
		delete channel;
		return NULL;
	}

	channel->transponder = DvbTransponder(transponder);

	return channel;
}

void DvbLineWriter::writeChannel(const DvbChannel *channel)
{
	int type = channel->transponder->getTransmissionType();

	writeInt(type);

	writeString(channel->name);
	writeInt(channel->number);

	writeString(channel->source);
	writeInt(channel->networkId);
	writeInt(channel->transportStreamId);
	writeInt(channel->serviceId);

	writeInt(channel->pmtPid);
	writeInt(channel->videoPid);
	writeInt(channel->audioPid);

	writeInt(channel->scrambled ? 1 : 0);

	switch (type) {
	case DvbTransponderBase::DvbC:
		writeTransponder(channel->transponder->getDvbCTransponder());
		break;
	case DvbTransponderBase::DvbS:
		writeTransponder(channel->transponder->getDvbSTransponder());
		break;
	case DvbTransponderBase::DvbT:
		writeTransponder(channel->transponder->getDvbTTransponder());
		break;
	case DvbTransponderBase::Atsc:
		writeTransponder(channel->transponder->getAtscTransponder());
		break;
	}
}
