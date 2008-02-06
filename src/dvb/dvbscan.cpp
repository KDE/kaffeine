/*
 * dvbscan.cpp
 *
 * Copyright (C) 2008 Christoph Pfister <christophpfister@gmail.com>
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

#include <QBoxLayout>
#include <QLabel>
#include <KLocalizedString>

#include "dvbchannel.h"
#include "dvbscan.h"
#include "dvbtab.h"

DvbScanDialog::DvbScanDialog(QWidget *parent, DvbChannelModel *channelModel) : KDialog(parent)
{
	setCaption(i18n("Configure Channels"));

	QWidget *mainWidget = new QWidget(this);
	QLayout *mainLayout = new QHBoxLayout(mainWidget);

	QWidget *rightWidget = new QWidget(mainWidget);
	QLayout *rightLayout = new QVBoxLayout(rightWidget);

	QWidget *topWidget = new QWidget(rightWidget);
	QLayout *topLayout = new QHBoxLayout(topWidget);

	QPushButton *button = new QPushButton(i18n("Scan"), topWidget);
	topLayout->addWidget(button);
	QLabel *label = new QLabel("Scan file from xx.xx.xxxx", topWidget);
	topLayout->addWidget(label);

	DvbChannelView *channelPreview = new DvbChannelView(rightWidget);
	rightLayout->addWidget(topWidget);
	rightLayout->addWidget(channelPreview);

	DvbChannelView *channelView = new DvbChannelView(mainWidget);
	channelView->setModel(channelModel);
	mainLayout->addWidget(channelView);
	mainLayout->addWidget(rightWidget);

	setMainWidget(mainWidget);
}
