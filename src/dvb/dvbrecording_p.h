/*
 * dvbrecording_p.h
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBRECORDING_P_H
#define DVBRECORDING_P_H

#include <QFile>
#include <QTimer>
#include "dvbchannel.h"
#include "dvbsi.h"

class DvbDevice;
class DvbManager;
class DvbRecording;

class DvbRecordingFile : private QObject, public QSharedData, private DvbPidFilter
{
	Q_OBJECT
public:
	explicit DvbRecordingFile(DvbManager *manager_);
	~DvbRecordingFile();

	// start() returns true if the recording is already running
	bool start(const DvbRecording &recording);
	void stop();

private slots:
	void deviceStateChanged();
	void pmtSectionChanged(const QByteArray &pmtSectionData_);
	void insertPatPmt();

private:
	void processData(const char data[188]);

	DvbManager *manager;
	DvbSharedChannel channel;
	QFile file;
	QList<QByteArray> buffers;
	DvbDevice *device;
	QList<int> pids;
	DvbPmtFilter pmtFilter;
	QByteArray pmtSectionData;
	DvbSectionGenerator patGenerator;
	DvbSectionGenerator pmtGenerator;
	QTimer patPmtTimer;
	bool pmtValid;
};

#endif /* DVBRECORDING_P_H */
