/*
 * main.cpp
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

#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocalizedString>
#include <KUniqueApplication>

#include "kaffeine.h"

class KaffeineApplication : public KUniqueApplication
{
public:
	KaffeineApplication() : kaffeine(NULL) { }

	~KaffeineApplication()
	{
		// FIXME *grr*
		// delete kaffeine;
	}

private:
	KaffeineApplication(const KaffeineApplication &);
	KaffeineApplication operator=(const KaffeineApplication &);

	int newInstance();

	Kaffeine *kaffeine;
};

int KaffeineApplication::newInstance()
{
	KUniqueApplication::newInstance();

	if (kaffeine)
		kaffeine->updateArgs();
	else
		kaffeine = new Kaffeine;

	return 0;
}

int main(int argc, char *argv[])
{
	// FIXME
	KAboutData aboutData("kaffeine", I18N_NOOP("Kaffeine Player"), "version", I18N_NOOP("description"),
			     KAboutData::License_GPL_V2);

	KCmdLineArgs::init(argc, argv, &aboutData);
	KCmdLineArgs::addCmdLineOptions(Kaffeine::cmdLineOptions);

	if (!KaffeineApplication::start())
		return 0;

	KaffeineApplication app;
	return app.exec();
}
