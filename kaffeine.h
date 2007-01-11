/*
 * kaffeine.h
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

#ifndef KAFFEINE_H
#define KAFFEINE_H

#include <config.h>

#include <QMultiMap>

#include <KAction>
#include <KActionCollection>
#include <KCmdLineOptions>
#include <KIcon>
#include <KMainWindow>

class KAction;

#include "mediawidget.h"

class Kaffeine : public KMainWindow
{
	Q_OBJECT

public:
	Kaffeine();
	~Kaffeine();

	static const KCmdLineOptions cmdLineOptions[];

	void updateArgs();

	enum stateFlag {
		stateNone	= 0,
		statePrevNext	= (1 << 0),
		statePlaying	= (1 << 1),

		stateAll	= 0xff
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

private slots:
	void actionOpen();
	void actionPlayPause();
	void actionQuit();

	// FIXME
	void newMediaState( MediaState status );

private:
	KAction *createAction(const QString &name, const QString &text,
		const KIcon &icon, stateFlags flags)
	{
		KAction *action = new KAction(icon, text, actionCollection());
		actionCollection()->addAction(name, action);
		flaggedActions.insert(flags, action);
		return action;
	}

	void setCurrentState(stateFlags newState);

	stateFlags currentState;

	QMultiMap<stateFlags, KAction *> flaggedActions;

	KAction *actionControlPrevious;
	KAction *actionControlPlayPause;
	KAction *actionControlStop;
	KAction *actionControlNext;
	KAction *actionControlVolume;
	KAction *actionControlMute;

	MediaWidget *player;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Kaffeine::stateFlags)

#endif /* KAFFEINE_H */
