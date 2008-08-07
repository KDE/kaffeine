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

#include <KDebug>

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
	DvbSdtEntry(int serviceId_, int networkId_, int transportStreamId_, bool scrambled_) :
		serviceId(serviceId_), networkId(networkId_), transportStreamId(transportStreamId_),
		scrambled(scrambled_) { }
	~DvbSdtEntry() { }

	int serviceId;
	int networkId;
	int transportStreamId;
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

	int getPid() const
	{
		return pid;
	}

	DvbScan::FilterType getType() const
	{
		return type;
	}

	bool startFilter(int pid_, DvbScan::FilterType type_)
	{
		Q_ASSERT(pid == -1);

		pid = pid_;
		type = type_;

		resetFilter();

		if (!scan->device->addPidFilter(pid, this)) {
			pid = -1;
			return false;
		}

		if (type != DvbScan::NitFilter) {
			timerId = startTimer(5000);
		} else {
			// FIXME hmmmmmmm
			timerId = startTimer(60000);
		}

		return true;
	}

	void stopFilter()
	{
		if (pid != -1) {
			killTimer(timerId);
			scan->device->removePidFilter(pid, this);

			pid = -1;
		}
	}

private:
	void processSection(const DvbSectionData &data)
	{
		scan->processSection(this, data);
	}

	void timerEvent(QTimerEvent *)
	{
		kWarning() << "timeout while reading section; type = " << type << " pid =" << pid;
		scan->filterTimeout(this);
	}

	DvbScan *scan;
	int pid;
	DvbScan::FilterType type;
	int timerId;
};

DvbScan::DvbScan(const QString &source_, DvbDevice *device_, const DvbTransponder &transponder_) :
	source(source_), device(device_), transponder(transponder_), isLive(true),
	transponderIndex(-1), state(ScanPat), snr(-1), patIndex(0), activeFilters(0)
{
	init();
}

DvbScan::DvbScan(const QString &source_, DvbDevice *device_,
	const QList<DvbTransponder> &transponders_) : source(source_), device(device_),
	isLive(false), transponders(transponders_), transponderIndex(0), state(ScanTune), snr(-1),
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

