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
#include <KLocalizedString>

int DvbPreviewChannel::columnCount()
{
	return 3;
}

QVariant DvbPreviewChannel::headerData(int column)
{
	switch (column) {
	case 0:
		return i18n("Name");
	case 1:
		return i18n("Provider");
	case 2:
		return i18n("SNR");
	default:
		return QVariant();
	}
}

QVariant DvbPreviewChannel::modelData(int column) const
{
	switch (column) {
	case 0:
		return name;
	case 1:
		return provider;
	case 2:
		return snr;
	default:
		return QVariant();
	}
}

DvbScan::DvbScan(const QString &source_, DvbDevice *device_, bool isLive_,
	const QList<DvbSharedTransponder> &transponderList_) :
	source(source_), device(device_), isLive(isLive_), transponderList(transponderList_),
	transponderIndex(0), state(ScanInit), currentPid(-1), patIndex(0)
{
	connect(&filter, SIGNAL(sectionFound(DvbSectionData)), this, SLOT(sectionFound(DvbSectionData)));
	connect(&timer, SIGNAL(timeout()), this, SLOT(sectionTimeout()));
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
		DvbPatSection section(standardSection);

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
			patEntries.append(patEntry);

			entry = entry.next();
		}

		updateState();
		break;
	    }

	case ScanPmt: {
		DvbPmtSection section(standardSection);

		if (!section.isValid()) {
			kDebug() << "invalid section";
		}

		kDebug() << "found a new PMT";

		DvbPreviewChannel channel;
		channel.pmtPid = currentPid;
		channel.serviceId = section.programNumber();

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

		channels.append(channel);

		updateState();
		break;
	    }

	case ScanSdt: {
		if (standardSection.tableId() != 0x42) {
			// there are also other tables in the SDT
			break;
		}

		DvbSdtSection section(standardSection, 0x42);

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

			if (it == channels.end()) {
				kDebug() << "unassignable SDT entry" << serviceId;
			}

			entry = entry.next();
		}

		updateState();
		break;
	    }
	}
}

void DvbScan::sectionTimeout()
{
	kWarning() << "timeout while reading section ( state =" << state << ")";
	updateState();
}

void DvbScan::updateState()
{
	stopFilter();
	timer.stop();

	switch (state) {
	case ScanInit: {
		// switch transponder
		transponder = transponderList.at(transponderIndex);

		// FIXME

		// set up PAT filter
		state = ScanPat;
		startFilter(0x0);
		break;
	    }

	case ScanPat:
		// fall through
	case ScanPmt: {

		while (patIndex < patEntries.size()) {
			const DvbPatEntry &entry = patEntries.at(patIndex);

			if (entry.programNumber == 0x0) {
				// special meaning
				patIndex++;
				continue;
			}

			// set up PMT filter
			state = ScanPmt;
			startFilter(entry.pid);
			break;
		}

		if (patIndex < patEntries.size()) {
			// advance to the next PMT entry for next call
			patIndex++;
			break;
		}

		if (!channels.empty()) {
			// set up SDT filter
			state = ScanSdt;
			startFilter(0x11);
			break;
		}

		// fall through
	    }

	case ScanSdt: {
		// FIXME
		// we're finished here :)

		int snr = device->getSnr();
		QList<DvbPreviewChannel>::iterator it;

		for (it = channels.begin(); it != channels.end();) {
			if (!it->isValid()) {
				it = channels.erase(it);
				continue;
			}

			it->source = source;
			it->setTransponder(transponder);
			it->snr = snr;

			++it;
		}

		emit foundChannels(channels);
		emit scanFinished();

		return;
	    }
	}

	timer.start(5000);
}
