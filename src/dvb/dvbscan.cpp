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

#include "../log.h"

#include <QBitArray>
#include <QVector>
#include <stdint.h>

#include "dvbdevice.h"
#include "dvbscan.h"
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
	DvbScanFilter(DvbScan *scan_, bool useOtherNit_) : scan(scan_), pid(-1), useOtherNit(useOtherNit_) { }

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
	struct sectCheck {
		int id;
		QBitArray check;
	};

	bool checkMultipleSection(const DvbStandardSection &section);
	bool isFinished();
	void processSection(const char *data, int size);
	void timerEvent(QTimerEvent *);

	DvbScan *scan;

	int pid;
	DvbScan::FilterType type;
	QVector<sectCheck> multipleSections;
	int timerId;
	bool useOtherNit;
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
		multipleSections.clear();

		pid = -1;
	}
}

bool DvbScanFilter::checkMultipleSection(const DvbStandardSection &section)
{
	int sectionCount = section.lastSectionNumber() + 1;
	int tableNumber = -1;
	int id = section.tableId() << 16 | section.tableIdExtension();

	for (int i = 0; i < multipleSections.size(); i++) {
		if (multipleSections.at(i).id == id) {
			tableNumber = i;
			break;
		}
	}
	if (tableNumber < 0) {
		tableNumber = multipleSections.size();
		multipleSections.resize(tableNumber + 1);
		multipleSections[tableNumber].id = id;
		multipleSections[tableNumber].check.resize(sectionCount);
	}

	if (section.sectionNumber() >= sectionCount) {
		qCWarning(logDvb, "Current section is bigger than the last one");
		sectionCount = section.sectionNumber() + 1;
	}

	QBitArray *check = &multipleSections[tableNumber].check;

	if (check->isEmpty()) {
		check->resize(sectionCount);
	} else {
		if (check->size() != sectionCount) {
			qCWarning(logDvb, "Inconsistent number of sections");

			if (check->size() < sectionCount)
				check->resize(sectionCount);
		}
	}

	if (check->testBit(section.sectionNumber())) {
		return false;
	}

	check->setBit(section.sectionNumber());
	return true;
}

bool DvbScanFilter::isFinished()
{
	for (int i = 0; i < multipleSections.size(); i++) {
		if (multipleSections[i].check.count(false) != 0)
			return false;
	}
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
		// FIXME: should we also handle other SDT table?

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


		if (!nitSection.isValid())
			return;

		if (!((nitSection.tableId() == 0x40) || (useOtherNit && (nitSection.tableId() == 0x41))))
			return;

		qDebug("Handling NIT table ID 0x%02x, extension 0x%04x", nitSection.tableId(), nitSection.tableIdExtension());

		if (!checkMultipleSection(nitSection)) {
			// already read this part
			return;
		}

		scan->processNit(nitSection);
		break;
	    }
	}

	if (isFinished())
		scan->filterFinished(this);
}

void DvbScanFilter::timerEvent(QTimerEvent *)
{
	qCWarning(logDvb, "Timeout while reading section; type = %d, PID = %d", type, pid);
	scan->filterFinished(this);
}

DvbScan::DvbScan(DvbDevice *device_, const QString &source_, const DvbTransponder &transponder_, bool useOtherNit_) :
	device(device_), source(source_), transponder(transponder_), isLive(true), isAuto(false), useOtherNit(useOtherNit_),
	transponderIndex(-1), state(ScanPat), patIndex(0), activeFilters(0)
{
	qDebug("Use other NIT is %s", useOtherNit ? "enabled" : "disabled");
}

DvbScan::DvbScan(DvbDevice *device_, const QString &source_,
	const QList<DvbTransponder> &transponders_, bool useOtherNit_) : device(device_), source(source_),
	isLive(false), isAuto(false), useOtherNit(useOtherNit_), transponders(transponders_), transponderIndex(0),
	state(ScanTune), patIndex(0), activeFilters(0)
{
	qDebug("Use other NIT is %s", useOtherNit ? "enabled" : "disabled");
}

