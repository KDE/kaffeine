/*
 * dvbdevice_linux.cpp
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

#include "dvbdevice_linux.h"

#include <QFile>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/DvbInterface>
#include <dmx.h>
#include <errno.h>
#include <fcntl.h>
#include <frontend.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../log.h"
#include "dvbtransponder.h"

// krazy:excludeall=syscalls

DvbLinuxDevice::DvbLinuxDevice(QObject *parent) : QThread(parent), ready(false), frontend(NULL),
	enabled(false), frontendFd(-1), dvrFd(-1), dvrBuffer(NULL, 0)
{
	dvrPipe[0] = -1;
	dvrPipe[1] = -1;
}

DvbLinuxDevice::~DvbLinuxDevice()
{
	stopDevice();
}

bool DvbLinuxDevice::isReady() const
{
	return ready;
}

void DvbLinuxDevice::startDevice(const QString &deviceId_)
{
	Q_ASSERT(!ready);
	int fd = open(QFile::encodeName(frontendPath).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	if (fd < 0) {
		Log("DvbLinuxDevice::startDevice: cannot open frontend") << frontendPath;
		return;
	}

	dvb_frontend_info frontend_info;
	memset(&frontend_info, 0, sizeof(frontend_info));

	if (ioctl(fd, FE_GET_INFO, &frontend_info) != 0) {
		Log("DvbLinuxDevice::startDevice: ioctl FE_GET_INFO failed for frontend") <<
			frontendPath;
		close(fd);
		return;
	}

	close(fd);
	deviceId = deviceId_;
	frontendName = QString::fromUtf8(frontend_info.name);

	switch (frontend_info.type) {
	case FE_QAM:
		transmissionTypes = DvbC;
		break;
	case FE_QPSK:
		transmissionTypes = DvbS;

		if (((frontend_info.caps & FE_CAN_2G_MODULATION) != 0) ||
		    (strcmp(frontend_info.name, "Conexant CX24116/CX24118") == 0) ||
		    (strcmp(frontend_info.name, "Genpix 8psk-to-USB2 DVB-S") == 0) ||
		    (strcmp(frontend_info.name, "STB0899 Multistandard") == 0)) {
			transmissionTypes |= DvbS2;
		}

		break;
	case FE_OFDM:
		transmissionTypes = DvbT;
		break;
	case FE_ATSC:
		transmissionTypes = Atsc;
		break;
	default:
		Log("DvbLinuxDevice::startDevice: unknown type") << frontend_info.type <<
			QLatin1String("for frontend") << frontendPath;
		return;
	}

	capabilities = 0;

	if ((frontend_info.caps & FE_CAN_QAM_AUTO) != 0) {
		capabilities |= DvbTModulationAuto;
	}

	if ((frontend_info.caps & FE_CAN_FEC_AUTO) != 0) {
		capabilities |= DvbTFecAuto;
	}

	if ((frontend_info.caps & FE_CAN_TRANSMISSION_MODE_AUTO) != 0) {
		capabilities |= DvbTTransmissionModeAuto;
	}

	if ((frontend_info.caps & FE_CAN_GUARD_INTERVAL_AUTO) != 0) {
		capabilities |= DvbTGuardIntervalAuto;
	}

	Log("DvbLinuxDevice::startDevice: found dvb device") << deviceId << '/' << frontendName;
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
	Q_ASSERT(enabled && (frontendFd < 0) && (dvrFd < 0));
	frontendFd = open(QFile::encodeName(frontendPath).constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);

	if (frontendFd < 0) {
		Log("DvbLinuxDevice::acquire: cannot open frontend") << frontendPath << frontendFd;
		return false;
	}

	dvrFd = open(QFile::encodeName(dvrPath).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	if (dvrFd < 0) {
		Log("DvbLinuxDevice::acquire: cannot open dvr") << dvrPath;
		close(frontendFd);
		frontendFd = -1;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::setTone(SecTone tone)
{
	Q_ASSERT(frontendFd >= 0);

	if (ioctl(frontendFd, FE_SET_TONE, (tone == ToneOn) ? SEC_TONE_ON : SEC_TONE_OFF) != 0) {
		Log("DvbLinuxDevice::setTone: ioctl FE_SET_TONE failed for frontend") <<
			frontendPath;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::setVoltage(SecVoltage voltage)
{
	Q_ASSERT(frontendFd >= 0);

	if (ioctl(frontendFd, FE_SET_VOLTAGE,
		  (voltage == Voltage18V) ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13) != 0) {
		Log("DvbLinuxDevice::setVoltage: ioctl FE_SET_VOLTAGE failed for frontend") <<
			frontendPath;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::sendMessage(const char *message, int length)
{
	Q_ASSERT((frontendFd >= 0) && (length >= 0) && (length <= 6));
	dvb_diseqc_master_cmd cmd;
	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.msg, message, length);
	cmd.msg_len = char(length);

	if (ioctl(frontendFd, FE_DISEQC_SEND_MASTER_CMD, &cmd) != 0) {
		Log("DvbLinuxDevice::sendMessage: "
		    "ioctl FE_DISEQC_SEND_MASTER_CMD failed for frontend") << frontendPath;
		return false;
	}

	return true;
}

bool DvbLinuxDevice::sendBurst(SecBurst burst)
{
	Q_ASSERT(frontendFd >= 0);

	if (ioctl(frontendFd, FE_DISEQC_SEND_BURST,
		  (burst == BurstMiniA) ? SEC_MINI_A : SEC_MINI_B) != 0) {
		Log("DvbLinuxDevice::sendBurst: ioctl FE_DISEQC_SEND_BURST failed for frontend") <<
			frontendPath;
		return false;
	}

	return true;
}

static fe_modulation_t convertDvbModulation(DvbCTransponder::Modulation modulation)
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

static fe_modulation_t convertDvbModulation(DvbS2Transponder::Modulation modulation)
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

static fe_rolloff convertDvbRollOff(DvbS2Transponder::RollOff rollOff)
{
	switch (rollOff) {
	case DvbS2Transponder::RollOff20: return ROLLOFF_20;
	case DvbS2Transponder::RollOff25: return ROLLOFF_25;
	case DvbS2Transponder::RollOff35: return ROLLOFF_35;
	case DvbS2Transponder::RollOffAuto: return ROLLOFF_AUTO;
	}

	return ROLLOFF_AUTO;
}

static fe_modulation_t convertDvbModulation(DvbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbTTransponder::Qpsk: return QPSK;
	case DvbTTransponder::Qam16: return QAM_16;
	case DvbTTransponder::Qam64: return QAM_64;
	case DvbTTransponder::ModulationAuto: return QAM_AUTO;
	}

	return QAM_AUTO;
}

static fe_modulation_t convertDvbModulation(AtscTransponder::Modulation modulation)
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

static fe_code_rate convertDvbFecRate(DvbTransponderBase::FecRate fecRate)
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

static fe_bandwidth convertDvbBandwidth(DvbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbTTransponder::Bandwidth6MHz: return BANDWIDTH_6_MHZ;
	case DvbTTransponder::Bandwidth7MHz: return BANDWIDTH_7_MHZ;
	case DvbTTransponder::Bandwidth8MHz: return BANDWIDTH_8_MHZ;
	case DvbTTransponder::BandwidthAuto: return BANDWIDTH_AUTO;
	}

	return BANDWIDTH_AUTO;
}

static fe_transmit_mode convertDvbTransmissionMode(DvbTTransponder::TransmissionMode mode)
{
	switch (mode) {
	case DvbTTransponder::TransmissionMode2k: return TRANSMISSION_MODE_2K;
	case DvbTTransponder::TransmissionMode4k: return TRANSMISSION_MODE_4K;
	case DvbTTransponder::TransmissionMode8k: return TRANSMISSION_MODE_8K;
	case DvbTTransponder::TransmissionModeAuto: return TRANSMISSION_MODE_AUTO;
	}

	return TRANSMISSION_MODE_AUTO;
}

static fe_guard_interval convertDvbGuardInterval(DvbTTransponder::GuardInterval guardInterval)
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

static fe_hierarchy convertDvbHierarchy(DvbTTransponder::Hierarchy hierarchy)
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

bool DvbLinuxDevice::tune(const DvbTransponder &transponder)
{
	Q_ASSERT(frontendFd >= 0);
	stopDvr();
	dvb_frontend_parameters params;

	switch (transponder.getTransmissionType()) {
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *dvbCTransponder = transponder.as<DvbCTransponder>();
		memset(&params, 0, sizeof(params));
		params.frequency = dvbCTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.qam.symbol_rate = dvbCTransponder->symbolRate;
		params.u.qam.fec_inner = convertDvbFecRate(dvbCTransponder->fecRate);
		params.u.qam.modulation = convertDvbModulation(dvbCTransponder->modulation);
		break;
	    }
	case DvbTransponderBase::DvbS: {
		const DvbSTransponder *dvbSTransponder = transponder.as<DvbSTransponder>();
		memset(&params, 0, sizeof(params));
		params.frequency = dvbSTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.qpsk.symbol_rate = dvbSTransponder->symbolRate;
		params.u.qpsk.fec_inner = convertDvbFecRate(dvbSTransponder->fecRate);
		break;
	    }
	case DvbTransponderBase::DvbS2: {
		const DvbS2Transponder *dvbS2Transponder = transponder.as<DvbS2Transponder>();
		dtv_property properties[9];
		memset(properties, 0, sizeof(properties));
		properties[0].cmd = DTV_DELIVERY_SYSTEM;
		properties[0].u.data = SYS_DVBS2;
		properties[1].cmd = DTV_FREQUENCY;
		properties[1].u.data = dvbS2Transponder->frequency;
		properties[2].cmd = DTV_SYMBOL_RATE;
		properties[2].u.data = dvbS2Transponder->symbolRate;
		properties[3].cmd = DTV_MODULATION;
		properties[3].u.data = convertDvbModulation(dvbS2Transponder->modulation);
		properties[4].cmd = DTV_ROLLOFF;
		properties[4].u.data = convertDvbRollOff(dvbS2Transponder->rollOff);
		properties[5].cmd = DTV_INVERSION;
		properties[5].u.data = INVERSION_AUTO;
		properties[6].cmd = DTV_PILOT;
		properties[6].u.data = PILOT_AUTO;
		properties[7].cmd = DTV_INNER_FEC;
		properties[7].u.data = convertDvbFecRate(dvbS2Transponder->fecRate);
		properties[8].cmd = DTV_TUNE;
		dtv_properties propertyList;
		memset(&propertyList, 0, sizeof(propertyList));
		propertyList.props = properties;
		propertyList.num = (sizeof(properties) / sizeof(properties[0]));

		if (ioctl(frontendFd, FE_SET_PROPERTY, &propertyList) != 0) {
			Log("DvbLinuxDevice::tune: ioctl FE_SET_PROPERTY failed for frontend") <<
				frontendPath;
			return false;
		}

		startDvr();
		return true;
	    }
	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *dvbTTransponder = transponder.as<DvbTTransponder>();
		memset(&params, 0, sizeof(params));
		params.frequency = dvbTTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.ofdm.bandwidth = convertDvbBandwidth(dvbTTransponder->bandwidth);
		params.u.ofdm.code_rate_HP = convertDvbFecRate(dvbTTransponder->fecRateHigh);
		params.u.ofdm.code_rate_LP = convertDvbFecRate(dvbTTransponder->fecRateLow);
		params.u.ofdm.constellation = convertDvbModulation(dvbTTransponder->modulation);
		params.u.ofdm.transmission_mode =
			convertDvbTransmissionMode(dvbTTransponder->transmissionMode);
		params.u.ofdm.guard_interval =
			convertDvbGuardInterval(dvbTTransponder->guardInterval);
		params.u.ofdm.hierarchy_information =
			convertDvbHierarchy(dvbTTransponder->hierarchy);
		break;
	    }
	case DvbTransponderBase::Atsc: {
		const AtscTransponder *atscTransponder = transponder.as<AtscTransponder>();
		memset(&params, 0, sizeof(params));
		params.frequency = atscTransponder->frequency;
		params.inversion = INVERSION_AUTO;
		params.u.vsb.modulation = convertDvbModulation(atscTransponder->modulation);
		break;
	    }
	default:
		Log("DvbLinuxDevice::tune: unknown transmission type") <<
			transponder.getTransmissionType();
		return false;
	}

	if (ioctl(frontendFd, FE_SET_FRONTEND, &params) != 0) {
		Log("DvbLinuxDevice::tune: ioctl FE_SET_FRONTEND failed for frontend") <<
			frontendPath;
		return false;
	}

	startDvr();
	return true;
}

bool DvbLinuxDevice::isTuned()
{
	Q_ASSERT(frontendFd >= 0);
	fe_status_t status;
	memset(&status, 0, sizeof(status));

	if (ioctl(frontendFd, FE_READ_STATUS, &status) != 0) {
		Log("DvbLinuxDevice::isTuned: ioctl FE_READ_STATUS failed for frontend") <<
			frontendPath;
		return false;
	}

	return ((status & FE_HAS_LOCK) != 0);
}

int DvbLinuxDevice::getSignal()
{
	Q_ASSERT(frontendFd >= 0);
	quint16 signal = 0;

	if (ioctl(frontendFd, FE_READ_SIGNAL_STRENGTH, &signal) != 0) {
		Log("DvbLinuxDevice::getSignal: "
		    "ioctl FE_READ_SIGNAL_STRENGTH failed for frontend") << frontendPath;
		return -1;
	}

	if (signal == 0) {
		// assume that reading signal strength isn't supported
		return -1;
	}

	return ((signal * 100 + 0x8001) >> 16);
}

int DvbLinuxDevice::getSnr()
{
	Q_ASSERT(frontendFd >= 0);
	quint16 snr = 0;

	if (ioctl(frontendFd, FE_READ_SNR, &snr) != 0) {
		Log("DvbLinuxDevice::getSnr: ioctl FE_READ_SNR failed for frontend") <<
			frontendPath;
		return -1;
	}

	if (snr == 0) {
		// assume that reading snr isn't supported
		return -1;
	}

	return ((snr * 100 + 0x8001) >> 16);
}

bool DvbLinuxDevice::addPidFilter(int pid)
{
	Q_ASSERT(frontendFd >= 0);

	if (dmxFds.contains(pid)) {
		Log("DvbLinuxDevice::addPidFilter: pid filter already set up for pid") << pid;
		return false;
	}

	int dmxFd = open(QFile::encodeName(demuxPath).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	if (dmxFd < 0) {
		Log("DvbLinuxDevice::addPidFilter: cannot open demux") << demuxPath;
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
		Log("DvbLinuxDevice::addPidFilter: cannot set up pid filter for demux") <<
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
		Log("DvbLinuxDevice::removePidFilter: no pid filter set up for pid") << pid;
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

	if (frontendFd >= 0) {
		close(frontendFd);
		frontendFd = -1;
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
			Log("DvbLinuxDevice::startDvr: cannot create pipe");
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

				Log("DvbLinuxDevice::startDvr: cannot read from dvr") << dvrPath;
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
			Log("DvbLinuxDevice::stopDvr: cannot write to pipe");
		}

		wait();
		char data;

		if (read(dvrPipe[0], &data, 1) != 1) {
			Log("DvbLinuxDevice::stopDvr: cannot read from pipe");
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

			Log("DvbLinuxDevice::run: poll failed");
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

				Log("DvbLinuxDevice::run: cannot read from dvr") << dvrPath;
				dataSize = int(read(dvrFd, dvrBuffer.data, bufferSize));

				if (dataSize < 0) {
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
						break;
					}

					if (errno == EINTR) {
						continue;
					}

					Log("DvbLinuxDevice::run: cannot read from dvr") <<
						dvrPath;
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
	foreach (const Solid::Device &device,
		 Solid::Device::listFromType(Solid::DeviceInterface::DvbInterface)) {
		componentAdded(device.udi());
	}
}

void DvbLinuxDeviceManager::componentAdded(const QString &udi)
{
	const Solid::DvbInterface *dvbInterface = Solid::Device(udi).as<Solid::DvbInterface>();

	if (dvbInterface == NULL) {
		return;
	}

	int adapter = dvbInterface->deviceAdapter();
	int index = dvbInterface->deviceIndex();
	QString devicePath = dvbInterface->device();

	if ((adapter < 0) || (adapter > 0x7fff) || (index < 0) || (index > 0x7fff)) {
		Log("DvbLinuxDeviceManager::componentAdded: "
		    "cannot determine adapter or index for device") << udi;
		return;
	}

	if (devicePath.isEmpty()) {
		Log("DvbLinuxDeviceManager::componentAdded: cannot determine path for device") <<
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

	switch (dvbInterface->deviceType()) {
	case Solid::DvbInterface::DvbCa:
		if (device->caPath.isEmpty()) {
			device->caPath = devicePath;
			device->caUdi = udi;
			udis.insert(udi, device);

			if (device->isReady()) {
				device->startCa();
			}
		}

		break;
	case Solid::DvbInterface::DvbDemux:
		if (device->demuxPath.isEmpty()) {
			device->demuxPath = devicePath;
			device->demuxUdi = udi;
			udis.insert(udi, device);
			addDevice = true;
		}

		break;
	case Solid::DvbInterface::DvbDvr:
		if (device->dvrPath.isEmpty()) {
			device->dvrPath = devicePath;
			device->dvrUdi = udi;
			udis.insert(udi, device);
			addDevice = true;
		}

		break;
	case Solid::DvbInterface::DvbFrontend:
		if (device->frontendPath.isEmpty()) {
			device->frontendPath = devicePath;
			device->frontendUdi = udi;
			udis.insert(udi, device);
			addDevice = true;
		}

		break;
	default:
		break;
	}

	if (addDevice && !device->demuxPath.isEmpty() && !device->dvrPath.isEmpty() &&
	    !device->frontendPath.isEmpty()) {
		QString path = QString(QLatin1String("/sys/class/dvb/dvb%1.frontend%2/")).arg(adapter).arg(index);
		QString deviceId;

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
	DvbLinuxDevice *device = udis.take(udi);

	if (device == NULL) {
		return;
	}

	bool removeDevice = false;

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
