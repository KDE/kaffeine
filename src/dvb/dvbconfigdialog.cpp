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
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <KComboBox>
#include <KLineEdit>
#include <KLocalizedString>
#include "dvbdevice.h"
#include "dvbmanager.h"

DvbConfigDialog::DvbConfigDialog(QWidget *parent, DvbManager *manager_) : KDialog(parent),
	manager(manager_)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setCaption(i18n("DVB Settings"));

	QTabWidget *tabWidget = new QTabWidget(this);
	setMainWidget(tabWidget);

	// FIXME general options

	int i = 1;

	foreach (const DvbDeviceConfig &deviceConfig, manager->getDeviceConfigs()) {
		DvbConfigPage *configPage = new DvbConfigPage(tabWidget, manager, deviceConfig);
		tabWidget->addTab(configPage, KIcon("video-television"), i18n("Device %1", i));
		configPages.append(configPage);
		++i;
	}

	connect(this, SIGNAL(accepted()), this, SLOT(dialogAccepted()));
}

DvbConfigDialog::~DvbConfigDialog()
{
}

void DvbConfigDialog::dialogAccepted()
{
	QList<QList<DvbConfig> > deviceConfigs;

	foreach (DvbConfigPage *configPage, configPages) {
		deviceConfigs.append(configPage->getConfigs());
	}

	manager->setDeviceConfigs(deviceConfigs);
}

DvbConfigObject::DvbConfigObject(QSpinBox *timeoutSpinBox, KComboBox *sourceBox_,
	KLineEdit *nameEdit_, const QString &defaultName_, DvbConfigBase *config_) :
	QObject(timeoutSpinBox), sourceBox(sourceBox_), nameEdit(nameEdit_),
	defaultName(defaultName_), config(config_)
{
	connect(timeoutSpinBox, SIGNAL(valueChanged(int)), this, SLOT(timeoutChanged(int)));
	connect(sourceBox, SIGNAL(currentIndexChanged(int)), this, SLOT(sourceChanged(int)));
	connect(nameEdit, SIGNAL(editingFinished()), this, SLOT(nameChanged()));

	sourceChanged(sourceBox->currentIndex());
	nameChanged();
}

DvbConfigObject::~DvbConfigObject()
{
}

void DvbConfigObject::timeoutChanged(int timeout)
{
	config->timeout = timeout;
}

void DvbConfigObject::sourceChanged(int index)
{
	if (index <= 0) {
		// no source selected
		nameEdit->setEnabled(false);
		config->scanSource.clear();
	} else {
		nameEdit->setEnabled(true);
		config->scanSource = sourceBox->currentText();
	}
}

void DvbConfigObject::nameChanged()
{
	QString name = nameEdit->text();

	if (name.isEmpty()) {
		nameEdit->setText(defaultName);
		config->name = defaultName;
	} else {
		config->name = name;
	}
}

DvbSConfigObject::DvbSConfigObject(QSpinBox *timeoutSpinBox, KComboBox *sourceBox_,
	QPushButton *configureButton_, DvbSConfig *config_) : QObject(timeoutSpinBox),
	sourceBox(sourceBox_), configureButton(configureButton_), config(config_)
{
	connect(timeoutSpinBox, SIGNAL(valueChanged(int)), this, SLOT(timeoutChanged(int)));
	connect(sourceBox, SIGNAL(currentIndexChanged(int)), this, SLOT(sourceChanged(int)));
	connect(configureButton, SIGNAL(clicked()), this, SLOT(configureLnb()));

	sourceChanged(sourceBox->currentIndex());
}

DvbSConfigObject::~DvbSConfigObject()
{
}

void DvbSConfigObject::timeoutChanged(int timeout)
{
	config->timeout = timeout;
}

void DvbSConfigObject::sourceChanged(int index)
{
	if (index <= 0) {
		// no source selected
		configureButton->setEnabled(false);
		config->name.clear();
		config->scanSource.clear();
	} else {
		configureButton->setEnabled(true);
		config->name = sourceBox->currentText();
		config->scanSource = config->name;
	}
}

