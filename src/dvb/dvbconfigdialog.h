/*
 * dvbconfigdialog.h
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

#ifndef DVBCONFIGDIALOG_H
#define DVBCONFIGDIALOG_H

#include <QBoxLayout>
#include <QDialog>
#include <QGridLayout>
#include <QLayoutItem>

class QBoxLayout;
class QButtonGroup;
class QCheckBox;
class QGridLayout;
class QLabel;
class QProgressBar;
class QSpinBox;
class QTreeWidget;
class KComboBox;
class QComboBox;
class KJob;
class QLineEdit;
class QTabWidget;
namespace KIO
{
class Job;
class TransferJob;
}
class DvbConfig;
class DvbConfigBase;
class DvbConfigPage;
class DvbDeviceConfig;
class DvbManager;
class DvbSConfigObject;
class DvbSLnbConfigObject;

class RegexInputLine : public QObject
{
	Q_OBJECT

public:
	int index;
	QLineEdit *lineEdit;
	QSpinBox *spinBox;
	QCheckBox *checkBox;

};

class DvbConfigDialog : public QDialog
{
	Q_OBJECT
public:
	DvbConfigDialog(DvbManager *manager_, QWidget *parent);
	~DvbConfigDialog();

signals:
	void removeRegex(DvbConfigPage *page);

private slots:
	void changeRecordingFolder();
	void changeTimeShiftFolder();
	void updateScanFile();
	void openScanFile();
	void newRegex();
	void removeRegex();
	void latitudeChanged(const QString &text);
	void longitudeChanged(const QString &text);
	void namingFormatChanged(QString text);
	void moveLeft(DvbConfigPage *configPage);
	void moveRight(DvbConfigPage *configPage);
	void remove(DvbConfigPage *configPage);

private:
	static double toLatitude(const QString &text, bool *ok);
	static double toLongitude(const QString &text, bool *ok);
	void removeWidgets(QGridLayout *layout, int row, int column, bool deleteWidgets);
	void initRegexButtons(QGridLayout *buttonGrid);
	//void deleteChildWidgets(QLayoutItem *item);

	void accept();

	DvbManager *manager;
	QTabWidget *tabWidget;
	QLineEdit *recordingFolderEdit;
	QLineEdit *timeShiftFolderEdit;
	QSpinBox *beginMarginBox;
	QSpinBox *endMarginBox;
	QLineEdit *namingFormat;
	QCheckBox *override6937CharsetBox;
	QCheckBox *createInfoFileBox;
	QCheckBox *scanWhenIdleBox;
	QLineEdit *latitudeEdit;
	QLineEdit *longitudeEdit;
	QPixmap validPixmap;
	QPixmap invalidPixmap;
	QLabel *latitudeValidLabel;
	QLabel *longitudeValidLabel;
	QLabel *namingFormatValidLabel;
	QList<DvbConfigPage *> configPages;
	QLineEdit *actionAfterRecordingLineEdit;
	QGridLayout *regexGrid;
	QList<RegexInputLine *> regexInputList;
};

class DvbScanFileDownloadDialog : public QDialog
{
	Q_OBJECT
public:
	DvbScanFileDownloadDialog(DvbManager *manager_, QWidget *parent);
	~DvbScanFileDownloadDialog();

private slots:
	void progressChanged(KJob *, unsigned long percent);
	void dataArrived(KIO::Job *, const QByteArray &data);
	void jobFinished();

private:
	DvbManager *manager;
	QProgressBar *progressBar;
	QLabel *label;
	KIO::TransferJob *job;
	QByteArray scanData;
	QVBoxLayout *mainLayout;
};

class DvbConfigPage : public QWidget
{
	Q_OBJECT
public:
	DvbConfigPage(QWidget *parent, DvbManager *manager, const DvbDeviceConfig *deviceConfig_);
	~DvbConfigPage();

	void setMoveLeftEnabled(bool enabled);
	void setMoveRightEnabled(bool enabled);

	const DvbDeviceConfig *getDeviceConfig() const;
	QList<DvbConfig> getConfigs();

signals:
	void moveLeft(DvbConfigPage *page);
	void moveRight(DvbConfigPage *page);
	void remove(DvbConfigPage *page);
	void resetConfig();

private slots:
	void moveLeft();
	void moveRight();
	void removeConfig();

private:
	void addHSeparator(const QString &title);

	const DvbDeviceConfig *deviceConfig;
	QBoxLayout *boxLayout;
	QPushButton *moveLeftButton;
	QPushButton *moveRightButton;
	QList<DvbConfig> configs;
	DvbSConfigObject *dvbSObject;
};

class DvbConfigObject : public QObject
{
	Q_OBJECT
public:
	DvbConfigObject(QWidget *parent, QBoxLayout *layout, DvbManager *manager,
		DvbConfigBase *config_);
	~DvbConfigObject();

private slots:
	void timeoutChanged(int timeout);
	void sourceChanged(int index);
	void nameChanged();
	void resetConfig();

private:
	DvbConfigBase *config;
	QString defaultName;
	QSpinBox *timeoutBox;
	KComboBox *sourceBox;
	QLineEdit *nameEdit;
};

class DvbSConfigObject : public QObject
{
	Q_OBJECT
public:
	DvbSConfigObject(QWidget *parent_, QBoxLayout *boxLayout, DvbManager *manager,
		const QList<DvbConfig> &configs, bool dvbS2);
	~DvbSConfigObject();

	void appendConfigs(QList<DvbConfig> &list);

signals:
	void setDiseqcVisible(bool visible);
	void setRotorVisible(bool visible); // common parts of usals / positions ui
	void setUsalsVisible(bool visible); // usals-specific parts of ui
	void setPositionsVisible(bool visible); // positions-specific parts of ui

private slots:
	void configChanged(int index);
	void addSatellite();
	void removeSatellite();
	void resetConfig();

private:
	DvbConfigBase *createConfig(int lnbNumber);

	QWidget *parent;
	DvbConfigBase *lnbConfig;
	QList<DvbConfig> diseqcConfigs;
	QList<DvbSLnbConfigObject *> lnbConfigs;
	QStringList sources;
	QGridLayout *layout;
	QSpinBox *timeoutBox;
	KComboBox *configBox;
	KComboBox *sourceBox;
	QSpinBox *rotorSpinBox;
	QTreeWidget *satelliteView;
};

class DvbSLnbConfigObject : public QObject
{
	Q_OBJECT
public:
	DvbSLnbConfigObject(QSpinBox *timeoutSpinBox, KComboBox *sourceBox_,
		QPushButton *configureButton_, DvbConfigBase *config_);
	~DvbSLnbConfigObject();

	void resetConfig();

private slots:
	void timeoutChanged(int value);
	void sourceChanged(int index);
	void configure();
	void selectType(int type);
	void dialogAccepted();

private:
	KComboBox *sourceBox;
	QPushButton *configureButton;
	DvbConfigBase *config;

	QButtonGroup *lnbSelectionGroup;
	QLabel *lowBandLabel;
	QLabel *switchLabel;
	QLabel *highBandLabel;
	QSpinBox *lowBandSpinBox;
	QSpinBox *switchSpinBox;
	QSpinBox *highBandSpinBox;
	int currentType;
};

#endif /* DVBCONFIGDIALOG_H */
