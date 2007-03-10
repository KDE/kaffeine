/*
 * manager.h
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

#ifndef MANAGER_H
#define MANAGER_H

#include <config.h>

#include <QWidget>

#include <KIcon>

class QButtonGroup;
class QPushButton;
class QStackedLayout;
class KAction;
class KToolBar;
class Kaffeine;
class MediaWidget;
class TabBase;

class ActionManager
{
public:
	enum stateFlag {
		stateAlways	= 0,
		statePrevNext	= (1 << 0),
		statePlaying	= (1 << 1),
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	ActionManager(Kaffeine *kaffeine, MediaWidget *mediaWidget);
	~ActionManager() { }

	void play()
	{
		setState(currentState | statePlaying);
	}

	void stop()
	{
		setState(currentState & statePrevNext);
	}

	bool isPlaying()
	{
		return (currentState & statePlaying) == statePlaying;
	}

private:
	void addAction(KActionCollection *collection, const QString &name,
		stateFlags flags, KAction *action);
	void setState(stateFlags newState);

	stateFlags currentState;
	QList<QPair<stateFlags, KAction *> > actionList;

	KAction *actionPlayPause;
	QWidget *widgetPositionSlider;
	QString textPlay;
	QString textPause;
	KIcon iconPlay;
	KIcon iconPause;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ActionManager::stateFlags)

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

#endif /* MANAGER_H */