void DvbSConfigObject::configureLnb()
{
	KDialog *dialog = new KDialog(configureButton);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setCaption(i18n("LNB settings"));

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

	radioButton = new QRadioButton(i18n("C-band multipoint LNB"), mainWidget);
	buttonGroup->addButton(radioButton, 3);
	gridLayout->addWidget(radioButton, 2, 0, 1, 2);

	radioButton = new QRadioButton(i18n("Custom LNB"), mainWidget);
	buttonGroup->addButton(radioButton, 4);
	gridLayout->addWidget(radioButton, 3, 0, 1, 2);

	QFrame *frame = new QFrame(mainWidget);
	frame->setFrameShape(QFrame::HLine);
	gridLayout->addWidget(frame, 4, 0, 1, 2);

	lowBandLabel = new QLabel(mainWidget);
	gridLayout->addWidget(lowBandLabel, 5, 0);

	switchLabel = new QLabel(i18n("Switch frequency (MHz)"), mainWidget);
	gridLayout->addWidget(switchLabel, 6, 0);

	highBandLabel = new QLabel(mainWidget);
	gridLayout->addWidget(highBandLabel, 7, 0);

	lowBandSpinBox = new QSpinBox(mainWidget);
	lowBandSpinBox->setRange(0, 13000);
	lowBandSpinBox->setValue(config->lowBandFrequency / 1000);
	gridLayout->addWidget(lowBandSpinBox, 5, 1);

	switchSpinBox = new QSpinBox(mainWidget);
	switchSpinBox->setRange(0, 13000);
	switchSpinBox->setValue(config->switchFrequency / 1000);
	gridLayout->addWidget(switchSpinBox, 6, 1);

	highBandSpinBox = new QSpinBox(mainWidget);
	highBandSpinBox->setRange(0, 13000);
	highBandSpinBox->setValue(config->highBandFrequency / 1000);
	gridLayout->addWidget(highBandSpinBox, 7, 1);

	gridLayout->addItem(new QSpacerItem(0, 0), 8, 0, 1, 2);
	gridLayout->setRowStretch(8, 1);

	int lnbType;

	if ((config->lowBandFrequency == 9750000) && (config->switchFrequency == 11700000) &&
	    (config->highBandFrequency == 10600000)) {
		lnbType = 1;
	} else if ((config->lowBandFrequency == 5150000) && (config->switchFrequency == 0) &&
		   (config->highBandFrequency == 0)) {
		lnbType = 2;
	} else if ((config->lowBandFrequency == 5750000) && (config->switchFrequency == 0) &&
		   (config->highBandFrequency == 5150000)) {
		lnbType = 3;
	} else {
		lnbType = 4;
	}

	currentType = 4;
	buttonGroup->button(lnbType)->setChecked(true);
	selectType(lnbType);

	connect(dialog, SIGNAL(accepted()), this, SLOT(dialogAccepted()));

	dialog->setModal(true);
	dialog->show();
}

void DvbSConfigObject::selectType(int type)
{
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
		lowBandSpinBox->setValue(5750);
		switchSpinBox->setValue(0);
		highBandSpinBox->setValue(5150);
		break;
	}

	if (switchSpinBox->value() != 0) {
		if (currentType != 1) {
			lowBandLabel->setText(i18n("Low band LOF (MHz)"));

			switchLabel->show();
			switchSpinBox->show();

			highBandLabel->setText(i18n("High band LOF (MHz)"));
			highBandLabel->show();
			highBandSpinBox->show();
		}
	} else if (highBandSpinBox->value() != 0) {
		if (currentType != 3) {
			lowBandLabel->setText(i18n("Horizontal LOF (MHz)"));

			switchLabel->hide();
			switchSpinBox->hide();

			highBandLabel->setText(i18n("Vertical LOF (MHz)"));
			highBandLabel->show();
			highBandSpinBox->show();
		}
	} else {
		if (currentType != 2) {
			lowBandLabel->setText(i18n("LOF (MHz)"));

			switchLabel->hide();
			switchSpinBox->hide();

			highBandLabel->hide();
			highBandSpinBox->hide();
		}
	}

	if ((currentType == 4) != (type == 4)) {
		if (type == 4) {
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
	}

	currentType = type;
}

