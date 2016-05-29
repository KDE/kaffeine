/*
 * dvbdevice_linux.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
 * Copyright (c) 2014 Mauro Carvalho Chehab <mchehab@infradead.org>
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

#include <dmx.h>
#include <errno.h>
#include <fcntl.h>
#include <frontend.h>
#include <poll.h>
#include <QDebug>
#include <QFile>
#include <QRegularExpressionMatch>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <sys/ioctl.h>
#include <unistd.h>

#include "dvbdevice_linux.h"
#include "dvbtransponder.h"

// krazy:excludeall=syscalls

DvbLinuxDevice::DvbLinuxDevice(QObject *parent) : QThread(parent), ready(false), frontend(NULL),
	enabled(false), dvrFd(-1), dvrBuffer(NULL, 0)
{
	dvrPipe[0] = -1;
	dvrPipe[1] = -1;
}

DvbLinuxDevice::~DvbLinuxDevice()
{
	stopDevice();
}

static const char loglevel[9][10] = {
	{"EMERG"},
	{"ALERT"},
	{"CRITICAL"},
	{"ERROR"},
	{"WARNING"},
	{"NOTICE"},
	{"INFO"},
	{"DEBUG"},
	{"OTHER"},
};

#define loglevels ((int)(sizeof(loglevel)/sizeof(*loglevel)))

static void dvbv5_log(int level, const char *fmt, ...)
{
	va_list ap;
	char log[1024];

	if (level >= loglevels)
		level = loglevels - 1;

	va_start(ap, fmt);
	vsnprintf(log, sizeof(log), fmt, ap);
	va_end(ap);

	qInfo() << "DvbLinuxDevice::libdvbv5" << loglevel[level] << log;
}

bool DvbLinuxDevice::isReady() const
{
	return ready;
}

void DvbLinuxDevice::startDevice(const QString &deviceId_)
{
	Q_ASSERT(!ready);
	struct dvb_v5_fe_parms *parms = dvb_fe_open2(adapter, index, 0, 0, dvbv5_log);

	if (!parms) {
		qInfo() << "DvbLinuxDevice::startDevice: cannot open frontend" << frontendPath;
		return;
	}

	transmissionTypes = 0;
	for (int i = 0; i < parms->num_systems; i++) {
		switch (parms->systems[i]) {
		case SYS_DVBS:
			transmissionTypes |= DvbS;
			break;
		case SYS_DVBS2:
			transmissionTypes |= DvbS2;
			break;
		case SYS_DVBT:
			transmissionTypes |= DvbT;
			break;
		case SYS_DVBT2:
			transmissionTypes |= DvbT2;
			break;
		case SYS_DVBC_ANNEX_A:
		case SYS_DVBC_ANNEX_C:
			transmissionTypes |= DvbC;
			break;
		case SYS_ATSC:
		case SYS_DVBC_ANNEX_B:
			transmissionTypes |= Atsc;
			break;
		case SYS_ISDBT:
			transmissionTypes |= IsdbT;
			break;
		default: /* not supported yet */
			qInfo() << "DvbLinuxDevice::startDevice: unsupported transmission type: " << parms->systems[i];
			break;
		}
	}

	deviceId = deviceId_;
	frontendName = QString::fromUtf8(parms->info.name);

	capabilities = 0;

	if ((parms->info.caps & FE_CAN_QAM_AUTO) != 0) {
		capabilities |= DvbTModulationAuto;
	}

	if ((parms->info.caps & FE_CAN_FEC_AUTO) != 0) {
		capabilities |= DvbTFecAuto;
	}

	if ((parms->info.caps & FE_CAN_TRANSMISSION_MODE_AUTO) != 0) {
		capabilities |= DvbTTransmissionModeAuto;
	}

	if ((parms->info.caps & FE_CAN_GUARD_INTERVAL_AUTO) != 0) {
		capabilities |= DvbTGuardIntervalAuto;
	}
	dvb_fe_close(parms);

	qInfo() << "DvbLinuxDevice::startDevice: found dvb device" << deviceId << "/" << frontendName;
	ready = true;
}

void DvbLinuxDevice::startCa()
{
	Q_ASSERT(ready && !caPath.isEmpty());

	if (enabled) {
		cam.startCa(caPath);
	}
}

void DvbLinuxDevice::stopCa()
{
	Q_ASSERT(ready && caPath.isEmpty());

	if (enabled) {
		cam.stopCa();
	}
}

void DvbLinuxDevice::stopDevice()
{
	setDeviceEnabled(false);
	ready = false;
}

QString DvbLinuxDevice::getDeviceId()
{
	Q_ASSERT(ready);
	return deviceId;
}

QString DvbLinuxDevice::getFrontendName()
{
	Q_ASSERT(ready);
	return frontendName;
}

DvbLinuxDevice::TransmissionTypes DvbLinuxDevice::getTransmissionTypes()
{
	Q_ASSERT(ready);
	return transmissionTypes;
}

DvbLinuxDevice::Capabilities DvbLinuxDevice::getCapabilities()
{
	Q_ASSERT(ready);
	return capabilities;
}

void DvbLinuxDevice::setFrontendDevice(DvbFrontendDevice *frontend_)
{
	frontend = frontend_;
}

void DvbLinuxDevice::setDeviceEnabled(bool enabled_)
{
	Q_ASSERT(ready);

	if (enabled != enabled_) {
		enabled = enabled_;

		if (enabled) {
			if (!caPath.isEmpty()) {
				cam.startCa(caPath);
			}
		} else {
			release();
			cam.stopCa();
		}
	}
}

