/*
 * maintabs.cpp
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

#include <config.h>

#include <QHBoxLayout>

#include "mediawidget.h"
#include "maintabs.h"

StartTab::StartTab(TabManager *tabManager_) : TabBase(tabManager_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
	QWidget *widget = new QWidget(this);
	widget->setAutoFillBackground(true);
	QPalette pal = widget->palette();
	pal.setColor(widget->backgroundRole(), QColor(0, 255, 0));
	widget->setPalette(pal);
	layout->addWidget(widget);
}

StartTab::~StartTab()
{
}

PlayerTab::PlayerTab(TabManager *tabManager_, MediaWidget *mediaWidget_)
	: TabBase(tabManager_), mediaWidget(mediaWidget_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
	layout->addWidget(mediaWidget);
}

PlayerTab::~PlayerTab()
{
}

void PlayerTab::internalActivate()
{
	layout()->addWidget(mediaWidget);
}
