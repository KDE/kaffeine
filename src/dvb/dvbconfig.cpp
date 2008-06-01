/*
 * dvbconfig.cpp
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

#include "dvbconfig.h"

#include <QGridLayout>
#include <QLabel>
#include <KLocalizedString>
#include "dvbchannel.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbtab.h"

QPair<int, bool> DvbSConfig::getParameters(const DvbSTransponder *transponder) const
{
	// FIXME hack
	int frequency = transponder->frequency;

	if (frequency >= 11700000) {
		return QPair<int, bool>(frequency - 10600000, true);
	} else {
		return QPair<int, bool>(frequency - 9750000, false);
	}
}

DvbConfigDialog::DvbConfigDialog(DvbTab *dvbTab) : KPageDialog(dvbTab)
{
	setCaption(i18n("DVB Settings"));
	setFaceType(KPageDialog::List);

	int deviceNumber = 1;

	foreach (DvbDevice *device, dvbTab->getDvbManager()->getDeviceList()) {
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
		addPage(page);
	}
}

DvbConfigDialog::~DvbConfigDialog()
{
}
