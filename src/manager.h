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

#include <QPushButton>

#include <KIcon>

class QButtonGroup;
class QPushButton;
class QStackedLayout;
class KAction;
class KActionCollection;
class KMultiTabBar;
class KRecentFilesAction;
class KUrl;
class Kaffeine;
class MediaWidget;
class TabBase;
class TabButton;

class Manager : public QWidget
{
	Q_OBJECT
public:
	enum stateFlag {
		stateAlways	= 0,

		/* view */
		statePlayer	= (1 << 0),
//		stateMinimal	= (1 << 1),
		stateFullscreen	= (1 << 2),

		/* playback */
		statePrevNext	= (1 << 3),
		statePlaying	= (1 << 4)
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	enum tabs {
		tabStart,
		tabPlayer,
		tabPlaylist,
		tabAudioCd,
		tabDvb
	};

	Manager(Kaffeine *kaffeine_);
	~Manager();

	Kaffeine *getKaffeine()
	{
		return kaffeine;
	}

	MediaWidget *getMediaWidget()
	{
		return mediaWidget;
	}

	void addRecentUrl(const KUrl &url);

	void activate(tabs tab);
	void activate(TabBase *tab);

	void setPlaying()
	{
		setState(currentState | statePlaying);
	}

	void setStopped()
	{
		setState(currentState & (statePrevNext | statePlayer));
	}

	void toggleFullscreen()
	{
		if (isPlaying()) {
			setState(currentState ^ stateFullscreen);
		}
	}

	bool isPlaying()
	{
		return (currentState & statePlaying) == statePlaying;
	}

private:
	void addAction(KActionCollection *collection, const QString &name,
		stateFlags flags, KAction *action);

	void setState(stateFlags newState);

	TabBase *createTab(const QString &name, TabBase *tab, int id);

	stateFlags currentState;
	QList<QPair<stateFlags, KAction *> > actionList;

	Kaffeine *kaffeine;
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
	TabBase *dvbTab;

	bool ignoreActivate;

	// FIXME quick hack
	KMultiTabBar *tabBar;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Manager::stateFlags)

class TabBase : public QWidget
{
	friend class Manager;
	Q_OBJECT
public:
	explicit TabBase(Manager *manager_) : QWidget(manager_),
		manager(manager_), button(NULL) { }
	~TabBase() { }

protected:
	Manager *const manager;

	virtual void activate() = 0;

private slots:
	void clicked()
	{
		manager->activate(this);
	}

private:
	QPushButton *button;
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
