/*
 * configurationdialog.cpp
 *
 * Copyright (C) 2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "configurationdialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <KComboBox>
#include <KLocalizedString>
#include "configuration.h"

ConfigurationDialog::ConfigurationDialog(QWidget *parent) : KPageDialog(parent)
{
	setCaption(i18nc("@title:window", "Configure Kaffeine"));

	QWidget *widget = new QWidget(this);
	QGridLayout *gridLayout = new QGridLayout(widget);

	startupDisplayModeBox = new KComboBox(widget);
	startupDisplayModeBox->addItem(i18nc("@item:inlistbox 'Startup display mode:'",
		"Normal Mode"));
	startupDisplayModeBox->addItem(i18nc("@item:inlistbox 'Startup display mode:'",
		"Minimal Mode"));
	startupDisplayModeBox->addItem(i18nc("@item:inlistbox 'Startup display mode:'",
		"Full Screen Mode"));
	startupDisplayModeBox->addItem(i18nc("@item:inlistbox 'Startup display mode:'",
		"Remember Last Setting"));
	startupDisplayModeBox->setCurrentIndex(Configuration::instance()->getStartupDisplayMode());
	gridLayout->addWidget(startupDisplayModeBox, 0, 1);

	QLabel *label = new QLabel(i18nc("@label:listbox", "Startup display mode:"), widget);
	label->setBuddy(startupDisplayModeBox);
	gridLayout->addWidget(label, 0, 0);

	shortSkipBox = new QSpinBox(widget);
	shortSkipBox->setRange(1, 600);
	shortSkipBox->setValue(Configuration::instance()->getShortSkipDuration());
	gridLayout->addWidget(shortSkipBox, 1, 1);

	label = new QLabel(i18nc("@label:spinbox", "Short skip duration:"), widget);
	label->setBuddy(shortSkipBox);
	gridLayout->addWidget(label, 1, 0);

	longSkipBox = new QSpinBox(widget);
	longSkipBox->setRange(1, 600);
	longSkipBox->setValue(Configuration::instance()->getLongSkipDuration());
	gridLayout->addWidget(longSkipBox, 2, 1);

	label = new QLabel(i18nc("@label:spinbox", "Long skip duration:"), widget);
	label->setBuddy(longSkipBox);
	gridLayout->addWidget(label, 2, 0);

	KPageWidgetItem *page = new KPageWidgetItem(widget, i18nc("@title:group", "General"));
	page->setIcon(KIcon("configure"));
	addPage(page);
}

ConfigurationDialog::~ConfigurationDialog()
{
}

void ConfigurationDialog::accept()
{
	Configuration *configuration = Configuration::instance();
	configuration->setStartupDisplayMode(startupDisplayModeBox->currentIndex());
	configuration->setShortSkipDuration(shortSkipBox->value());
	configuration->setLongSkipDuration(longSkipBox->value());
	KPageDialog::accept();
}