bool DvbLinuxDevice::acquire()
{
	Q_ASSERT(enabled && (!dvbv5_parms) && (dvrFd < 0));
	dvbv5_parms = dvb_fe_open2(adapter, index, 0, 0, dvbv5_log);

	if (!dvbv5_parms) {
		qInfo() << "DvbLinuxDevice::acquire: cannot open frontend" << frontendPath;
		return false;
	}

	dvrFd = open(QFile::encodeName(dvrPath).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	if (dvrFd < 0) {
		qInfo() << "DvbLinuxDevice::acquire: cannot open dvr" << dvrPath;
		dvb_fe_close(dvbv5_parms);
		dvbv5_parms = NULL;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::setTone(SecTone tone)
{
	Q_ASSERT(dvbv5_parms);

	if (dvb_fe_sec_tone(dvbv5_parms,
		  (tone == ToneOn) ? SEC_TONE_ON : SEC_TONE_OFF) != 0) {
		qInfo() << "DvbLinuxDevice::setTone: ioctl FE_SET_TONE failed for frontend" <<
			frontendPath;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::setVoltage(SecVoltage voltage)
{
	Q_ASSERT(dvbv5_parms);

	if (dvb_fe_lnb_high_voltage(dvbv5_parms, voltage == Voltage18V) != 0) {
		qInfo() << "DvbLinuxDevice::setVoltage: ioctl FE_SET_VOLTAGE failed for frontend" <<
			frontendPath;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::sendMessage(const char *message, int length)
{
	Q_ASSERT(dvbv5_parms && (length >= 0) && (length <= 6));

	if (dvb_fe_diseqc_cmd(dvbv5_parms, length, (const unsigned char *)message) != 0) {
		qInfo() << "DvbLinuxDevice::sendMessage: "
		    "ioctl FE_DISEQC_SEND_MASTER_CMD failed for frontend" << frontendPath;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::sendBurst(SecBurst burst)
{
	Q_ASSERT(dvbv5_parms);

	if (dvb_fe_diseqc_burst(dvbv5_parms, burst == BurstMiniB) != 0) {
		qInfo() << "DvbLinuxDevice::sendBurst: ioctl FE_DISEQC_SEND_BURST failed for frontend" <<
			frontendPath;
		return false;
	}

	return true;
}

static fe_modulation_t toDvbModulation(DvbCTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbCTransponder::Qam16: return QAM_16;
	case DvbCTransponder::Qam32: return QAM_32;
	case DvbCTransponder::Qam64: return QAM_64;
	case DvbCTransponder::Qam128: return QAM_128;
	case DvbCTransponder::Qam256: return QAM_256;
	case DvbCTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_modulation_t toDvbModulation(DvbS2Transponder::Modulation modulation)
{
	switch (modulation) {
	case DvbS2Transponder::Qpsk: return QPSK;
	case DvbS2Transponder::Psk8: return PSK_8;
	case DvbS2Transponder::Apsk16: return APSK_16;
	case DvbS2Transponder::Apsk32: return APSK_32;
	case DvbS2Transponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_rolloff toDvbRollOff(DvbS2Transponder::RollOff rollOff)
{
	switch (rollOff) {
	case DvbS2Transponder::RollOff20: return ROLLOFF_20;
	case DvbS2Transponder::RollOff25: return ROLLOFF_25;
	case DvbS2Transponder::RollOff35: return ROLLOFF_35;
	case DvbS2Transponder::RollOffAuto: return ROLLOFF_AUTO;
	}

	return ROLLOFF_AUTO;
}

static fe_modulation_t toDvbModulation(DvbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbTTransponder::Qpsk: return QPSK;
	case DvbTTransponder::Qam16: return QAM_16;
	case DvbTTransponder::Qam64: return QAM_64;
	case DvbTTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_modulation_t toDvbModulation(DvbT2Transponder::Modulation modulation)
{
	switch (modulation) {
	case DvbT2Transponder::Qpsk: return QPSK;
	case DvbT2Transponder::Qam16: return QAM_16;
	case DvbT2Transponder::Qam64: return QAM_64;
	case DvbT2Transponder::Qam256: return QAM_256;
	case DvbT2Transponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_modulation_t toDvbModulation(IsdbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case IsdbTTransponder::Qpsk: return QPSK;
	case IsdbTTransponder::Dqpsk: return DQPSK;
	case IsdbTTransponder::Qam16: return QAM_16;
	case IsdbTTransponder::Qam64: return QAM_64;
	case IsdbTTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_modulation_t toDvbModulation(AtscTransponder::Modulation modulation)
{
	switch (modulation) {
	case AtscTransponder::Qam64: return QAM_64;
	case AtscTransponder::Qam256: return QAM_256;
	case AtscTransponder::Vsb8: return VSB_8;
	case AtscTransponder::Vsb16: return VSB_16;
	case AtscTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_code_rate toDvbFecRate(DvbTransponderBase::FecRate fecRate)
{
	switch (fecRate) {
	case DvbTransponderBase::FecNone: return FEC_NONE;
	case DvbTransponderBase::Fec1_2: return FEC_1_2;
	case DvbTransponderBase::Fec1_3: return FEC_AUTO; // FIXME
	case DvbTransponderBase::Fec1_4: return FEC_AUTO; // FIXME
	case DvbTransponderBase::Fec2_3: return FEC_2_3;
	case DvbTransponderBase::Fec2_5: return FEC_AUTO; // FIXME
	case DvbTransponderBase::Fec3_4: return FEC_3_4;
	case DvbTransponderBase::Fec3_5: return FEC_3_5;
	case DvbTransponderBase::Fec4_5: return FEC_4_5;
	case DvbTransponderBase::Fec5_6: return FEC_5_6;
	case DvbTransponderBase::Fec6_7: return FEC_6_7;
	case DvbTransponderBase::Fec7_8: return FEC_7_8;
	case DvbTransponderBase::Fec8_9: return FEC_8_9;
	case DvbTransponderBase::Fec9_10: return FEC_9_10;
	case DvbTransponderBase::FecAuto: return FEC_AUTO;
	}

	return FEC_AUTO;
}

static uint32_t toDvbBandwidth(DvbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbTTransponder::Bandwidth5MHz: return 5000000;
	case DvbTTransponder::Bandwidth6MHz: return 6000000;
	case DvbTTransponder::Bandwidth7MHz: return 7000000;
	case DvbTTransponder::Bandwidth8MHz: return 8000000;
	case DvbTTransponder::BandwidthAuto: return 0;
	}

	return BANDWIDTH_AUTO;
}

static uint32_t toDvbBandwidth(DvbT2Transponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbT2Transponder::Bandwidth1_7MHz:	return 1700000;
	case DvbT2Transponder::Bandwidth5MHz:	return 5000000;
	case DvbT2Transponder::Bandwidth6MHz:	return 6000000;
	case DvbT2Transponder::Bandwidth7MHz:	return 7000000;
	case DvbT2Transponder::Bandwidth8MHz:	return 8000000;
	case DvbT2Transponder::Bandwidth10MHz:	return 10000000;
	case DvbT2Transponder::BandwidthAuto:	return 0;
	}

	return BANDWIDTH_AUTO;
}

static uint32_t toDvbBandwidth(IsdbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case IsdbTTransponder::Bandwidth6MHz: return 6000000;
	case IsdbTTransponder::Bandwidth7MHz: return 7000000;
	case IsdbTTransponder::Bandwidth8MHz: return 8000000;
	}

	return BANDWIDTH_AUTO;
}

static fe_transmit_mode toDvbTransmissionMode(DvbTTransponder::TransmissionMode mode)
{
	switch (mode) {
	case DvbTTransponder::TransmissionMode2k: return TRANSMISSION_MODE_2K;
	case DvbTTransponder::TransmissionMode4k: return TRANSMISSION_MODE_4K;
	case DvbTTransponder::TransmissionMode8k: return TRANSMISSION_MODE_8K;
	case DvbTTransponder::TransmissionModeAuto: return TRANSMISSION_MODE_AUTO;
	}

	return TRANSMISSION_MODE_AUTO;
}

static fe_transmit_mode toDvbTransmissionMode(DvbT2Transponder::TransmissionMode mode)
{
	switch (mode) {
	case DvbT2Transponder::TransmissionMode1k:  return TRANSMISSION_MODE_1K;
	case DvbT2Transponder::TransmissionMode2k:  return TRANSMISSION_MODE_2K;
	case DvbT2Transponder::TransmissionMode4k:  return TRANSMISSION_MODE_4K;
	case DvbT2Transponder::TransmissionMode8k:  return TRANSMISSION_MODE_8K;
	case DvbT2Transponder::TransmissionMode16k: return TRANSMISSION_MODE_16K;
	case DvbT2Transponder::TransmissionMode32k: return TRANSMISSION_MODE_32K;
	case DvbT2Transponder::TransmissionModeAuto: return TRANSMISSION_MODE_AUTO;
	}

	return TRANSMISSION_MODE_AUTO;
}

static fe_transmit_mode toDvbTransmissionMode(IsdbTTransponder::TransmissionMode mode)
{
	switch (mode) {
	case IsdbTTransponder::TransmissionMode2k: return TRANSMISSION_MODE_2K;
	case IsdbTTransponder::TransmissionMode4k: return TRANSMISSION_MODE_4K;
	case IsdbTTransponder::TransmissionMode8k: return TRANSMISSION_MODE_8K;
	case IsdbTTransponder::TransmissionModeAuto: return TRANSMISSION_MODE_AUTO;
	}

	return TRANSMISSION_MODE_AUTO;
}

static fe_guard_interval toDvbGuardInterval(DvbTTransponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case DvbTTransponder::GuardInterval1_4: return GUARD_INTERVAL_1_4;
	case DvbTTransponder::GuardInterval1_8: return GUARD_INTERVAL_1_8;
	case DvbTTransponder::GuardInterval1_16: return GUARD_INTERVAL_1_16;
	case DvbTTransponder::GuardInterval1_32: return GUARD_INTERVAL_1_32;
	case DvbTTransponder::GuardIntervalAuto: return GUARD_INTERVAL_AUTO;
	}

	return GUARD_INTERVAL_AUTO;
}

static fe_guard_interval toDvbGuardInterval(DvbT2Transponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case DvbT2Transponder::GuardInterval1_4:	return GUARD_INTERVAL_1_4;
	case DvbT2Transponder::GuardInterval19_128:	return GUARD_INTERVAL_19_128;
	case DvbT2Transponder::GuardInterval1_8:	return GUARD_INTERVAL_1_8;
	case DvbT2Transponder::GuardInterval19_256:	return GUARD_INTERVAL_19_256;
	case DvbT2Transponder::GuardInterval1_16:	return GUARD_INTERVAL_1_16;
	case DvbT2Transponder::GuardInterval1_32:	return GUARD_INTERVAL_1_32;
	case DvbT2Transponder::GuardInterval1_128:	return GUARD_INTERVAL_1_128;
	case DvbT2Transponder::GuardIntervalAuto:	return GUARD_INTERVAL_AUTO;
	}

	return GUARD_INTERVAL_AUTO;
}

static fe_guard_interval toDvbGuardInterval(IsdbTTransponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case IsdbTTransponder::GuardInterval1_4: return GUARD_INTERVAL_1_4;
	case IsdbTTransponder::GuardInterval1_8: return GUARD_INTERVAL_1_8;
	case IsdbTTransponder::GuardInterval1_16: return GUARD_INTERVAL_1_16;
	case IsdbTTransponder::GuardInterval1_32: return GUARD_INTERVAL_1_32;
	case IsdbTTransponder::GuardIntervalAuto: return GUARD_INTERVAL_AUTO;
	}

	return GUARD_INTERVAL_AUTO;
}

static uint32_t toDvbPartialReception(IsdbTTransponder::PartialReception partialReception)
{
	switch (partialReception) {
	case IsdbTTransponder::PR_disabled: return 0;
	case IsdbTTransponder::PR_enabled: return 1;
	case IsdbTTransponder::PR_AUTO: return (uint32_t)-1;
	}

	return (uint32_t)-1;
}

static uint32_t toDvbSoundBroadcasting(IsdbTTransponder::SoundBroadcasting partialReception)
{
	switch (partialReception) {
	case IsdbTTransponder::SB_disabled: return 0;
	case IsdbTTransponder::SB_enabled: return 1;
	case IsdbTTransponder::SB_AUTO: return (uint32_t)-1;
	}

	return (uint32_t)-1;
}

static uint32_t toDvbInterleaving(IsdbTTransponder::Interleaving interleaving)
{
	switch (interleaving) {
	case IsdbTTransponder::I_0: return 0;
	case IsdbTTransponder::I_1: return 1;
	case IsdbTTransponder::I_2: return 2;
	case IsdbTTransponder::I_4: return 4;
	case IsdbTTransponder::I_8: return 8;
	case IsdbTTransponder::I_16: return 16;
	case IsdbTTransponder::I_AUTO: return (uint32_t)-1;
	}

	return (uint32_t)-1;
}

static fe_hierarchy toDvbHierarchy(DvbTTransponder::Hierarchy hierarchy)
{
	switch (hierarchy) {
	case DvbTTransponder::HierarchyNone: return HIERARCHY_NONE;
	case DvbTTransponder::Hierarchy1: return HIERARCHY_1;
	case DvbTTransponder::Hierarchy2: return HIERARCHY_2;
	case DvbTTransponder::Hierarchy4: return HIERARCHY_4;
	case DvbTTransponder::HierarchyAuto: return HIERARCHY_AUTO;
	}

	return HIERARCHY_AUTO;
}

static fe_hierarchy toDvbHierarchy(DvbT2Transponder::Hierarchy hierarchy)
{
	switch (hierarchy) {
	case DvbT2Transponder::HierarchyNone: return HIERARCHY_NONE;
	case DvbT2Transponder::Hierarchy1: return HIERARCHY_1;
	case DvbT2Transponder::Hierarchy2: return HIERARCHY_2;
	case DvbT2Transponder::Hierarchy4: return HIERARCHY_4;
	case DvbT2Transponder::HierarchyAuto: return HIERARCHY_AUTO;
	}

	return HIERARCHY_AUTO;
}

static DvbCTransponder::Modulation DvbCtoModulation(uint32_t modulation)
{
	switch (modulation) {
	case QAM_16:   return DvbCTransponder::Qam16;
	case QAM_32:   return DvbCTransponder::Qam32;
	case QAM_64:   return DvbCTransponder::Qam64;
	case QAM_128:  return DvbCTransponder::Qam128;
	case QAM_256:  return DvbCTransponder::Qam256;
	default:       return DvbCTransponder::ModulationAuto;
	}
}

static DvbS2Transponder::Modulation DvbS2toModulation(uint32_t modulation)
{
	switch (modulation) {
	case QPSK:     return DvbS2Transponder::Qpsk;
	case PSK_8:    return DvbS2Transponder::Psk8;
	case APSK_16:  return DvbS2Transponder::Apsk16;
	case APSK_32:  return DvbS2Transponder::Apsk32;
	default:       return DvbS2Transponder::ModulationAuto;
	}
}

static DvbS2Transponder::RollOff DvbS2toRollOff(uint32_t rollOff)
{
	switch (rollOff) {
	case ROLLOFF_20:   return DvbS2Transponder::RollOff20;
	case ROLLOFF_25:   return DvbS2Transponder::RollOff25;
	case ROLLOFF_35:   return DvbS2Transponder::RollOff35;
	default: return DvbS2Transponder::RollOffAuto;
	}
}

static DvbTTransponder::Modulation DvbTtoModulation(uint32_t modulation)
{
	switch (modulation) {
	case QPSK:     return DvbTTransponder::Qpsk;
	case QAM_16:   return DvbTTransponder::Qam16;
	case QAM_64:   return DvbTTransponder::Qam64;
	default:       return DvbTTransponder::ModulationAuto;
	}
}

static DvbT2Transponder::Modulation DvbT2toModulation(uint32_t modulation)
{
	switch (modulation) {
	case QPSK:     return DvbT2Transponder::Qpsk;
	case QAM_16:   return DvbT2Transponder::Qam16;
	case QAM_64:   return DvbT2Transponder::Qam64;
	case QAM_256:  return DvbT2Transponder::Qam256;
	default:       return DvbT2Transponder::ModulationAuto;
	}
}

static IsdbTTransponder::Modulation IsdbTtoModulation(uint32_t modulation)
{
	switch (modulation) {
	case QPSK:     return IsdbTTransponder::Qpsk;
	case DQPSK:    return IsdbTTransponder::Dqpsk;
	case QAM_16:   return IsdbTTransponder::Qam16;
	case QAM_64:   return IsdbTTransponder::Qam64;
	default:       return IsdbTTransponder::ModulationAuto;
	}
}

static AtscTransponder::Modulation AtsctoModulation(uint32_t modulation)
{
	switch (modulation) {
	case QAM_64:   return AtscTransponder::Qam64;
	case QAM_256:  return AtscTransponder::Qam256;
	case VSB_8:    return AtscTransponder::Vsb8;
	case VSB_16:   return AtscTransponder::Vsb16;
	default:       return AtscTransponder::ModulationAuto;
	}
}

static DvbTransponderBase::FecRate DvbtoFecRate(uint32_t fecRate)
{
	switch (fecRate) {
	case FEC_NONE: return DvbTransponderBase::FecNone;
	case FEC_1_2:  return DvbTransponderBase::Fec1_2;
//	case FEC_AUTO: return DvbTransponderBase::Fec1_3; // FIXME
//	case FEC_AUTO: return DvbTransponderBase::Fec1_4; // FIXME
	case FEC_2_3:  return DvbTransponderBase::Fec2_3;
//	case FEC_AUTO: return DvbTransponderBase::Fec2_5; // FIXME
	case FEC_3_4:  return DvbTransponderBase::Fec3_4;
	case FEC_3_5:  return DvbTransponderBase::Fec3_5;
	case FEC_4_5:  return DvbTransponderBase::Fec4_5;
	case FEC_5_6:  return DvbTransponderBase::Fec5_6;
	case FEC_6_7:  return DvbTransponderBase::Fec6_7;
	case FEC_7_8:  return DvbTransponderBase::Fec7_8;
	case FEC_8_9:  return DvbTransponderBase::Fec8_9;
	case FEC_9_10: return DvbTransponderBase::Fec9_10;
	default:       return DvbTransponderBase::FecAuto;
	}
}

static DvbTTransponder::Bandwidth DvbTtoBandwidth(uint32_t bandwidth)
{
	switch (bandwidth) {
	case 5000000: return DvbTTransponder::Bandwidth5MHz;
	case 6000000: return DvbTTransponder::Bandwidth6MHz;
	case 7000000: return DvbTTransponder::Bandwidth7MHz;
	case 8000000: return DvbTTransponder::Bandwidth8MHz;
	default:      return DvbTTransponder::BandwidthAuto;
	}
}

static DvbT2Transponder::Bandwidth DvbT2toBandwidth(uint32_t bandwidth)
{
	switch (bandwidth) {
	case 1700000:  return DvbT2Transponder::Bandwidth1_7MHz;
	case 5000000:  return DvbT2Transponder::Bandwidth5MHz;
	case 6000000:  return DvbT2Transponder::Bandwidth6MHz;
	case 7000000:  return DvbT2Transponder::Bandwidth7MHz;
	case 8000000:  return DvbT2Transponder::Bandwidth8MHz;
	case 10000000: return DvbT2Transponder::Bandwidth10MHz;
	default:       return DvbT2Transponder::BandwidthAuto;
	}
}

static IsdbTTransponder::Bandwidth IsdbTtoBandwidth(uint32_t bandwidth)
{
	switch (bandwidth) {
	default:      return IsdbTTransponder::Bandwidth6MHz;
	case 7000000: return IsdbTTransponder::Bandwidth7MHz;
	case 8000000: return IsdbTTransponder::Bandwidth8MHz;
	}
}

static DvbTTransponder::TransmissionMode DvbTtoTransmissionMode(uint32_t mode)
{
	switch (mode) {
	case TRANSMISSION_MODE_2K: return DvbTTransponder::TransmissionMode2k;
	case TRANSMISSION_MODE_4K: return DvbTTransponder::TransmissionMode4k;
	case TRANSMISSION_MODE_8K: return DvbTTransponder::TransmissionMode8k;
	default:                   return DvbTTransponder::TransmissionModeAuto;
	}
}

static DvbT2Transponder::TransmissionMode DvbT2toTransmissionMode(uint32_t mode)
{
	switch (mode) {
	case TRANSMISSION_MODE_1K: return DvbT2Transponder::TransmissionMode1k;
	case TRANSMISSION_MODE_2K: return DvbT2Transponder::TransmissionMode2k;
	case TRANSMISSION_MODE_4K: return DvbT2Transponder::TransmissionMode4k;
	case TRANSMISSION_MODE_8K: return DvbT2Transponder::TransmissionMode8k;
	case TRANSMISSION_MODE_16K: return DvbT2Transponder::TransmissionMode16k;
	case TRANSMISSION_MODE_32K: return DvbT2Transponder::TransmissionMode32k;
	default:                   return DvbT2Transponder::TransmissionModeAuto;
	}
}

static IsdbTTransponder::TransmissionMode IsdbTtoTransmissionMode(uint32_t mode)
{
	switch (mode) {
	case TRANSMISSION_MODE_2K: return IsdbTTransponder::TransmissionMode2k;
	case TRANSMISSION_MODE_4K: return IsdbTTransponder::TransmissionMode4k;
	case TRANSMISSION_MODE_8K: return IsdbTTransponder::TransmissionMode8k;
	default:                   return IsdbTTransponder::TransmissionModeAuto;
	}
}

static DvbTTransponder::GuardInterval DvbTtoGuardInterval(uint32_t guardInterval)
{
	switch (guardInterval) {
	case GUARD_INTERVAL_1_4:  return DvbTTransponder::GuardInterval1_4;
	case GUARD_INTERVAL_1_8:  return DvbTTransponder::GuardInterval1_8;
	case GUARD_INTERVAL_1_16: return DvbTTransponder::GuardInterval1_16;
	case GUARD_INTERVAL_1_32: return DvbTTransponder::GuardInterval1_32;
	default:                  return DvbTTransponder::GuardIntervalAuto;
	}
}

static DvbT2Transponder::GuardInterval DvbT2toGuardInterval(uint32_t guardInterval)
{
	switch (guardInterval) {
	case GUARD_INTERVAL_1_4:	return DvbT2Transponder::GuardInterval1_4;
	case GUARD_INTERVAL_19_128:	return DvbT2Transponder::GuardInterval19_128;
	case GUARD_INTERVAL_1_8:	return DvbT2Transponder::GuardInterval1_8;
	case GUARD_INTERVAL_19_256:	return DvbT2Transponder::GuardInterval19_256;
	case GUARD_INTERVAL_1_16:	return DvbT2Transponder::GuardInterval1_16;
	case GUARD_INTERVAL_1_32:	return DvbT2Transponder::GuardInterval1_32;
	case GUARD_INTERVAL_1_128:	return DvbT2Transponder::GuardInterval1_128;
	default:			return DvbT2Transponder::GuardIntervalAuto;
	}
}

static IsdbTTransponder::GuardInterval IsdbTtoGuardInterval(uint32_t guardInterval)
{
	switch (guardInterval) {
	case GUARD_INTERVAL_1_4:  return IsdbTTransponder::GuardInterval1_4;
	case GUARD_INTERVAL_1_8:  return IsdbTTransponder::GuardInterval1_8;
	case GUARD_INTERVAL_1_16: return IsdbTTransponder::GuardInterval1_16;
	case GUARD_INTERVAL_1_32: return IsdbTTransponder::GuardInterval1_32;
	default:                  return IsdbTTransponder::GuardIntervalAuto;
	}
}

static IsdbTTransponder::PartialReception IsdbTtoPartialReception(uint32_t partialReception)
{
	switch (partialReception) {
	case 0:  return IsdbTTransponder::PR_disabled;
	case 1:  return IsdbTTransponder::PR_enabled;
	default: return IsdbTTransponder::PR_AUTO;
	}
}

static IsdbTTransponder::SoundBroadcasting IsdbTtoSoundBroadcasting(uint32_t partialReception)
{
	switch (partialReception) {
	case 0:  return IsdbTTransponder::SB_disabled;
	case 1:  return IsdbTTransponder::SB_enabled;
	default: return IsdbTTransponder::SB_AUTO;
	}
}

static IsdbTTransponder::Interleaving IsdbTtoInterleaving(uint32_t interleaving)
{
	switch (interleaving) {
	case 0:  return IsdbTTransponder::I_0;
	case 1:  return IsdbTTransponder::I_1;
	case 2:  return IsdbTTransponder::I_2;
	case 4:  return IsdbTTransponder::I_4;
	case 8:  return IsdbTTransponder::I_8;
	case 16: return IsdbTTransponder::I_16;
	default: return IsdbTTransponder::I_AUTO;
	}
}

static DvbTTransponder::Hierarchy DvbTtoHierarchy(uint32_t hierarchy)
{
	switch (hierarchy) {
	case HIERARCHY_NONE: return DvbTTransponder::HierarchyNone;
	case HIERARCHY_1:    return DvbTTransponder::Hierarchy1;
	case HIERARCHY_2:    return DvbTTransponder::Hierarchy2;
	case HIERARCHY_4:    return DvbTTransponder::Hierarchy4;
	default:             return DvbTTransponder::HierarchyAuto;
	}
}

static DvbT2Transponder::Hierarchy DvbT2toHierarchy(uint32_t hierarchy)
{
	switch (hierarchy) {
	case HIERARCHY_NONE: return DvbT2Transponder::HierarchyNone;
	case HIERARCHY_1:    return DvbT2Transponder::Hierarchy1;
	case HIERARCHY_2:    return DvbT2Transponder::Hierarchy2;
	case HIERARCHY_4:    return DvbT2Transponder::Hierarchy4;
	default:             return DvbT2Transponder::HierarchyAuto;
	}
}

bool DvbLinuxDevice::tune(const DvbTransponder &transponder)
{
	Q_ASSERT(dvbv5_parms);
	stopDvr();
	unsigned delsys;

	switch (transponder.getTransmissionType()) {
	case DvbTransponderBase::DvbS: {
		const DvbSTransponder *dvbSTransponder = transponder.as<DvbSTransponder>();

		delsys = SYS_DVBS;
		dvb_fe_store_parm(dvbv5_parms, DTV_FREQUENCY, dvbSTransponder->frequency);
		dvb_fe_store_parm(dvbv5_parms, DTV_INVERSION, INVERSION_AUTO);
		dvb_fe_store_parm(dvbv5_parms, DTV_SYMBOL_RATE, dvbSTransponder->symbolRate);
		dvb_fe_store_parm(dvbv5_parms, DTV_INNER_FEC, toDvbFecRate(dvbSTransponder->fecRate));
		break;
	    }
	case DvbTransponderBase::DvbS2: {
		const DvbS2Transponder *dvbS2Transponder = transponder.as<DvbS2Transponder>();

		delsys = SYS_DVBS2;
		dvb_fe_store_parm(dvbv5_parms, DTV_FREQUENCY, dvbS2Transponder->frequency);
		dvb_fe_store_parm(dvbv5_parms, DTV_INVERSION, INVERSION_AUTO);
		dvb_fe_store_parm(dvbv5_parms, DTV_SYMBOL_RATE, dvbS2Transponder->symbolRate);
		dvb_fe_store_parm(dvbv5_parms, DTV_INNER_FEC, toDvbFecRate(dvbS2Transponder->fecRate));
		dvb_fe_store_parm(dvbv5_parms, DTV_MODULATION, toDvbModulation(dvbS2Transponder->modulation));
		dvb_fe_store_parm(dvbv5_parms, DTV_PILOT, PILOT_AUTO);
		dvb_fe_store_parm(dvbv5_parms, DTV_ROLLOFF, toDvbRollOff(dvbS2Transponder->rollOff));
		break;
	    }
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *dvbCTransponder = transponder.as<DvbCTransponder>();

		delsys = SYS_DVBC_ANNEX_A;
		dvb_fe_store_parm(dvbv5_parms, DTV_FREQUENCY, dvbCTransponder->frequency);
		dvb_fe_store_parm(dvbv5_parms, DTV_MODULATION, toDvbModulation(dvbCTransponder->modulation));
		dvb_fe_store_parm(dvbv5_parms, DTV_INVERSION, INVERSION_AUTO);
		dvb_fe_store_parm(dvbv5_parms, DTV_SYMBOL_RATE, dvbCTransponder->symbolRate);
		dvb_fe_store_parm(dvbv5_parms, DTV_INNER_FEC, toDvbFecRate(dvbCTransponder->fecRate));

		break;
	    }
	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *dvbTTransponder = transponder.as<DvbTTransponder>();

		delsys = SYS_DVBT;
		dvb_fe_store_parm(dvbv5_parms, DTV_FREQUENCY, dvbTTransponder->frequency);
		dvb_fe_store_parm(dvbv5_parms, DTV_MODULATION, toDvbModulation(dvbTTransponder->modulation));
		dvb_fe_store_parm(dvbv5_parms, DTV_BANDWIDTH_HZ, toDvbBandwidth(dvbTTransponder->bandwidth));
		dvb_fe_store_parm(dvbv5_parms, DTV_INVERSION, INVERSION_AUTO);
		dvb_fe_store_parm(dvbv5_parms, DTV_CODE_RATE_HP, toDvbFecRate(dvbTTransponder->fecRateHigh));
		dvb_fe_store_parm(dvbv5_parms, DTV_CODE_RATE_LP, toDvbFecRate(dvbTTransponder->fecRateLow));
		dvb_fe_store_parm(dvbv5_parms, DTV_GUARD_INTERVAL, toDvbGuardInterval(dvbTTransponder->guardInterval));
		dvb_fe_store_parm(dvbv5_parms, DTV_TRANSMISSION_MODE, toDvbTransmissionMode(dvbTTransponder->transmissionMode));
		dvb_fe_store_parm(dvbv5_parms, DTV_HIERARCHY, toDvbHierarchy(dvbTTransponder->hierarchy));
		break;
	    }
	case DvbTransponderBase::DvbT2: {
		const DvbT2Transponder *dvbT2Transponder = transponder.as<DvbT2Transponder>();

		delsys = SYS_DVBT2;
		dvb_fe_store_parm(dvbv5_parms, DTV_FREQUENCY, dvbT2Transponder->frequency);
		dvb_fe_store_parm(dvbv5_parms, DTV_MODULATION, toDvbModulation(dvbT2Transponder->modulation));
		dvb_fe_store_parm(dvbv5_parms, DTV_BANDWIDTH_HZ, toDvbBandwidth(dvbT2Transponder->bandwidth));
		dvb_fe_store_parm(dvbv5_parms, DTV_INVERSION, INVERSION_AUTO);
		dvb_fe_store_parm(dvbv5_parms, DTV_CODE_RATE_HP, toDvbFecRate(dvbT2Transponder->fecRateHigh));
		dvb_fe_store_parm(dvbv5_parms, DTV_CODE_RATE_LP, toDvbFecRate(dvbT2Transponder->fecRateLow));
		dvb_fe_store_parm(dvbv5_parms, DTV_GUARD_INTERVAL, toDvbGuardInterval(dvbT2Transponder->guardInterval));
		dvb_fe_store_parm(dvbv5_parms, DTV_TRANSMISSION_MODE, toDvbTransmissionMode(dvbT2Transponder->transmissionMode));
		dvb_fe_store_parm(dvbv5_parms, DTV_HIERARCHY, toDvbHierarchy(dvbT2Transponder->hierarchy));
		dvb_fe_store_parm(dvbv5_parms, DTV_STREAM_ID, dvbT2Transponder->streamId);
		break;
	    }
	case DvbTransponderBase::Atsc: {
		const AtscTransponder *atscTransponder = transponder.as<AtscTransponder>();

		switch (atscTransponder->modulation) {
		case AtscTransponder::Vsb8:
		case AtscTransponder::Vsb16:
			delsys = SYS_ATSC;
			break;
		default:
			delsys = SYS_DVBC_ANNEX_B;
		}
		dvb_fe_store_parm(dvbv5_parms, DTV_FREQUENCY, atscTransponder->frequency);
		dvb_fe_store_parm(dvbv5_parms, DTV_MODULATION, toDvbModulation(atscTransponder->modulation));
/*		dvb_fe_store_parm(dvbv5_parms, DTV_INVERSION, INVERSION_AUTO); */
		break;
	    }
	case DvbTransponderBase::IsdbT: {
		const IsdbTTransponder *isdbTTransponder = transponder.as<IsdbTTransponder>();
		int i;
		uint32_t layers = 0;

		delsys = SYS_ISDBT;
		dvb_fe_store_parm(dvbv5_parms, DTV_FREQUENCY, isdbTTransponder->frequency);
		dvb_fe_store_parm(dvbv5_parms, DTV_BANDWIDTH_HZ, toDvbBandwidth(isdbTTransponder->bandwidth));
		dvb_fe_store_parm(dvbv5_parms, DTV_INVERSION, INVERSION_AUTO);
		dvb_fe_store_parm(dvbv5_parms, DTV_GUARD_INTERVAL, toDvbGuardInterval(isdbTTransponder->guardInterval));
		dvb_fe_store_parm(dvbv5_parms, DTV_TRANSMISSION_MODE, toDvbTransmissionMode(isdbTTransponder->transmissionMode));

		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_PARTIAL_RECEPTION, toDvbPartialReception(isdbTTransponder->partialReception));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_SOUND_BROADCASTING, toDvbSoundBroadcasting(isdbTTransponder->soundBroadcasting));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_SB_SUBCHANNEL_ID, isdbTTransponder->subChannelId);
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_SB_SEGMENT_IDX, isdbTTransponder->sbSegmentIdx);
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_SB_SEGMENT_COUNT, isdbTTransponder->sbSegmentCount);

		for (i = 0; i < 3; i++) {
			if (isdbTTransponder->layerEnabled[i])
				layers |= 1 << i;
		}
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYER_ENABLED, layers);

		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERA_FEC, toDvbFecRate(isdbTTransponder->fecRate[0]));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERA_MODULATION, toDvbModulation(isdbTTransponder->modulation[0]));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERA_SEGMENT_COUNT, isdbTTransponder->segmentCount[0]);
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERA_TIME_INTERLEAVING, toDvbInterleaving(isdbTTransponder->interleaving[0]));

		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERB_FEC, toDvbFecRate(isdbTTransponder->fecRate[1]));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERB_MODULATION, toDvbModulation(isdbTTransponder->modulation[1]));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERB_SEGMENT_COUNT, isdbTTransponder->segmentCount[1]);
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERA_TIME_INTERLEAVING, toDvbInterleaving(isdbTTransponder->interleaving[1]));

		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERC_FEC, toDvbFecRate(isdbTTransponder->fecRate[2]));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERC_MODULATION, toDvbModulation(isdbTTransponder->modulation[2]));
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERC_SEGMENT_COUNT, isdbTTransponder->segmentCount[2]);
		dvb_fe_store_parm(dvbv5_parms, DTV_ISDBT_LAYERA_TIME_INTERLEAVING, toDvbInterleaving(isdbTTransponder->interleaving[2]));

		break;
	    }
	case DvbTransponderBase::Invalid:
		qInfo() << "DvbLinuxDevice::getProps: unknown transmission type";
		return false;
	default:
		qInfo() << "DvbLinuxDevice::tune: unknown transmission type" <<
			transponder.getTransmissionType();
		return false;
	}

	dvb_fe_store_parm(dvbv5_parms, DTV_DELIVERY_SYSTEM, delsys);


	if (dvb_fe_set_parms(dvbv5_parms) != 0) {
		qInfo() << "DvbLinuxDevice::tune: ioctl FE_SET_PROPERTY failed for frontend" <<
			frontendPath;
		return false;
	}

	startDvr();
	return true;
}