DvbScan::DvbScan(DvbDevice *device_, const QString &source_, const QString &autoScanSource, bool useOtherNit_) :
	device(device_), source(source_), isLive(false), isAuto(true), useOtherNit(useOtherNit_), transponderIndex(0),
	state(ScanTune), patIndex(0), activeFilters(0)
{
	qDebug("Use other NIT is %s", useOtherNit ? "enabled" : "disabled");

	// Seek for DVB-T transponders

	if ((autoScanSource == QLatin1String("AUTO-T-Normal")) ||
	    (autoScanSource == QLatin1String("AUTO-T-Offsets")) ||
	    (autoScanSource == QLatin1String("AUTO-T2-Normal")) ||
	    (autoScanSource == QLatin1String("AUTO-T2-Offsets"))) {
		bool offsets = (autoScanSource == QLatin1String("AUTO-T-Offsets")) || (autoScanSource == QLatin1String("AUTO-T2-Offsets"));

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
	} else if ((autoScanSource == QLatin1String("AUTO-T-Australia")) ||
		   (autoScanSource == QLatin1String("AUTO-T2-Australia"))) {
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
	} else if ((autoScanSource == QLatin1String("AUTO-T-Italy")) ||
		   (autoScanSource == QLatin1String("AUTO-T2-Italy"))) {
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
	} else if ((autoScanSource == QLatin1String("AUTO-T-Taiwan"))||
		   (autoScanSource == QLatin1String("AUTO-T2-Taiwan"))) {
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

	// Seek for DVB-T2 transponders

	if ((autoScanSource == QLatin1String("AUTO-T2-Normal")) ||
		   (autoScanSource == QLatin1String("AUTO-T2-Offsets"))) {
		bool offsets = (autoScanSource == QLatin1String("AUTO-T2-Offsets"));

		for (int frequency = 177500000; frequency <= 226500000; frequency += 7000000) {
			DvbTransponder currentTransponder(DvbTransponderBase::DvbT2);
			DvbT2Transponder *dvbT2Transponder = currentTransponder.as<DvbT2Transponder>();
			dvbT2Transponder->frequency = frequency;
			dvbT2Transponder->bandwidth = DvbT2Transponder::Bandwidth7MHz;
			dvbT2Transponder->modulation = DvbT2Transponder::ModulationAuto;
			dvbT2Transponder->fecRateHigh = DvbT2Transponder::FecAuto;
			dvbT2Transponder->fecRateLow = DvbT2Transponder::FecNone;
			dvbT2Transponder->transmissionMode = DvbT2Transponder::TransmissionModeAuto;
			dvbT2Transponder->guardInterval = DvbT2Transponder::GuardIntervalAuto;
			dvbT2Transponder->hierarchy = DvbT2Transponder::HierarchyNone;
			dvbT2Transponder->streamId = 0;
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

				DvbTransponder currentTransponder(DvbTransponderBase::DvbT2);
				DvbT2Transponder *dvbT2Transponder = currentTransponder.as<DvbT2Transponder>();
				dvbT2Transponder->frequency = frequency + offset;
				dvbT2Transponder->bandwidth = DvbT2Transponder::Bandwidth8MHz;
				dvbT2Transponder->modulation = DvbT2Transponder::ModulationAuto;
				dvbT2Transponder->fecRateHigh = DvbT2Transponder::FecAuto;
				dvbT2Transponder->fecRateLow = DvbT2Transponder::FecNone;
				dvbT2Transponder->transmissionMode = DvbT2Transponder::TransmissionModeAuto;
				dvbT2Transponder->guardInterval = DvbT2Transponder::GuardIntervalAuto;
				dvbT2Transponder->hierarchy = DvbT2Transponder::HierarchyNone;
				dvbT2Transponder->streamId = 0;
				transponders.append(currentTransponder);
			}
		}
	} else if (autoScanSource == QLatin1String("AUTO-T2-Australia")) {
		for (int frequency = 177500000; frequency <= 226500000; frequency += 7000000) {
			for (int i = 0; i < 2; ++i) {
				int offset = 0;

				if (i == 1) {
					offset = 125000;
				}

				DvbTransponder currentTransponder(DvbTransponderBase::DvbT2);
				DvbT2Transponder *dvbT2Transponder =
					currentTransponder.as<DvbT2Transponder>();
				dvbT2Transponder->frequency = frequency + offset;
				dvbT2Transponder->bandwidth = DvbT2Transponder::Bandwidth7MHz;
				dvbT2Transponder->modulation = DvbT2Transponder::ModulationAuto;
				dvbT2Transponder->fecRateHigh = DvbT2Transponder::FecAuto;
				dvbT2Transponder->fecRateLow = DvbT2Transponder::FecNone;
				dvbT2Transponder->transmissionMode =
					DvbT2Transponder::TransmissionModeAuto;
				dvbT2Transponder->guardInterval =
					DvbT2Transponder::GuardIntervalAuto;
				dvbT2Transponder->hierarchy = DvbT2Transponder::HierarchyNone;
				dvbT2Transponder->streamId = 0;
				transponders.append(currentTransponder);
			}
		}

		for (int frequency = 529500000; frequency <= 816500000; frequency += 7000000) {
			for (int i = 0; i < 2; ++i) {
				int offset = 0;

				if (i == 1) {
					offset = 125000;
				}

				DvbTransponder currentTransponder(DvbTransponderBase::DvbT2);
				DvbT2Transponder *dvbT2Transponder =
					currentTransponder.as<DvbT2Transponder>();
				dvbT2Transponder->frequency = frequency + offset;
				dvbT2Transponder->bandwidth = DvbT2Transponder::Bandwidth7MHz;
				dvbT2Transponder->modulation = DvbT2Transponder::ModulationAuto;
				dvbT2Transponder->fecRateHigh = DvbT2Transponder::FecAuto;
				dvbT2Transponder->fecRateLow = DvbT2Transponder::FecNone;
				dvbT2Transponder->transmissionMode =
					DvbT2Transponder::TransmissionModeAuto;
				dvbT2Transponder->guardInterval =
					DvbT2Transponder::GuardIntervalAuto;
				dvbT2Transponder->hierarchy = DvbT2Transponder::HierarchyNone;
				dvbT2Transponder->streamId = 0;
				transponders.append(currentTransponder);
			}
		}
	} else if (autoScanSource == QLatin1String("AUTO-T2-Italy")) {
		static const int italyVhf[] = { 177500000, 186000000, 194500000, 203500000,
						212500000, 219500000, 226500000 };

		for (unsigned i = 0; i < (sizeof(italyVhf) / sizeof(italyVhf[0])); ++i) {
			for (int j = 0; j < 2; ++j) {
				DvbTransponder currentTransponder(DvbTransponderBase::DvbT2);
				DvbT2Transponder *dvbT2Transponder =
					currentTransponder.as<DvbT2Transponder>();
				dvbT2Transponder->frequency = italyVhf[i];
				dvbT2Transponder->bandwidth = ((j == 0) ?
					DvbT2Transponder::Bandwidth7MHz :
					DvbT2Transponder::Bandwidth8MHz);
				dvbT2Transponder->modulation = DvbT2Transponder::ModulationAuto;
				dvbT2Transponder->fecRateHigh = DvbT2Transponder::FecAuto;
				dvbT2Transponder->fecRateLow = DvbT2Transponder::FecNone;
				dvbT2Transponder->transmissionMode =
					DvbT2Transponder::TransmissionModeAuto;
				dvbT2Transponder->guardInterval =
					DvbT2Transponder::GuardIntervalAuto;
				dvbT2Transponder->hierarchy = DvbT2Transponder::HierarchyNone;
				dvbT2Transponder->streamId = 0;
				transponders.append(currentTransponder);
			}
		}

		for (int frequency = 474000000; frequency <= 858000000; frequency += 8000000) {
			DvbTransponder currentTransponder(DvbTransponderBase::DvbT2);
			DvbT2Transponder *dvbT2Transponder =
				currentTransponder.as<DvbT2Transponder>();
			dvbT2Transponder->frequency = frequency;
			dvbT2Transponder->bandwidth = DvbT2Transponder::Bandwidth8MHz;
			dvbT2Transponder->modulation = DvbT2Transponder::ModulationAuto;
			dvbT2Transponder->fecRateHigh = DvbT2Transponder::FecAuto;
			dvbT2Transponder->fecRateLow = DvbT2Transponder::FecNone;
			dvbT2Transponder->transmissionMode = DvbT2Transponder::TransmissionModeAuto;
			dvbT2Transponder->guardInterval = DvbT2Transponder::GuardIntervalAuto;
			dvbT2Transponder->hierarchy = DvbT2Transponder::HierarchyNone;
			dvbT2Transponder->streamId = 0;
			transponders.append(currentTransponder);
		}
	} else if (autoScanSource == QLatin1String("AUTO-T2-Taiwan")) {
		for (int frequency = 527000000; frequency <= 599000000; frequency += 6000000) {
			DvbTransponder currentTransponder(DvbTransponderBase::DvbT2);
			DvbT2Transponder *dvbT2Transponder =
				currentTransponder.as<DvbT2Transponder>();
			dvbT2Transponder->frequency = frequency;
			dvbT2Transponder->bandwidth = DvbT2Transponder::Bandwidth6MHz;
			dvbT2Transponder->modulation = DvbT2Transponder::ModulationAuto;
			dvbT2Transponder->fecRateHigh = DvbT2Transponder::FecAuto;
			dvbT2Transponder->fecRateLow = DvbT2Transponder::FecNone;
			dvbT2Transponder->transmissionMode = DvbT2Transponder::TransmissionModeAuto;
			dvbT2Transponder->guardInterval = DvbT2Transponder::GuardIntervalAuto;
			dvbT2Transponder->hierarchy = DvbT2Transponder::HierarchyNone;
			dvbT2Transponder->streamId = 0;
			transponders.append(currentTransponder);
		}
	}

	// Seek for ISDB-T transponders

	if (autoScanSource == QLatin1String("AUTO-UHF-6MHz")) {
		for (int frequency = 473142857; frequency <= 803142857; frequency += 6000000) {
			DvbTransponder currentTransponder(DvbTransponderBase::IsdbT);
			IsdbTTransponder *isdbTTransponder =
				currentTransponder.as<IsdbTTransponder>();
			isdbTTransponder->frequency = frequency;
			isdbTTransponder->bandwidth = IsdbTTransponder::Bandwidth6MHz;
			isdbTTransponder->transmissionMode = IsdbTTransponder::TransmissionModeAuto;
			isdbTTransponder->guardInterval = IsdbTTransponder::GuardIntervalAuto;
			isdbTTransponder->partialReception = IsdbTTransponder::PR_AUTO;
			isdbTTransponder->soundBroadcasting = IsdbTTransponder::SB_disabled;
			for (int i = 0; i < 3; i++) {
				isdbTTransponder->layerEnabled[i] = true;
				isdbTTransponder->modulation[i] = IsdbTTransponder::ModulationAuto;
				isdbTTransponder->fecRate[i] = IsdbTTransponder::FecAuto;
				isdbTTransponder->interleaving[i] = IsdbTTransponder::I_AUTO;
				isdbTTransponder->segmentCount[i] = 15;
			}
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
		qCWarning(logDvb, "Device was released. Stopping scan");
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
		DvbScanFilter *filter = new DvbScanFilter(this, useOtherNit);

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

			snr = device->getSnr(scale);

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
				qDebug("Found channel %s", qPrintable(channel.name));
			}

			if (!channels.isEmpty()) {
				emit foundChannels(channels);
			}

			if (isLive) {
				qCInfo(logDvb, "Scanning while live stream. Can't change the transponder");
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

			qDebug("Transponder %d/%d", transponderIndex, transponders.size());
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

			qDebug("New PAT entry: pid %d, program %d", entry.pid(), entry.programNumber());
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
		switch (scale) {
		case DvbBackendDevice::dBuV:
		case DvbBackendDevice::NotSupported: {
			channel.snr = "";
			break;
		}
		case DvbBackendDevice::Percentage: {
			channel.snr = QString::number(snr, 'f', 0) + "%";
			break;
		}
		case DvbBackendDevice::Decibel: {
			channel.snr = QString::number(snr, 'f', 2) + " dB";
			break;
		}
		};
		channels.append(channel);

		qDebug("New channel: PID %d, service ID %d", pid, section.programNumber());
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

			qDebug("New SDT entry: service ID 0x%04x, name '%s', provider '%s'", entry.serviceId(), qPrintable(sdtEntry.name), qPrintable(sdtEntry.provider));
			sdtEntries.append(sdtEntry);
		}
	}
}

