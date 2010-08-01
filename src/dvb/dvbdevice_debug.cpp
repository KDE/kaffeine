/*
 * dvbdevice_debug.cpp
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

#include "dvbdevice_debug.h"

#include <QDir>
#include <QTimer>
#include <KDebug>
#include "dvbchannel.h"

extern "C" Q_DECL_EXPORT QObject *create_device_manager()
{
	return new DvbDeviceManager;
}

DvbDebugDevice::DvbDebugDevice() : buffer(NULL)
{
	kDebug();
}

DvbDebugDevice::~DvbDebugDevice()
{
	kDebug();
}

void DvbDebugDevice::setBuffer(DvbAbstractDeviceBuffer *buffer_)
{
	kDebug();
	buffer = buffer_;
}

QString DvbDebugDevice::getDeviceId()
{
	kDebug();
	return "D1234";
}

QString DvbDebugDevice::getFrontendName()
{
	kDebug();
	return "MyFrontend";
}

DvbDebugDevice::TransmissionTypes DvbDebugDevice::getTransmissionTypes()
{
	kDebug();
	return DvbDebugDevice::DvbT;
}

DvbDebugDevice::Capabilities DvbDebugDevice::getCapabilities()
{
	kDebug();
	return 0;
}

bool DvbDebugDevice::acquire()
{
	kDebug();
	return true;
}

bool DvbDebugDevice::setTone(SecTone tone)
{
	kDebug() << ((tone == ToneOff) ? "Off" : "On");
	return true;
}

bool DvbDebugDevice::setVoltage(SecVoltage voltage)
{
	kDebug() << ((voltage == Voltage13V) ? "13V" : "18V");
	return true;
}

bool DvbDebugDevice::sendMessage(const char *message, int length)
{
	QString string = QByteArray(message, length).toHex();

	for (int i = 2; i < string.size(); i += 3) {
		string.insert(i, ' ');
	}

	kDebug() << string;
	return true;
}

bool DvbDebugDevice::sendBurst(SecBurst burst)
{
	kDebug() << ((burst == BurstMiniA) ? "MiniA" : "MiniB");
	return true;
}

bool DvbDebugDevice::tune(const DvbTransponder &transponder)
{
	switch (transponder->getTransmissionType()) {
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *dvbCTransponder = transponder->getDvbCTransponder();
		kDebug() << "DvbC:" << dvbCTransponder->frequency << dvbCTransponder->symbolRate
			 << dvbCTransponder->modulation << dvbCTransponder->fecRate;
		break;
	    }

	case DvbTransponderBase::DvbS: {
		const DvbSTransponder *dvbSTransponder = transponder->getDvbSTransponder();
		kDebug() << "DvbS:" << dvbSTransponder->polarization << dvbSTransponder->frequency
			 << dvbSTransponder->symbolRate << dvbSTransponder->fecRate;
		break;
	    }

	case DvbTransponderBase::DvbS2: {
		const DvbS2Transponder *dvbS2Transponder = transponder->getDvbS2Transponder();
		kDebug() << "DvbS2:" << dvbS2Transponder->polarization
			 << dvbS2Transponder->frequency << dvbS2Transponder->symbolRate
			 << dvbS2Transponder->fecRate << dvbS2Transponder->modulation
			 << dvbS2Transponder->rollOff;
		break;
	    }

	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *dvbTTransponder = transponder->getDvbTTransponder();
		kDebug() << "DvbT:" << dvbTTransponder->frequency << dvbTTransponder->bandwidth
			 << dvbTTransponder->modulation << dvbTTransponder->fecRateHigh
			 << dvbTTransponder->fecRateLow << dvbTTransponder->transmissionMode
			 << dvbTTransponder->guardInterval << dvbTTransponder->hierarchy;
		break;
	    }

	case DvbTransponderBase::Atsc: {
		const AtscTransponder *atscTransponder = transponder->getAtscTransponder();
		kDebug() << "Atsc:" << atscTransponder->frequency << atscTransponder->modulation;
		break;
	    }

	default:
		kWarning() << "unknown transmission type" << transponder->getTransmissionType();
		return false;
	}

	QTimer::singleShot(150, this, SLOT(submitData()));
	return true;
}

int DvbDebugDevice::getSignal()
{
	kDebug();
	return 60;
}

int DvbDebugDevice::getSnr()
{
	kDebug();
	return 65;
}

bool DvbDebugDevice::isTuned()
{
	kDebug();
	return true;
}

bool DvbDebugDevice::addPidFilter(int pid)
{
	kDebug() << pid;
	return true;
}

void DvbDebugDevice::removePidFilter(int pid)
{
	kDebug() << pid;
}

void DvbDebugDevice::startDescrambling(const DvbPmtSection &pmtSection)
{
	Q_UNUSED(pmtSection)
	kDebug();
}

void DvbDebugDevice::stopDescrambling(int serviceId)
{
	kDebug() << serviceId;
}

void DvbDebugDevice::release()
{
	kDebug();
}

void DvbDebugDevice::submitData()
{
	QFile file(QDir::home().filePath("test.bin"));

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "cannot open" << file.fileName();
		return;
	}

	int bufferSize = buffer->size();

	while (true) {
		int size = file.read(buffer->getCurrent(), bufferSize);
		buffer->submitCurrent(size / 188);

		if (size != bufferSize) {
			break;
		}
	}
}

void DvbDebugDevice::execute(Command command, ReturnData returnData, Data data)
{
	DvbBackendAdapter<DvbDebugDevice>::execute(this, command, returnData, data);
}

DvbDeviceManager::DvbDeviceManager() : device(NULL)
{
	kDebug();
}

DvbDeviceManager::~DvbDeviceManager()
{
	kDebug();
	delete device;
}

int DvbDeviceManager::backendMagic()
{
	kDebug();
	return dvbBackendMagic;
}

void DvbDeviceManager::doColdPlug()
{
	kDebug();
	device = new DvbDebugDevice();
	emit deviceAdded(device);
}
