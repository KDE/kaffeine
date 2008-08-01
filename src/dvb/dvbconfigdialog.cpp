/*
 * dvbconfigdialog.cpp
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

#include "dvbconfigdialog.h"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <KLocalizedString>
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbtab.h"

DvbConfigDialog::DvbConfigDialog(DvbTab *dvbTab) : KPageDialog(dvbTab)
{
	setCaption(i18n("DVB Settings"));
	setFaceType(KPageDialog::List);

	DvbManager *manager = dvbTab->getDvbManager();
	int deviceNumber = 1;

	foreach (DvbDevice *device, manager->getDevices()) {
		if (device->getDeviceState() == DvbDevice::DeviceNotReady) {
			continue;
		}

		QWidget *pageWidget = new QWidget();
		QBoxLayout *pageLayout = new QVBoxLayout(pageWidget);

		// general information box

		QString name = device->getFrontendName();
		pageLayout->addWidget(new QLabel(i18n("<qt><b>Name:</b> %1</qt>").arg(name)));

		DvbDevice::TransmissionTypes transmissionTypes = device->getTransmissionTypes();

		if ((transmissionTypes & DvbDevice::DvbS) != 0) {
			DvbSConfigBox *configBox = new DvbSConfigBox(pageWidget, manager, device);
			connect(this, SIGNAL(accepted()), configBox, SLOT(dialogAccepted()));
			pageLayout->addWidget(configBox);
		}

		// manage unused space

		pageLayout->addStretch(1);

		// add page

		QString pageName(i18n("Device %1", deviceNumber));
		++deviceNumber;

		KPageWidgetItem *page = new KPageWidgetItem(pageWidget, pageName);
		page->setIcon(KIcon("media-flash"));
		addPage(page);
	}
}

DvbConfigDialog::~DvbConfigDialog()
{
}

DvbSLnbConfig::DvbSLnbConfig(QPushButton *configureButton_, QComboBox *sourceBox_,
	const DvbSConfig &config_) : QObject(configureButton_), configureButton(configureButton_),
	sourceBox(sourceBox_), config(config_)
{
	connect(sourceBox, SIGNAL(currentIndexChanged(int)), this, SLOT(sourceChanged(int)));
	connect(configureButton, SIGNAL(clicked()), this, SLOT(configureLnb()));

	sourceChanged(sourceBox->currentIndex());
}

DvbSLnbConfig::~DvbSLnbConfig()
{
}

const DvbSConfig &DvbSLnbConfig::getConfig()
{
	if (sourceBox->currentIndex() <= 0) {
		// no source selected
		config.source = QString();
	} else {
		config.source = sourceBox->currentText();
	}

	return config;
}

void DvbSLnbConfig::sourceChanged(int index)
{
	configureButton->setEnabled(index > 0);
}

void DvbSLnbConfig::configureLnb()
{
	KDialog *dialog = new KDialog(configureButton);
	dialog->setCaption(i18n("LNB settings"));
	connect(dialog, SIGNAL(accepted()), this, SLOT(dialogAccepted()));

	QWidget *mainWidget = new QWidget(dialog);
	QGridLayout *gridLayout = new QGridLayout(mainWidget);
	dialog->setMainWidget(mainWidget);

	QButtonGroup *buttonGroup = new QButtonGroup(mainWidget);
	connect(buttonGroup, SIGNAL(buttonClicked(int)), this, SLOT(selectType(int)));

	QRadioButton *radioButton = new QRadioButton(i18n("Universal LNB"), mainWidget);
	buttonGroup->addButton(radioButton, 1);
	gridLayout->addWidget(radioButton, 0, 0, 1, 2);

	radioButton = new QRadioButton(i18n("C-band LNB"), mainWidget);
	buttonGroup->addButton(radioButton, 2);
	gridLayout->addWidget(radioButton, 1, 0, 1, 2);

	radioButton = new QRadioButton(i18n("Custom LNB"), mainWidget);
	buttonGroup->addButton(radioButton, 3);
	gridLayout->addWidget(radioButton, 2, 0, 1, 2);

	QFrame *frame = new QFrame(mainWidget);
	frame->setFrameShape(QFrame::HLine);
	gridLayout->addWidget(frame, 3, 0, 1, 2);

	lowBandLabel = new QLabel(i18n("Low-band frequency (MHz)"), mainWidget);
	gridLayout->addWidget(lowBandLabel, 4, 0);

	switchLabel = new QLabel(i18n("Switch frequency (MHz)"), mainWidget);
	gridLayout->addWidget(switchLabel, 5, 0);

	highBandLabel = new QLabel(i18n("High-band frequency (MHz)"), mainWidget);
	gridLayout->addWidget(highBandLabel, 6, 0);

	lowBandSpinBox = new QSpinBox(mainWidget);
	lowBandSpinBox->setRange(0, 13000);
	lowBandSpinBox->setValue(config.lowBandFrequency / 1000);
	gridLayout->addWidget(lowBandSpinBox, 4, 1);

	switchSpinBox = new QSpinBox(mainWidget);
	switchSpinBox->setRange(0, 13000);
	switchSpinBox->setValue(config.switchFrequency / 1000);
	gridLayout->addWidget(switchSpinBox, 5, 1);

	highBandSpinBox = new QSpinBox(mainWidget);
	highBandSpinBox->setRange(0, 13000);
	highBandSpinBox->setValue(config.highBandFrequency / 1000);
	gridLayout->addWidget(highBandSpinBox, 6, 1);

	int lnbType;
	if ((config.lowBandFrequency == 9750000) && (config.switchFrequency == 11700000) &&
	    (config.highBandFrequency == 10600000)) {
		lnbType = 1;
	} else if ((config.lowBandFrequency == 5150000) && (config.switchFrequency == 0)) {
		lnbType = 2;
	} else {
		lnbType = 3;
	}

	customEnabled = true;
	buttonGroup->button(lnbType)->setChecked(true);
	selectType(lnbType);

	dialog->setModal(true);
	dialog->show();
}

void DvbSLnbConfig::selectType(int type)
{
	bool enableCustom = false;

	switch (type) {
	case 1:
		lowBandSpinBox->setValue(9750);
		switchSpinBox->setValue(11700);
		highBandSpinBox->setValue(10600);
		break;

	case 2:
		lowBandSpinBox->setValue(5150);
		switchSpinBox->setValue(0);
		highBandSpinBox->setValue(0);
		break;

	case 3:
		enableCustom = true;
		break;
	}

	if (customEnabled != enableCustom) {
		if (enableCustom) {
			lowBandLabel->setEnabled(true);
			switchLabel->setEnabled(true);
			highBandLabel->setEnabled(true);
			lowBandSpinBox->setEnabled(true);
			switchSpinBox->setEnabled(true);
			highBandSpinBox->setEnabled(true);
		} else {
			lowBandLabel->setEnabled(false);
			switchLabel->setEnabled(false);
			highBandLabel->setEnabled(false);
			lowBandSpinBox->setEnabled(false);
			switchSpinBox->setEnabled(false);
			highBandSpinBox->setEnabled(false);
		}

		customEnabled = enableCustom;
	}
}

void DvbSLnbConfig::dialogAccepted()
{
	config.lowBandFrequency = lowBandSpinBox->value() * 1000;
	config.switchFrequency = switchSpinBox->value() * 1000;
	config.highBandFrequency = highBandSpinBox->value() * 1000;
}

DvbSConfigBox::DvbSConfigBox(QWidget *parent, DvbManager *manager_, DvbDevice *device_) :
	QGroupBox(parent), manager(manager_), device(device_)
{
	setTitle(i18n("DVB-S"));
	QGridLayout *gridLayout = new QGridLayout(this);

	gridLayout->addWidget(new QLabel(i18n("Tuner timeout (ms):")), 0, 0);

	QSpinBox *spinBox = new QSpinBox(this);
	spinBox->setRange(100, 5000);
	spinBox->setSingleStep(100);
	spinBox->setValue(1500);
	gridLayout->addWidget(spinBox, 0, 1);

	deviceConfig = manager->getDeviceConfig(device);

	for (int i = 1; i <= 4; ++i) {
		QPushButton *pushButton = new QPushButton(i18n("LNB %1 settings").arg(i), this);
		gridLayout->addWidget(pushButton, i, 0);

		QComboBox *comboBox = new QComboBox(this);
		QStringList sources = manager->getScanSources(DvbManager::DvbS);
		comboBox->addItem(i18n("No source"));
		comboBox->addItems(sources);
		gridLayout->addWidget(comboBox, i, 1);

		int lnbNumber = i - 1;
		const DvbSConfig *config = NULL;

		foreach (const DvbConfig &it, deviceConfig.first) {
			if ((it->getTransmissionType() == DvbConfigBase::DvbS) &&
			    (it->getDvbSConfig()->lnbNumber == lnbNumber)) {
				config = it->getDvbSConfig();
				break;
			}
		}

		DvbSLnbConfig *lnbConfig;

		if (config != NULL) {
			comboBox->setCurrentIndex(sources.indexOf(config->source) + 1);
			lnbConfig = new DvbSLnbConfig(pushButton, comboBox, *config);
		} else {
			lnbConfig = new DvbSLnbConfig(pushButton, comboBox, DvbSConfig(lnbNumber));
		}

		lnbConfigs.append(lnbConfig);
	}
}

DvbSConfigBox::~DvbSConfigBox()
{
}

void DvbSConfigBox::dialogAccepted()
{
	if (deviceConfig.second < 0) {
		return;
	}

	for (int i = 0; i < deviceConfig.first.size(); ++i) {
		const DvbConfig &config = deviceConfig.first.at(i);

		if (config->getTransmissionType() == DvbConfigBase::DvbS) {
			deviceConfig.first.removeAt(i);
			--i;
		}
	}

	foreach (DvbSLnbConfig *lnbConfig, lnbConfigs) {
		const DvbSConfig &config = lnbConfig->getConfig();

		if (!config.source.isEmpty()) {
			deviceConfig.first.append(DvbConfig(new DvbSConfig(config)));
		}
	}

	manager->setDeviceConfig(deviceConfig);
}
