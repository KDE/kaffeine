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

class DvbPreviewChannel : public DvbChannel
{
public:
	DvbPreviewChannel() : snr(-1) { }
	~DvbPreviewChannel() { }

	bool isValid() const
	{
		return !name.isEmpty() && ((videoPid != -1) || !audioPids.isEmpty());
	}

	/*
	 * assigned in the SDT
	 */

	// QString name;
	// int networkId; // may be -1 (not present)
	// int transportStreamId; // may be -1 (not present)
	// bool scrambled;
	QString provider;

	/*
	 * assigned in the PMT
	 */

	// int pmtPid;
	// int serviceId;
	// int videoPid; // may be -1 (not present)
	QList<int> audioPids;

	/*
	 * assigned later
	 */

	// QString source;
	// DvbSharedTransponder transponder;
	int snr; // percent

	/*
	 * assigned when adding channel to the main list
	 */

	// int number;
	// int audioPid; // may be -1 (not present)

	/*
	 * model functions
	 */

	static int columnCount();
	static QVariant headerData(int column);
	QVariant modelData(int column) const;
};

class DvbPreviewChannelModel : public DvbGenericChannelModel<DvbPreviewChannel>
{
public:
	explicit DvbPreviewChannelModel(QObject *parent) :
		DvbGenericChannelModel<DvbPreviewChannel>(parent) { }
	~DvbPreviewChannelModel() { }
};

class DvbPatEntry
{
public:
	DvbPatEntry(int programNumber_, int pid_) : programNumber(programNumber_), pid(pid_) { }
	~DvbPatEntry() { }

	int programNumber;
	int pid;
};

class DvbScan : public QObject
{
	Q_OBJECT
public:
	enum State
	{
		ScanInit,
		ScanPat,
		ScanSdt,
		ScanPmt,
		ScanNit
	};

	DvbScan(const QString &source_, DvbDevice *device_, bool isLive_,
		const QList<DvbSharedTransponder> &transponderList_);
	~DvbScan();

signals:
	void foundChannels(const QList<DvbPreviewChannel> &channels);
	void scanFinished();

private slots:
	void sectionFound(const DvbSectionData &data);
	void sectionTimeout();

private:
	void startFilter(int pid)
	{
		Q_ASSERT(currentPid == -1);
		filter.resetFilter();
		device->addPidFilter(pid, &filter);
		currentPid = pid;
	}

	void stopFilter()
	{
		if (currentPid != -1) {
			device->removePidFilter(currentPid, &filter);
			currentPid = -1;
		}
	}

	void updateState();

	const QString source;
	DvbDevice *device;
	DvbSharedTransponder transponder;

	const bool isLive;
	QList<DvbSharedTransponder> transponderList;
	int transponderIndex;

	State state;
	QTimer timer;
	DvbSectionFilter filter;
	int currentPid;

	QList<DvbPatEntry> patEntries;
	int patIndex;

	QList<DvbPreviewChannel> channels;
};

#endif /* DVBSCAN_H */