bool DvbLinuxDevice::getProps(DvbTransponder &transponder)
{
	Q_ASSERT(dvbv5_parms);
	uint32_t value;

	/* Update properties with the detected stuff */
	if (!isTuned())
		return false;

	dvb_fe_get_parms(dvbv5_parms);

	switch (transponder.getTransmissionType()) {
	case DvbTransponderBase::DvbS: {
		DvbSTransponder *dvbSTransponder = transponder.as<DvbSTransponder>();

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_FREQUENCY, &value);
		dvbSTransponder->frequency = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_SYMBOL_RATE, &value);
		dvbSTransponder->symbolRate  = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_INNER_FEC, &value);
		dvbSTransponder->fecRate = DvbtoFecRate(value);
		break;
	    }
	case DvbTransponderBase::DvbS2: {
		DvbS2Transponder *dvbS2Transponder = transponder.as<DvbS2Transponder>();

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_FREQUENCY, &value);
		dvbS2Transponder->frequency = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_SYMBOL_RATE, &value);
		dvbS2Transponder->symbolRate = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_INNER_FEC, &value);
		dvbS2Transponder->fecRate = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_MODULATION, &value);
		dvbS2Transponder->modulation = DvbS2toModulation(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ROLLOFF, &value);
		dvbS2Transponder->rollOff = DvbS2toRollOff(value);
		break;
	    }
	case DvbTransponderBase::DvbC: {
		DvbCTransponder *dvbCTransponder = transponder.as<DvbCTransponder>();

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_FREQUENCY, &value);
		dvbCTransponder->frequency = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_MODULATION, &value);
		dvbCTransponder->modulation = DvbCtoModulation(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_SYMBOL_RATE, &value);
		dvbCTransponder->symbolRate = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_INNER_FEC, &value);
		dvbCTransponder->fecRate = DvbtoFecRate(value);

		break;
	    }
	case DvbTransponderBase::DvbT: {
		DvbTTransponder *dvbTTransponder = transponder.as<DvbTTransponder>();

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_FREQUENCY, &value);
		dvbTTransponder->frequency = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_MODULATION, &value);
		dvbTTransponder->modulation = DvbTtoModulation(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_BANDWIDTH_HZ, &value);
		dvbTTransponder->bandwidth = DvbTtoBandwidth(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_CODE_RATE_HP, &value);
		dvbTTransponder->fecRateHigh = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_CODE_RATE_LP, &value);
		dvbTTransponder->fecRateLow = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_GUARD_INTERVAL, &value);
		dvbTTransponder->guardInterval = DvbTtoGuardInterval(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_TRANSMISSION_MODE, &value);
		dvbTTransponder->transmissionMode = DvbTtoTransmissionMode(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_HIERARCHY, &value);
		dvbTTransponder->hierarchy = DvbTtoHierarchy(value);
		break;
	    }
	case DvbTransponderBase::DvbT2: {
		DvbT2Transponder *dvbT2Transponder = transponder.as<DvbT2Transponder>();

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_FREQUENCY, &value);
		dvbT2Transponder->frequency = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_MODULATION, &value);
		dvbT2Transponder->modulation = DvbT2toModulation(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_BANDWIDTH_HZ, &value);
		dvbT2Transponder->bandwidth = DvbT2toBandwidth(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_CODE_RATE_HP, &value);
		dvbT2Transponder->fecRateHigh = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_CODE_RATE_LP, &value);
		dvbT2Transponder->fecRateLow = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_GUARD_INTERVAL, &value);
		dvbT2Transponder->guardInterval = DvbT2toGuardInterval(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_TRANSMISSION_MODE, &value);
		dvbT2Transponder->transmissionMode = DvbT2toTransmissionMode(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_HIERARCHY, &value);
		dvbT2Transponder->hierarchy = DvbT2toHierarchy(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_STREAM_ID, &value);
		dvbT2Transponder->streamId = (int)value;
		break;
	    }
	case DvbTransponderBase::Atsc: {
		AtscTransponder *atscTransponder = transponder.as<AtscTransponder>();

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_FREQUENCY, &value);
		atscTransponder->frequency = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_MODULATION, &value);
		atscTransponder->modulation = AtsctoModulation(value);
		break;
	    }
	case DvbTransponderBase::IsdbT: {
		IsdbTTransponder *isdbTTransponder = transponder.as<IsdbTTransponder>();
		int i;
		uint32_t layers = 0;

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_FREQUENCY, &value);
		isdbTTransponder->frequency = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_BANDWIDTH_HZ, &value);
		isdbTTransponder->bandwidth = IsdbTtoBandwidth(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_GUARD_INTERVAL, &value);
		isdbTTransponder->guardInterval = IsdbTtoGuardInterval(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_TRANSMISSION_MODE, &value);
		isdbTTransponder->transmissionMode = IsdbTtoTransmissionMode(value);

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_PARTIAL_RECEPTION, &value);
		isdbTTransponder->partialReception = IsdbTtoPartialReception(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_SOUND_BROADCASTING, &value);
		isdbTTransponder->soundBroadcasting = IsdbTtoSoundBroadcasting(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_SB_SUBCHANNEL_ID, &value);
		isdbTTransponder->subChannelId = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_SB_SEGMENT_IDX, &value);
		isdbTTransponder->sbSegmentIdx = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_SB_SEGMENT_COUNT, &value);
		isdbTTransponder->sbSegmentCount = (int)value;

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYER_ENABLED, &value);
		layers = (int)value;
		for (i = 0; i < 3; i++) {
			if (isdbTTransponder->layerEnabled[i])
				layers |= 1 << i;
		}

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERA_FEC, &value);
		isdbTTransponder->fecRate[0] = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERA_MODULATION, &value);
		isdbTTransponder->modulation[0] = IsdbTtoModulation(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERA_SEGMENT_COUNT, &value);
		isdbTTransponder->segmentCount[0] = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERA_TIME_INTERLEAVING, &value);
		isdbTTransponder->interleaving[0] = IsdbTtoInterleaving(value);

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERB_FEC, &value);
		isdbTTransponder->fecRate[1] = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERB_MODULATION, &value);
		isdbTTransponder->modulation[1] = IsdbTtoModulation(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERB_SEGMENT_COUNT, &value);
		isdbTTransponder->segmentCount[1] = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERB_TIME_INTERLEAVING, &value);
		isdbTTransponder->interleaving[1] = IsdbTtoInterleaving(value);

		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERC_FEC, &value);
		isdbTTransponder->fecRate[2] = DvbtoFecRate(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERC_MODULATION, &value);
		isdbTTransponder->modulation[2] = IsdbTtoModulation(value);
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERC_SEGMENT_COUNT, &value);
		isdbTTransponder->segmentCount[2] = (int)value;
		dvb_fe_retrieve_parm(dvbv5_parms, DTV_ISDBT_LAYERC_TIME_INTERLEAVING, &value);
		isdbTTransponder->interleaving[2] = IsdbTtoInterleaving(value);

		break;
	    }
	case DvbTransponderBase::Invalid:
		qInfo() << "DvbLinuxDevice::getProps: unknown transmission type";
		return false;
	default:
		qInfo() << "DvbLinuxDevice::getProps: unknown transmission type" <<
			transponder.getTransmissionType();
		return false;
	}
	return true;
}