void DvbScan::processSection(DvbScanFilter *filter, const DvbSectionData &data)
{
	DvbSection genericSection(data);

	if (!genericSection.isValid()) {
		kDebug() << "invalid section";
		return;
	}

/*
	if ((standardSection.sectionNumber() != 0) || (standardSection.lastSectionNumber() != 0)) {
		kWarning() << "section numbers > 0 aren't supported";
	}
*/

	switch (filter->getType()) {
	case PatFilter: {
		if (genericSection.tableId() != 0x0) {
			return;
		}

		DvbPatSection section(genericSection);

		if (!section.isValid()) {
			kDebug() << "invalid section";
			return;
		}

		kDebug() << "found a new PAT";

		DvbPatSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid PAT entry";
				break;
			}

			DvbPatEntry patEntry(entry.programNumber(), entry.pid());

			if (patEntry.programNumber != 0x0) {
				// skip 0x0 which has a special meaning
				patEntries.append(patEntry);
			}

			entry = entry.next();
		}

		break;
	    }

	case PmtFilter: {
		if (genericSection.tableId() != 0x2) {
			return;
		}

		DvbPmtSection section(genericSection);

		if (!section.isValid()) {
			kDebug() << "invalid section";
			return;
		}

		kDebug() << "found a new PMT";

		DvbPreviewChannel channel;
		DvbPmtSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid PMT entry";
				break;
			}

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

			entry = entry.next();
		}

		if ((channel.videoPid != -1) || !channel.audioPids.isEmpty()) {
			channel.name = "[" + QString::number(section.programNumber()) + "]";
			channel.source = source;
			channel.serviceId = section.programNumber();
			channel.pmtPid = filter->getPid();
			channel.transponder = transponder;
			channel.snr = snr;
			channels.append(channel);
		}

		break;
	    }

	case SdtFilter: {
		if (genericSection.tableId() != 0x42) {
			// there are also other tables in the SDT
			return;
		}

		DvbSdtSection section(genericSection);

		if (!section.isValid()) {
			kDebug() << "invalid section";
			return;
		}

		kDebug() << "found a new SDT";

		DvbSdtSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid SDT entry";
				break;
			}

			DvbSdtEntry sdtEntry(entry.serviceId(), section.originalNetworkId(),
					     section.transportStreamId(), entry.isScrambled());

			DvbDescriptor descriptor = entry.descriptors();

			while (!descriptor.isEmpty()) {
				if (!descriptor.isValid()) {
					kDebug() << "invalid descriptor";
					break;
				}

				if (descriptor.descriptorTag() != 0x48) {
					descriptor = descriptor.next();
					continue;
				}

				DvbServiceDescriptor serviceDescriptor(descriptor);

				if (!serviceDescriptor.isValid()) {
					descriptor = descriptor.next();
					kDebug() << "invalid service descriptor";
					continue;
				}

				sdtEntry.name = serviceDescriptor.serviceName();
				sdtEntry.provider = serviceDescriptor.providerName();

				break;
			}

			sdtEntries.append(sdtEntry);

			entry = entry.next();
		}

		break;
	    }

	case NitFilter: {
		if (genericSection.tableId() != 0x40) {
			// we are only interested in the current network
			return;
		}

		DvbNitSection section(genericSection);

		if (!section.isValid()) {
			kDebug() << "invalid section";
			return;
		}

		kDebug() << "found a new NIT";

		DvbNitSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid NIT entry";
				break;
			}

			DvbDescriptor descriptor = entry.descriptors();

			while (!descriptor.isEmpty()) {
				if (!descriptor.isValid()) {
					kDebug() << "invalid descriptor";
					break;
				}

				if (descriptor.descriptorTag() != 0x43) {
					descriptor = descriptor.next();
					continue;
				}

				DvbSatelliteDescriptor satDescriptor(descriptor);

				if (!satDescriptor.isValid()) {
					kDebug() << "invalid satellite descriptor";
					descriptor = descriptor.next();
					continue;
				}

				if (satDescriptor.isDvbS2()) {
					kDebug() << "ignoring non-DVB-S descriptor";
					descriptor = descriptor.next();
					continue;
				}

				DvbSTransponder *transponder = new DvbSTransponder();
				transponder->frequency = DvbDescriptor::bcdToInt(satDescriptor.frequency(), 10);

				switch (satDescriptor.polarization()) {
				case 0:
					transponder->polarization = DvbSTransponder::Horizontal;
				case 1:
					transponder->polarization = DvbSTransponder::Vertical;
				case 2:
					transponder->polarization = DvbSTransponder::CircularLeft;
				default:
					transponder->polarization = DvbSTransponder::CircularRight;
				}

				transponder->symbolRate = DvbDescriptor::bcdToInt(satDescriptor.symbolRate(), 100);

				switch (satDescriptor.fecRate()) {
				case 1:
					transponder->fecRate = DvbTransponderBase::Fec1_2;
				case 2:
					transponder->fecRate = DvbTransponderBase::Fec2_3;
				case 3:
					transponder->fecRate = DvbTransponderBase::Fec3_4;
				case 4:
					transponder->fecRate = DvbTransponderBase::Fec5_6;
				case 5:
					transponder->fecRate = DvbTransponderBase::Fec7_8;
				case 6:
					transponder->fecRate = DvbTransponderBase::Fec8_9;
				case 8:
					transponder->fecRate = DvbTransponderBase::Fec4_5;
				default:
					// this includes rates like 3/5 and 9/10
					transponder->fecRate = DvbTransponderBase::FecAuto;
				}

				bool found = false;

				foreach (const DvbTransponder &it, transponders) {
					const DvbSTransponder *sIt = it->getDvbSTransponder();

					if (sIt == NULL) {
						continue;
					}

					if ((sIt->polarization == transponder->polarization) &&
					    (sIt->frequency == transponder->frequency)) {
						found = true;
						break;
					}
				}

				if (!found) {
					kDebug() << "added [" << transponder->frequency << transponder->polarization << transponder->symbolRate << transponder->fecRate << "]";
					transponders.append(DvbTransponder(transponder));
				} else {
					delete transponder;
				}

				descriptor = descriptor.next();
			}

			entry = entry.next();
		}

		break;
	    }
	}

	filter->stopFilter();
	--activeFilters;

	updateState();
}

void DvbScan::filterTimeout(DvbScanFilter *filter)
{
	filter->stopFilter();
	--activeFilters;

	updateState();
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
	} else {
		DvbScanFilter *filter = new DvbScanFilter(this);

		if (!filter->startFilter(pid, type)) {
			delete filter;
			return false;
		}

		filters.append(filter);
		++activeFilters;
	}

	return true;
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

			state = ScanSdt;
		    }
			// fall through
		case ScanSdt: {
			if (!startFilter(0x11, SdtFilter)) {
				return;
			}

			state = ScanNit;
		    }
			// fall through
		case ScanNit: {
			if (!isLive) {
				if (!startFilter(0x10, NitFilter)) {
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

			foreach (const DvbSdtEntry &sdtEntry, sdtEntries) {
				for (int i = 0; i < channels.size(); ++i) {
					const DvbPreviewChannel &channel = channels.at(i);

					if (channel.serviceId == sdtEntry.serviceId) {
						DvbPreviewChannel &it = channels[i];

						it.networkId = sdtEntry.networkId;
						it.transportStreamId = sdtEntry.transportStreamId;
						it.scrambled = sdtEntry.scrambled;

						if (!sdtEntry.name.isEmpty()) {
							it.name = sdtEntry.name;
						}

						it.provider = sdtEntry.provider;
					}
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

			device->stopDevice();

			state = ScanTune;
		    }
			// fall through
		case ScanTune: {
			switch (device->getDeviceState()) {
			case DvbDevice::DeviceIdle: {
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
