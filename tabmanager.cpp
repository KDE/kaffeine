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
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedLayout>

#include <KLocalizedString>
#include <KToolBar>

#include "mediawidget.h"
#include "tabmanager.h"
#include "tabmanager.moc"

class StartTab : public TabBase
{
public:
	explicit StartTab(TabManager *tabManager_);
	~StartTab() { }

private:
	void internalActivate() { }
};

StartTab::StartTab(TabManager *tabManager_) : TabBase(tabManager_)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);
	layout->setSpacing(0);
	QLabel *label = new QLabel(i18n("<font size=\"+4\"><b>[Kaffeine Player]</b><br>caffeine for your desktop!</font>"));
	label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
	label->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum));
	QPalette pal = label->palette();
	pal.setColor(label->backgroundRole(), QColor(127, 127, 255));
	label->setPalette(pal);
	label->setAutoFillBackground(true);
	layout->addWidget(label);
	QWidget *widget = new QWidget(this);
	pal = widget->palette();
	pal.setColor(widget->backgroundRole(), QColor(255, 255, 255));
	widget->setPalette(pal);
	widget->setAutoFillBackground(true);
	layout->addWidget(widget);
}

class PlayerTab : public TabBase
{
public:
	PlayerTab(TabManager *tabManager_, MediaWidget *mediaWidget_);
	~PlayerTab() { }

private:
	void internalActivate()
	{
		layout()->addWidget(mediaWidget);
	}

	MediaWidget *mediaWidget;
};

PlayerTab::PlayerTab(TabManager *tabManager_, MediaWidget *mediaWidget_)
	: TabBase(tabManager_), mediaWidget(mediaWidget_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
	layout->addWidget(mediaWidget);
}

void TabBase::activate()
{
	if (!ignoreActivate) {
		ignoreActivate = true;
		emit activating(this);
		ignoreActivate = false;
		internalActivate();
	}
}

TabManager::TabManager(QWidget *parent, KToolBar *toolBar_,
	MediaWidget *mediaWidget) : QWidget(parent), toolBar(toolBar_)
{
	stackedLayout = new QStackedLayout(this);
	buttonGroup = new QButtonGroup(this);

	startTab = new StartTab(this);
	playerTab = new PlayerTab(this, mediaWidget);

	addTab(i18n("Start"), startTab);
	addTab(i18n("Player"), playerTab);

	startTab->activate();
}

void TabManager::addTab(const QString &name, TabBase *tab)
{
	QPushButton *pushButton = new QPushButton(name, toolBar);
	pushButton->setCheckable(true);
	pushButton->setFocusPolicy(Qt::NoFocus);
	connect(pushButton, SIGNAL(clicked(bool)), tab, SLOT(activate()));
	connect(tab, SIGNAL(activating(TabBase *)), pushButton, SLOT(click()));
	connect(tab, SIGNAL(activating(TabBase *)), this, SLOT(activating(TabBase *)));
	toolBar->addWidget(pushButton);
	buttonGroup->addButton(pushButton);
	stackedLayout->addWidget(tab);
}

void TabManager::activating(TabBase *tab)
{
	stackedLayout->setCurrentWidget(tab);
}
