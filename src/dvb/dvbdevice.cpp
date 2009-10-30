/*
 * dvbdevice.cpp
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbdevice.h"

#include <unistd.h>
#include <cmath>
#include <QCoreApplication>
#include <QDir>
#include <KDebug>
#include "dvbconfig.h"
#include "dvbmanager.h"

struct DvbFilterData
{
	char packets[21][188];
	int count;
	DvbFilterData *next;
};

class DvbFilterInternal
{
public:
	DvbFilterInternal() : activeFilters(0) { }
	~DvbFilterInternal() { }

	QList<DvbPidFilter *> filters;
	int activeFilters;
};

class DvbDummyFilter : public DvbPidFilter
{
public:
	DvbDummyFilter() { }
	~DvbDummyFilter() { }

	void processData(const char [188]) { }
};

class DvbDataDumper : public DvbPidFilter
{
public:
	DvbDataDumper();
	~DvbDataDumper();

	void processData(const char [188]);

private:
	QFile file;
};

DvbDataDumper::DvbDataDumper()
{
	file.setFileName(QDir::homePath() + '/' + "KaffeineDvbDump-" +
		QString("%1").arg(qrand(), 16) + ".bin");

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "couldn't open" << file.fileName();
	}
}

DvbDataDumper::~DvbDataDumper()
{
}

void DvbDataDumper::processData(const char data[188])
{
	file.write(data, 188);
}

DvbDevice::DvbDevice(DvbBackendDevice *backendDevice_, QObject *parent) : QObject(parent),
	backendDevice(backendDevice_), deviceState(DeviceReleased), dataDumper(NULL),
	cleanUpFilters(false), isAuto(false)
{
	backendDevice->buffer = this;

	connect(&frontendTimer, SIGNAL(timeout()), this, SLOT(frontendEvent()));
	dummyFilter = new DvbDummyFilter;

	currentUnused = new DvbFilterData;
	currentUnused->next = currentUnused;
	currentUsed = currentUnused;
	totalBuffers = 1;
	usedBuffers = 0;
}

DvbDevice::~DvbDevice()
{
	release();

	for (int i = 0; i < totalBuffers; ++i) {
		DvbFilterData *temp = currentUnused->next;
		delete currentUnused;
		currentUnused = temp;
	}

	delete dataDumper;
	delete dummyFilter;
}

DvbBackendDevice::TransmissionTypes DvbDevice::getTransmissionTypes() const
{
	return backendDevice->getTransmissionTypes();
}

QString DvbDevice::getDeviceId() const
{
	return backendDevice->getDeviceId();
}

QString DvbDevice::getFrontendName() const
{
	return backendDevice->getFrontendName();
}

void DvbDevice::tune(const DvbTransponder &transponder)
{
	DvbTransponderBase::TransmissionType transmissionType = transponder->getTransmissionType();

	if ((transmissionType != DvbTransponderBase::DvbS) &&
	    (transmissionType != DvbTransponderBase::DvbS2)) {
		if (backendDevice->tune(transponder)) {
			setDeviceState(DeviceTuning);
			frontendTimeout = config->timeout;
			frontendTimer.start(100);
			discardBuffers();
		} else {
			setDeviceState(DeviceIdle);
		}

		return;
	}

	bool moveRotor = false;

	const DvbSTransponder *dvbSTransponder = NULL;
	const DvbS2Transponder *dvbS2Transponder = NULL;

	if (transmissionType == DvbTransponderBase::DvbS) {
		dvbSTransponder = transponder->getDvbSTransponder();
	} else {
		// DVB-S2
		dvbS2Transponder = transponder->getDvbS2Transponder();
		dvbSTransponder = dvbS2Transponder;
	}

	// parameters

	bool horPolar = (dvbSTransponder->polarization == DvbSTransponder::Horizontal) ||
			(dvbSTransponder->polarization == DvbSTransponder::CircularLeft);

	int frequency = dvbSTransponder->frequency;
	bool highBand = false;

	if (config->switchFrequency != 0) {
		// dual LO (low / high)
		if (frequency < config->switchFrequency) {
			frequency = abs(frequency - config->lowBandFrequency);
		} else {
			frequency = abs(frequency - config->highBandFrequency);
			highBand = true;
		}
	} else if (config->highBandFrequency != 0) {
		// single LO (horizontal / vertical)
		if (horPolar) {
			frequency = abs(frequency - config->lowBandFrequency);
		} else {
			frequency = abs(frequency - config->highBandFrequency);
		}
	} else {
		// single LO
		frequency = abs(frequency - config->lowBandFrequency);
	}

	// tone off

	backendDevice->setTone(DvbBackendDevice::ToneOff);

	// horizontal / circular left --> 18V ; vertical / circular right --> 13V

	backendDevice->setVoltage(horPolar ? DvbBackendDevice::Voltage18V :
				  DvbBackendDevice::Voltage13V);

	// diseqc / rotor

	usleep(15000);

	switch (config->configuration) {
	case DvbConfigBase::DiseqcSwitch: {
		char cmd[] = { 0xe0, 0x10, 0x38, 0x00 };
		cmd[3] = 0xf0 | (config->lnbNumber << 2) | (horPolar ? 2 : 0) | (highBand ? 1 : 0);
		backendDevice->sendMessage(cmd, sizeof(cmd));
		usleep(15000);

		backendDevice->sendBurst(((config->lnbNumber & 0x1) == 0) ?
			DvbBackendDevice::BurstMiniA : DvbBackendDevice::BurstMiniB);
		usleep(15000);
		break;
	    }

	case DvbConfigBase::UsalsRotor: {
		QString source = config->scanSource;
		source.remove(0, source.lastIndexOf('-') + 1);

		bool ok = false;
		double position = 0;

		if (source.endsWith('E')) {
			source.chop(1);
			position = source.toDouble(&ok);
		} else if (source.endsWith('W')) {
			source.chop(1);
			position = -source.toDouble(&ok);
		}

		if (!ok) {
			kWarning() << "couldn't extract orbital position";
		}

		double longitudeDiff = DvbManager::getLongitude() - position;

		double latRad = DvbManager::getLatitude() * M_PI / 180;
		double longDiffRad = longitudeDiff * M_PI / 180;
		double temp = cos(latRad) * cos(longDiffRad);
		double temp2 = -sin(latRad) * cos(longDiffRad) / sqrt(1 - temp * temp);

		// deal with corner cases
		if (temp2 < -1) {
			temp2 = -1;
		} else if (temp2 > 1) {
			temp2 = 1;
		} else if (temp2 != temp2) {
			temp2 = 1;
		}

		double azimuth = acos(temp2) * 180 / M_PI;

		if (((longitudeDiff > 0) && (longitudeDiff < 180)) || (longitudeDiff < -180)) {
			azimuth = 360 - azimuth;
		}

		int value = (azimuth * 16) + 0.5;

		if (value == (360 * 16)) {
			value = 0;
		}

		char cmd[] = { 0xe0, 0x31, 0x6e, value / 256, value % 256 };
		backendDevice->sendMessage(cmd, sizeof(cmd));
		usleep(15000);
		moveRotor = true;
		break;
	    }

	case DvbConfigBase::PositionsRotor: {
		char cmd[] = { 0xe0, 0x31, 0x6b, config->lnbNumber };
		backendDevice->sendMessage(cmd, sizeof(cmd));
		usleep(15000);
		moveRotor = true;
		break;
	    }
	}

	// low band --> tone off ; high band --> tone on

	backendDevice->setTone(highBand ? DvbBackendDevice::ToneOn : DvbBackendDevice::ToneOff);

	// tune

	DvbSTransponder *intermediate = NULL;

	if (transmissionType == DvbTransponderBase::DvbS) {
		intermediate = new DvbSTransponder(*dvbSTransponder);
	} else {
		// DVB-S2
		intermediate = new DvbS2Transponder(*dvbS2Transponder);
	}

	intermediate->frequency = frequency;

	if (backendDevice->tune(DvbTransponder(intermediate))) {
		if (!moveRotor) {
			setDeviceState(DeviceTuning);
			frontendTimeout = config->timeout;
		} else {
			setDeviceState(DeviceRotorMoving);
			frontendTimeout = 15000;
		}

		frontendTimer.start(100);
		discardBuffers();
	} else {
		setDeviceState(DeviceIdle);
	}
}

void DvbDevice::autoTune(const DvbTransponder &transponder)
{
	if (transponder->getTransmissionType() != DvbTransponderBase::DvbT) {
		kWarning() << "can't handle != DVB-T";
		return;
	}

	isAuto = true;
	autoTTransponder = new DvbTTransponder(*transponder->getDvbTTransponder());
	autoTransponder = DvbTransponder(autoTTransponder);
	capabilities = backendDevice->getCapabilities();

	// we have to iterate over unsupported AUTO values

	if ((capabilities & DvbBackendDevice::DvbTFecAuto) == 0) {
		autoTTransponder->fecRateHigh = DvbTTransponder::Fec2_3;
	}

	if ((capabilities & DvbBackendDevice::DvbTGuardIntervalAuto) == 0) {
		autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_8;
	}

	if ((capabilities & DvbBackendDevice::DvbTModulationAuto) == 0) {
		autoTTransponder->modulation = DvbTTransponder::Qam64;
	}

	if ((capabilities & DvbBackendDevice::DvbTTransmissionModeAuto) == 0) {
		autoTTransponder->transmissionMode = DvbTTransponder::TransmissionMode8k;
	}

	tune(autoTransponder);
}

bool DvbDevice::addPidFilter(int pid, DvbPidFilter *filter)
{
	QMap<int, DvbFilterInternal>::iterator it = filters.find(pid);

	if (it == filters.end()) {
		it = filters.insert(pid, DvbFilterInternal());

		if (dataDumper != 0) {
			it->filters.append(dataDumper);
		}
	}

	if ((it->activeFilters == 0) && !backendDevice->addPidFilter(pid)) {
		return false;
	}

	if (it->filters.contains(filter)) {
		kWarning() << "using the same filter for the same pid more than once";
		return true;
	}

	it->filters.append(filter);
	++it->activeFilters;
	return true;
}

void DvbDevice::removePidFilter(int pid, DvbPidFilter *filter)
{
	QMap<int, DvbFilterInternal>::iterator it = filters.find(pid);
	int index;

	if (it != filters.end()) {
		index = it->filters.indexOf(filter);
	} else {
		index = -1;
	}

	if (index < 0) {
		kWarning() << "trying to remove a nonexistent filter";
		return;
	}

	it->filters.replace(index, dummyFilter);
	--it->activeFilters;

	if (it->activeFilters == 0) {
		backendDevice->removePidFilter(pid);
	}

	cleanUpFilters = true;
}

bool DvbDevice::isTuned() const
{
	return backendDevice->isTuned();
}

int DvbDevice::getSignal() const
{
	return backendDevice->getSignal();
}

int DvbDevice::getSnr() const
{
	return backendDevice->getSnr();
}

DvbTransponder DvbDevice::getAutoTransponder() const
{
	// FIXME query back information like frequency - tuning parameters - ...
	return autoTransponder;
}

bool DvbDevice::acquire(const QSharedDataPointer<DvbConfigBase> &config_)
{
	Q_ASSERT(deviceState == DeviceReleased);

	if (backendDevice->acquire()) {
		config = config_;
		setDeviceState(DeviceIdle);
		return true;
	}

	return false;
}

void DvbDevice::reacquire(const QSharedDataPointer<DvbConfigBase> &config_)
{
	Q_ASSERT(deviceState != DeviceReleased);
	setDeviceState(DeviceReleased);
	stop();
	config = config_;
	setDeviceState(DeviceIdle);
}

void DvbDevice::release()
{
	setDeviceState(DeviceReleased);
	stop();
	backendDevice->release();
}

void DvbDevice::enableDvbDump()
{
	if (dataDumper != NULL) {
		return;
	}

	dataDumper = new DvbDataDumper();

	QMap<int, DvbFilterInternal>::iterator it = filters.begin();
	QMap<int, DvbFilterInternal>::iterator end = filters.end();

	for (; it != end; ++it) {
		it->filters.append(dataDumper);
	}
}

void DvbDevice::frontendEvent()
{
	if (backendDevice->isTuned()) {
		kDebug() << "tuning succeeded";
		frontendTimer.stop();
		setDeviceState(DeviceTuned);
		return;
	}

	// FIXME progress bar when moving rotor

	frontendTimeout -= 100;

	if (frontendTimeout <= 0) {
		frontendTimer.stop();

		if (!isAuto) {
			kWarning() << "tuning failed";
			setDeviceState(DeviceIdle);
			return;
		}

		int signal = backendDevice->getSignal();

		if ((signal != -1) && (signal < 15)) {
			// signal too weak
			kWarning() << "tuning failed";
			setDeviceState(DeviceIdle);
			return;
		}

		bool carry = true;

		if (carry && ((capabilities & DvbBackendDevice::DvbTFecAuto) == 0)) {
			switch (autoTTransponder->fecRateHigh) {
			case DvbTTransponder::Fec2_3:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec3_4;
				carry = false;
				break;
			case DvbTTransponder::Fec3_4:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec1_2;
				carry = false;
				break;
			case DvbTTransponder::Fec1_2:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec5_6;
				carry = false;
				break;
			case DvbTTransponder::Fec5_6:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec7_8;
				carry = false;
				break;
			default:
				autoTTransponder->fecRateHigh = DvbTTransponder::Fec2_3;
				break;
			}
		}

		if (carry && ((capabilities & DvbBackendDevice::DvbTGuardIntervalAuto) == 0)) {
			switch (autoTTransponder->guardInterval) {
			case DvbTTransponder::GuardInterval1_8:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_32;
				carry = false;
				break;
			case DvbTTransponder::GuardInterval1_32:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_4;
				carry = false;
				break;
			case DvbTTransponder::GuardInterval1_4:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_16;
				carry = false;
				break;
			case DvbTTransponder::GuardInterval1_16:
			case DvbTTransponder::GuardIntervalAuto:
				autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_8;
				break;
			}
		}

		if (carry && ((capabilities & DvbBackendDevice::DvbTModulationAuto) == 0)) {
			switch (autoTTransponder->modulation) {
			case DvbTTransponder::Qam64:
				autoTTransponder->modulation = DvbTTransponder::Qam16;
				carry = false;
				break;
			case DvbTTransponder::Qam16:
				autoTTransponder->modulation = DvbTTransponder::Qpsk;
				carry = false;
				break;
			case DvbTTransponder::Qpsk:
			case DvbTTransponder::ModulationAuto:
				autoTTransponder->modulation = DvbTTransponder::Qam64;
				break;
			}
		}

		if (carry && ((capabilities & DvbBackendDevice::DvbTTransmissionModeAuto) == 0)) {
			switch (autoTTransponder->transmissionMode) {
			case DvbTTransponder::TransmissionMode8k:
				autoTTransponder->transmissionMode = DvbTTransponder::TransmissionMode2k;
				carry = false;
				break;
			case DvbTTransponder::TransmissionMode2k:
			case DvbTTransponder::TransmissionModeAuto:
				autoTTransponder->transmissionMode = DvbTTransponder::TransmissionMode8k;
				break;
			}
		}

		if (!carry) {
			tune(autoTransponder);
		} else {
			kWarning() << "tuning failed";
			setDeviceState(DeviceIdle);
		}
	}
}

void DvbDevice::setDeviceState(DeviceState newState)
{
	if (deviceState != newState) {
		deviceState = newState;
		emit stateChanged();
	}
}

void DvbDevice::discardBuffers()
{
	if (usedBuffers > 0) {
		while (true) {
			currentUsed = currentUsed->next;

			if (usedBuffers.fetchAndAddOrdered(-1) == 1) {
				break;
			}
		}
	}

	currentUsed->count = 0;
}

void DvbDevice::stop()
{
	isAuto = false;
	frontendTimer.stop();

	QMap<int, DvbFilterInternal>::iterator end = filters.end();

	for (QMap<int, DvbFilterInternal>::iterator it = filters.begin(); it != end; ++it) {
		QList<DvbPidFilter *> &internalFilters = it->filters;

		for (int i = 0; i < internalFilters.size(); ++i) {
			DvbPidFilter *filter = internalFilters.at(i);

			if ((filter != dummyFilter) && (filter != dataDumper)) {
				int pid = it.key();
				kWarning() << "removing pending filter" << pid << filter;

				internalFilters.replace(i, dummyFilter);
				--it->activeFilters;

				if (it->activeFilters == 0) {
					backendDevice->removePidFilter(pid);
				}

				cleanUpFilters = true;
			}
		}
	}
}

int DvbDevice::size()
{
	return 21 * 188;
}

char *DvbDevice::getCurrent()
{
	return currentUnused->packets[0];
}

void DvbDevice::submitCurrent(int packets)
{
	currentUnused->count = packets;

	if ((usedBuffers + 1) >= totalBuffers) {
		DvbFilterData *newBuffer = new DvbFilterData;
		newBuffer->next = currentUnused->next;
		currentUnused->next = newBuffer;
		++totalBuffers;
	}

	currentUnused = currentUnused->next;

	if (usedBuffers.fetchAndAddOrdered(1) == 0) {
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
	}
}

void DvbDevice::customEvent(QEvent *)
{
	if (cleanUpFilters) {
		cleanUpFilters = false;
		QMap<int, DvbFilterInternal>::iterator it = filters.begin();
		QMap<int, DvbFilterInternal>::iterator end = filters.end();

		while (it != end) {
			if (it->activeFilters == 0) {
				it = filters.erase(it);
			} else {
				it->filters.removeAll(dummyFilter);
				++it;
			}
		}
	}

	if (usedBuffers <= 0) {
		return;
	}

	while (true) {
		for (int i = 0; i < currentUsed->count; ++i) {
			char *packet = currentUsed->packets[i];

			if ((packet[1] & 0x80) != 0) {
				// transport error indicator
				continue;
			}

			int pid = ((static_cast<unsigned char>(packet[1]) << 8) |
				static_cast<unsigned char>(packet[2])) & ((1 << 13) - 1);

			QMap<int, DvbFilterInternal>::const_iterator it = filters.constFind(pid);

			if (it == filters.end()) {
				continue;
			}

			const QList<DvbPidFilter *> &pidFilters = it->filters;
			int pidFiltersSize = pidFilters.size();

			for (int j = 0; j < pidFiltersSize; ++j) {
				pidFilters.at(j)->processData(packet);
			}
		}

		currentUsed = currentUsed->next;

		if (usedBuffers.fetchAndAddOrdered(-1) == 1) {
			break;
		}
	}
}
