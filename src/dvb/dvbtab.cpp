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
#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/DeviceNotifier>
#include <Solid/DvbInterface>
#include <KLocalizedString>

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

	devices.append(new DvbDevice(adapter, index));
}
