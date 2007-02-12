/*
 * tabmanager.cpp
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

#include <QButtonGroup>
#include <QPushButton>
#include <QStackedLayout>

#include <KToolBar>

#include "tabmanager.h"

#include "tabmanager.moc"

void TabBase::activate()
{
	emit activating(this);
	internalActivate();
}

TabManager::TabManager(QWidget *parent, KToolBar *toolBar_) : QWidget(parent),
	toolBar(toolBar_)
{
	stackedLayout = new QStackedLayout(this);
	buttonGroup = new QButtonGroup(this);
}

void TabManager::addTab(const QString &name, TabBase *tab)
{
	stackedLayout->addWidget(tab);
	QPushButton *pushButton = new QPushButton(name, toolBar);
	pushButton->setCheckable(true);
	pushButton->setFocusPolicy(Qt::NoFocus);
	connect(pushButton, SIGNAL(clicked(bool)), tab, SLOT(activate()));
//	connect(tab, SIGNAL(activating(TabBase *)), pushButton, SLOT(click()));
	connect(tab, SIGNAL(activating(TabBase *)), this, SLOT(activating(TabBase *)));
	toolBar->addWidget(pushButton);
	buttonGroup->addButton(pushButton);
}

void TabManager::activating(TabBase *tab)
{
	stackedLayout->setCurrentWidget(tab);
}
