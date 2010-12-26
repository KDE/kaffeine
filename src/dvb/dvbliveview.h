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

#include <QExplicitlySharedDataPointer>
#include <QTimer>

class DvbChannel;
class DvbDevice;
class DvbLiveViewInternal;
class DvbManager;
class DvbPmtSection;
class MediaWidget;
class OsdWidget;

class DvbLiveView : public QObject
{
	Q_OBJECT
public:
	DvbLiveView(DvbManager *manager_, QObject *parent);
	~DvbLiveView();

	const DvbChannel *getChannel() const;
	DvbDevice *getDevice() const;

	void playChannel(const DvbChannel *channel_);

public slots:
	void toggleOsd();

private slots:
	void pmtSectionChanged(const DvbPmtSection &pmtSection);
	void insertPatPmt();
	void deviceStateChanged();
	void changeAudioStream(int index);
	void changeSubtitle(int index);
	void prepareTimeShift();
	void startTimeShift();
	void showOsd();
	void osdTimeout();
	void liveStopped();

private:
	void startDevice();
	void stopDevice();
	void updatePids(bool forcePatPmtUpdate = false);

	DvbManager *manager;
	MediaWidget *mediaWidget;
	OsdWidget *osdWidget;
	DvbLiveViewInternal *internal;

	QExplicitlySharedDataPointer<const DvbChannel> channel;
	DvbDevice *device;
	QList<int> pids;
	QTimer patPmtTimer;
	QTimer osdTimer;

	int videoPid;
	int audioPid;
	int subtitlePid;
	QList<int> audioPids;
	QList<int> subtitlePids;
};

#endif /* DVBLIVEVIEW_H */
