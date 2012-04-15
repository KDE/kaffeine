/*
 * dvbscan.cpp
 *
 * Copyright (C) 2008-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbscan.h"

#include <QBitArray>
#include "../log.h"
#include "dvbdevice.h"
#include "dvbsi.h"

class DvbPatEntry
{
public:
	DvbPatEntry(int programNumber_, int pid_) : programNumber(programNumber_), pid(pid_) { }
	~DvbPatEntry() { }

	int programNumber;
	int pid;
};

Q_DECLARE_TYPEINFO(DvbPatEntry, Q_MOVABLE_TYPE);

class DvbSdtEntry
{
public:
	DvbSdtEntry(int serviceId_, int networkId_, bool scrambled_) : serviceId(serviceId_),
		networkId(networkId_), scrambled(scrambled_) { }
	~DvbSdtEntry() { }

	int serviceId;
	int networkId;
	bool scrambled;
	QString name;
	QString provider;
};

class DvbScanFilter : public DvbSectionFilter, QObject
{
public:
	DvbScanFilter(DvbScan *scan_) : scan(scan_), pid(-1) { }

	~DvbScanFilter()
	{
		stopFilter();
	}

	bool isActive() const
	{
		return (pid != -1);
	}

	bool startFilter(int pid_, DvbScan::FilterType type_);
	void stopFilter();

private:
	bool checkMultipleSection(const DvbStandardSection &section);
	void processSection(const char *data, int size);
	void timerEvent(QTimerEvent *);

	DvbScan *scan;

	int pid;
	DvbScan::FilterType type;
	QBitArray multipleSections;
	int timerId;
};

bool DvbScanFilter::startFilter(int pid_, DvbScan::FilterType type_)
{
	Q_ASSERT(pid == -1);

	pid = pid_;
	type = type_;
	multipleSections.clear();

	if (!scan->device->addSectionFilter(pid, this)) {
		pid = -1;
		return false;
	}

	// FIXME check timings
	if (type != DvbScan::NitFilter) {
		timerId = startTimer(5000);
	} else {
		timerId = startTimer(20000);
	}

	return true;
}

void DvbScanFilter::stopFilter()
{
	if (pid != -1) {
		killTimer(timerId);
		scan->device->removeSectionFilter(pid, this);

		pid = -1;
	}
}

bool DvbScanFilter::checkMultipleSection(const DvbStandardSection &section)
{
	int sectionCount = section.lastSectionNumber() + 1;

	if (section.sectionNumber() >= sectionCount) {
		Log("DvbScanFilter::checkMultipleSection: current > last");
		sectionCount = section.sectionNumber() + 1;
	}

	if (multipleSections.isEmpty()) {
		multipleSections.resize(sectionCount);
	} else {
		if (multipleSections.size() != sectionCount) {
			Log("DvbScanFilter::checkMultipleSection: "
			    "inconsistent number of sections");

			if (multipleSections.size() < sectionCount) {
				multipleSections.resize(sectionCount);
			}
		}
	}

	if (multipleSections.testBit(section.sectionNumber())) {
		return false;
	}

	multipleSections.setBit(section.sectionNumber());
	return true;
}

void DvbScanFilter::processSection(const char *data, int size)
{
	switch (type) {
	case DvbScan::PatFilter: {
		DvbPatSection patSection(data, size);

		if (!patSection.isValid() || (patSection.tableId() != 0x0)) {
			return;
		}

		if (!checkMultipleSection(patSection)) {
			// already read this part
			return;
		}

		scan->processPat(patSection);
		break;
	    }
	case DvbScan::PmtFilter: {
		DvbPmtSection pmtSection(data, size);

		if (!pmtSection.isValid() || (pmtSection.tableId() != 0x2)) {
			return;
		}

		if (!checkMultipleSection(pmtSection)) {
			// already read this part
			return;
		}

		scan->processPmt(pmtSection, pid);
		break;
	    }
	case DvbScan::SdtFilter: {
		DvbSdtSection sdtSection(data, size);

		if (!sdtSection.isValid() || (sdtSection.tableId() != 0x42)) {
			// there are also other tables in the SDT
			return;
		}

		if (!checkMultipleSection(sdtSection)) {
			// already read this part
			return;
		}

		scan->processSdt(sdtSection);
		break;
	    }
	case DvbScan::VctFilter: {
		AtscVctSection vctSection(data, size);

		if (!vctSection.isValid() ||
		    ((vctSection.tableId() != 0xc8) && (vctSection.tableId() != 0xc9))) {
			// there are also other tables in the VCT
			return;
		}

		if (!checkMultipleSection(vctSection)) {
			// already read this part
			return;
		}

		scan->processVct(vctSection);
		break;
	    }
	case DvbScan::NitFilter: {
		DvbNitSection nitSection(data, size);

		if (!nitSection.isValid() || (nitSection.tableId() != 0x40)) {
			// we are only interested in the current network
			return;
		}

		if (!checkMultipleSection(nitSection)) {
			// already read this part
			return;
		}

		scan->processNit(nitSection);
		break;
	    }
	}

	if (multipleSections.count(false) == 0) {
		scan->filterFinished(this);
	}
}

void DvbScanFilter::timerEvent(QTimerEvent *)
{
	Log("DvbScanFilter::timerEvent: timeout while reading section; type =") << type <<
		QLatin1String("pid =") << pid;
	scan->filterFinished(this);
}

DvbScan::DvbScan(DvbDevice *device_, const QString &source_, const DvbTransponder &transponder_) :
	device(device_), source(source_), transponder(transponder_), isLive(true), isAuto(false),
	transponderIndex(-1), state(ScanPat), patIndex(0), activeFilters(0)
{
}

DvbScan::DvbScan(DvbDevice *device_, const QString &source_,
	const QList<DvbTransponder> &transponders_) : device(device_), source(source_),
	isLive(false), isAuto(false), transponders(transponders_), transponderIndex(0),
	state(ScanTune), patIndex(0), activeFilters(0)
{
}

DvbScan::DvbScan(DvbDevice *device_, const QString &source_, const QString &autoScanSource) :
	device(device_), source(source_), isLive(false), isAuto(true), transponderIndex(0),
	state(ScanTune), patIndex(0), activeFilters(0)
{
	if ((autoScanSource == QLatin1String("AUTO-Normal")) || (autoScanSource == QLatin1String("AUTO-Offsets"))) {
		bool offsets = (autoScanSource == QLatin1String("AUTO-Offsets"));

		for (int frequency = 177500000; frequency <= 226500000; frequency += 7000000) {
			DvbTransponder currentTransponder(DvbTransponderBase::DvbT);
			DvbTTransponder *dvbTTransponder = currentTransponder.as<DvbTTransponder>();
			dvbTTransponder->frequency = frequency;
			dvbTTransponder->bandwidth = DvbTTransponder::Bandwidth7MHz;
			dvbTTransponder->modulation = DvbTTransponder::ModulationAuto;
			dvbTTransponder->fecRateHigh = DvbTTransponder::FecAuto;
			dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
			dvbTTransponder->transmissionMode = DvbTTransponder::TransmissionModeAuto;
			dvbTTransponder->guardInterval = DvbTTransponder::GuardIntervalAuto;
			dvbTTransponder->hierarchy = DvbTTransponder::HierarchyNone;
			transponders.append(currentTransponder);
		}

		for (int frequency = 474000000; frequency <= 858000000; frequency += 8000000) {
			for (int i = 0; i < 3; ++i) {
				if ((i != 0) && (!offsets)) {
					break;
				}

				int offset = 0;

				if (i == 1) {
					offset = -167000;
				} else if (i == 2) {
					offset = 167000;
				}

				DvbTransponder currentTransponder(DvbTransponderBase::DvbT);
				DvbTTransponder *dvbTTransponder =
					currentTransponder.as<DvbTTransponder>();
				dvbTTransponder->frequency = frequency + offset;
				dvbTTransponder->bandwidth = DvbTTransponder::Bandwidth8MHz;
				dvbTTransponder->modulation = DvbTTransponder::ModulationAuto;
				dvbTTransponder->fecRateHigh = DvbTTransponder::FecAuto;
				dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
				dvbTTransponder->transmissionMode =
					DvbTTransponder::TransmissionModeAuto;
				dvbTTransponder->guardInterval =
					DvbTTransponder::GuardIntervalAuto;
				dvbTTransponder->hierarchy = DvbTTransponder::HierarchyNone;
				transponders.append(currentTransponder);
			}
		}
	} else if (autoScanSource == QLatin1String("AUTO-Australia")) {
		for (int frequency = 177500000; frequency <= 226500000; frequency += 7000000) {
			for (int i = 0; i < 2; ++i) {
				int offset = 0;

				if (i == 1) {
					offset = 125000;
				}

				DvbTransponder currentTransponder(DvbTransponderBase::DvbT);
				DvbTTransponder *dvbTTransponder =
					currentTransponder.as<DvbTTransponder>();
				dvbTTransponder->frequency = frequency + offset;
				dvbTTransponder->bandwidth = DvbTTransponder::Bandwidth7MHz;
				dvbTTransponder->modulation = DvbTTransponder::ModulationAuto;
				dvbTTransponder->fecRateHigh = DvbTTransponder::FecAuto;
				dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
				dvbTTransponder->transmissionMode =
					DvbTTransponder::TransmissionModeAuto;
				dvbTTransponder->guardInterval =
					DvbTTransponder::GuardIntervalAuto;
				dvbTTransponder->hierarchy = DvbTTransponder::HierarchyNone;
				transponders.append(currentTransponder);
			}
		}

		for (int frequency = 529500000; frequency <= 816500000; frequency += 7000000) {
			for (int i = 0; i < 2; ++i) {
				int offset = 0;

				if (i == 1) {
					offset = 125000;
				}

				DvbTransponder currentTransponder(DvbTransponderBase::DvbT);
				DvbTTransponder *dvbTTransponder =
					currentTransponder.as<DvbTTransponder>();
				dvbTTransponder->frequency = frequency + offset;
				dvbTTransponder->bandwidth = DvbTTransponder::Bandwidth7MHz;
				dvbTTransponder->modulation = DvbTTransponder::ModulationAuto;
				dvbTTransponder->fecRateHigh = DvbTTransponder::FecAuto;
				dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
				dvbTTransponder->transmissionMode =
					DvbTTransponder::TransmissionModeAuto;
				dvbTTransponder->guardInterval =
					DvbTTransponder::GuardIntervalAuto;
				dvbTTransponder->hierarchy = DvbTTransponder::HierarchyNone;
				transponders.append(currentTransponder);
			}
		}
	} else if (autoScanSource == QLatin1String("AUTO-Italy")) {
		static const int italyVhf[] = { 177500000, 186000000, 194500000, 203500000,
						212500000, 219500000, 226500000 };

		for (unsigned i = 0; i < (sizeof(italyVhf) / sizeof(italyVhf[0])); ++i) {
			for (int j = 0; j < 2; ++j) {
				DvbTransponder currentTransponder(DvbTransponderBase::DvbT);
				DvbTTransponder *dvbTTransponder =
					currentTransponder.as<DvbTTransponder>();
				dvbTTransponder->frequency = italyVhf[i];
				dvbTTransponder->bandwidth = ((j == 0) ?
					DvbTTransponder::Bandwidth7MHz :
					DvbTTransponder::Bandwidth8MHz);
				dvbTTransponder->modulation = DvbTTransponder::ModulationAuto;
				dvbTTransponder->fecRateHigh = DvbTTransponder::FecAuto;
				dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
				dvbTTransponder->transmissionMode =
					DvbTTransponder::TransmissionModeAuto;
				dvbTTransponder->guardInterval =
					DvbTTransponder::GuardIntervalAuto;
				dvbTTransponder->hierarchy = DvbTTransponder::HierarchyNone;
				transponders.append(currentTransponder);
			}
		}

		for (int frequency = 474000000; frequency <= 858000000; frequency += 8000000) {
			DvbTransponder currentTransponder(DvbTransponderBase::DvbT);
			DvbTTransponder *dvbTTransponder =
				currentTransponder.as<DvbTTransponder>();
			dvbTTransponder->frequency = frequency;
			dvbTTransponder->bandwidth = DvbTTransponder::Bandwidth8MHz;
			dvbTTransponder->modulation = DvbTTransponder::ModulationAuto;
			dvbTTransponder->fecRateHigh = DvbTTransponder::FecAuto;
			dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
			dvbTTransponder->transmissionMode = DvbTTransponder::TransmissionModeAuto;
			dvbTTransponder->guardInterval = DvbTTransponder::GuardIntervalAuto;
			dvbTTransponder->hierarchy = DvbTTransponder::HierarchyNone;
			transponders.append(currentTransponder);
		}
	} else if (autoScanSource == QLatin1String("AUTO-Taiwan")) {
		for (int frequency = 527000000; frequency <= 599000000; frequency += 6000000) {
			DvbTransponder currentTransponder(DvbTransponderBase::DvbT);
			DvbTTransponder *dvbTTransponder =
				currentTransponder.as<DvbTTransponder>();
			dvbTTransponder->frequency = frequency;
			dvbTTransponder->bandwidth = DvbTTransponder::Bandwidth6MHz;
			dvbTTransponder->modulation = DvbTTransponder::ModulationAuto;
			dvbTTransponder->fecRateHigh = DvbTTransponder::FecAuto;
			dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
			dvbTTransponder->transmissionMode = DvbTTransponder::TransmissionModeAuto;
			dvbTTransponder->guardInterval = DvbTTransponder::GuardIntervalAuto;
			dvbTTransponder->hierarchy = DvbTTransponder::HierarchyNone;
			transponders.append(currentTransponder);
		}
	}
}

DvbScan::~DvbScan()
{
	qDeleteAll(filters);
}

void DvbScan::start()
{
	connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
	updateState();
}

void DvbScan::deviceStateChanged()
{
	if (device->getDeviceState() == DvbDevice::DeviceReleased) {
		emit scanFinished();
		return;
	}

	if (state == ScanTuning) {
		updateState();
	}
}

bool DvbScan::startFilter(int pid, FilterType type)
{
	if (activeFilters != filters.size()) {
		foreach (DvbScanFilter *filter, filters) {
			if (!filter->isActive()) {
				if (!filter->startFilter(pid, type)) {
					return false;
				}

				++activeFilters;
				return true;
			}
		}

		Q_ASSERT(false);
	} else if (activeFilters < 10) {
		DvbScanFilter *filter = new DvbScanFilter(this);

		if (!filter->startFilter(pid, type)) {
			delete filter;
			return false;
		}

		filters.append(filter);
		++activeFilters;
		return true;
	}

	return false;
}

void DvbScan::updateState()
{
	while (true) {
		switch (state) {
		case ScanPat: {
			if (!startFilter(0x0, PatFilter)) {
				return;
			}

			snr = device->getSnr();

			state = ScanNit;
		    }
			// fall through
		case ScanNit: {
			if (!isLive && !isAuto &&
			    (transponder.getTransmissionType() != DvbTransponderBase::Atsc)) {
				if (!startFilter(0x10, NitFilter)) {
					return;
				}
			}

			state = ScanSdt;
		    }
			// fall through
		case ScanSdt: {
			if (transponder.getTransmissionType() != DvbTransponderBase::Atsc) {
				if (!startFilter(0x11, SdtFilter)) {
					return;
				}
			} else {
				if (!startFilter(0x1ffb, VctFilter)) {
					return;
				}
			}

			state = ScanPmt;
		    }
			// fall through
		case ScanPmt: {
			while (patIndex < patEntries.size()) {
				if (!startFilter(patEntries.at(patIndex).pid, PmtFilter)) {
					return;
				}

				++patIndex;
			}

			if (activeFilters != 0) {
				return;
			}

			for (int i = 0; i < channels.size(); ++i) {
				DvbPreviewChannel &channel = channels[i];

				foreach (const DvbSdtEntry &sdtEntry, sdtEntries) {
					if (channel.serviceId == sdtEntry.serviceId) {
						channel.name = sdtEntry.name;
						channel.networkId = sdtEntry.networkId;
						channel.isScrambled = sdtEntry.scrambled;
						channel.provider = sdtEntry.provider;
						break;
					}
				}

				if (channel.name.isEmpty()) {
					channel.name = QString(QLatin1String("#0 %1:%2")).
						       arg(channel.transportStreamId).
						       arg(channel.serviceId);
				}
			}

			if (!channels.isEmpty()) {
				emit foundChannels(channels);
			}

			if (isLive) {
				emit scanFinished();
				return;
			}

			patEntries.clear();
			patIndex = 0;
			sdtEntries.clear();
			channels.clear();

			state = ScanTune;
		    }
			// fall through
		case ScanTune: {
			if (transponders.size() > 0) {
				emit scanProgress((100 * transponderIndex) / transponders.size());
			}

			if (transponderIndex >= transponders.size()) {
				emit scanFinished();
				return;
			}

			transponder = transponders.at(transponderIndex);
			++transponderIndex;

			state = ScanTuning;

			if (!isAuto) {
				device->tune(transponder);
			} else {
				device->autoTune(transponder);
			}

			return;
		    }

		case ScanTuning: {
			switch (device->getDeviceState()) {
			case DvbDevice::DeviceIdle:
				state = ScanTune;
				break;

			case DvbDevice::DeviceTuned:
				if (isAuto) {
					transponders[transponderIndex - 1] =
						device->getAutoTransponder();
				}

				state = ScanPat;
				break;

			default:
				return;
			}

			break;
		    }
		}
	}
}

void DvbScan::processPat(const DvbPatSection &section)
{
	transportStreamId = section.transportStreamId();

	for (DvbPatSectionEntry entry = section.entries(); entry.isValid(); entry.advance()) {
		if (entry.programNumber() != 0x0) {
			// skip 0x0 which has a special meaning
			patEntries.append(DvbPatEntry(entry.programNumber(), entry.pid()));
		}
	}
}

void DvbScan::processPmt(const DvbPmtSection &section, int pid)
{
	DvbPreviewChannel channel;

	DvbPmtParser parser(section);
	channel.hasVideo = (parser.videoPid >= 0);

	if (!parser.audioPids.isEmpty()) {
		channel.audioPid = parser.audioPids.at(0).first;
	}

	if (channel.hasVideo || (channel.audioPid >= 0)) {
		channel.source = source;
		channel.transponder = transponder;
		channel.transportStreamId = transportStreamId;
		channel.pmtPid = pid;
		channel.pmtSectionData = section.toByteArray();
		channel.serviceId = section.programNumber();
		channel.snr = snr;
		channels.append(channel);
	}
}

void DvbScan::processSdt(const DvbSdtSection &section)
{
	for (DvbSdtSectionEntry entry = section.entries(); entry.isValid(); entry.advance()) {
		DvbSdtEntry sdtEntry(entry.serviceId(), section.originalNetworkId(),
				     entry.isScrambled());

		for (DvbDescriptor descriptor = entry.descriptors(); descriptor.isValid();
		     descriptor.advance()) {
			if (descriptor.descriptorTag() != 0x48) {
				continue;
			}

			DvbServiceDescriptor serviceDescriptor(descriptor);

			if (!serviceDescriptor.isValid()) {
				continue;
			}

			sdtEntry.name = serviceDescriptor.serviceName();
			sdtEntry.provider = serviceDescriptor.providerName();
			break;
		}

		sdtEntries.append(sdtEntry);
	}
}

void DvbScan::processVct(const AtscVctSection &section)
{
	int i = section.entryCount();

	for (AtscVctSectionEntry entry = section.entries(); (i > 0) && (entry.isValid());
	     --i, entry.advance()) {
		QString majorminor = QString(QLatin1String("%1-%2 ")).
			arg(entry.majorNumber(), 3, 10, QLatin1Char('0')).arg(entry.minorNumber());

		DvbSdtEntry sdtEntry(entry.programNumber(), entry.sourceId(), entry.isScrambled());

		// Each VCT section has it's own list of descriptors
		// See A/65C table 6.25a for the list of descriptors
		for (DvbDescriptor descriptor = entry.descriptors(); descriptor.isValid();
		     descriptor.advance()) {
			if (descriptor.descriptorTag() != 0xa0) {
				continue;
			}

			// Extended Channel Name Descriptor
			AtscChannelNameDescriptor nameDescriptor(descriptor);
			if (!nameDescriptor.isValid()) {
				continue;
			}
			sdtEntry.name = majorminor + nameDescriptor.name();
		}

		if (sdtEntry.name.isEmpty()) {
			// Extended Channel name not available, fall back
			// to the short name
			QChar shortName[] = { entry.shortName1(),
					      entry.shortName2(),
					      entry.shortName3(),
					      entry.shortName4(),
					      entry.shortName5(),
					      entry.shortName6(),
					      entry.shortName7(), 0 };
			int nameLength = 0;
			while (shortName[nameLength] != 0) {
				++nameLength;
			}
			sdtEntry.name = majorminor + QString(shortName, nameLength);
		}
		sdtEntries.append(sdtEntry);
	}
}

void DvbScan::processNit(const DvbNitSection &section)
{
	for (DvbNitSectionEntry entry = section.entries(); entry.isValid(); entry.advance()) {
		for (DvbDescriptor descriptor = entry.descriptors(); descriptor.isValid();
		     descriptor.advance()) {
			processNitDescriptor(descriptor);
		}
	}
}

static DvbCTransponder::Modulation extractDvbCModulation(const DvbCableDescriptor &descriptor)
{
	switch (descriptor.modulation()) {
	case 1:
		return DvbCTransponder::Qam16;
	case 2:
		return DvbCTransponder::Qam32;
	case 3:
		return DvbCTransponder::Qam64;
	case 4:
		return DvbCTransponder::Qam128;
	case 5:
		return DvbCTransponder::Qam256;
	default:
		return DvbCTransponder::ModulationAuto;
	}
}

static DvbCTransponder::FecRate extractDvbCFecRate(const DvbCableDescriptor &descriptor)
{
	switch (descriptor.fecRate()) {
	case 1:
		return DvbTransponderBase::Fec1_2;
	case 2:
		return DvbTransponderBase::Fec2_3;
	case 3:
		return DvbTransponderBase::Fec3_4;
	case 4:
		return DvbTransponderBase::Fec5_6;
	case 5:
		return DvbTransponderBase::Fec7_8;
	case 6:
		return DvbTransponderBase::Fec8_9;
	case 7:
		return DvbTransponderBase::Fec3_5;
	case 8:
		return DvbTransponderBase::Fec4_5;
	case 9:
		return DvbTransponderBase::Fec9_10;
	case 15:
		return DvbTransponderBase::FecNone;
	default:
		return DvbTransponderBase::FecAuto;
	}
}

static DvbSTransponder::Polarization extractDvbSPolarization(
	const DvbSatelliteDescriptor &descriptor)
{
	switch (descriptor.polarization()) {
	case 0:
		return DvbSTransponder::Horizontal;
	case 1:
		return DvbSTransponder::Vertical;
	case 2:
		return DvbSTransponder::CircularLeft;
	default:
		return DvbSTransponder::CircularRight;
	}
}

static DvbSTransponder::FecRate extractDvbSFecRate(const DvbSatelliteDescriptor &descriptor)
{
	switch (descriptor.fecRate()) {
	case 1:
		return DvbTransponderBase::Fec1_2;
	case 2:
		return DvbTransponderBase::Fec2_3;
	case 3:
		return DvbTransponderBase::Fec3_4;
	case 4:
		return DvbTransponderBase::Fec5_6;
	case 5:
		return DvbTransponderBase::Fec7_8;
	case 6:
		return DvbTransponderBase::Fec8_9;
	case 7:
		return DvbTransponderBase::Fec3_5;
	case 8:
		return DvbTransponderBase::Fec4_5;
	case 9:
		return DvbTransponderBase::Fec9_10;
	default:
		return DvbTransponderBase::FecAuto;
	}
}

static DvbS2Transponder::Modulation extractDvbS2Modulation(
	const DvbSatelliteDescriptor &descriptor)
{
	switch (descriptor.modulation()) {
	case 1:
		return DvbS2Transponder::Qpsk;
	case 2:
		return DvbS2Transponder::Psk8;
	default:
		return DvbS2Transponder::ModulationAuto;
	}
}

static DvbS2Transponder::RollOff extractDvbS2RollOff(const DvbSatelliteDescriptor &descriptor)
{
	switch (descriptor.rollOff()) {
	case 0:
		return DvbS2Transponder::RollOff35;
	case 1:
		return DvbS2Transponder::RollOff25;
	case 2:
		return DvbS2Transponder::RollOff20;
	default:
		return DvbS2Transponder::RollOffAuto;
	}
}

static DvbTTransponder::Bandwidth extractDvbTBandwidth(const DvbTerrestrialDescriptor &descriptor)
{
	switch (descriptor.bandwidth()) {
	case 0:
		return DvbTTransponder::Bandwidth8MHz;
	case 1:
		return DvbTTransponder::Bandwidth7MHz;
	case 2:
		return DvbTTransponder::Bandwidth6MHz;
	default:
		// this includes 5 MHz
		return DvbTTransponder::BandwidthAuto;
	}
}

static DvbTTransponder::Modulation extractDvbTModulation(
	const DvbTerrestrialDescriptor &descriptor)
{
	switch (descriptor.constellation()) {
	case 0:
		return DvbTTransponder::Qpsk;
	case 1:
		return DvbTTransponder::Qam16;
	case 2:
		return DvbTTransponder::Qam64;
	default:
		return DvbTTransponder::ModulationAuto;
	}
}

static DvbTTransponder::FecRate extractDvbTFecRateHigh(const DvbTerrestrialDescriptor &descriptor)
{
	switch (descriptor.fecRateHigh()) {
	case 0:
		return DvbTTransponder::Fec1_2;
	case 1:
		return DvbTTransponder::Fec2_3;
	case 2:
		return DvbTTransponder::Fec3_4;
	case 3:
		return DvbTTransponder::Fec5_6;
	case 4:
		return DvbTTransponder::Fec7_8;
	default:
		return DvbTTransponder::FecAuto;
	}
}

static DvbTTransponder::FecRate extractDvbTFecRateLow(const DvbTerrestrialDescriptor &descriptor)
{
	switch (descriptor.fecRateLow()) {
	case 0:
		return DvbTTransponder::Fec1_2;
	case 1:
		return DvbTTransponder::Fec2_3;
	case 2:
		return DvbTTransponder::Fec3_4;
	case 3:
		return DvbTTransponder::Fec5_6;
	case 4:
		return DvbTTransponder::Fec7_8;
	default:
		return DvbTTransponder::FecAuto;
	}
}

static DvbTTransponder::TransmissionMode extractDvbTTransmissionMode(
	const DvbTerrestrialDescriptor &descriptor)
{
	switch (descriptor.transmissionMode()) {
	case 0:
		return DvbTTransponder::TransmissionMode2k;
	case 1:
		return DvbTTransponder::TransmissionMode8k;
	case 2:
		return DvbTTransponder::TransmissionMode4k;
	default:
		return DvbTTransponder::TransmissionModeAuto;
	}
}

static DvbTTransponder::GuardInterval extractDvbTGuardInterval(
	const DvbTerrestrialDescriptor &descriptor)
{
	switch (descriptor.guardInterval()) {
	case 0:
		return DvbTTransponder::GuardInterval1_32;
	case 1:
		return DvbTTransponder::GuardInterval1_16;
	case 2:
		return DvbTTransponder::GuardInterval1_8;
	default:
		return DvbTTransponder::GuardInterval1_4;
	}
}

static DvbTTransponder::Hierarchy extractDvbTHierarchy(const DvbTerrestrialDescriptor &descriptor)
{
	switch (descriptor.hierarchy() & 0x3) {
	case 0:
		return DvbTTransponder::HierarchyNone;
	case 1:
		return DvbTTransponder::Hierarchy1;
	case 2:
		return DvbTTransponder::Hierarchy2;
	default:
		return DvbTTransponder::Hierarchy4;
	}
}

void DvbScan::processNitDescriptor(const DvbDescriptor &descriptor)
{
	DvbTransponder newTransponder;

	switch (transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC: {
		if (descriptor.descriptorTag() != 0x44) {
			break;
		}

		DvbCableDescriptor cableDescriptor(descriptor);

		if (!cableDescriptor.isValid()) {
			break;
		}

		newTransponder = DvbTransponder(DvbTransponderBase::DvbC);
		DvbCTransponder *dvbCTransponder = newTransponder.as<DvbCTransponder>();
		dvbCTransponder->frequency =
			DvbDescriptor::bcdToInt(cableDescriptor.frequency(), 100);
		dvbCTransponder->symbolRate =
			DvbDescriptor::bcdToInt(cableDescriptor.symbolRate(), 100);
		dvbCTransponder->modulation = extractDvbCModulation(cableDescriptor);
		dvbCTransponder->fecRate = extractDvbCFecRate(cableDescriptor);
		break;
	    }
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2: {
		if (descriptor.descriptorTag() != 0x43) {
			break;
		}

		DvbSatelliteDescriptor satelliteDescriptor(descriptor);

		if (!satelliteDescriptor.isValid()) {
			break;
		}

		DvbSTransponder *dvbSTransponder;

		if (!satelliteDescriptor.isDvbS2()) {
			newTransponder = DvbTransponder(DvbTransponderBase::DvbS);
			dvbSTransponder = newTransponder.as<DvbSTransponder>();
		} else {
			if ((device->getTransmissionTypes() & DvbDevice::DvbS2) == 0) {
				break;
			}

			newTransponder = DvbTransponder(DvbTransponderBase::DvbS2);
			DvbS2Transponder *dvbS2Transponder = newTransponder.as<DvbS2Transponder>();
			dvbS2Transponder->modulation = extractDvbS2Modulation(satelliteDescriptor);
			dvbS2Transponder->rollOff = extractDvbS2RollOff(satelliteDescriptor);
			dvbSTransponder = dvbS2Transponder;
		}

		dvbSTransponder->frequency =
			DvbDescriptor::bcdToInt(satelliteDescriptor.frequency(), 10);
		dvbSTransponder->polarization = extractDvbSPolarization(satelliteDescriptor);
		dvbSTransponder->symbolRate =
			DvbDescriptor::bcdToInt(satelliteDescriptor.symbolRate(), 100);
		dvbSTransponder->fecRate = extractDvbSFecRate(satelliteDescriptor);
		break;
	    }
	case DvbTransponderBase::DvbT: {
		if (descriptor.descriptorTag() != 0x5a) {
			break;
		}

		DvbTerrestrialDescriptor terrestrialDescriptor(descriptor);

		if (!terrestrialDescriptor.isValid()) {
			break;
		}

		newTransponder = DvbTransponder(DvbTransponderBase::DvbT);
		DvbTTransponder *dvbTTransponder = newTransponder.as<DvbTTransponder>();
		dvbTTransponder->frequency = terrestrialDescriptor.frequency() * 10;
		dvbTTransponder->bandwidth = extractDvbTBandwidth(terrestrialDescriptor);
		dvbTTransponder->modulation = extractDvbTModulation(terrestrialDescriptor);
		dvbTTransponder->fecRateHigh = extractDvbTFecRateHigh(terrestrialDescriptor);
		dvbTTransponder->fecRateLow = extractDvbTFecRateLow(terrestrialDescriptor);
		dvbTTransponder->transmissionMode =
			extractDvbTTransmissionMode(terrestrialDescriptor);
		dvbTTransponder->guardInterval = extractDvbTGuardInterval(terrestrialDescriptor);
		dvbTTransponder->hierarchy = extractDvbTHierarchy(terrestrialDescriptor);

		if (dvbTTransponder->hierarchy == DvbTTransponder::HierarchyNone) {
			dvbTTransponder->fecRateLow = DvbTTransponder::FecNone;
		}

		break;
	    }
	case DvbTransponderBase::Atsc:
		break;
	}

	if (newTransponder.isValid()) {
		bool duplicate = false;

		foreach (const DvbTransponder &existingTransponder, transponders) {
			if (existingTransponder.corresponds(newTransponder)) {
				duplicate = true;
				break;
			}
		}

		if (!duplicate) {
			transponders.append(newTransponder);
		}
	}
}

void DvbScan::filterFinished(DvbScanFilter *filter)
{
	filter->stopFilter();
	--activeFilters;
	updateState();
}