void DvbSConfigObject::dialogAccepted()
{
	config->lowBandFrequency = lowBandSpinBox->value() * 1000;
	config->switchFrequency = switchSpinBox->value() * 1000;
	config->highBandFrequency = highBandSpinBox->value() * 1000;
}

DvbConfigPage::DvbConfigPage(QWidget *parent, DvbManager *manager,
	const DvbDeviceConfig &deviceConfig) : QWidget(parent)
{
	QGridLayout *gridLayout = new QGridLayout(this);

	QString name = deviceConfig.frontendName;
	gridLayout->addWidget(new QLabel(i18n("Name: %1", name)), 0, 0, 1, 2);

	int i = 0;
	DvbDevice *device = deviceConfig.device;

	if (device == NULL) {
		QFrame *frame = new QFrame(this);
		frame->setFrameShape(QFrame::HLine);
		gridLayout->addWidget(frame, ++i, 0, 1, 2);

		gridLayout->addWidget(new QLabel(i18n("device not connected")), ++i, 0, 1, 2);

		gridLayout->addItem(new QSpacerItem(0, 0), ++i, 0, 1, 2);
		gridLayout->setRowStretch(i, 1);

		return;
	}

	DvbDevice::TransmissionTypes transmissionTypes = device->getTransmissionTypes();

	if ((transmissionTypes & DvbDevice::DvbC) != 0) {
		DvbCConfig *config = NULL;

		foreach (const DvbConfig &it, deviceConfig.configs) {
			if (it->getTransmissionType() == DvbConfigBase::DvbC) {
				config = new DvbCConfig(*it->getDvbCConfig());
				break;
			}
		}

		if (config == NULL) {
			config = new DvbCConfig();
			config->timeout = 1500;
		}

		configs.append(DvbConfig(config));
		createObject(gridLayout, i, manager, config);
	}

	if ((transmissionTypes & DvbDevice::DvbS) != 0) {
		QFrame *frame = new QFrame(this);
		frame->setFrameShape(QFrame::HLine);
		gridLayout->addWidget(frame, ++i, 0, 1, 2);

		gridLayout->addWidget(new QLabel(i18n("DVB-S")), ++i, 0, 1, 2, Qt::AlignCenter);

		gridLayout->addWidget(new QLabel(i18n("Tuner timeout (ms):")), ++i, 0);

		QSpinBox *spinBox = new QSpinBox(this);
		spinBox->setRange(100, 5000);
		spinBox->setSingleStep(100);
		spinBox->setValue(1500);
		gridLayout->addWidget(spinBox, i, 1);

		for (int lnbNumber = 0; lnbNumber < 4; ++lnbNumber) {
			DvbSConfig *config = NULL;

			for (int j = 0;; ++j) {
				if (j == deviceConfig.configs.size()) {
					config = new DvbSConfig();
					config->timeout = 1500;
					config->lnbNumber = lnbNumber;
					config->lowBandFrequency = 9750000;
					config->switchFrequency = 11700000;
					config->highBandFrequency = 10600000;
					break;
				}

				const DvbConfig &it = deviceConfig.configs.at(j);

				if ((it->getTransmissionType() == DvbConfigBase::DvbS) &&
				    (it->getDvbSConfig()->lnbNumber == lnbNumber)) {
					config = new DvbSConfig(*it->getDvbSConfig());
					spinBox->setValue(config->timeout);
					break;
				}
			}

			configs.append(DvbConfig(config));

			QPushButton *pushButton =
				new QPushButton(i18n("LNB %1 settings", lnbNumber + 1), this);
			gridLayout->addWidget(pushButton, ++i, 0);

			KComboBox *comboBox = new KComboBox(this);
			QStringList sources = manager->getScanSources(DvbManager::DvbS);
			comboBox->addItem(i18n("No source"));
			comboBox->addItems(sources);
			comboBox->setCurrentIndex(sources.indexOf(config->scanSource) + 1);
			gridLayout->addWidget(comboBox, i, 1);

			new DvbSConfigObject(spinBox, comboBox, pushButton, config);
		}
	}

	if ((transmissionTypes & DvbDevice::DvbT) != 0) {
		DvbTConfig *config = NULL;

		foreach (const DvbConfig &it, deviceConfig.configs) {
			if (it->getTransmissionType() == DvbConfigBase::DvbT) {
				config = new DvbTConfig(*it->getDvbTConfig());
				break;
			}
		}

		if (config == NULL) {
			config = new DvbTConfig();
			config->timeout = 1500;
		}

		configs.append(DvbConfig(config));
		createObject(gridLayout, i, manager, config);
	}

	if ((transmissionTypes & DvbDevice::Atsc) != 0) {
		AtscConfig *config = NULL;

		foreach (const DvbConfig &it, deviceConfig.configs) {
			if (it->getTransmissionType() == DvbConfigBase::Atsc) {
				config = new AtscConfig(*it->getAtscConfig());
				break;
			}
		}

		if (config == NULL) {
			config = new AtscConfig();
			config->timeout = 1500;
		}

		configs.append(DvbConfig(config));
		createObject(gridLayout, i, manager, config);
	}

	gridLayout->addItem(new QSpacerItem(0, 0), ++i, 0, 1, 2);
	gridLayout->setRowStretch(i, 1);
}

