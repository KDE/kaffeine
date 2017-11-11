/*
 * dvbdevice.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "../log.h"

#include <QCoreApplication>
#include <QDir>
#include <unistd.h>

#include <cmath>

#include "dvbconfig.h"
#include "dvbdevice.h"
#include "dvbdevice_p.h"
#include "dvbmanager.h"
#include "dvbsi.h"

class DvbFilterInternal
{
public:
	DvbFilterInternal() : activeFilters(0) { }
	~DvbFilterInternal() { }

	QList<DvbPidFilter *> filters;
	int activeFilters;
};

class DvbSectionFilterInternal : public DvbPidFilter
{
public:
	DvbSectionFilterInternal() : activeSectionFilters(0), continuityCounter(0),
		wrongCrcIndex(0), bufferValid(false)
	{
		memset(wrongCrcs, 0, sizeof(wrongCrcs));
	}

	~DvbSectionFilterInternal() { }

	QList<DvbSectionFilter *> sectionFilters;
	int activeSectionFilters;

private:
	void processData(const char [188]);
	void processSections(bool force);

	unsigned char continuityCounter;
	unsigned char wrongCrcIndex;
	bool bufferValid;
	QByteArray buffer;
	int wrongCrcs[8];
};

// FIXME some debug messages may be printed too often

void DvbSectionFilterInternal::processData(const char data[188])
{
	if ((data[3] & 0x10) == 0) {
		// no payload
		qCDebug(logDvb, "MPEG-TS session doesn't have a payload");
		return;
	}

	unsigned char continuity = (data[3] & 0x0f);

	if (bufferValid) {
		if (continuity == continuityCounter) {
			qCDebug(logDvb, "Section duplication: received %d, expecting %d", continuity, continuityCounter + 1);
			return;
		}

		if (continuity != ((continuityCounter + 1) & 0x0f)) {
			qCDebug(logDvb, "Section discontinuity: received %d, expecting %d", continuity, continuityCounter + 1);
			bufferValid = false;
		}
	}

	continuityCounter = continuity;

	bool sectionStart = ((data[1] & 0x40) != 0);
	const char *payload;
	int payloadLength;

	if ((data[3] & 0x20) == 0) {
		// adaptation field not present
		payload = (data + 4);
		payloadLength = (188 - 4);
	} else {
		// adaptation field present
		unsigned char length = data[4];

		if (length > 182) {
			qCDebug(logDvb, "Received a section without  payload or corrupted");
			return;
		}

		payload = (data + 5 + length);
		payloadLength = (188 - 5 - length);
	}

	// be careful that playloadLength is > 0 at this point

	if (sectionStart) {
		int pointer = quint8(payload[0]);

		if (pointer >= payloadLength) {
			qCDebug(logDvb, "Section with invalid payload pointer");
			pointer = (payloadLength - 1);
		}

		if (bufferValid) {
			buffer.append(payload + 1, pointer);
			processSections(true);
		} else {
			bufferValid = true;
		}

		payload += (pointer + 1);
		payloadLength -= (pointer + 1);
	}

	buffer.append(payload, payloadLength);
	processSections(false);
}

void DvbSectionFilterInternal::processSections(bool force)
{
	const char *it = buffer.constBegin();
	const char *end = buffer.constEnd();

	while (it != end) {
		if (static_cast<unsigned char>(it[0]) == 0xff) {
			// table id == 0xff means padding
			it = end;
			break;
		}

		if ((end - it) < 3) {
			if (force) {
				qCDebug(logDvb, "Section with stray data");
				it = end;
			}

			break;
		}

		const char *sectionEnd = (it + (((static_cast<unsigned char>(it[1]) & 0x0f) << 8) |
			static_cast<unsigned char>(it[2])) + 3);

		if (force && (sectionEnd > end)) {
			qCDebug(logDvb, "Short section");
			sectionEnd = end;
		}

		if (sectionEnd <= end) {
			int size = int(sectionEnd - it);
			int crc = DvbStandardSection::verifyCrc32(it, size);
			bool crcOk;

			if (crc == 0) {
				crcOk = true;
			} else {
				for (int i = 0;; ++i) {
					if (i == (sizeof(wrongCrcs) / sizeof(wrongCrcs[0]))) {
						crcOk = false;
						wrongCrcs[wrongCrcIndex] = crc;

						if ((++wrongCrcIndex) == i) {
							wrongCrcIndex = 0;
						}

						break;
					}

					if (wrongCrcs[i] == crc) {
						crcOk = true;
						break;
					}
				}
			}

			if (crcOk) {
				for (int i = 0; i < sectionFilters.size(); ++i) {
					sectionFilters.at(i)->processSection(it, size);
				}
			}

			it = sectionEnd;
			continue;
		}

		break;
	}

	buffer.remove(0, int(it - buffer.constBegin()));
}

class DvbDataDumper : public QFile, public DvbPidFilter
{
public:
	DvbDataDumper();
	~DvbDataDumper();

	void processData(const char [188]);
};

DvbDataDumper::DvbDataDumper()
{
	setFileName(QDir::homePath() + QLatin1String("/KaffeineDvbDump-") + QString::number(qrand(), 16) +
		QLatin1String(".bin"));

	if (!open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCWarning(logDev, "Can't open %s", qPrintable(fileName()));
	}
}

DvbDataDumper::~DvbDataDumper()
{
}

void DvbDataDumper::processData(const char data[188])
{
	write(data, 188);
}

DvbDevice::DvbDevice(DvbBackendDevice *backend_, QObject *parent) : QObject(parent),
	backend(backend_), deviceState(DeviceReleased), dataDumper(NULL), cleanUpFilters(false),
	isAuto(false), unusedBuffersHead(NULL), usedBuffersHead(NULL), usedBuffersTail(NULL)
{
	backend->setFrontendDevice(this);
	backend->setDeviceEnabled(true); // FIXME

	connect(&frontendTimer, SIGNAL(timeout()), this, SLOT(frontendEvent()));
}

DvbDevice::~DvbDevice()
{
	backend->release();

	for (DvbDeviceDataBuffer *buffer = unusedBuffersHead; buffer != NULL;) {
		DvbDeviceDataBuffer *nextBuffer = buffer->next;
		delete buffer;
		buffer = nextBuffer;
	}

	for (DvbDeviceDataBuffer *buffer = usedBuffersHead; buffer != NULL;) {
		DvbDeviceDataBuffer *nextBuffer = buffer->next;
		delete buffer;
		buffer = nextBuffer;
	}
}

DvbDevice::TransmissionTypes DvbDevice::getTransmissionTypes() const
{
	return backend->getTransmissionTypes();
}

QString DvbDevice::getDeviceId() const
{
	return backend->getDeviceId();
}

QString DvbDevice::getFrontendName() const
{
	return backend->getFrontendName();
}

void DvbDevice::tune(const DvbTransponder &transponder)
{
	DvbTransponderBase::TransmissionType transmissionType = transponder.getTransmissionType();

	autoTransponder.setTransmissionType(transmissionType);

	if ((transmissionType != DvbTransponderBase::DvbS) &&
	    (transmissionType != DvbTransponderBase::DvbS2)) {
		if (backend->tune(transponder)) {
			setDeviceState(DeviceTuning);
			frontendTimeout = config->timeout;
			frontendTimer.start(100);
			discardBuffers();
		} else {
			setDeviceState(DeviceTuning);
			setDeviceState(DeviceIdle);
			autoTransponder.setTransmissionType(DvbTransponderBase::Invalid);
		}

		return;
	}

	bool moveRotor = false;
	DvbTransponder intermediate = transponder;

	// DVB LNBf IF and DiSeqC switch settings

	int satNumber = -1;	// No DiseqC Switch

	if (config->configuration == DvbConfigBase::DiseqcSwitch)
		satNumber = config->lnbNumber;

	// FIXME: add support for SCR/Unicable
	if (!backend->satSetup(config->currentLnb.alias, satNumber, 0))
		return;

	backend->setHighVoltage(config->higherVoltage);

	// rotor

	switch (config->configuration) {
	case DvbConfigBase::DiseqcSwitch:
	case DvbConfigBase::NoDiseqc:
		// Everything was already prepared via satSetup().
		break;

	case DvbConfigBase::UsalsRotor: {
		QString source = config->scanSource;
		source.remove(0, source.lastIndexOf(QLatin1Char('-')) + 1);

		bool ok = false;
		double orbitalPosition = 0;

		if (source.endsWith(QLatin1Char('E'))) {
			source.chop(1);
			orbitalPosition = source.toDouble(&ok);
		} else if (source.endsWith(QLatin1Char('W'))) {
			source.chop(1);
			orbitalPosition = (-source.toDouble(&ok));
		}

		if (!ok)
			qCWarning(logDev, "Can't extract orbital position from %s", qPrintable(config->scanSource));

		double radius = 6378;
		double semiMajorAxis = 42164;
		double temp = (radius * cos(config->latitude * M_PI / 180));
		double temp2 = ((orbitalPosition - config->longitude) * M_PI / 180);
		double angle = (temp2 + atan(sin(temp2) / ((semiMajorAxis / temp) - cos(temp2))));
		int value = 0;

		if (angle >= 0) {
			// east
			value = int((16 * angle * 180 / M_PI) + 0.5);
			value |= 0xe000;
		} else {
			// west
			value = int((16 * (-angle) * 180 / M_PI) + 0.5);
			value |= 0xd000;
		}

		char cmd[] = { char(0xe0), 0x31, 0x6e, char(value / 256), char(value % 256) };
		usleep(15000);
		backend->sendMessage(cmd, sizeof(cmd));
		usleep(15000);
		moveRotor = true;
		break;
	    }

	case DvbConfigBase::PositionsRotor: {
		char cmd[] = { char(0xe0), 0x31, 0x6b, char(config->lnbNumber) };
		usleep(15000);
		backend->sendMessage(cmd, sizeof(cmd));
		usleep(15000);
		moveRotor = true;
		break;
	    }
	}

	// tune

	if (backend->tune(intermediate)) {
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
		setDeviceState(DeviceTuning);
		setDeviceState(DeviceIdle);
		autoTransponder.setTransmissionType(DvbTransponderBase::Invalid);
	}
}

void DvbDevice::autoTune(const DvbTransponder &transponder)
{
	DvbTransponderBase::TransmissionType transmissionType = transponder.getTransmissionType();

	autoTransponder = transponder;

	if (transmissionType == DvbTransponderBase::DvbT) {
		DvbTTransponder *autoTTransponder = autoTransponder.as<DvbTTransponder>();
		capabilities = backend->getCapabilities();

		// we have to iterate over unsupported AUTO values

		if ((capabilities & DvbTFecAuto) == 0) {
			autoTTransponder->fecRateHigh = DvbTTransponder::Fec2_3;
		}

		if ((capabilities & DvbTGuardIntervalAuto) == 0) {
			autoTTransponder->guardInterval = DvbTTransponder::GuardInterval1_8;
		}

		if ((capabilities & DvbTModulationAuto) == 0) {
			autoTTransponder->modulation = DvbTTransponder::Qam64;
		}

		if ((capabilities & DvbTTransmissionModeAuto) == 0) {
			autoTTransponder->transmissionMode = DvbTTransponder::TransmissionMode8k;
		}
		isAuto = true;
		tune(autoTransponder);
	} else if (transmissionType == DvbTransponderBase::DvbT2) {
		// I guess all DVB-T2 devices support auto-detection
		isAuto = true;
		tune(autoTransponder);
	} else if (transmissionType == DvbTransponderBase::IsdbT) {
		// ISDB-T Currently, all ISDB-T tuners should support auto mode
		isAuto = true;
		tune(autoTransponder);
	} else {
		qCWarning(logDev, "Can't do auto-tune for %d", transmissionType);
		return;
	}

	isAuto = true;
	tune(autoTransponder);
}

bool DvbDevice::addPidFilter(int pid, DvbPidFilter *filter)
{
	QMap<int, DvbFilterInternal>::iterator it = filters.find(pid);

	if (it == filters.end()) {
		it = filters.insert(pid, DvbFilterInternal());

		if (dataDumper != NULL) {
			it->filters.append(dataDumper);
		}
	}

	if (it->activeFilters == 0) {
		if (!backend->addPidFilter(pid)) {
			cleanUpFilters = true;
			return false;
		}
	}

	if (it->filters.contains(filter)) {
		qCInfo(logDev, "Using the same filter for the same pid more than once");
		return true;
	}

	it->filters.append(filter);
	++it->activeFilters;
	return true;
}

bool DvbDevice::addSectionFilter(int pid, DvbSectionFilter *filter)
{
	QMap<int, DvbSectionFilterInternal>::iterator it = sectionFilters.find(pid);

	if (it == sectionFilters.end()) {
		it = sectionFilters.insert(pid, DvbSectionFilterInternal());
	}

	if (it->activeSectionFilters == 0) {
		if (!addPidFilter(pid, &(*it))) {
			cleanUpFilters = true;
			return false;
		}
	}

	if (it->sectionFilters.contains(filter)) {
		qCInfo(logDev, "Using the same filter for the same pid more than once");
		return true;
	}

	it->sectionFilters.append(filter);
	++it->activeSectionFilters;
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
		qCWarning(logDev, "Trying to remove a nonexistent filter");
		return;
	}

	it->filters.replace(index, &dummyPidFilter);
	--it->activeFilters;

	if (it->activeFilters == 0) {
		backend->removePidFilter(pid);
	}

	cleanUpFilters = true;
}

void DvbDevice::removeSectionFilter(int pid, DvbSectionFilter *filter)
{
	QMap<int, DvbSectionFilterInternal>::iterator it = sectionFilters.find(pid);
	int index;

	if (it != sectionFilters.end()) {
		index = it->sectionFilters.indexOf(filter);
	} else {
		index = -1;
	}

	if (index < 0) {
		qCWarning(logDev, "Trying to remove a nonexistent filter");
		return;
	}

	it->sectionFilters.replace(index, &dummySectionFilter);
	--it->activeSectionFilters;

	if (it->activeSectionFilters == 0) {
		removePidFilter(pid, &(*it));
	}

	cleanUpFilters = true;
}

void DvbDevice::startDescrambling(const QByteArray &pmtSectionData, QObject *user)
{
	DvbPmtSection pmtSection(pmtSectionData);

	if (!pmtSection.isValid()) {
		qCWarning(logDev, "PMT section is invalid");
	}

	int serviceId = pmtSection.programNumber();

	if (!descramblingServices.contains(serviceId)) {
		backend->startDescrambling(pmtSectionData);
	}

	if (!descramblingServices.contains(serviceId, user)) {
		descramblingServices.insert(serviceId, user);
	}
}

void DvbDevice::stopDescrambling(const QByteArray &pmtSectionData, QObject *user)
{
	DvbPmtSection pmtSection(pmtSectionData);

	if (!pmtSection.isValid()) {
		qCWarning(logDev, "PMT section is invalid");
	}

	int serviceId = pmtSection.programNumber();

	if (!descramblingServices.contains(serviceId, user)) {
		qCInfo(logDev, "Service has not been started while stop descrambling");
		return;
	}

	descramblingServices.remove(serviceId, user);

	if (!descramblingServices.contains(serviceId)) {
		backend->stopDescrambling(serviceId);
	}
}

bool DvbDevice::isTuned() const
{
	return backend->isTuned();
}

bool DvbDevice::getProps(DvbTransponder &transponder) const
{
	return backend->getProps(transponder);
}

float DvbDevice::getSignal(DvbBackendDevice::Scale &scale) const
{
	return backend->getSignal(scale);
}

float DvbDevice::getSnr(DvbBackendDevice::Scale &scale) const
{
	return backend->getSnr(scale);
}

DvbTransponder DvbDevice::getAutoTransponder() const
{
	// FIXME query back information like frequency - tuning parameters - ...
	return autoTransponder;
}

bool DvbDevice::acquire(const DvbConfigBase *config_)
{
	Q_ASSERT(deviceState == DeviceReleased);

	if (backend->acquire()) {
		config = config_;
		setDeviceState(DeviceIdle);
		autoTransponder.setTransmissionType(DvbTransponderBase::Invalid);
		return true;
	}

	return false;
}

void DvbDevice::reacquire(const DvbConfigBase *config_)
{
	Q_ASSERT(deviceState != DeviceReleased);
	setDeviceState(DeviceReleased);
	stop();
	config = config_;
	setDeviceState(DeviceIdle);
	autoTransponder.setTransmissionType(DvbTransponderBase::Invalid);
}

void DvbDevice::release()
{
	setDeviceState(DeviceReleased);
	stop();
	backend->release();
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

	backend->enableDvbDump();
}

void DvbDevice::frontendEvent()
{
	DvbTransponderBase::TransmissionType transmissionType = autoTransponder.getTransmissionType();

	if (backend->isTuned()) {
		qCDebug(logDvb, "tuning succeeded on %.2f MHz", backend->getFrqMHz());
		frontendTimer.stop();
		backend->getProps(autoTransponder);
		setDeviceState(DeviceTuned);
		return;
	}

	// FIXME progress bar when moving rotor

	frontendTimeout -= 100;

	if (frontendTimeout <= 0) {
		frontendTimer.stop();

		if (!isAuto) {
			qCDebug(logDvb, "tuning failed on %.2f MHz", backend->getFrqMHz());
			setDeviceState(DeviceIdle);
			autoTransponder.setTransmissionType(DvbTransponderBase::Invalid);
			return;
		}

		bool carry = true;

		/*
		 * As ISDB-T always support auto-scan, we only need to simulate
		 * it for DVB-T
		 */
		if (transmissionType == DvbTransponderBase::DvbT) {
			DvbBackendDevice::Scale scale;

			DvbTTransponder *autoTTransponder = autoTransponder.as<DvbTTransponder>();
			float signal = backend->getSignal(scale);

			if ((scale != DvbBackendDevice::NotSupported) && (signal < 15)) {
				// signal too weak
				qCInfo(logDev, "Signal too weak on %.2f MHz", backend->getFrqMHz());
			/*
			 * FIXME: ignoring a too weak signal is not so easy,
			 * as it depends on the scale, and scale is broken on
			 * several drivers. Also, signal itself is not a good
			 * indicator of the quality. Better to just print a
			 * warning, and not fail.
			 */
#if 0
				setDeviceState(DeviceIdle);
				autoTransponder.setTransmissionType(DvbTransponderBase::Invalid);
				return;
#endif
			}

			if (carry && ((capabilities & DvbTFecAuto) == 0)) {
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

			if (carry && ((capabilities & DvbTGuardIntervalAuto) == 0)) {
				switch (autoTTransponder->guardInterval) {
				case DvbTTransponder::GuardInterval1_8:
					autoTTransponder->guardInterval =
						DvbTTransponder::GuardInterval1_32;
					carry = false;
					break;
				case DvbTTransponder::GuardInterval1_32:
					autoTTransponder->guardInterval =
						DvbTTransponder::GuardInterval1_4;
					carry = false;
					break;
				case DvbTTransponder::GuardInterval1_4:
					autoTTransponder->guardInterval =
						DvbTTransponder::GuardInterval1_16;
					carry = false;
					break;
				case DvbTTransponder::GuardInterval1_16:
				case DvbTTransponder::GuardIntervalAuto:
					autoTTransponder->guardInterval =
						DvbTTransponder::GuardInterval1_8;
					break;
				}
			}

			if (carry && ((capabilities & DvbTModulationAuto) == 0)) {
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

			if (carry && ((capabilities & DvbTTransmissionModeAuto) == 0)) {
				switch (autoTTransponder->transmissionMode) {
				case DvbTTransponder::TransmissionMode8k:
					autoTTransponder->transmissionMode =
						DvbTTransponder::TransmissionMode2k;
					carry = false;
					break;
				case DvbTTransponder::TransmissionMode2k:
	/* outcommented so that clearly no compatibility problem arises
					autoTTransponder->transmissionMode =
						DvbTTransponder::TransmissionMode4k;
					carry = false;
					break;
	*/
				case DvbTTransponder::TransmissionMode4k:
				case DvbTTransponder::TransmissionModeAuto:
					autoTTransponder->transmissionMode =
						DvbTTransponder::TransmissionMode8k;
					break;
				}
			}
		}

		if (!carry) {
			tune(autoTransponder);
		} else {
			qCDebug(logDvb, "tuning failed on %.2f MHz", backend->getFrqMHz());;
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
	dataChannelMutex.lock();

	if (usedBuffersHead != NULL) {
		usedBuffersHead->size = 0;
		DvbDeviceDataBuffer *nextBuffer = usedBuffersHead->next;
		usedBuffersHead->next = NULL;

		if (nextBuffer != NULL) {
			nextBuffer->next = unusedBuffersHead;
			unusedBuffersHead = nextBuffer;
		}
	}

	dataChannelMutex.unlock();
}

void DvbDevice::stop()
{
	isAuto = false;
	frontendTimer.stop();

	for (QMap<int, DvbFilterInternal>::ConstIterator it = filters.constBegin();
	     it != filters.constEnd(); ++it) {
		foreach (DvbPidFilter *filter, it->filters) {
			if ((filter != &dummyPidFilter) && (filter != dataDumper)) {
				int pid = it.key();
				qCDebug(logDvb, "removing pending filter %d", pid);
				removePidFilter(pid, filter);
			}
		}
	}

	for (QMap<int, DvbSectionFilterInternal>::ConstIterator it = sectionFilters.constBegin();
	     it != sectionFilters.constEnd(); ++it) {
		foreach (DvbSectionFilter *sectionFilter, it->sectionFilters) {
			if (sectionFilter != &dummySectionFilter) {
				int pid = it.key();
				qCDebug(logDvb, "removing pending filter %d", pid);
				removeSectionFilter(pid, sectionFilter);
			}
		}
	}
}

DvbDataBuffer DvbDevice::getBuffer()
{
	dataChannelMutex.lock();
	DvbDeviceDataBuffer *buffer = unusedBuffersHead;

	if (buffer != NULL) {
		unusedBuffersHead = buffer->next;
		dataChannelMutex.unlock();
	} else {
		dataChannelMutex.unlock();
		buffer = new DvbDeviceDataBuffer;
	}

	return DvbDataBuffer(buffer->data, sizeof(buffer->data));
}

void DvbDevice::writeBuffer(const DvbDataBuffer &dataBuffer)
{
	DvbDeviceDataBuffer *buffer = reinterpret_cast<DvbDeviceDataBuffer *>(dataBuffer.data);
	Q_ASSERT(buffer->data == dataBuffer.data);

	if (dataBuffer.dataSize > 0) {
		buffer->size = dataBuffer.dataSize;
		dataChannelMutex.lock();
		bool wakeUp = false;

		if (usedBuffersHead != NULL) {
			usedBuffersTail->next = buffer;
		} else {
			usedBuffersHead = buffer;
			wakeUp = true;
		}

		usedBuffersTail = buffer;
		usedBuffersTail->next = NULL;
		dataChannelMutex.unlock();

		if (wakeUp) {
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		}
	} else {
		dataChannelMutex.lock();
		buffer->next = unusedBuffersHead;
		unusedBuffersHead = buffer;
		dataChannelMutex.unlock();
	}
}

void DvbDevice::customEvent(QEvent *)
{
	if (cleanUpFilters) {
		cleanUpFilters = false;

		{
			QMap<int, DvbFilterInternal>::iterator it = filters.begin();
			QMap<int, DvbFilterInternal>::iterator end = filters.end();

			while (it != end) {
				if (it->activeFilters == 0) {
					it = filters.erase(it);
				} else {
					it->filters.removeAll(&dummyPidFilter);
					++it;
				}
			}
		}

		{
			QMap<int, DvbSectionFilterInternal>::iterator it = sectionFilters.begin();
			QMap<int, DvbSectionFilterInternal>::iterator end = sectionFilters.end();

			while (it != end) {
				if (it->activeSectionFilters == 0) {
					it = sectionFilters.erase(it);
				} else {
					it->sectionFilters.removeAll(&dummySectionFilter);
					++it;
				}
			}
		}
	}

	DvbDeviceDataBuffer *buffer = NULL;

	while (true) {
		dataChannelMutex.lock();

		if (buffer != NULL) {
			usedBuffersHead = buffer->next;
			buffer->next = unusedBuffersHead;
			unusedBuffersHead = buffer;
		}

		buffer = usedBuffersHead;
		dataChannelMutex.unlock();

		if (buffer == NULL) {
			break;
		}

		for (int i = 0; i < buffer->size; i += 188) {
			char *packet = (buffer->data + i);

			if ((packet[1] & 0x80) != 0) {
				// transport error indicator
				continue;
			}

			int pid = ((static_cast<unsigned char>(packet[1]) << 8) |
				static_cast<unsigned char>(packet[2])) & ((1 << 13) - 1);

			QMap<int, DvbFilterInternal>::const_iterator it = filters.constFind(pid);

			if (it == filters.constEnd()) {
				continue;
			}

			const QList<DvbPidFilter *> &pidFilters = it->filters;
			int pidFiltersSize = pidFilters.size();

			for (int j = 0; j < pidFiltersSize; ++j) {
				pidFilters.at(j)->processData(packet);
			}
		}
	}
}