bool DvbLinuxDevice::isTuned()
{
	Q_ASSERT(dvbv5_parms);
	uint32_t status = 0;

	if (dvb_fe_get_stats(dvbv5_parms) != 0) {
		qInfo() << "DvbLinuxDevice::isTuned: ioctl FE_READ_STATUS failed for frontend" <<
			frontendPath;
		return false;
	}

	if (dvb_fe_retrieve_stats(dvbv5_parms, DTV_STATUS, &status) != 0) {
		qInfo() << "DvbLinuxDevice::isTuned: ioctl FE_READ_STATUS failed for frontend" <<
			frontendPath;
		return false;
	}

	return ((status & FE_HAS_LOCK) != 0);
}

int DvbLinuxDevice::getSignal()
{
	Q_ASSERT(dvbv5_parms);
	struct dtv_stats *stat;
	int signal;

	if (dvb_fe_get_stats(dvbv5_parms) != 0) {
		qInfo() << "DvbLinuxDevice::isTuned: ioctl FE_READ_STATUS failed for frontend" <<
			frontendPath;
		return false;
	}

	stat = dvb_fe_retrieve_stats_layer(dvbv5_parms, DTV_STAT_SIGNAL_STRENGTH, 0);
	if (!stat)
		return -1;

	switch (stat->scale) {
	case FE_SCALE_RELATIVE:
		signal = (100 * stat->uvalue) / 65535;
		break;
	case FE_SCALE_DECIBEL:
		// Convert to dBuV @ 75 ohms, to be positive and typically smaller than 100
		signal = (108800 + stat->svalue) / 1000;
		break;
	default:
		return -1;
	}

	// Assert that signal will be within the expected range
	if (signal > 100)
		signal = 100;
	else if (signal < 0)
		signal = 0;

	return signal;
}

