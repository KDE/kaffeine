/*
 * dvbtab.h
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

#ifndef DVBTAB_H
#define DVBTAB_H

#include <QTimer>
#include <config-kaffeine.h>
#include "../tabbase.h"
#include "dvbrecording.h"

class QSplitter;
class KAction;
class KActionCollection;
class KMenu;
class DvbChannelView;
class DvbTimeShiftCleaner;
class MediaWidget;

class DvbTab : public TabBase
{
	Q_OBJECT
public:
	DvbTab(KMenu *menu, KActionCollection *collection, MediaWidget *mediaWidget_);
	~DvbTab();

	void playChannel(const QString &nameOrNumber);
	void playLastChannel();

	void toggleOsd();
	void toggleInstantRecord();

	void enableDvbDump();

public slots:
	void osdKeyPressed(int key);
	void mayCloseApplication(bool *ok, QWidget *parent);

private slots:
	void showChannelDialog();
	void showEpgDialog();
	void showRecordingDialog();
	void instantRecord(bool checked);
	void programRemoved(const DvbRecordingKey &recordingKey);
	void configureDvb();
	void tuneOsdChannel();
	void playChannel(const QModelIndex &index);
	void previousChannel();
	void nextChannel();
	void cleanTimeShiftFiles();

private:
	void activate();
	void playChannel(int row);

	MediaWidget *mediaWidget;
	DvbManager *manager;
	KAction *instantRecordAction;
	DvbRecordingKey instantRecordingKey;
	QSplitter *splitter;
	DvbChannelView *channelView;
	QLayout *mediaLayout;
	QString osdChannel;
	QTimer osdChannelTimer;
	QString currentChannel;
	QString lastChannel;

	DvbTimeShiftCleaner *timeShiftCleaner;
};

#ifndef HAVE_DVB
#error HAVE_DVB must be defined
#endif /* HAVE_DVB */

#if HAVE_DVB == 0
inline void DvbTab::playChannel(QString const &) { }
inline void DvbTab::playLastChannel() { }
inline void DvbTab::toggleOsd() { }
inline void DvbTab::toggleInstantRecord() { }
inline void DvbTab::osdKeyPressed(int) { }
#endif /* HAVE_DVB == 0 */

#endif /* DVBTAB_H */
