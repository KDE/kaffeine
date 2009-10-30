/*
 * dvbliveview.h
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBLIVEVIEW_H
#define DVBLIVEVIEW_H

#include <QObject>
#include <QSharedDataPointer>
#include <QTimer>

class DvbChannel;
class DvbDevice;
class DvbEitFilter;
class DvbLiveViewInternal;
class DvbManager;
class DvbOsd;
class DvbPmtSection;
class OsdWidget;
class MediaWidget;

class DvbLiveView : public QObject
{
	Q_OBJECT
public:
	explicit DvbLiveView(DvbManager *manager_);
	~DvbLiveView();

	QSharedDataPointer<DvbChannel> getChannel() const
	{
		return channel;
	}

	DvbDevice *getDevice() const
	{
		return device;
	}

	void playChannel(const QSharedDataPointer<DvbChannel> &channel_);

private slots:
	void deviceStateChanged();
	void insertPatPmt();
	void pmtSectionChanged(const DvbPmtSection &pmtSection);
	void liveStopped();
	void changeAudioStream(int index);
	void changeSubtitle(int index);
	void prepareTimeShift();
	void startTimeShift();
	void showOsd();
	void toggleOsd();
	void osdTimeout();
	void osdKeyPressed(int key);
	void tuneOsdChannel();

private:
	void startDevice();
	void stopDevice();

	DvbManager *manager;
	MediaWidget *mediaWidget;
	OsdWidget *osdWidget;
	DvbLiveViewInternal *internal;

	QSharedDataPointer<DvbChannel> channel;
	DvbDevice *device;
	QList<int> pids;
	QTimer patPmtTimer;
	QTimer fastRetuneTimer;

	int audioPid;
	int subtitlePid;
	QList<int> audioPids;
	QList<int> subtitlePids;

	QString osdChannel;
	QTimer osdChannelTimer;
	QTimer osdTimer;
};

#endif /* DVBLIVEVIEW_H */
