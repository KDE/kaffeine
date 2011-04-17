/*
 * configuration.h
 *
 * Copyright (C) 2011 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QObject>

class Configuration : public QObject
{
	Q_OBJECT
private:
	Configuration();
	~Configuration();

public:
	static Configuration *instance();

	enum StartupDisplayMode {
		StartupNormalMode = 0,
		StartupMinimalMode = 1,
		StartupFullScreenMode = 2,
		StartupRememberLastSetting = 3,
		StartupLastValue = StartupRememberLastSetting
	};

	StartupDisplayMode getStartupDisplayMode() const
	{
		return startupDisplayMode;
	}

	void setStartupDisplayMode(int newStartupDisplayMode);

	int getShortSkipDuration() const
	{
		return shortSkipDuration;
	}

	void setShortSkipDuration(int newShortSkipDuration);

	int getLongSkipDuration() const
	{
		return longSkipDuration;
	}

	void setLongSkipDuration(int newLongSkipDuration);

signals:
	void shortSkipDurationChanged(int shortSkipDuration);
	void longSkipDurationChanged(int longSkipDuration);

private:
	static Configuration *globalInstance;

	StartupDisplayMode startupDisplayMode;
	int shortSkipDuration;
	int longSkipDuration;
};

#endif /* CONFIGURATION_H */