int DvbLinuxDevice::getSnr()
{
	Q_ASSERT(dvbv5_parms);
	struct dtv_stats *stat;
	int cnr;

	if (dvb_fe_get_stats(dvbv5_parms) != 0) {
		qInfo() << "DvbLinuxDevice::isTuned: ioctl FE_READ_STATUS failed for frontend" <<
			frontendPath;
		return false;
	}

	stat = dvb_fe_retrieve_stats_layer(dvbv5_parms, DTV_STAT_CNR, 0);
	if (!stat)
		return -1;

	switch (stat->scale) {
	case FE_SCALE_RELATIVE:
		cnr = (100 * stat->uvalue) / 65535;
		break;
	case FE_SCALE_DECIBEL:
		cnr = (stat->svalue / 1000);
		break;
	default:
		return -1;
	}
	// Assert that CNR will be within the expected range
	if (cnr > 100)
		cnr = 100;
	else if (cnr < 0)
		cnr = 0;

	return cnr;
}

bool DvbLinuxDevice::addPidFilter(int pid)
{
	Q_ASSERT(frontendFd >= 0);

	if (dmxFds.contains(pid)) {
		qInfo() << "DvbLinuxDevice::addPidFilter: pid filter already set up for pid" << pid;
		return false;
	}

	int dmxFd = open(QFile::encodeName(demuxPath).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	if (dmxFd < 0) {
		qInfo() << "DvbLinuxDevice::addPidFilter: cannot open demux" << demuxPath;
		return false;
	}

	dmx_pes_filter_params pes_filter;
	memset(&pes_filter, 0, sizeof(pes_filter));
	pes_filter.pid = ushort(pid);
	pes_filter.input = DMX_IN_FRONTEND;
	pes_filter.output = DMX_OUT_TS_TAP;
	pes_filter.pes_type = DMX_PES_OTHER;
	pes_filter.flags = DMX_IMMEDIATE_START;

	if (ioctl(dmxFd, DMX_SET_PES_FILTER, &pes_filter) != 0) {
		qInfo() << "DvbLinuxDevice::addPidFilter: cannot set up pid filter for demux" <<
			demuxPath;
		close(dmxFd);
		return false;
	}

	dmxFds.insert(pid, dmxFd);
	return true;
}

void DvbLinuxDevice::removePidFilter(int pid)
{
	Q_ASSERT(frontendFd >= 0);

	if (!dmxFds.contains(pid)) {
		qInfo() << "DvbLinuxDevice::removePidFilter: no pid filter set up for pid" << pid;
		return;
	}

	close(dmxFds.take(pid));
}

void DvbLinuxDevice::startDescrambling(const QByteArray &pmtSectionData)
{
	cam.startDescrambling(pmtSectionData);
}

void DvbLinuxDevice::stopDescrambling(int serviceId)
{
	cam.stopDescrambling(serviceId);
}

void DvbLinuxDevice::release()
{
	stopDvr();

	if (dvrBuffer.data != NULL) {
		dvrBuffer.dataSize = 0;
		frontend->writeBuffer(dvrBuffer);
		dvrBuffer.data = NULL;
	}

	if (dvrPipe[0] >= 0) {
		close(dvrPipe[0]);
		dvrPipe[0] = -1;
	}

	if (dvrPipe[1] >= 0) {
		close(dvrPipe[1]);
		dvrPipe[1] = -1;
	}

	if (dvrFd >= 0) {
		close(dvrFd);
		dvrFd = -1;
	}

	foreach (int dmxFd, dmxFds) {
		close(dmxFd);
	}

	dmxFds.clear();

	if (dvbv5_parms) {
		dvb_fe_close(dvbv5_parms);
		dvbv5_parms = NULL;
	}
}

void DvbLinuxDevice::startDvr()
{
	Q_ASSERT((dvrFd >= 0) && !isRunning());

	if ((dvrPipe[0] < 0) || (dvrPipe[1] < 0)) {
		if (pipe(dvrPipe) != 0) {
			dvrPipe[0] = -1;
			dvrPipe[1] = -1;
		}

		if ((dvrPipe[0] < 0) || (dvrPipe[1] < 0)) {
			qInfo() << "DvbLinuxDevice::startDvr: cannot create pipe";
			return;
		}
	}

	if (dvrBuffer.data == NULL) {
		dvrBuffer = frontend->getBuffer();
	}

	while (true) {
		int bufferSize = dvrBuffer.bufferSize;
		int dataSize = int(read(dvrFd, dvrBuffer.data, bufferSize));

		if (dataSize < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				break;
			}

			if (errno == EINTR) {
				continue;
			}

			dataSize = int(read(dvrFd, dvrBuffer.data, bufferSize));

			if (dataSize < 0) {
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					break;
				}

				if (errno == EINTR) {
					continue;
				}

				qInfo() << "DvbLinuxDevice::startDvr: cannot read from dvr" << dvrPath << errno;
				return;
			}
		}

		if (dataSize != bufferSize) {
			break;
		}
	}

	start();
}

