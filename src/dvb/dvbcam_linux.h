/*
 * dvbcam_linux.h
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

#ifndef DVBCAM_LINUX_H
#define DVBCAM_LINUX_H

#include <QMap>
#include <QTimer>

class QSocketNotifier;
class DvbLinuxCamService;
class DvbPmtSection;

class DvbLinuxCam : QObject
{
	Q_OBJECT
public:
	DvbLinuxCam();
	~DvbLinuxCam();

	void startCa(const QString &path);
	void startDescrambling(const QByteArray &pmtSection);
	void stopDescrambling(int serviceId);
	void stopCa();

private slots:
	void pollModule();
	void readyRead();

public:
	enum PendingCommand {
		ExpectingReply = (1 << 0),
		ResetCa = (1 << 1),
		SendCreateTransportConnection = (1 << 2),
		SendReceiveData = (1 << 3),
		SendPoll = (1 << 4),
		SendProfileEnquiry = (1 << 5),
		SendProfileChange = (1 << 6),
		SendApplicationInfoEnquiry = (1 << 7),
		SendCaInfoEnquiry = (1 << 8)
	};

	Q_DECLARE_FLAGS(PendingCommands, PendingCommand)

private:
	Q_DISABLE_COPY(DvbLinuxCam)

	enum TransportLayerTag {
		StatusByte = 0x80,
		ReceiveData = 0x81,
		CreateTransportConnection = 0x82,
		CreateTransportConnectionReply = 0x83,
		DataLast = 0xa0
	};

	enum {
		ConnectionId = 0x01,
		HeaderSize = 17
	};

	enum SessionLayerTag {
		OpenSessionRequest = 0x91,
		OpenSessionResponse = 0x92,
		SessionNumber = 0x90
	};

	enum Resources {
		ResourceManager = 0x00010041,
		ApplicationInformation = 0x00020041,
		ConditionalAccess = 0x00030041
	};

	enum SessionNumbers {
		ResourceManagerSession = 1,
		ApplicationInformationSession = 2,
		ConditionalAccessSession = 3
	};

	enum ApplicationLayerTag {
		ProfileEnquiry = 0x9f8010,
		ProfileReply = 0x9f8011,
		ProfileChange = 0x9f8012,
		ApplicationInfoEnquiry = 0x9f8020,
		ApplicationInfo = 0x9f8021,
		CaInfoEnquiry = 0x9f8030,
		CaInfo = 0x9f8031,
		CaPmt = 0x9f8032
	};

	enum CaPmtListManagement {
		Only = 0x03,
		Add = 0x04,
		Update = 0x05
	};

	enum CaPmtCommand {
		Descramble = 0x01,
		StopDescrambling = 0x04
	};

	bool detectSlot();
	int decodeLength(const unsigned char *&data, int &size);
	void resize(int messageSize);
	void handleTransportLayer(const unsigned char *data, int size);
	void handleSessionLayer(const unsigned char *data, int size);
	void handleApplicationLayer(const unsigned char *data, int size);
	void handlePendingCommands();
	void customEvent(QEvent *event);
	void sendCaPmt(const DvbPmtSection &pmtSection, CaPmtListManagement listManagement,
		CaPmtCommand command);
	void sendApplicationLayerMessage(ApplicationLayerTag tag, char *data, char *end);
	void sendSessionLayerMessage(SessionLayerTag tag, char *data, char *end);
	void sendTransportLayerMessage(TransportLayerTag tag, char *data, char *end);

	int caFd;
	int slot;
	QTimer pollTimer;
	QSocketNotifier *socketNotifier;
	PendingCommands pendingCommands;
	QByteArray message;
	char *messageData;
	bool ready;
	bool eventPosted;
	QMap<int, DvbLinuxCamService> services;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DvbLinuxCam::PendingCommands)

#endif /* DVBCAM_LINUX_H */
