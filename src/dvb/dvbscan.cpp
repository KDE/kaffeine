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

DvbScan::DvbScan(const QString &source_, DvbDevice *device_, const DvbTransponder &transponder_) :
	source(source_), device(device_), transponder(transponder_), isLive(true),
	transponderIndex(-1), state(ScanInit), currentPid(-1)
{
	init();
}

DvbScan::DvbScan(const QString &source_, DvbDevice *device_,
	const QList<DvbTransponder> &transponderList_) : source(source_), device(device_),
	isLive(false), transponderList(transponderList_), transponderIndex(0), state(ScanTune),
	currentPid(-1)
{
	init();
}

void DvbScan::init()
{
	connect(&filter, SIGNAL(sectionFound(DvbSectionData)), this, SLOT(sectionFound(DvbSectionData)));
	connect(&timer, SIGNAL(timeout()), this, SLOT(sectionTimeout()));
	connect(device, SIGNAL(stateChanged()), this, SLOT(deviceStateChanged()));
	updateState();
}

DvbScan::~DvbScan()
{
	stopFilter();
}

void DvbScan::sectionFound(const DvbSectionData &data)
{
	DvbStandardSection standardSection(data);

	if (!standardSection.isValid()) {
		kDebug() << "invalid section";
		return;
	}

	if ((standardSection.sectionNumber() != 0) || (standardSection.lastSectionNumber() != 0)) {
		kWarning() << "section numbers > 0 aren't supported";
	}

	switch (state) {
	case ScanPat: {
		DvbPatSection section(data);

		if (!section.isValid()) {
			kDebug() << "invalid section";
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

		updateState();
		break;
	    }

	case ScanPmt: {
		DvbPmtSection section(data);

		if (!section.isValid()) {
			kDebug() << "invalid section";
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
			channel.pmtPid = currentPid;
			channel.transponder = transponder;
			channel.snr = snr;
			channels.append(channel);
		}

		updateState();
		break;
	    }

	case ScanSdt: {
		if (standardSection.tableId() != 0x42) {
			// there are also other tables in the SDT
			break;
		}

		DvbSdtSection section(data);

		if (!section.isValid()) {
			kDebug() << "invalid section";
		}

		kDebug() << "found a new SDT";

		DvbSdtSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid SDT entry";
				break;
			}

			int serviceId = entry.serviceId();
			QList<DvbPreviewChannel>::iterator it;

			for (it = channels.begin(); it != channels.end(); ++it) {
				if (it->serviceId != serviceId) {
					continue;
				}

				it->networkId = section.originalNetworkId();
				it->transportStreamId = section.transportStreamId();
				it->scrambled = entry.isScrambled();

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

					it->name = serviceDescriptor.serviceName();
					it->provider = serviceDescriptor.providerName();

					break;
				}

				break;
			}

			entry = entry.next();
		}

		updateState();
		break;
	    }

	case ScanNit: {
		if (standardSection.tableId() != 0x40) {
			// we are only interested in the current network
			break;
		}

		DvbNitSection section(data);

		if (!section.isValid()) {
			kDebug() << "invalid section";
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
				transponder->frequency = satDescriptor.frequency();

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

				transponder->symbolRate = satDescriptor.symbolRate();

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

				foreach (const DvbTransponder &it, transponderList) {
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
					kDebug() << "added [" << transponder->frequency << transponder->polarization << "]";
					transponderList.append(DvbTransponder(transponder));
				} else {
					delete transponder;
				}

				descriptor = descriptor.next();
			}

			entry = entry.next();
		}

		updateState();
		break;
	    }

	case ScanInit:
	case ScanTune: {
		Q_ASSERT(false);
	    }
	}
}

void DvbScan::sectionTimeout()
{
	kWarning() << "timeout while reading section; state =" << state;
	updateState();
}

void DvbScan::deviceStateChanged()
{
	switch (device->getDeviceState()) {
	case DvbDevice::DeviceIdle: {
/*		if (state == ScanTune) {
			updateState();
		} */

		break;
	    }

	case DvbDevice::DeviceNotReady:
	case DvbDevice::DeviceRotorMoving:
	case DvbDevice::DeviceTuning: {
		// do nothing
		break;
	    }

	case DvbDevice::DeviceTuned: {
		Q_ASSERT(state == ScanTune);
		state = ScanInit;
		updateState();
		break;
	    }
	}
}

void DvbScan::updateState()
{
	if ((progress & Tuned) == 0) {
		if ((progress & Tuning) != 0) {
			return;
		}

		// switch transponder

		Q_ASSERT(isLive == false);

		patEntries.clear();
		patIndex = 0;
		channels.clear();

		if (transponderIndex >= transponderList.size()) {
			emit scanFinished();
			break;
		}

		transponder = transponderList.at(transponderIndex);
		++transponderIndex;

		// tune to the next transponder
		device->stopDevice();
		device->tuneDevice(transponder);

		progress |= Tuning;
		return;
	}

	if ((progress & PatScanning) == 0) {
		if (!startFilter(DvbScanFilter::Pat, 0x0)) {
			return;
		}

		progress |= PatScanning;
	}

	if ((progress & SdtScanning) == 0) {
		if (!startFilter(DvbScanFilter::Sdt, 0x11)) {
			return;
		}

		progress |= SdtScanning;
	}

	if (((progress & NitScanning) == 0) && !isLive) {
		if (!startFilter(DvbScanFilter::Nit, 0x10)) {
			return;
		}

		progress |= NitScanning;
	}

	while (patIndex < patEntries.size()) {
		if (!startFilter(DvbScanFilter::Pmt, patEntries.at(PatIndex).pid)) {
			return;
		}

		++patIndex;
	}



	stopFilter();

	switch (state) {
	case ScanInit: {
		// set up PAT filter
		patEntries.clear();
		patIndex = 0;
		startFilter(0x0);
		state = ScanPat;
		break;
	    }

	case ScanPat: {
		snr = device->getSnr();
	    }
		// fall through
	case ScanPmt: {
		if (patIndex < patEntries.size()) {
			// set up PMT filter and advance to the next PAT entry
			startFilter(patEntries.at(patIndex).pid);
			patIndex++;
			state = ScanPmt;
			break;
		}

		if (!channels.empty()) {
			// set up SDT filter
			startFilter(0x11);
			state = ScanSdt;
			break;
		}
	    }
		// fall through
	case ScanSdt: {
		if (!channels.isEmpty()) {
			emit foundChannels(channels);
			channels.clear();
		}

		if (isLive) {
			emit scanFinished();
			break;
		}

		// set up NIT filter
		startFilter(0x10);
		state = ScanNit;
		break;
	    }

	case ScanNit:
	case ScanTune: {
	    }
	}
}
