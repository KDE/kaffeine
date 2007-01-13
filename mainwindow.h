/*
 * mainwindow.h
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <config.h>

#include <QList>
#include <QPair>

#include <KMainWindow>

#include "kaffeine.h"

class KAction;
class KIcon;

class MainWindow : public KMainWindow
{
	Q_OBJECT

public:
	MainWindow(Kaffeine *kaffeine_);
	~MainWindow();

	void play()
	{
		setState(currentState | statePlaying);
	}

	void stop()
	{
		setState(currentState & statePrevNext);
	}

private slots:
	void actionPlayPause();

private:
	enum stateFlag {
		stateAlways	= 0,
		statePrevNext	= (1 << 0),
		statePlaying	= (1 << 1),

		stateAll	= 0xff
	};

	Q_DECLARE_FLAGS(stateFlags, stateFlag)

	void addAction(const QString &name, stateFlags flags, KAction *action);
	void setState(stateFlags newState);

	Kaffeine *kaffeine;

	stateFlags currentState;
	QList<QPair<stateFlags, KAction *> > actionList;

	KAction *controlsPlayPause;
};

#endif /* MAINWINDOW_H */
