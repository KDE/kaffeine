/*
 * configuration.cpp
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

#include "configuration.h"

#include <KConfig>
#include <KConfigGroup>
#include <KGlobal>
#include "log.h"

Configuration::Configuration()
{
	startupDisplayMode = StartupNormalMode;
	int value = KGlobal::config()->group("MainWindow").readEntry("StartupDisplayMode",
		static_cast<int>(startupDisplayMode));

	if ((value >= 0) && (value <= StartupLastValue)) {
		startupDisplayMode = static_cast<StartupDisplayMode>(value);
	} else {
		Log("Configuration::Configuration: unknown startup display mode") << value;
	}

	shortSkipDuration =
		KGlobal::config()->group("MediaObject").readEntry("ShortSkipDuration", 15);
	longSkipDuration =
		KGlobal::config()->group("MediaObject").readEntry("LongSkipDuration", 60);
}

Configuration::~Configuration()
{
}

Configuration *Configuration::instance()
{
	if (globalInstance == NULL) {
		globalInstance = new Configuration();
	}

	return globalInstance;
}

void Configuration::setStartupDisplayMode(int newStartupDisplayMode)
{
	if ((newStartupDisplayMode >= 0) && (newStartupDisplayMode <= StartupLastValue)) {
		startupDisplayMode = static_cast<StartupDisplayMode>(newStartupDisplayMode);
		KGlobal::config()->group("MainWindow").writeEntry("StartupDisplayMode",
			static_cast<int>(startupDisplayMode));
	} else {
		Log("Configuration::setStartupDisplayMode: unknown startup display mode") <<
			newStartupDisplayMode;
	}
}

void Configuration::setShortSkipDuration(int newShortSkipDuration)
{
	shortSkipDuration = newShortSkipDuration;
	KGlobal::config()->group("MediaObject").writeEntry("ShortSkipDuration", shortSkipDuration);
	emit shortSkipDurationChanged(shortSkipDuration);
}

void Configuration::setLongSkipDuration(int newLongSkipDuration)
{
	longSkipDuration = newLongSkipDuration;
	KGlobal::config()->group("MediaObject").writeEntry("LongSkipDuration", longSkipDuration);
	emit longSkipDurationChanged(longSkipDuration);
}

Configuration *Configuration::globalInstance = NULL;
