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
#include <QMessageBox>
#include <QSplitter>
#include <QTreeWidget>

#include <KLocalizedString>

#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/DeviceNotifier>
#include <Solid/DvbInterface>
using namespace Solid;

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
	QMessageBox::information(manager->getKaffeine(), "Configure DVB", "Test");
}

void DvbTab::internalActivate()
{
	mediaLayout->addWidget(manager->getMediaWidget());
}

void DvbTab::customEvent(QEvent *)
{
	QObject *notifier = DeviceNotifier::instance();

	connect(notifier, SIGNAL(deviceAdded(QString)), this, SLOT(deviceAdded(QString)));
	connect(notifier, SIGNAL(deviceRemoved(QString)), this, SLOT(deviceRemoved(QString)));

	QList<Device> devices = Device::listFromType(DeviceInterface::DvbInterface);
	foreach (const Device &device, devices) {
		const DvbInterface *dvbDevice = device.as<DvbInterface>();
		if (dvbDevice != 0) {
			componentAdded(dvbDevice);
		}
	}
}

void DvbTab::deviceAdded(const QString &udi)
{
	const DvbInterface *dvbDevice = Device(udi).as<DvbInterface>();
	if (dvbDevice != 0) {
		componentAdded(dvbDevice);
	}
}

void DvbTab::deviceRemoved(const QString &)
{
	// FIXME
}

void DvbTab::componentAdded(const DvbInterface *component)
{
	int adapter = component->deviceAdapter();
	int index = component->deviceIndex();

	if ((adapter < 0) || (index < 0)) {
		// FIXME how to get udi?
		kWarning() << k_lineinfo << "couldn't determine adapter and/or index for \n";
		return;
	}

	foreach (DvbDevice *device, devices) {
		if ((device->getAdapter() == adapter) && (device->getIndex() == index)) {
			device->componentAdded(component);
			return;
		}
	}

	devices.append(new DvbDevice(adapter, index));
}
