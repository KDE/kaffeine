/*
 * dvbscandialog.h
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

#ifndef DVBSCANDIALOG_H
#define DVBSCANDIALOG_H

#include <QLabel>
#include <QTimer>
#include <KDialog>

class QCheckBox;
class QProgressBar;
class QTreeView;
class KComboBox;
class KLed;
class DvbChannelModel;
class DvbDevice;
class DvbGradProgress;
class DvbManager;
class DvbPreviewChannel;
class DvbPreviewChannelTableModel;
class DvbScan;

class DvbScanDialog : public KDialog
{
	Q_OBJECT
public:
	DvbScanDialog(DvbManager *manager_, QWidget *parent);
	~DvbScanDialog();

private slots:
	void scanButtonClicked(bool checked);
	void dialogAccepted();

	void foundChannels(const QList<DvbPreviewChannel> &channels);
	void scanFinished();

	void updateStatus();

	void addSelectedChannels();
	void addFilteredChannels();

private:
	void setDevice(DvbDevice *newDevice);

	DvbManager *manager;
	DvbChannelModel *channelModel;
	KComboBox *sourceBox;
	QPushButton *scanButton;
	QProgressBar *progressBar;
	DvbGradProgress *signalWidget;
	DvbGradProgress *snrWidget;
	KLed *tunedLed;
	QCheckBox *ftaCheckBox;
	QCheckBox *radioCheckBox;
	QCheckBox *tvCheckBox;
	QCheckBox *providerCheckBox;
	QStringList providers;
	KComboBox *providerBox;
	DvbPreviewChannelTableModel *previewModel;
	QTreeView *scanResultsView;

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
