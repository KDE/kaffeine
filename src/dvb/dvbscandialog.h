/*
 * dvbscandialog.h
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

#ifndef DVBSCANDIALOG_H
#define DVBSCANDIALOG_H

#include <QLabel>
#include <QTimer>
#include <KDialog>

class DvbChannel;
class DvbChannelModel;
class DvbDevice;
class DvbPreviewChannel;
class DvbPreviewChannelModel;
class DvbScan;
class DvbSectionData;
class DvbTab;
class Ui_DvbScanDialog;

class DvbScanDialog : public KDialog
{
	Q_OBJECT
public:
	explicit DvbScanDialog(DvbTab *dvbTab_);
	~DvbScanDialog();

private slots:
	void scanButtonClicked(bool checked);
	void dialogAccepted();

	void foundChannels(const QList<DvbPreviewChannel> &channels);
	void scanProgress(int percentage);
	void scanFinished();

	void updateStatus();

	void addSelectedChannels();
	void addFilteredChannels();
	void deleteAllChannels();

private:
	void addUpdateChannels(const QList<const DvbPreviewChannel *> &channelList);
	void setDevice(DvbDevice *newDevice);

	DvbTab *dvbTab;
	Ui_DvbScanDialog *ui;
	DvbChannelModel *channelModel;
	DvbPreviewChannelModel *previewModel;

	DvbDevice *device;
	QTimer statusTimer;
	bool isLive;

	DvbScan *internal;
};

class DvbGradProgress : public QLabel
{
public:
	explicit DvbGradProgress(QWidget *parent);
	~DvbGradProgress();

	void setValue(int value_);

protected:
	void paintEvent(QPaintEvent *event);

private:
	int value;
};

#endif /* DVBSCANDIALOG_H */
