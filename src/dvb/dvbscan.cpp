/*
 * dvbscan.cpp
 *
 * Copyright (C) 2008 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbscan.h"

#include <QBitArray>
#include <KDebug>
#include "dvbsi.h"

class DvbPatEntry
{
public:
	DvbPatEntry(int programNumber_, int pid_) : programNumber(programNumber_), pid(pid_) { }
	~DvbPatEntry() { }

	int programNumber;
	int pid;
};

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
	void processSection(const DvbSectionData &data);
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

	resetFilter();

	pid = pid_;
	type = type_;
	multipleSections.clear();

	if (!scan->device->addPidFilter(pid, this)) {
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
		scan->device->removePidFilter(pid, this);

		pid = -1;
	}
}

bool DvbScanFilter::checkMultipleSection(const DvbStandardSection &section)
{
	int sectionCount = section.lastSectionNumber() + 1;

	if (section.sectionNumber() >= sectionCount) {
		kDebug() << "current > last";
		sectionCount = section.sectionNumber() + 1;
	}

	if (multipleSections.isEmpty()) {
		multipleSections.resize(sectionCount);
	} else {
		if (multipleSections.size() != sectionCount) {
			kDebug() << "inconsistent number of sections";

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

void DvbScanFilter::processSection(const DvbSectionData &data)
{
	DvbSection section(data);

	if (!section.isValid()) {
		return;
	}

	switch (type) {
	case DvbScan::PatFilter: {
		if (section.tableId() != 0x0) {
			return;
		}

		DvbPatSection patSection(section);

		if (!patSection.isValid()) {
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
		if (section.tableId() != 0x2) {
			return;
		}

		DvbPmtSection pmtSection(section);

		if (!pmtSection.isValid()) {
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
		if (section.tableId() != 0x42) {
			// there are also other tables in the SDT
			return;
		}

		DvbSdtSection sdtSection(section);

		if (!sdtSection.isValid()) {
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
		if ((section.tableId() != 0xc8) && (section.tableId() != 0xc9)) {
			// there are also other tables in the VCT
			return;
		}

		AtscVctSection vctSection(section);

		if (!vctSection.isValid()) {
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
		if (section.tableId() != 0x40) {
			// we are only interested in the current network
			return;
		}

		DvbNitSection nitSection(section);

		if (!nitSection.isValid()) {
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
	kWarning() << "timeout while reading section; type =" << type << "pid =" << pid;
	scan->filterFinished(this);
}

DvbScan::DvbScan(const QString &source_, DvbDevice *device_, const DvbTransponder &transponder_) :
	source(source_), device(device_), transponder(transponder_), isLive(true),
	transponderIndex(-1), state(ScanPat), patIndex(0), activeFilters(0)
{
	init();
}

DvbScan::DvbScan(const QString &source_, DvbDevice *device_,
	const QList<DvbTransponder> &transponders_) : source(source_), device(device_),
	isLive(false), transponders(transponders_), transponderIndex(0), state(ScanTune),
	patIndex(0), activeFilters(0)
{
	init();
}

void DvbScan::init()
{
	connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));

	updateState();
}

DvbScan::~DvbScan()
{
	qDeleteAll(filters);
}

void DvbScan::deviceStateChanged()
{
	if (device->getDeviceState() == DvbDevice::DeviceNotReady) {
		emit scanFinished();
	} else if (state == ScanTune) {
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
			if ((!isLive) &&
			    (transponder->getTransmissionType() != DvbTransponderBase::Atsc)) {
				if (!startFilter(0x10, NitFilter)) {
					return;
				}
			}

			state = ScanSdt;
		    }
			// fall through
		case ScanSdt: {
			if (transponder->getTransmissionType() != DvbTransponderBase::Atsc) {
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
						channel.networkId = sdtEntry.networkId;
						channel.scrambled = sdtEntry.scrambled;
						channel.name = sdtEntry.name;
						channel.provider = sdtEntry.provider;
						break;
					}
				}

				if (channel.name.isEmpty()) {
					channel.name = QString("[%1:%2]").
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

			device->stop();

			state = ScanTune;
		    }
			// fall through
		case ScanTune: {
			switch (device->getDeviceState()) {
			case DvbDevice::DeviceIdle:
			case DvbDevice::DeviceTuningFailed: {
				emit scanProgress((100 * transponderIndex) / transponders.size());

				if (transponderIndex >= transponders.size()) {
					emit scanFinished();
					return;
				}

				transponder = transponders.at(transponderIndex);
				++transponderIndex;

				device->tuneDevice(transponder);
				break;
			    }

			case DvbDevice::DeviceTuned: {
				state = ScanPat;
				break;
			    }

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

	for (DvbPmtSectionEntry entry = section.entries(); entry.isValid(); entry.advance()) {
		switch (entry.streamType()) {
		case 0x01:   // MPEG1 video
		case 0x02:   // MPEG2 video
		case 0x10:   // MPEG4 video
		case 0x1b: { // H264 video
			channel.videoPid = entry.pid();
			break;
		    }

		case 0x03:   // MPEG1 audio
		case 0x04:   // MPEG2 audio
		case 0x0f:   // AAC audio
		case 0x11:   // AAC / LATM audio
		case 0x81:   // AC-3 audio (ATSC specific)
		case 0x87: { // enhanced AC-3 audio (ATSC specific)
			channel.audioPids.append(entry.pid());
			break;
		    }
		}
	}

	if ((channel.videoPid != -1) || !channel.audioPids.isEmpty()) {
		channel.source = source;
		channel.transportStreamId = transportStreamId;
		channel.serviceId = section.programNumber();
		channel.pmtPid = pid;
		channel.transponder = transponder;
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
		QString majorminor = QString("%1-%2 ").arg(entry.majorNumber(), 3, 10, QLatin1Char('0')).arg(entry.minorNumber());

		DvbSdtEntry sdtEntry(entry.programNumber(), -1, entry.isScrambled());

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
		for (DvbDescriptor descriptor = entry.descriptors();  descriptor.isValid();
		     descriptor.advance()) {
			bool found = false;

			switch (transponder->getTransmissionType()) {
			case DvbTransponderBase::DvbC: {
				if (descriptor.descriptorTag() != 0x44) {
					break;
				}

				found = true;

				DvbCableDescriptor cabDescriptor(descriptor);

				if (!cabDescriptor.isValid()) {
					break;
				}

				DvbCTransponder *transponder = new DvbCTransponder();

				transponder->frequency =
					DvbDescriptor::bcdToInt(cabDescriptor.frequency(), 100);

				transponder->symbolRate =
					DvbDescriptor::bcdToInt(cabDescriptor.symbolRate(), 100);

				switch (cabDescriptor.modulation()) {
				case 1:
					transponder->modulation = DvbCTransponder::Qam16;
					break;
				case 2:
					transponder->modulation = DvbCTransponder::Qam32;
					break;
				case 3:
					transponder->modulation = DvbCTransponder::Qam64;
					break;
				case 4:
					transponder->modulation = DvbCTransponder::Qam128;
					break;
				case 5:
					transponder->modulation = DvbCTransponder::Qam256;
					break;
				default:
					transponder->modulation = DvbCTransponder::ModulationAuto;
					break;
				}

				switch (cabDescriptor.fecRate()) {
				case 1:
					transponder->fecRate = DvbTransponderBase::Fec1_2;
					break;
				case 2:
					transponder->fecRate = DvbTransponderBase::Fec2_3;
					break;
				case 3:
					transponder->fecRate = DvbTransponderBase::Fec3_4;
					break;
				case 4:
					transponder->fecRate = DvbTransponderBase::Fec5_6;
					break;
				case 5:
					transponder->fecRate = DvbTransponderBase::Fec7_8;
					break;
				case 6:
					transponder->fecRate = DvbTransponderBase::Fec8_9;
					break;
				case 8:
					transponder->fecRate = DvbTransponderBase::Fec4_5;
					break;
				case 15:
					transponder->fecRate = DvbTransponderBase::FecNone;
					break;
				default:
					// this includes rates like 3/5 and 9/10
					transponder->fecRate = DvbTransponderBase::FecAuto;
				}

				for (int i = 0;; ++i) {
					if (i == transponders.size()) {
						transponders.append(DvbTransponder(transponder));
						break;
					}

					const DvbCTransponder *it =
						transponders.at(i)->getDvbCTransponder();

					if (it == NULL) {
						continue;
					}

					if (abs(it->frequency - transponder->frequency) < 2000000) {
						delete transponder;
						break;
					}
				}

				break;
			    }

			case DvbTransponderBase::DvbS: {
				if (descriptor.descriptorTag() != 0x43) {
					break;
				}

				found = true;

				DvbSatelliteDescriptor satDescriptor(descriptor);

				if (!satDescriptor.isValid()) {
					break;
				}

				if (satDescriptor.isDvbS2()) {
					// ignore DVB-S2 descriptor
					break;
				}

				DvbSTransponder *transponder = new DvbSTransponder();

				transponder->frequency =
					DvbDescriptor::bcdToInt(satDescriptor.frequency(), 10);

				switch (satDescriptor.polarization()) {
				case 0:
					transponder->polarization = DvbSTransponder::Horizontal;
					break;
				case 1:
					transponder->polarization = DvbSTransponder::Vertical;
					break;
				case 2:
					transponder->polarization = DvbSTransponder::CircularLeft;
					break;
				default:
					transponder->polarization = DvbSTransponder::CircularRight;
					break;
				}

				transponder->symbolRate =
					DvbDescriptor::bcdToInt(satDescriptor.symbolRate(), 100);

				switch (satDescriptor.fecRate()) {
				case 1:
					transponder->fecRate = DvbTransponderBase::Fec1_2;
					break;
				case 2:
					transponder->fecRate = DvbTransponderBase::Fec2_3;
					break;
				case 3:
					transponder->fecRate = DvbTransponderBase::Fec3_4;
					break;
				case 4:
					transponder->fecRate = DvbTransponderBase::Fec5_6;
					break;
				case 5:
					transponder->fecRate = DvbTransponderBase::Fec7_8;
					break;
				case 6:
					transponder->fecRate = DvbTransponderBase::Fec8_9;
					break;
				case 8:
					transponder->fecRate = DvbTransponderBase::Fec4_5;
					break;
				default:
					// this includes rates like 3/5 and 9/10
					transponder->fecRate = DvbTransponderBase::FecAuto;
				}

				for (int i = 0;; ++i) {
					if (i == transponders.size()) {
						transponders.append(DvbTransponder(transponder));
						break;
					}

					const DvbSTransponder *it =
						transponders.at(i)->getDvbSTransponder();

					if (it == NULL) {
						continue;
					}

					if ((abs(it->frequency - transponder->frequency) < 2000) &&
					    (it->polarization == transponder->polarization)) {
						delete transponder;
						break;
					}
				}

				break;
			    }

			case DvbTransponderBase::DvbT: {
				if (descriptor.descriptorTag() != 0x5a) {
					break;
				}

				found = true;

				DvbTerrestrialDescriptor terDescriptor(descriptor);

				if (!terDescriptor.isValid()) {
					break;
				}

				DvbTTransponder *transponder = new DvbTTransponder();

				transponder->frequency = terDescriptor.frequency() * 10;

				switch (terDescriptor.bandwidth()) {
				case 0:
					transponder->bandwidth = DvbTTransponder::Bandwidth8Mhz;
					break;
				case 1:
					transponder->bandwidth = DvbTTransponder::Bandwidth7Mhz;
					break;
				case 2:
					transponder->bandwidth = DvbTTransponder::Bandwidth6Mhz;
					break;
				default:
					// this includes 5 MHz
					transponder->bandwidth = DvbTTransponder::BandwidthAuto;
					break;
				}

				switch (terDescriptor.constellation()) {
				case 0:
					transponder->modulation = DvbTTransponder::Qpsk;
					break;
				case 1:
					transponder->modulation = DvbTTransponder::Qam16;
					break;
				case 2:
					transponder->modulation = DvbTTransponder::Qam64;
					break;
				default:
					transponder->modulation = DvbTTransponder::ModulationAuto;
					break;
				}

				switch (terDescriptor.fecRateHigh()) {
				case 0:
					transponder->fecRateHigh = DvbTTransponder::Fec1_2;
					break;
				case 1:
					transponder->fecRateHigh = DvbTTransponder::Fec2_3;
					break;
				case 2:
					transponder->fecRateHigh = DvbTTransponder::Fec3_4;
					break;
				case 3:
					transponder->fecRateHigh = DvbTTransponder::Fec5_6;
					break;
				case 4:
					transponder->fecRateHigh = DvbTTransponder::Fec7_8;
					break;
				default:
					transponder->fecRateHigh = DvbTTransponder::FecAuto;
					break;
				}

				switch (terDescriptor.fecRateLow()) {
				case 0:
					transponder->fecRateLow = DvbTTransponder::Fec1_2;
					break;
				case 1:
					transponder->fecRateLow = DvbTTransponder::Fec2_3;
					break;
				case 2:
					transponder->fecRateLow = DvbTTransponder::Fec3_4;
					break;
				case 3:
					transponder->fecRateLow = DvbTTransponder::Fec5_6;
					break;
				case 4:
					transponder->fecRateLow = DvbTTransponder::Fec7_8;
					break;
				default:
					transponder->fecRateLow = DvbTTransponder::FecAuto;
					break;
				}

				switch (terDescriptor.transmissionMode()) {
				case 0:
					transponder->transmissionMode =
						DvbTTransponder::TransmissionMode2k;
					break;
				case 1:
					transponder->transmissionMode =
						DvbTTransponder::TransmissionMode8k;
					break;
				default:
					// this includes 4k
					transponder->transmissionMode =
						DvbTTransponder::TransmissionModeAuto;
					break;
				}

				switch (terDescriptor.guardInterval()) {
				case 0:
					transponder->guardInterval =
						DvbTTransponder::GuardInterval1_32;
					break;
				case 1:
					transponder->guardInterval =
						DvbTTransponder::GuardInterval1_16;
					break;
				case 2:
					transponder->guardInterval =
						DvbTTransponder::GuardInterval1_8;
					break;
				default:
					transponder->guardInterval =
						DvbTTransponder::GuardInterval1_4;
					break;
				}

				switch (terDescriptor.hierarchy() & 0x3) {
				case 0:
					transponder->fecRateLow = DvbTTransponder::FecNone;
					transponder->hierarchy = DvbTTransponder::HierarchyNone;
					break;
				case 1:
					transponder->hierarchy = DvbTTransponder::Hierarchy1;
					break;
				case 2:
					transponder->hierarchy = DvbTTransponder::Hierarchy2;
					break;
				default:
					transponder->hierarchy = DvbTTransponder::Hierarchy4;
					break;
				}

				for (int i = 0;; ++i) {
					if (i == transponders.size()) {
						transponders.append(DvbTransponder(transponder));
						break;
					}

					const DvbTTransponder *it =
						transponders.at(i)->getDvbTTransponder();

					if (it == NULL) {
						continue;
					}

					if (abs(it->frequency - transponder->frequency) < 2000000) {
						delete transponder;
						break;
					}
				}

				break;
			    }

			default:
				break;
			}

			if (found) {
				break;
			}
		}
	}
}

void DvbScan::filterFinished(DvbScanFilter *filter)
{
	filter->stopFilter();
	--activeFilters;
	updateState();
}
