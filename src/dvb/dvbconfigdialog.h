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

#include <KPageDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class DvbConfig;
class DvbConfigBase;
class DvbConfigPage;
class DvbDeviceConfig;
class DvbManager;
class DvbSConfig;
class DvbTab;

class DvbConfigDialog : public KPageDialog
{
	Q_OBJECT
public:
	explicit DvbConfigDialog(DvbTab *dvbTab_);
	~DvbConfigDialog();

private slots:
	void dialogAccepted();

private:
	DvbManager *manager;
	QList<DvbConfigPage *> configPages;
};

class DvbConfigObject : public QObject
{
	Q_OBJECT
public:
	DvbConfigObject(QSpinBox *timeoutSpinBox, QComboBox *sourceBox_, QLineEdit *nameEdit_,
		const QString &defaultName_, DvbConfigBase *config_);
	~DvbConfigObject();

private slots:
	void timeoutChanged(int timeout);
	void sourceChanged(int index);
	void nameChanged();

private:
	QComboBox *sourceBox;
	QLineEdit *nameEdit;
	QString defaultName;
	DvbConfigBase *config;
};

class DvbSConfigObject : public QObject
{
	Q_OBJECT
public:
	DvbSConfigObject(QSpinBox *timeoutSpinBox, QComboBox *sourceBox_,
		QPushButton *configureButton_, DvbSConfig *config_);
	~DvbSConfigObject();

private slots:
	void timeoutChanged(int timeout);
	void sourceChanged(int index);
	void configureLnb();
	void selectType(int type);
	void dialogAccepted();

private:
	QComboBox *sourceBox;
	QPushButton *configureButton;
	DvbSConfig *config;

	QLabel *lowBandLabel;
	QLabel *switchLabel;
	QLabel *highBandLabel;
	QSpinBox *lowBandSpinBox;
	QSpinBox *switchSpinBox;
	QSpinBox *highBandSpinBox;
	bool customEnabled;
};

class DvbConfigPage : public QWidget
{
public:
	DvbConfigPage(DvbManager *manager, const DvbDeviceConfig &deviceConfig);
	~DvbConfigPage();

	QList<DvbConfig> getConfigs();

private:
	QList<DvbConfig> configs;
};

#endif /* DVBCONFIGDIALOG_H */
