/*
 * dvbbackenddevice.h
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBBACKENDDEVICE_H
#define DVBBACKENDDEVICE_H

#include <QString>

class DvbPmtSection;
class DvbTransponder;

static const int dvbBackendMagic = 0x67a2c7f1;

class DvbAbstractDeviceBuffer
{
public:
	DvbAbstractDeviceBuffer() { }
	virtual ~DvbAbstractDeviceBuffer() { }

	// all those functions must be callable from a QThread

	virtual int size() = 0; // must be a multiple of 188
	virtual char *getCurrent() = 0;
	virtual void submitCurrent(int packets) = 0;
};

class DvbBackendDeviceBase
{
public:
	enum TransmissionType {
		DvbC	= (1 << 0),
		DvbS	= (1 << 1),
		DvbS2	= (1 << 4),
		DvbT	= (1 << 2),
		Atsc	= (1 << 3)
	};

	Q_DECLARE_FLAGS(TransmissionTypes, TransmissionType)

	enum Capability {
		DvbTModulationAuto		= (1 << 0),
		DvbTFecAuto			= (1 << 1),
		DvbTTransmissionModeAuto	= (1 << 2),
		DvbTGuardIntervalAuto		= (1 << 3)
	};

	Q_DECLARE_FLAGS(Capabilities, Capability)

	enum SecTone {
		ToneOff = 0,
		ToneOn  = 1
	};

	enum SecVoltage {
		Voltage13V = 0,
		Voltage18V = 1
	};

	enum SecBurst {
		BurstMiniA = 0,
		BurstMiniB = 1
	};

	enum Command {
		SetBuffer		=  0,
		GetDeviceId		=  1,
		GetFrontendName		=  2,
		GetTransmissionTypes	=  3,
		GetCapabilities		=  4,
		Acquire			=  5,
		SetTone			=  6,
		SetVoltage		=  7,
		SendMessage		=  8,
		SendBurst		=  9,
		Tune			= 10,
		GetSignal		= 11,
		GetSnr			= 12,
		IsTuned			= 13,
		AddPidFilter		= 14,
		RemovePidFilter		= 15,
		StartDescrambling	= 16,
		StopDescrambling	= 17,
		Release			= 18
	};

	class MessageData
	{
	public:
		MessageData(const char *messageData_, int messageLength_) :
			messageData(messageData_), messageLength(messageLength_) { }
		~MessageData() { }

		const char *messageData;
		int messageLength;
	};

	class ReturnData
	{
	public:
		ReturnData() { }
		explicit ReturnData(bool &boolean_) { data.boolean = &boolean_; }
		explicit ReturnData(int &integer_) { data.integer = &integer_; }
		explicit ReturnData(Capabilities &capabilities_)
			{ data.capabilities = &capabilities_; }
		explicit ReturnData(TransmissionTypes &transmissionTypes_)
			{ data.transmissionTypes = &transmissionTypes_; }
		explicit ReturnData(QString &string_) { data.string = &string_; }
		~ReturnData() { }

		union {
			bool *boolean;
			int *integer;
			Capabilities *capabilities;
			TransmissionTypes *transmissionTypes;
			QString *string;
		} data;
	};

	class Data
	{
	public:
		Data() { }
		explicit Data(int &integer_) { data.integer = integer_; }
		explicit Data(SecBurst &burst_) { data.burst = burst_; }
		explicit Data(SecTone &tone_) { data.tone = tone_; }
		explicit Data(SecVoltage &voltage_) { data.voltage = voltage_; }
		explicit Data(DvbAbstractDeviceBuffer *buffer) { data.buffer = buffer; }
		explicit Data(const MessageData &message_) { data.message = &message_; }
		explicit Data(const DvbPmtSection &pmtSection_) { data.pmtSection = &pmtSection_; }
		explicit Data(const DvbTransponder &transponder_)
			{ data.transponder = &transponder_; }
		~Data() { }

		union {
			int integer;
			SecBurst burst;
			SecTone tone;
			SecVoltage voltage;
			DvbAbstractDeviceBuffer *buffer;
			const MessageData *message;
			const DvbPmtSection *pmtSection;
			const DvbTransponder *transponder;
		} data;
	};

protected:
	DvbBackendDeviceBase() { }
	~DvbBackendDeviceBase() { }
};

class DvbBackendDevice : public DvbBackendDeviceBase
{
public:
	DvbBackendDevice() { }
	virtual ~DvbBackendDevice() { }

	virtual void execute(Command command, ReturnData returnData, Data data) = 0;
};

class DvbBackendDeviceAdapter : public DvbBackendDeviceBase
{
public:
	DvbBackendDeviceAdapter() : device(NULL) { }
	~DvbBackendDeviceAdapter() { }

	void setBuffer(DvbAbstractDeviceBuffer *buffer)
	{
		device->execute(SetBuffer, ReturnData(), Data(buffer));
	}

	QString getDeviceId()
	{
		QString result;
		device->execute(GetDeviceId, ReturnData(result), Data());
		return result;
	}

	QString getFrontendName()
	{
		QString result;
		device->execute(GetFrontendName, ReturnData(result), Data());
		return result;
	}

	TransmissionTypes getTransmissionTypes()
	{
		TransmissionTypes result = 0;
		device->execute(GetTransmissionTypes, ReturnData(result), Data());
		return result;
	}

	Capabilities getCapabilities()
	{
		Capabilities result = 0;
		device->execute(GetCapabilities, ReturnData(result), Data());
		return result;
	}

	bool acquire()
	{
		bool result = false;
		device->execute(Acquire, ReturnData(result), Data());
		return result;
	}

	bool setTone(SecTone tone)
	{
		bool result = false;
		device->execute(SetTone, ReturnData(result), Data(tone));
		return result;
	}

	bool setVoltage(SecVoltage voltage)
	{
		bool result = false;
		device->execute(SetVoltage, ReturnData(result), Data(voltage));
		return result;
	}

	bool sendMessage(const char *message, int length)
	{
		bool result = false;
		device->execute(SendMessage, ReturnData(result), Data(MessageData(message, length)));
		return result;
	}

	bool sendBurst(SecBurst burst)
	{
		bool result = false;
		device->execute(SendBurst, ReturnData(result), Data(burst));
		return result;
	}

	bool tune(const DvbTransponder &transponder)
	{
		bool result = false;
		device->execute(Tune, ReturnData(result), Data(transponder));
		return result;
	}

	int getSignal() // 0 - 100 ; -1 = unsupported
	{
		int result = -1;
		device->execute(GetSignal, ReturnData(result), Data());
		return result;
	}

	int getSnr() // 0 - 100 ; -1 = unsupported
	{
		int result = -1;
		device->execute(GetSnr, ReturnData(result), Data());
		return result;
	}

	bool isTuned()
	{
		bool result = false;
		device->execute(IsTuned, ReturnData(result), Data());
		return result;
	}

	bool addPidFilter(int pid)
	{
		bool result = false;
		device->execute(AddPidFilter, ReturnData(result), Data(pid));
		return result;
	}

	void removePidFilter(int pid)
	{
		device->execute(RemovePidFilter, ReturnData(), Data(pid));
	}

	void startDescrambling(const DvbPmtSection &pmtSection)
	{
		device->execute(StartDescrambling, ReturnData(), Data(pmtSection));
	}

	void stopDescrambling(int serviceId)
	{
		device->execute(StopDescrambling, ReturnData(), Data(serviceId));
	}

	void release()
	{
		device->execute(Release, ReturnData(), Data());
	}

	DvbBackendDevice *device;
};

template<class T> class DvbBackendAdapter : public DvbBackendDeviceBase
{
public:
	static void execute(T *instance, Command command, ReturnData returnData, Data data)
	{
		switch (command) {
		case SetBuffer:
			instance->setBuffer(data.data.buffer);
			break;
		case GetDeviceId:
			*returnData.data.string = instance->getDeviceId();
			break;
		case GetFrontendName:
			*returnData.data.string = instance->getFrontendName();
			break;
		case GetTransmissionTypes:
			*returnData.data.transmissionTypes = instance->getTransmissionTypes();
			break;
		case GetCapabilities:
			*returnData.data.capabilities = instance->getCapabilities();
			break;
		case Acquire:
			*returnData.data.boolean = instance->acquire();
			break;
		case SetTone:
			*returnData.data.boolean = instance->setTone(data.data.tone);
			break;
		case SetVoltage:
			*returnData.data.boolean = instance->setVoltage(data.data.voltage);
			break;
		case SendMessage:
			*returnData.data.boolean = instance->sendMessage(data.data.message->messageData,
				data.data.message->messageLength);
			break;
		case SendBurst:
			*returnData.data.boolean = instance->sendBurst(data.data.burst);
			break;
		case Tune:
			*returnData.data.boolean = instance->tune(*data.data.transponder);
			break;
		case GetSignal:
			*returnData.data.integer = instance->getSignal();
			break;
		case GetSnr:
			*returnData.data.integer = instance->getSnr();
			break;
		case IsTuned:
			*returnData.data.boolean = instance->isTuned();
			break;
		case AddPidFilter:
			*returnData.data.boolean = instance->addPidFilter(data.data.integer);
			break;
		case RemovePidFilter:
			instance->removePidFilter(data.data.integer);
			break;
		case StartDescrambling:
			instance->startDescrambling(*data.data.pmtSection);
			break;
		case StopDescrambling:
			instance->stopDescrambling(data.data.integer);
			break;
		case Release:
			instance->release();
			break;
		}
	}
};

#endif /* DVBBACKENDDEVICE_H */
