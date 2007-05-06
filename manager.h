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

#include <config-kaffeine.h>

#include <QPushButton>

#include <KIcon>

class QButtonGroup;
class QPushButton;
class QStackedLayout;
class KAction;
class KActionCollection;
class KRecentFilesAction;
class Kaffeine;
class MediaWidget;
class TabBase;

class Manager : public QWidget
{
	Q_OBJECT
public:
	enum stateFlag {
		stateAlways	= 0,
		statePrevNext	= (1 << 0),
		statePlaying	= (1 << 1),
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	Manager(Kaffeine *kaffeine);
	~Manager();

	MediaWidget *getMediaWidget()
	{
		return mediaWidget;
	}

	TabBase *getStartTab()
	{
		return startTab;
	}

	TabBase *getPlayerTab()
	{
		return playerTab;
	}

	void addRecentUrl(const KUrl &url);

	void setPlaying()
	{
		setState(currentState | statePlaying);
	}

	void setStopped()
	{
		setState(currentState & statePrevNext);
	}

	bool isPlaying()
	{
		return (currentState & statePlaying) == statePlaying;
	}

private slots:
	void activating(TabBase *tab);

private:
	void addAction(KActionCollection *collection, const QString &name,
		stateFlags flags, KAction *action);

	void setState(stateFlags newState);

	QPushButton *addTab(const QString &name, TabBase *tab);

	stateFlags currentState;
	QList<QPair<stateFlags, KAction *> > actionList;

	MediaWidget *mediaWidget;

	KRecentFilesAction *actionOpenRecent;
	KAction *actionPlayPause;
	QWidget *widgetPositionSlider;
	QString textPlay;
	QString textPause;
	KIcon iconPlay;
	KIcon iconPause;

	QButtonGroup *buttonGroup;
	QStackedLayout *stackedLayout;
	TabBase *startTab;
	TabBase *playerTab;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Manager::stateFlags)

class TabBase : public QWidget
{
	Q_OBJECT
public:
	explicit TabBase(Manager *manager_) : QWidget(manager_),
		manager(manager_), ignoreActivate(false) { }
	~TabBase() { }

public slots:
	void activate();

signals:
	void activating(TabBase *tab);

protected:
	virtual void internalActivate() = 0;

private:
	Manager *manager;
	bool ignoreActivate;
};

class TabButton : public QPushButton
{
	Q_OBJECT
public:
	explicit TabButton(const QString &name);
	~TabButton();

private slots:
	void orientationChanged(Qt::Orientation orientation);

private:
	void changeEvent(QEvent *event);

	QPixmap *horizontal;
	QPixmap *vertical;
};

#endif /* MANAGER_H */
