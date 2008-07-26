/*
 * dvbconfigdialog.h
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBCONFIGDIALOG_H
#define DVBCONFIGDIALOG_H

#include <QGroupBox>
#include <KPageDialog>
#include "dvbconfig.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class DvbDevice;
class DvbManager;
class DvbTab;

class DvbConfigDialog : public KPageDialog
{
public:
	explicit DvbConfigDialog(DvbTab *dvbTab_);
	~DvbConfigDialog();
};

class DvbSLnbConfig : public QObject
{
	Q_OBJECT
public:
	DvbSLnbConfig(QPushButton *configureButton_, QComboBox *sourceBox_,
		const DvbSConfig &config_);
	~DvbSLnbConfig();

	void setEnabled(bool enabled);

	DvbSConfig getConfig();

private slots:
	void configureLnb();
	void selectType(int type);
	void dialogAccepted();

private:
	QPushButton *configureButton;
	QComboBox *sourceBox;
	DvbSConfig config;

	QLabel *lowBandLabel;
	QLabel *switchLabel;
	QLabel *highBandLabel;
	QSpinBox *lowBandSpinBox;
	QSpinBox *switchSpinBox;
	QSpinBox *highBandSpinBox;
	bool customEnabled;
};

class DvbSConfigBox : public QGroupBox
{
	Q_OBJECT
public:
	DvbSConfigBox(QWidget *parent, DvbDevice *device_, DvbManager *manager);
	~DvbSConfigBox();

public slots:
	void dialogAccepted();

private slots:
	void lnbCountChanged(int count);

private:
	DvbDevice *device;
	QList<DvbSLnbConfig *> lnbConfigs;
};

#endif /* DVBCONFIGDIALOG_H */
