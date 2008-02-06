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
#include <QSplitter>
#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/DeviceNotifier>
#include <Solid/DvbInterface>
#include <KDebug>
#include <KLineEdit>
#include <KLocalizedString>
#include <KPageDialog>

#include "../engine.h"
#include "../kaffeine.h"
#include "../manager.h"
#include "../mediawidget.h"
#include "dvbchannel.h"
#include "dvbconfig.h"
#include "dvbdevice.h"
#include "dvbscan.h"
#include "dvbtab.h"

// FIXME - DvbStream is just a demo hack

class DvbStream : public DvbLiveFeed, public DvbPidFilter
{
public:
	DvbStream(DvbDevice *device_) : device(device_), bufferPos(0)
	{
		buffer.resize(188 * 64);
	}

	~DvbStream() { }

private:
	void setPaused(bool /*paused*/)
	{
		// timeshift etc
	}

	void stop()
	{
		device->stopDevice();
	}

	void processData(const char data[188])
	{
		// FIXME too hacky
		memcpy(buffer.data() + bufferPos, data, 188);
		bufferPos += 188;
		if (bufferPos == (188 * 64)) {
			emit writeData(buffer);
			buffer.clear();
			buffer.resize(188 * 64);
			bufferPos = 0;
		}
	}

	DvbDevice *device;
	QByteArray buffer;
	int bufferPos;
};

DvbTab::DvbTab(Manager *manager_) : TabBase(manager_), dvbStream(NULL)
{
	QBoxLayout *widgetLayout = new QHBoxLayout(this);
	widgetLayout->setMargin(0);

	QSplitter *splitter = new QSplitter(this);
	widgetLayout->addWidget(splitter);

	QWidget *leftSideWidget = new QWidget(splitter);
	QBoxLayout *leftSideLayout = new QVBoxLayout(leftSideWidget);
	leftSideLayout->setMargin(3);

	QWidget *searchBoxWidget = new QWidget(leftSideWidget);
	QBoxLayout *searchBoxLayout = new QHBoxLayout(searchBoxWidget);
	searchBoxLayout->setMargin(0);
	leftSideLayout->addWidget(searchBoxWidget);

	searchBoxLayout->addWidget(new QLabel(i18n("Search:"), searchBoxWidget));

	KLineEdit *lineEdit = new KLineEdit(searchBoxWidget);
	lineEdit->setClearButtonShown(true);
	searchBoxLayout->addWidget(lineEdit);

	// FIXME - just a demo hack
	QList<DvbSharedChannel> list;
	list.append(DvbSharedChannel(new DvbChannel("sample", 1, "", new DvbSTransponder(DvbSTransponder::Horizontal, 11953000, 27500000, DvbSTransponder::FecAuto), 0, 0)));
	list.append(DvbSharedChannel(new DvbChannel("channel", 3, "", new DvbSTransponder(DvbSTransponder::Horizontal, 11953000, 27500000, DvbSTransponder::FecAuto), 0, 0)));
	list.append(DvbSharedChannel(new DvbChannel("test", 2, "", new DvbSTransponder(DvbSTransponder::Horizontal, 11953000, 27500000, DvbSTransponder::FecAuto), 0, 0)));
	channelModel = new DvbChannelModel(list, this);

	// FIXME - just a demo hack
	DvbChannelView *channels = new DvbChannelView(leftSideWidget);
	channels->setModel(channelModel);
	connect(channels, SIGNAL(activated(QModelIndex)), this, SLOT(channelActivated()));
	connect(lineEdit, SIGNAL(textChanged(QString)), channels, SLOT(setFilterRegExp(QString)));
	leftSideLayout->addWidget(channels);

	QWidget *mediaContainer = new QWidget(splitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);

	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);

	QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

DvbTab::~DvbTab()
{
	delete dvbStream;
	qDeleteAll(devices);
}

void DvbTab::configureChannels()
{
	DvbScanDialog dialog(this, channelModel);

	dialog.exec();
}

void DvbTab::configureDvb()
{
	KPageDialog *dialog = new KPageDialog(manager->getKaffeine());
	dialog->setCaption(i18n("DVB Settings"));
	dialog->setFaceType(KPageDialog::List);

	int deviceNumber = 1;

	foreach (DvbDevice *device, devices) {
		if (device->getDeviceState() == DvbDevice::DeviceNotReady) {
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

void DvbTab::componentRemoved(const QString &udi)
{
	foreach (DvbDevice *device, devices) {
		if (device->componentRemoved(udi)) {
			break;
		}
	}
}

// FIXME - just a demo hack
void DvbTab::channelActivated()
{
	if (devices.isEmpty()) {
		return;
	}

	DvbDevice *device = devices.begin().value();

	delete dvbStream;
	dvbStream = new DvbStream(device);

	DvbSTransponder transponder(DvbSTransponder::Horizontal, 11953000, 27500000, DvbSTransponder::FecAuto);
	DvbSConfig config("test");
	device->tuneDevice(transponder, config);

	device->addPidFilter(110, dvbStream);
	device->addPidFilter(120, dvbStream);

	manager->getMediaWidget()->playDvb(dvbStream);
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
		kWarning() << "couldn't determine adapter and/or index for" << component.udi();
		return;
	}

	int internal_index = (adapter << 16) | index;

	DvbDevice *device = devices.value(internal_index);

	if (device == NULL) {
		device = new DvbDevice();
		devices.insert(internal_index, device);
	}

	device->componentAdded(component);
}
