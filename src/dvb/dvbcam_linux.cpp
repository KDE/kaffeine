/*
 * dvbcam_linux.cpp
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

#include "dvbcam_linux.h"

#include <QCoreApplication>
#include <QFile>
#include <QSocketNotifier>
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/ca.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../log.h"
#include "dvbsi.h"

// krazy:excludeall=syscalls

class DvbLinuxCamService
{
public:
	DvbLinuxCamService() : pendingAction(Add) { }
	~DvbLinuxCamService() { }

	enum PendingActions {
		Nothing,
		Add,
		Update,
		Remove
	};

	PendingActions pendingAction;
	QByteArray pmtSectionData;
};

DvbLinuxCam::DvbLinuxCam() : caFd(-1), socketNotifier(NULL), ready(false), eventPosted(false)
{
	connect(&pollTimer, SIGNAL(timeout()), this, SLOT(pollModule()));
}

DvbLinuxCam::~DvbLinuxCam()
{
}

void DvbLinuxCam::startCa(const QString &path)
{
	Q_ASSERT(caFd < 0);
	caFd = open(QFile::encodeName(path).constData(), O_RDWR | O_NONBLOCK);

	if (caFd < 0) {
		Log("DvbLinuxCam::startCa: cannot open") << path;
		return;
	}

	slot = -1;
	pollTimer.start(5000);
	pendingCommands = ResetCa;

	if (!detectSlot()) {
		pendingCommands = 0;
	}
}

void DvbLinuxCam::startDescrambling(const QByteArray &pmtSectionData)
{
	DvbPmtSection pmtSection(pmtSectionData);

	if (!pmtSection.isValid()) {
		Log("DvbLinuxCam::startDescrambling: pmt section is invalid");
		return;
	}

	int serviceId = pmtSection.programNumber();
	QMap<int, DvbLinuxCamService>::iterator it = services.find(serviceId);

	if (it == services.end()) {
		it = services.insert(serviceId, DvbLinuxCamService());
	}

	if (it->pendingAction != DvbLinuxCamService::Add) {
		it->pendingAction = DvbLinuxCamService::Update;
	}

	it->pmtSectionData = pmtSectionData;

	if (ready && !eventPosted) {
		eventPosted = true;
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
	}
}

void DvbLinuxCam::stopDescrambling(int serviceId)
{
	QMap<int, DvbLinuxCamService>::iterator it = services.find(serviceId);

	if (it == services.end()) {
		Log("DvbLinuxCam::stopDescrambling: cannot find service id") << serviceId;
		return;
	}

	switch (it->pendingAction) {
	case DvbLinuxCamService::Nothing:
	case DvbLinuxCamService::Update:
		it->pendingAction = DvbLinuxCamService::Remove;
		break;
	case DvbLinuxCamService::Add:
		services.erase(it);
		return;
	case DvbLinuxCamService::Remove:
		Log("DvbLinuxCam::stopDescrambling: service is already being removed");
		services.erase(it);
		return;
	}

	if (ready && !eventPosted) {
		eventPosted = true;
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
	}
}

void DvbLinuxCam::stopCa()
{
	services.clear();
	ready = false;
	eventPosted = false;

	delete socketNotifier;
	socketNotifier = NULL;

	pollTimer.stop();

	if (caFd >= 0) {
		close(caFd);
		caFd = -1;
	}
}

void DvbLinuxCam::pollModule()
{
	if (slot < 0) {
		detectSlot();
	} else {
		if ((pendingCommands & ExpectingReply) != 0) {
			pendingCommands &= ~ExpectingReply;
			Log("DvbLinuxCam::pollModule: request timed out");
		}

		if (pendingCommands == 0) {
			pendingCommands |= SendPoll;
		}

		handlePendingCommands();
	}
}

void DvbLinuxCam::readyRead()
{
	QByteArray buffer;
	buffer.resize(256);
	int size = 0;

	while (true) {
		int bytesRead = int(read(caFd, buffer.data() + size, buffer.size() - size));

		if ((bytesRead < 0) && (errno == EINTR)) {
			continue;
		}

		if (bytesRead == (buffer.size() - size)) {
			size += bytesRead;
			buffer.resize(4 * buffer.size());
			continue;
		}

		if (bytesRead > 0) {
			size += bytesRead;
		}

		break;
	}

	const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer.constData());

	if ((size >= 2) && (data[0] == slot) && (data[1] == ConnectionId)) {
		pendingCommands &= ~ExpectingReply;
		handleTransportLayer(data + 2, size - 2);
		handlePendingCommands();
	} else {
		Log("DvbLinuxCam::readyRead: unknown recipient");
	}
}

bool DvbLinuxCam::detectSlot()
{
	Q_ASSERT((caFd >= 0) && (slot < 0));
	ca_caps_t caInfo;
	memset(&caInfo, 0, sizeof(caInfo));

	if (ioctl(caFd, CA_GET_CAP, &caInfo) != 0) {
		Log("DvbLinuxCam::detectSlot: cannot perform ioctl CA_GET_CAP");
		return false;
	}

	for (uint i = 0; i < caInfo.slot_num; ++i) {
		ca_slot_info_t slotInfo;
		memset(&slotInfo, 0, sizeof(slotInfo));
		slotInfo.num = i;

		if (ioctl(caFd, CA_GET_SLOT_INFO, &slotInfo) != 0) {
			Log("DvbLinuxCam::detectSlot: "
			    "cannot perform ioctl CA_GET_SLOT_INFO for slot") << slot;
			continue;
		}

		if ((slotInfo.type & CA_CI_LINK) == 0) {
			Log("DvbLinuxCam::detectSlot: unknown type") << slotInfo.type;
			continue;
		}

		if ((slotInfo.flags & CA_CI_MODULE_READY) != 0) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		return false;
	}

	if (socketNotifier == NULL) {
		socketNotifier = new QSocketNotifier(caFd, QSocketNotifier::Read, this);
		connect(socketNotifier, SIGNAL(activated(int)), this, SLOT(readyRead()));
	}

	if (pendingCommands == 0) {
		pendingCommands |= SendCreateTransportConnection;
	}

	message.resize(64);
	messageData = message.data() + HeaderSize;
	handlePendingCommands();
	return true;
}

int DvbLinuxCam::decodeLength(const unsigned char *&data, int &size)
{
	int length = 0;

	if (size > 0) {
		length = data[0];
	}

	if ((length & 0x80) == 0) {
		++data;
		--size;
	} else {
		int temp = (length & 0x7f) + 1;
		length = 0;

		if (temp <= size) {
			for (int i = 1; i < temp; ++i) {
				length = ((length << 8) | data[i]);
			}
		}

		data += temp;
		size -= temp;
	}

	if (length > size) {
		length = size;
	}

	return length;
}

void DvbLinuxCam::resize(int messageSize)
{
	if (message.size() < (messageSize + HeaderSize)) {
		message.resize(2 * (messageSize + HeaderSize));
		messageData = message.data() + HeaderSize;
	}
}

void DvbLinuxCam::handleTransportLayer(const unsigned char *data, int size)
{
	while (size > 0) {
		int tag = data[0];
		++data;
		--size;
		int length = decodeLength(data, size);

		switch (tag) {
		case StatusByte:
			if ((length < 2) || (data[0] != ConnectionId)) {
				size = 0;
				Log("DvbLinuxCam::handleTransportLayer: "
				    "invalid StatusByte object");
				break;
			}

			if ((data[1] & 0x80) != 0) {
				pendingCommands |= SendReceiveData;
			}

			break;
		case CreateTransportConnectionReply:
			if ((length < 1) || (data[0] != ConnectionId)) {
				size = 0;
				Log("DvbLinuxCam::handleTransportLayer: "
				    "invalid CreateTransportConnectionReply object");
				break;
			}

			pendingCommands &= ~(SendCreateTransportConnection);
			break;
		case DataLast:
			if ((length < 1) || (data[0] != ConnectionId)) {
				size = 0;
				Log("DvbLinuxCam::handleTransportLayer: invalid DataLast object");
				break;
			}

			handleSessionLayer(data + 1, length - 1);
			break;
		default:
			Log("DvbLinuxCam::handleTransportLayer: unknown tag") << tag;
			break;
		}

		data += length;
		size -= length;
	}
}

void DvbLinuxCam::handleSessionLayer(const unsigned char *data, int size)
{
	if (size > 0) {
		int tag = data[0];
		++data;
		--size;
		int length = decodeLength(data, size);

		switch (tag) {
		case OpenSessionRequest: {
			if (length < 4) {
				Log("DvbLinuxCam::handleSessionLayer: "
				    "invalid OpenSessionRequest object");
				break;
			}

			unsigned int resource = ((data[0] << 24) | (data[1] << 16) |
				(data[2] << 8) | data[3]);

			messageData[0] = 0x00;
			messageData[1] = (resource >> 24);
			messageData[2] = (resource >> 16) & 0xff;
			messageData[3] = (resource >> 8) & 0xff;
			messageData[4] = resource & 0xff;
			messageData[5] = 0x00;
			messageData[6] = 0x00;

			switch (resource) {
			case ResourceManager:
				messageData[6] = ResourceManagerSession;
				pendingCommands |= SendProfileEnquiry;
				break;
			case ApplicationInformation:
				messageData[6] = ApplicationInformationSession;
				pendingCommands |= SendApplicationInfoEnquiry;
				break;
			case ConditionalAccess:
				messageData[6] = ConditionalAccessSession;
				pendingCommands |= SendCaInfoEnquiry;
				break;
			default:
				messageData[0] = quint8(0xf0);
				break;
			}

			sendSessionLayerMessage(OpenSessionResponse, messageData, messageData + 7);
			break;
		    }
		case SessionNumber:
			if (length < 2) {
				Log("DvbLinuxCam::handleSessionLayer: "
				    "invalid SessionNumber object");
				break;
			}

			handleApplicationLayer(data + length, size - length);
			break;
		default:
			Log("DvbLinuxCam::handleSessionLayer: unknown tag") << tag;
			break;
		}
	}
}

void DvbLinuxCam::handleApplicationLayer(const unsigned char *data, int size)
{
	while (size >= 3) {
		int tag = ((data[0] << 16) | (data[1] << 8) | data[2]);
		data += 3;
		size -= 3;
		int length = decodeLength(data, size);

		switch (tag) {
		case ProfileEnquiry:
			messageData[0] = ((ResourceManager >> 24) & 0xff);
			messageData[1] = ((ResourceManager >> 16) & 0xff);
			messageData[2] = ((ResourceManager >> 8) & 0xff);
			messageData[3] = (ResourceManager & 0xff);
			messageData[4] = ((ApplicationInformation >> 24) & 0xff);
			messageData[5] = ((ApplicationInformation >> 16) & 0xff);
			messageData[6] = ((ApplicationInformation >> 8) & 0xff);
			messageData[7] = (ApplicationInformation & 0xff);
			messageData[8] = ((ConditionalAccess >> 24) & 0xff);
			messageData[9] = ((ConditionalAccess >> 16) & 0xff);
			messageData[10] = ((ConditionalAccess >> 8) & 0xff);
			messageData[11] = (ConditionalAccess & 0xff);
			sendApplicationLayerMessage(ProfileReply, messageData, messageData + 12);
			break;
		case ProfileReply:
			pendingCommands |= SendProfileChange;
			break;
		case ApplicationInfo:
			break;
		case CaInfo:
			ready = true;
			eventPosted = true;
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
			break;
		default:
			Log("DvbLinuxCam::handleApplicationLayer: unknown tag") << tag;
			break;
		}

		data += length;
		size -= length;
	}
}

void DvbLinuxCam::handlePendingCommands()
{
	if ((pendingCommands & ExpectingReply) == 0) {
		int pendingCommand = pendingCommands & (~pendingCommands + 1);
		pendingCommands &= ~pendingCommand;

		switch (pendingCommand) {
		case 0:
			break;
		case ResetCa:
			if (ioctl(caFd, CA_RESET, 0xff) != 0) {
				Log("DvbLinuxCam::handlePendingCommands: "
				    "cannot perform ioctl CA_RESET");
			}

			Log("DvbLinuxCam::handlePendingCommands: --> reset");
			slot = -1;
			pollTimer.start(100);
			pendingCommands = 0;
			break;
		case SendCreateTransportConnection:
			messageData[0] = ConnectionId;
			sendTransportLayerMessage(CreateTransportConnection, messageData,
				messageData + 1);
			pendingCommands |= SendCreateTransportConnection;
			break;
		case SendPoll:
			messageData[0] = ConnectionId;
			sendTransportLayerMessage(DataLast, messageData, messageData + 1);
			break;
		case SendReceiveData:
			messageData[0] = ConnectionId;
			sendTransportLayerMessage(ReceiveData, messageData, messageData + 1);
			break;
		case SendProfileEnquiry:
			sendApplicationLayerMessage(ProfileEnquiry, messageData, messageData);
			break;
		case SendProfileChange:
			sendApplicationLayerMessage(ProfileChange, messageData, messageData);
			break;
		case SendApplicationInfoEnquiry:
			sendApplicationLayerMessage(ApplicationInfoEnquiry, messageData,
				messageData);
			break;
		case SendCaInfoEnquiry:
			sendApplicationLayerMessage(CaInfoEnquiry, messageData, messageData);
			break;
		default:
			Log("DvbLinuxCam::handlePendingCommands: unknown pending command") <<
				pendingCommand;
			break;
		}
	}
}

void DvbLinuxCam::customEvent(QEvent *event)
{
	Q_UNUSED(event)

	if (!ready) {
		return;
	}

	int activeCaPmts = 0;

	for (QMap<int, DvbLinuxCamService>::iterator it = services.begin();
	     it != services.end();) {
		if (it->pendingAction == DvbLinuxCamService::Remove) {
			DvbPmtSection pmtSection(it->pmtSectionData);
			sendCaPmt(pmtSection, Update, StopDescrambling);
			it = services.erase(it);
		} else {
			++activeCaPmts;
			++it;
		}
	}

	for (QMap<int, DvbLinuxCamService>::iterator it = services.begin();
	     it != services.end(); ++it) {
		switch (it->pendingAction) {
		case DvbLinuxCamService::Nothing:
			continue;
		case DvbLinuxCamService::Add:
			if (activeCaPmts == 1) {
				DvbPmtSection pmtSection(it->pmtSectionData);
				sendCaPmt(pmtSection, Only, Descramble);
			} else {
				DvbPmtSection pmtSection(it->pmtSectionData);
				sendCaPmt(pmtSection, Add, Descramble);
			}

			break;
		case DvbLinuxCamService::Update: {
			DvbPmtSection pmtSection(it->pmtSectionData);
			sendCaPmt(pmtSection, Update, Descramble);
			break;
		    }
		case DvbLinuxCamService::Remove:
			Log("DvbLinuxCam::customEvent: impossible");
			break;
		}

		it->pendingAction = DvbLinuxCamService::Nothing;
	}

	eventPosted = false;
}

void DvbLinuxCam::sendCaPmt(const DvbPmtSection &pmtSection, CaPmtListManagement listManagement,
	CaPmtCommand command)
{
	messageData[0] = listManagement;
	messageData[1] = ((pmtSection.programNumber() >> 8) & 0xff);
	messageData[2] = (pmtSection.programNumber() & 0xff);
	messageData[3] = (((pmtSection.versionNumber() << 1) & 0xff) |
		(pmtSection.currentNextIndicator() ? 0x01 : 0x00));

	int index = 6;
	int lengthIndex = 4;
	int length = 0;

	for (DvbDescriptor descriptor = pmtSection.descriptors(); descriptor.isValid();
	     descriptor.advance()) {
		if (descriptor.descriptorTag() == 0x09) {
			resize(index + 1 + descriptor.getLength());

			if (length == 0) {
				messageData[index++] = command;
				++length;
			}

			memcpy(messageData + index, descriptor.getData(), descriptor.getLength());
			index += descriptor.getLength();
			length += descriptor.getLength();
		}
	}

	messageData[lengthIndex] = ((length >> 8) & 0xff);
	messageData[lengthIndex + 1] = (length & 0xff);

	for (DvbPmtSectionEntry entry = pmtSection.entries(); entry.isValid(); entry.advance()) {
		resize(index + 5);
		messageData[index++] = (entry.streamType() & 0xff);
		messageData[index++] = ((entry.pid() >> 8) & 0xff);
		messageData[index++] = (entry.pid() & 0xff);
		lengthIndex = index;
		index += 2;
		length = 0;

		for (DvbDescriptor descriptor = entry.descriptors(); descriptor.isValid();
		     descriptor.advance()) {
			if (descriptor.descriptorTag() == 0x09) {
				resize(index + 1 + descriptor.getLength());

				if (length == 0) {
					messageData[index++] = command;
					++length;
				}

				memcpy(messageData + index, descriptor.getData(),
					descriptor.getLength());
				index += descriptor.getLength();
				length += descriptor.getLength();
			}
		}

		messageData[lengthIndex] = ((length >> 8) & 0xff);
		messageData[lengthIndex + 1] = (length & 0xff);
	}

	sendApplicationLayerMessage(CaPmt, messageData, messageData + index);
}

void DvbLinuxCam::sendTransportLayerMessage(TransportLayerTag tag, char *data, char *end)
{
	uint length = uint(end - data);
	Q_ASSERT(length < 0x10000);

	if (length < 0x80) {
		*(--data) = (length & 0xff);
	} else {
		*(--data) = (length & 0xff);
		*(--data) = ((length >> 8) & 0xff);
		*(--data) = quint8(0x82);
	}

	*(--data) = tag;
	*(--data) = ConnectionId;
	*(--data) = (slot & 0xff);
	length = uint(end - data);

	if (write(caFd, data, length) != length) {
		Log("DvbLinuxCam::sendTransportLayerMessage: cannot send message of length") <<
			length;
	}

	pendingCommands |= ExpectingReply;
	pollTimer.start(400);
}

void DvbLinuxCam::sendSessionLayerMessage(SessionLayerTag tag, char *data, char *end)
{
	switch (tag) {
	case OpenSessionResponse:
		Q_ASSERT((end - data) == 0x07);
		*(--data) = 0x07;
		break;
	case SessionNumber:
		Q_ASSERT((end - data) >= 0x02);
		*(--data) = 0x02;
		break;
	default:
		Q_ASSERT(false);
		return;
	}

	*(--data) = tag;
	*(--data) = ConnectionId;
	sendTransportLayerMessage(DataLast, data, end);
}

void DvbLinuxCam::sendApplicationLayerMessage(ApplicationLayerTag tag, char *data, char *end)
{
	uint length = uint(end - data);
	Q_ASSERT(length < 0x10000);

	if (length < 0x80) {
		*(--data) = (length & 0xff);
	} else {
		*(--data) = (length & 0xff);
		*(--data) = ((length >> 8) & 0xff);
		*(--data) = quint8(0x82);
	}

	*(--data) = (tag & 0xff);
	*(--data) = ((tag >> 8) & 0xff);
	*(--data) = (tag >> 16);

	switch (tag) {
	case ProfileEnquiry:
	case ProfileReply:
	case ProfileChange:
		*(--data) = ResourceManagerSession;
		*(--data) = (ResourceManagerSession >> 8);
		break;
	case ApplicationInfoEnquiry:
		*(--data) = ApplicationInformationSession;
		*(--data) = (ApplicationInformationSession >> 8);
		break;
	case CaInfoEnquiry:
	case CaPmt:
		*(--data) = ConditionalAccessSession;
		*(--data) = (ConditionalAccessSession >> 8);
		break;
	default:
		Q_ASSERT(false);
		return;
	}

	sendSessionLayerMessage(SessionNumber, data, end);
}
