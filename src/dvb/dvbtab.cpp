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

#include <QHBoxLayout>
#include <QSplitter>
#include <QTreeWidget>

#include <KLocalizedString>

#include "../manager.h"
#include "../mediawidget.h"
#include "dvbtab.h"

DvbTab::DvbTab(Manager *manager_, MediaWidget *mediaWidget_) : TabBase(manager_),
	mediaWidget(mediaWidget_)
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
}

void DvbTab::internalActivate()
{
	mediaLayout->addWidget(mediaWidget);
}
