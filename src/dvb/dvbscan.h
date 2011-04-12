/*
 * dvbscan.h
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

#ifndef DVBSCAN_H
#define DVBSCAN_H

#include "dvbchannel.h"

class AtscVctSection;
class DvbDescriptor;
class DvbDevice;
class DvbNitSection;
class DvbPatEntry;
class DvbPatSection;
class DvbPmtSection;
class DvbScanFilter;
class DvbSdtEntry;
class DvbSdtSection;

class DvbPreviewChannel : public DvbChannel
{
public:
	DvbPreviewChannel() : snr(-1) { }
	~DvbPreviewChannel() { }

	/*
	 * assigned when reading PMT
	 */

	// QString source;
	// DvbTransponder transponder;
	// int transportStreamId;
	// int pmtPid;
	// QByteArray pmtSectionData;
	// int serviceId;
	// int audioPid; // may be -1 (not present)
	// bool hasVideo;
	int snr; // percent

	/*
	 * assigned when reading SDT
	 */

	// QString name;
	// int networkId; // may be -1 (not present); ATSC meaning: source id
	// bool isScrambled;
	QString provider;

	/*
	 * assigned when adding channel to the main list
	 */

	// int number;
};

class DvbScan : public QObject
{
	friend class DvbScanFilter;
	Q_OBJECT
public:
	DvbScan(DvbDevice *device_, const QString &source_, const DvbTransponder &transponder_);
	DvbScan(DvbDevice *device_, const QString &source_,
		const QList<DvbTransponder> &transponders_);
	DvbScan(DvbDevice *device_, const QString &source_, const QString &autoScanSource);
	~DvbScan();

	void start();

signals:
	void foundChannels(const QList<DvbPreviewChannel> &channels);
	void scanProgress(int percentage);
	void scanFinished();

private slots:
	void deviceStateChanged();

private:
	enum FilterType
	{
		PatFilter,
		PmtFilter,
		SdtFilter,
		VctFilter,
		NitFilter
	};

	enum State
	{
		ScanPat,
		ScanNit,
		ScanSdt,
		ScanPmt,
		ScanTune,
		ScanTuning
	};

	bool startFilter(int pid, FilterType type);
	void updateState();

	void processPat(const DvbPatSection &section);
	void processPmt(const DvbPmtSection &section, int pid);
	void processSdt(const DvbSdtSection &section);
	void processVct(const AtscVctSection &section);
	void processNit(const DvbNitSection &section);
	void processNitDescriptor(const DvbDescriptor &descriptor);
	void filterFinished(DvbScanFilter *filter);

	DvbDevice *device;
	QString source;
	DvbTransponder transponder;
	bool isLive;
	bool isAuto;

	// only used if isLive is false
	QList<DvbTransponder> transponders;
	int transponderIndex;

	State state;
	QList<DvbPatEntry> patEntries;
	int patIndex;
	QList<DvbSdtEntry> sdtEntries;
	QList<DvbPreviewChannel> channels;

	int snr;
	int transportStreamId;

	QList<DvbScanFilter *> filters;
	int activeFilters;
};

#endif /* DVBSCAN_H */