void DvbScan::processVct(const AtscVctSection &section)
{
	AtscVctSectionEntry entry = section.entries();
	int entryCount = section.entryCount();

	for (int i = 0; i < entryCount && (entry.isValid()); i++) {
		QString majorminor = QString(QLatin1String("%1-%2 ")).
			arg(entry.majorNumber(), 3, 10, QLatin1Char('0')).arg(entry.minorNumber());

		DvbSdtEntry sdtEntry(entry.programNumber(), entry.sourceId(), entry.isScrambled());

		// Each VCT section has it's own list of descriptors
		// See A/65C table 6.25a for the list of descriptors
		for (DvbDescriptor descriptor = entry.descriptors(); descriptor.isValid();
		     descriptor.advance()) {
			if (descriptor.descriptorTag() == 0xa0) {
				// Extended Channel Name Descriptor
				AtscChannelNameDescriptor nameDescriptor(descriptor);
				if (!nameDescriptor.isValid()) {
					continue;
				}
				sdtEntry.name = majorminor + nameDescriptor.name();
			}
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

		qDebug("New SDT entry: name %s", qPrintable(sdtEntry.name));
		sdtEntries.append(sdtEntry);

		if (i < entryCount - 1)
			entry.advance();
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
	case 3:
		return DvbTTransponder::Bandwidth5MHz;
	default:
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

// FIXME: Implement DvbT2Descriptor

void DvbScan::processNitDescriptor(const DvbDescriptor &descriptor)
{
	DvbTransponder newTransponder(transponder.getTransmissionType());

	switch (transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		qCWarning(logDvb, "Invalid transponder type");
		return;
	case DvbTransponderBase::DvbC: {
		if (descriptor.descriptorTag() != 0x44) {
			return;
		}

		DvbCableDescriptor cableDescriptor(descriptor);

		if (!cableDescriptor.isValid()) {
			return;
		}

		newTransponder = DvbTransponder(DvbTransponderBase::DvbC);
		DvbCTransponder *dvbCTransponder = newTransponder.as<DvbCTransponder>();
		dvbCTransponder->frequency =
			DvbDescriptor::bcdToInt(cableDescriptor.frequency(), 100);
		dvbCTransponder->symbolRate =
			DvbDescriptor::bcdToInt(cableDescriptor.symbolRate(), 100);
		dvbCTransponder->modulation = extractDvbCModulation(cableDescriptor);
		dvbCTransponder->fecRate = extractDvbCFecRate(cableDescriptor);

		qDebug("Added transponder: %.2f MHz", dvbCTransponder->frequency / 1000000.);
		break;
	    }
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2: {
		if (descriptor.descriptorTag() != 0x43) {
			return;
		}

		DvbSatelliteDescriptor satelliteDescriptor(descriptor);

		if (!satelliteDescriptor.isValid()) {
			return;
		}

		DvbSTransponder *dvbSTransponder;

		if (!satelliteDescriptor.isDvbS2()) {
			newTransponder = DvbTransponder(DvbTransponderBase::DvbS);
			dvbSTransponder = newTransponder.as<DvbSTransponder>();
		} else {
			if ((device->getTransmissionTypes() & DvbDevice::DvbS2) == 0) {
				return;
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

		qDebug("Added transponder: %.2f MHz", dvbSTransponder->frequency / 1000000.);
		break;
	    }
	case DvbTransponderBase::DvbT2:
		// FIXME: Implement T2_delivery_system_descriptor
		// decriptor 0x7f, extension descriptor 0x04  or use libdvbv5

	case DvbTransponderBase::DvbT: {
		if (descriptor.descriptorTag() != 0x5a) {
			return;
		}

		DvbTerrestrialDescriptor terrestrialDescriptor(descriptor);

		if (!terrestrialDescriptor.isValid()) {
			return;
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

		qDebug("Added transponder: %.2f MHz", dvbTTransponder->frequency / 1000000.);
		break;
	    }
	case DvbTransponderBase::Atsc:
		return;
	case DvbTransponderBase::IsdbT:
		if (descriptor.descriptorTag() != 0xfa) {
			return;
		}

		IsdbTerrestrialDescriptor IsdbTDescriptor(descriptor);

		if (!IsdbTDescriptor.isValid()) {
			return;
		}

		for (int i = 0; i < IsdbTDescriptor.frequencyLength(); i++) {
			newTransponder = DvbTransponder(DvbTransponderBase::IsdbT);
			IsdbTTransponder *isdbTTransponder = newTransponder.as<IsdbTTransponder>();

			isdbTTransponder->frequency =
				(uint32_t)((((uint64_t)IsdbTDescriptor.frequency(i)) * 1000000ul) / 7);
			isdbTTransponder->bandwidth = IsdbTTransponder::Bandwidth6MHz;
			isdbTTransponder->transmissionMode = IsdbTTransponder::TransmissionModeAuto;
			isdbTTransponder->guardInterval = IsdbTTransponder::GuardIntervalAuto;
			isdbTTransponder->partialReception = IsdbTTransponder::PR_AUTO;
			isdbTTransponder->soundBroadcasting = IsdbTTransponder::SB_disabled;
			for (int i = 0; i < 3; i++) {
				isdbTTransponder->layerEnabled[i] = true;
				isdbTTransponder->modulation[i] = IsdbTTransponder::ModulationAuto;
				isdbTTransponder->fecRate[i] = IsdbTTransponder::FecAuto;
				isdbTTransponder->interleaving[i] = IsdbTTransponder::I_AUTO;
				isdbTTransponder->segmentCount[i] = 15;
			}

			bool duplicate = false;

			foreach (const DvbTransponder &existingTransponder, transponders) {
				if (existingTransponder.corresponds(newTransponder)) {
					duplicate = true;
					break;
				}
			}
			if (duplicate)
				continue;

			transponders.append(newTransponder);
			qDebug("Added transponder: %.2f MHz", isdbTTransponder->frequency / 1000000.);
		}
		return;
	}


	// New transponder was found. Add it
	foreach (const DvbTransponder &existingTransponder, transponders) {
		if (existingTransponder.corresponds(newTransponder))
			return;
	}

	transponders.append(newTransponder);
}

void DvbScan::filterFinished(DvbScanFilter *filter)
{
	filter->stopFilter();
	--activeFilters;
	updateState();
}