DvbConfigPage::~DvbConfigPage()
{
}

QList<DvbConfig> DvbConfigPage::getConfigs()
{
	for (int i = 0; i < configs.count(); ++i) {
		const DvbConfig &config = configs.at(i);

		if (config->name.isEmpty() || config->scanSource.isEmpty()) {
			configs.removeAt(i);
			--i;
		}
	}

	return configs;
}

void DvbConfigPage::createObject(QGridLayout *gridLayout, int &i, DvbManager *manager,
	DvbConfigBase *config)
{
	QFrame *frame = new QFrame(this);
	frame->setFrameShape(QFrame::HLine);
	gridLayout->addWidget(frame, ++i, 0, 1, 2);

	QString defaultName;
	QStringList sources;

	switch (config->getTransmissionType()) {
	case DvbConfigBase::DvbC:
		defaultName = i18n("Cable");
		sources = manager->getScanSources(DvbManager::DvbC);
		gridLayout->addWidget(new QLabel(i18n("DVB-C")), ++i, 0, 1, 2, Qt::AlignCenter);
		break;
	case DvbConfigBase::DvbS:
		Q_ASSERT(false);
		break;
	case DvbConfigBase::DvbT:
		defaultName = i18n("Terrestrial");
		sources = manager->getScanSources(DvbManager::DvbT);
		gridLayout->addWidget(new QLabel(i18n("DVB-T")), ++i, 0, 1, 2, Qt::AlignCenter);
		break;
	case DvbConfigBase::Atsc:
		defaultName = i18n("Atsc");
		sources = manager->getScanSources(DvbManager::Atsc);
		gridLayout->addWidget(new QLabel(i18n("ATSC")), ++i, 0, 1, 2, Qt::AlignCenter);
		break;
	}

	gridLayout->addWidget(new QLabel(i18n("Tuner timeout (ms):")), ++i, 0);

	QSpinBox *spinBox = new QSpinBox(this);
	spinBox->setRange(100, 5000);
	spinBox->setSingleStep(100);
	spinBox->setValue(config->timeout);
	gridLayout->addWidget(spinBox, i, 1);

	gridLayout->addWidget(new QLabel(i18n("Source:")), ++i, 0);

	KComboBox *comboBox = new KComboBox(this);
	comboBox->addItem(i18n("No source"));
	comboBox->addItems(sources);
	comboBox->setCurrentIndex(sources.indexOf(config->scanSource) + 1);
	gridLayout->addWidget(comboBox, i, 1);

	gridLayout->addWidget(new QLabel(i18n("Name:")), ++i, 0);

	KLineEdit *lineEdit = new KLineEdit(this);
	lineEdit->setText(config->name);
	gridLayout->addWidget(lineEdit, i, 1);

	new DvbConfigObject(spinBox, comboBox, lineEdit, defaultName, config);
}