void DvbLinuxDevice::stopDvr()
{
	if (isRunning()) {
		Q_ASSERT((dvrPipe[0] >= 0) && (dvrPipe[1] >= 0));

		if (write(dvrPipe[1], " ", 1) != 1) {
			qInfo() << "DvbLinuxDevice::stopDvr: cannot write to pipe";
		}

		wait();
		char data;

		if (read(dvrPipe[0], &data, 1) != 1) {
			qInfo() << "DvbLinuxDevice::stopDvr: cannot read from pipe";
		}
	}
}

void DvbLinuxDevice::run()
{
	Q_ASSERT((dvrFd >= 0) && (dvrPipe[0] >= 0) && (dvrBuffer.data != NULL));
	pollfd pollFds[2];
	memset(&pollFds, 0, sizeof(pollFds));
	pollFds[0].fd = dvrPipe[0];
	pollFds[0].events = POLLIN;
	pollFds[1].fd = dvrFd;
	pollFds[1].events = POLLIN;

	while (true) {
		if (poll(pollFds, 2, -1) < 0) {
			if (errno == EINTR) {
				continue;
			}

			qInfo() << "DvbLinuxDevice::run: poll failed";
			return;
		}

		if ((pollFds[0].revents & POLLIN) != 0) {
			return;
		}

		while (true) {
			int bufferSize = dvrBuffer.bufferSize;
			int dataSize = int(read(dvrFd, dvrBuffer.data, bufferSize));

			if (dataSize < 0) {
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					break;
				}

				if (errno == EINTR) {
					continue;
				}

				qInfo() << "DvbLinuxDevice::run: cannot read from dvr" << dvrPath << errno;
				dataSize = int(read(dvrFd, dvrBuffer.data, bufferSize));

				if (dataSize < 0) {
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
						break;
					}

					if (errno == EINTR) {
						continue;
					}

					qInfo() << "DvbLinuxDevice::run: cannot read from dvr" <<
						dvrPath << errno;
					return;
				}
			}

			if (dataSize > 0) {
				dvrBuffer.dataSize = dataSize;
				frontend->writeBuffer(dvrBuffer);
				dvrBuffer = frontend->getBuffer();
			}

			if (dataSize != bufferSize) {
				break;
			}
		}

		msleep(10);
	}
}

