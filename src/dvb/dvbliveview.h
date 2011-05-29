/*
 * dvbliveview.h
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include <QTimer>
#include "../mediawidget.h"
#include "dvbchannel.h"

class DvbDevice;
class DvbLiveViewInternal;
class DvbManager;

class DvbLiveView : public QObject
{
	Q_OBJECT
public:
	DvbLiveView(DvbManager *manager_, QObject *parent);
	~DvbLiveView();

	const DvbSharedChannel &getChannel() const;
	DvbDevice *getDevice() const;

	void playChannel(const DvbSharedChannel &channel_);

public slots:
	void toggleOsd();

signals:
	void previous();
	void next();

private slots:
	void pmtSectionChanged(const QByteArray &pmtSectionData);
	void insertPatPmt();
	void deviceStateChanged();
	void showOsd();
	void osdTimeout();

	void currentAudioStreamChanged(int currentAudioStream);
	void currentSubtitleChanged(int currentSubtitle);
	void replay();
	void playbackFinished();
	void playbackStatusChanged(MediaWidget::PlaybackStatus playbackStatus);

private:
	void startDevice();
	void stopDevice();
	void updatePids(bool forcePatPmtUpdate = false);

	DvbManager *manager;
	MediaWidget *mediaWidget;
	OsdWidget *osdWidget;
	DvbLiveViewInternal *internal;

	DvbSharedChannel channel;
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
