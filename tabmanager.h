/*
 * tabmanager.h
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

#ifndef TABMANAGER_H
#define TABMANAGER_H

#include <config.h>

#include <QWidget>

class QButtonGroup;
class QPushButton;
class QStackedLayout;
class KToolBar;
class MediaWidget;
class TabBase;

class TabManager : public QWidget
{
	Q_OBJECT
public:
	TabManager(QWidget *parent, KToolBar *toolBar_,
		MediaWidget *mediaWidget);
	~TabManager() { }

	void addTab(const QString &name, TabBase *tab);

	TabBase *getStartTab()
	{
		return startTab;
	}

	TabBase *getPlayerTab()
	{
		return playerTab;
	}

private slots:
	void activating(TabBase *tab);

private:
	KToolBar *toolBar;
	QButtonGroup *buttonGroup;
	QStackedLayout *stackedLayout;
	TabBase *startTab;
	TabBase *playerTab;
};

class TabBase : public QWidget
{
	Q_OBJECT
public:
	explicit TabBase(TabManager *tabManager_) : QWidget(tabManager_),
		tabManager(tabManager_), ignoreActivate(false) { }
	~TabBase() { }

public slots:
	void activate();

signals:
	void activating(TabBase *tab);

protected:
	virtual void internalActivate() = 0;

private:
	TabManager *tabManager;
	bool ignoreActivate;
};

#endif /* TABMANAGER_H */