DvbLinuxDeviceManager::DvbLinuxDeviceManager(QObject *parent) : QObject(parent)
{
	QObject *notifier = Solid::DeviceNotifier::instance();
	connect(notifier, SIGNAL(deviceAdded(QString)), this, SLOT(componentAdded(QString)));
	connect(notifier, SIGNAL(deviceRemoved(QString)), this, SLOT(componentRemoved(QString)));
}

DvbLinuxDeviceManager::~DvbLinuxDeviceManager()
{
}

void DvbLinuxDeviceManager::doColdPlug()
{
	foreach (const Solid::Device &device, Solid::Device::allDevices()) {
		componentAdded(device.udi());
	}
}

void DvbLinuxDeviceManager::componentAdded(const QString &udi)
{
	QRegularExpressionMatch match;
	bool ok;

	QRegularExpression rejex = QRegularExpression("/dvb/dvb(\\d+).(\\w+)(\\d+)");
	if (!udi.contains(rejex, &match))
		return;

	int adapter = match.captured(1).toShort(&ok, 10);
	if (!ok)
		return;
	QString type = match.captured(2);
	int index = match.captured(3).toShort(&ok, 10);
	if (!ok)
		return;

	QString devicePath = QString(QLatin1String("/dev/dvb/adapter%1/%2%3")).arg(adapter).arg(type).arg(index);

	if ((adapter < 0) || (adapter > 0x7fff) || (index < 0) || (index > 0x7fff)) {
		qInfo() << "DvbLinuxDeviceManager::componentAdded: "
		    "cannot determine adapter or index for device" << udi;
		return;
	}

	if (devicePath.isEmpty()) {
		qInfo() << "DvbLinuxDeviceManager::componentAdded: cannot determine path for device" <<
			udi;
		return;
	}

	int deviceIndex = ((adapter << 16) | index);
	DvbLinuxDevice *device = devices.value(deviceIndex);

	if (device == NULL) {
		device = new DvbLinuxDevice(this);
		devices.insert(deviceIndex, device);
	}

	bool addDevice = false;

	if (!type.compare("ca")) {
		if (device->caPath.isEmpty()) {
			device->caPath = devicePath;
			device->caUdi = udi;
			udis.insert(udi, device);

			if (device->isReady())
				device->startCa();
		}
	} else if (!type.compare("demux")) {
		if (device->demuxPath.isEmpty()) {
			device->demuxPath = devicePath;
			device->demuxUdi = udi;
			udis.insert(udi, device);
			addDevice = true;
		}
	} else if (!type.compare("dvr")) {
		if (device->dvrPath.isEmpty()) {
			device->dvrPath = devicePath;
			device->dvrUdi = udi;
			udis.insert(udi, device);
			addDevice = true;
		}
	} else if (!type.compare("frontend")) {
		if (device->frontendPath.isEmpty()) {
			device->frontendPath = devicePath;
			device->frontendUdi = udi;
			udis.insert(udi, device);
			addDevice = true;
		}
	}

	if (addDevice && !device->demuxPath.isEmpty() && !device->dvrPath.isEmpty() &&
	    !device->frontendPath.isEmpty()) {
		QString path = QString(QLatin1String("/sys/class/dvb/dvb%1.frontend%2/")).arg(adapter).arg(index);
		QString deviceId;
		device->adapter = adapter;
		device->index = index;
		device->dvbv5_parms = NULL;
		if (QFile::exists(path + QLatin1String("device/vendor"))) {
			// PCI device
			int vendor = readSysAttr(path + QLatin1String("device/vendor"));
			int pciDevice = readSysAttr(path + QLatin1String("device/device"));
			int subsystemVendor = readSysAttr(path + QLatin1String("device/subsystem_vendor"));
			int subsystemDevice = readSysAttr(path + QLatin1String("device/subsystem_device"));

			if ((vendor >= 0) && (pciDevice >= 0) && (subsystemVendor >= 0) && (subsystemDevice >= 0)) {
				deviceId = QLatin1Char('P');
				deviceId += QString(QLatin1String("%1")).arg(vendor, 4, 16, QLatin1Char('0'));
				deviceId += QString(QLatin1String("%1")).arg(pciDevice, 4, 16, QLatin1Char('0'));
				deviceId += QString(QLatin1String("%1")).arg(subsystemVendor, 4, 16, QLatin1Char('0'));
				deviceId += QString(QLatin1String("%1")).arg(subsystemDevice, 4, 16, QLatin1Char('0'));
			}
		} else if (QFile::exists(path + QLatin1String("device/idVendor"))) {
			// USB device
			int vendor = readSysAttr(path + QLatin1String("device/idVendor"));
			int product = readSysAttr(path + QLatin1String("device/idProduct"));

			if ((vendor >= 0) && (product >= 0)) {
				deviceId = QLatin1Char('U');
				deviceId += QString(QLatin1String("%1")).arg(vendor, 4, 16, QLatin1Char('0'));
				deviceId += QString(QLatin1String("%1")).arg(product, 4, 16, QLatin1Char('0'));
			}
		}

		device->startDevice(deviceId);

		if (device->isReady()) {
			emit deviceAdded(device);
		}
	}
}

