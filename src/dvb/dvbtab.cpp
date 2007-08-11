/*
 * dvbtab.cpp
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QTreeWidget>
#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/DeviceNotifier>
#include <Solid/DvbInterface>
#include <KLocalizedString>
#include <KPageDialog>

#include "../kaffeine.h"
#include "../manager.h"
#include "../mediawidget.h"
#include "dvbdevice.h"
#include "dvbtab.h"
#include "dvbtab.moc"

DvbTab::DvbTab(Manager *manager_) : TabBase(manager_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);

	QSplitter *splitter = new QSplitter(this);
	layout->addWidget(splitter);

	QTreeWidget *channels = new QTreeWidget(splitter);
	channels->setColumnCount(2);
	channels->setHeaderLabels(QStringList("Name") << "Number");

	QWidget *mediaContainer = new QWidget(splitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);

	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);

	QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

DvbTab::~DvbTab()
{
	qDeleteAll(devices);
}

void DvbTab::configureDvb()
{
	KPageDialog *dialog = new KPageDialog(manager->getKaffeine());
	dialog->setCaption(i18n("DVB Settings"));
	dialog->setFaceType(KPageDialog::List);

	int deviceNumber = 1;

	foreach (DvbDevice *device, devices) {
		if (!device->isReady()) {
			continue;
		}

		QWidget *widget = new QWidget();
		QGridLayout *gridLayout = new QGridLayout(widget);

		// name

		gridLayout->addWidget(new QLabel(i18n("<qt><b>Name:</b></qt>")), 0, 0);
		gridLayout->addWidget(new QLabel(device->getFrontendName()), 0, 1);

		// type

		gridLayout->addWidget(new QLabel(i18n("<qt><b>Type:</b></qt>")), 1, 0);

		QString text;
		DvbDevice::TransmissionTypes transmissionTypes = device->getTransmissionTypes();
		if ((transmissionTypes & DvbDevice::DvbC) != 0) {
			text = i18n("DVB-C");
		}
		if ((transmissionTypes & DvbDevice::DvbS) != 0) {
			if (!text.isEmpty()) {
				text += " / ";
			}
			text += i18n("DVB-S");
		}
		if ((transmissionTypes & DvbDevice::DvbT) != 0) {
			if (!text.isEmpty()) {
				text += " / ";
			}
			text += i18n("DVB-T");
		}
		if ((transmissionTypes & DvbDevice::Atsc) != 0) {
			if (!text.isEmpty()) {
				text += " / ";
			}
			text += i18n("ATSC");
		}

		gridLayout->addWidget(new QLabel(text), 1, 1);

		// add page

		QString pageName(i18n("Device %1", deviceNumber));
		++deviceNumber;

		KPageWidgetItem *page = new KPageWidgetItem(widget, pageName);
		page->setIcon(KIcon("media-flash"));
		dialog->addPage(page);
	}

	dialog->exec();
}

void DvbTab::activate()
{
	mediaLayout->addWidget(manager->getMediaWidget());
}

void DvbTab::customEvent(QEvent *)
{
	QObject *notifier = Solid::DeviceNotifier::instance();

	connect(notifier, SIGNAL(deviceAdded(QString)), this, SLOT(componentAdded(QString)));
	connect(notifier, SIGNAL(deviceRemoved(QString)), this, SLOT(componentRemoved(QString)));

	QList<Solid::Device> devices = Solid::Device::listFromType(Solid::DeviceInterface::DvbInterface);
	foreach (const Solid::Device &device, devices) {
		componentAdded(device);
	}
}

void DvbTab::componentAdded(const QString &udi)
{
	componentAdded(Solid::Device(udi));
}

void DvbTab::componentRemoved(const QString &/*udi*/)
{
	// FIXME Solid::Device::as<DvbInterface>() is always NULL
	// FIXME we need to keep our own list of udis :(
}

void DvbTab::componentAdded(const Solid::Device &component)
{
	const Solid::DvbInterface *dvbInterface = component.as<Solid::DvbInterface>();

	if (dvbInterface == NULL) {
		return;
	}

	int adapter = dvbInterface->deviceAdapter();
	int index = dvbInterface->deviceIndex();

	if ((adapter < 0) || (index < 0)) {
		kWarning() << k_funcinfo << "couldn't determine adapter and/or index for" << component.udi();
		return;
	}

	foreach (DvbDevice *device, devices) {
		if ((device->getAdapter() == adapter) && (device->getIndex() == index)) {
			device->componentAdded(component);
			return;
		}
	}

	DvbDevice *device = new DvbDevice(adapter, index);
	device->componentAdded(component);
	devices.append(device);
}
