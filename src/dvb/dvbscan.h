/*
 * dvbscan.h
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

#ifndef DVBSCAN_H
#define DVBSCAN_H

#include "dvbchannel.h"
#include "dvbsi.h"

class DvbPatEntry;

class DvbPreviewChannel : public DvbChannelBase
{
public:
	DvbPreviewChannel() : snr(-1) { }
	~DvbPreviewChannel() { }

	bool isValid() const
	{
		return !name.isEmpty() && ((videoPid != -1) || !audioPids.isEmpty());
	}

	/*
	 * assigned when reading PMT
	 */

	// QString name; // a generic name is assigned
	// QString source;
	// int serviceId;
	// int pmtPid;
	// int videoPid; // may be -1 (not present)
	// DvbSharedTransponder transponder;
	QList<int> audioPids;
	int snr; // percent

	/*
	 * assigned when reading SDT
	 */

	// QString name;
	// int networkId; // may be -1 (not present)
	// int transportStreamId; // may be -1 (not present)
	// bool scrambled;
	QString provider;

	/*
	 * assigned when adding channel to the main list
	 */

	// int number;
	// int audioPid; // may be -1 (not present)
};

class DvbScan : public QObject
{
	Q_OBJECT
public:
	DvbScan(const QString &source_, DvbDevice *device_, const DvbTransponder &transponder_);
	DvbScan(const QString &source_, DvbDevice *device_,
		const QList<DvbTransponder> &transponderList_);
	~DvbScan();

signals:
	void foundChannels(const QList<DvbPreviewChannel> &channels);
	void scanFinished();

private slots:
	void sectionFound(const DvbSectionData &data);
	void sectionTimeout();
	void deviceStateChanged();

private:
	enum ProgressFlag
	{
		Tuning      = (1 << 0),
		Tuned       = (1 << 1),
		PatScanning = (1 << 2),
		PatScanned  = (1 << 3),
		SdtScanning = (1 << 4),
		SdtScanned  = (1 << 5),
		NitScanning = (1 << 6),
		NitScanned  = (1 << 7),
		PmtScanning = (1 << 8),
		PmtScanned  = (1 << 9)
	};

	Q_DECLARE_FLAGS(ProgressFlags, ProgressFlag)

	void init();

	void updateState();

	QString source;
	DvbDevice *device;
	DvbTransponder transponder;
	bool isLive;

	// only used if isLive is false
	QList<DvbTransponder> transponderList;
	int transponderIndex;

	ProgressFlags progress;
	QList<DvbPatEntry> patEntries;
	int patIndex;
	QList<DvbPreviewChannel> channels;

	// QList<DvbScanFilter> filters;
};

class DvbScanFilter : public DvbSectionFilter
{
public:
	DvbScanFilter() { }
	~DvbScanFilter() { }

signals:
	

private:
	DvbDevice *device;
	int currentPid;
	QTimer timer;
	int snr;
};

#endif /* DVBSCAN_H */