void DvbLinuxDeviceManager::componentRemoved(const QString &udi)
{
	QRegularExpression rejex = QRegularExpression("/dvb/dvb(\\d+).(\\w+)(\\d+)");
	if (!udi.contains(rejex))
		return;

	DvbLinuxDevice *device = udis.take(udi);

	// The device is not mapped. Just return
	if (!device)
		return;

	bool removeDevice = false;

	qInfo() << "Digital TV device removed: " << udi;

	if (udi == device->caUdi) {
		device->caPath.clear();
		device->caUdi.clear();

		if (device->isReady()) {
			device->stopCa();
		}
	} else if (udi == device->demuxUdi) {
		device->demuxPath.clear();
		device->demuxUdi.clear();
		removeDevice = true;
	} else if (udi == device->dvrUdi) {
		device->dvrPath.clear();
		device->dvrUdi.clear();
		removeDevice = true;
	} else if (udi == device->frontendUdi) {
		device->frontendPath.clear();
		device->frontendUdi.clear();
		removeDevice = true;
	}

	if (removeDevice && device->isReady()) {
		emit deviceRemoved(device);
		device->stopDevice();
	}
}

int DvbLinuxDeviceManager::readSysAttr(const QString &path)
{
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly)) {
		return -1;
	}

	QByteArray data = file.read(8);

	if ((data.size() == 0) || (data.size() == 8)) {
		return -1;
	}

	bool ok = false;
	int value = data.simplified().toInt(&ok, 0);

	if (!ok || (value < 0) || (value > 0xffff)) {
		return -1;
	}

	return value;
}
