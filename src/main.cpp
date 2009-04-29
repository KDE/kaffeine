/*
 * main.cpp
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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

#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocalizedString>
#include <KUniqueApplication>

#include "kaffeine.h"

class KaffeineApplication : public KUniqueApplication
{
public:
	KaffeineApplication() : firstInstance(true)
	{
		kaffeine = new Kaffeine();
		kaffeine->parseArgs();
	}

	~KaffeineApplication()
	{
		// unlike qt, kde sets Qt::WA_DeleteOnClose and needs it to work properly ...
	}

private:
	int newInstance();

	Kaffeine *kaffeine;
	bool firstInstance;
};

int KaffeineApplication::newInstance()
{
	if (firstInstance) {
		// using KFileWidget, newInstance() might be called _during_ kaffeine construction
		firstInstance = false;
	} else {
		kaffeine->parseArgs();
	}

	return KUniqueApplication::newInstance();
}

int main(int argc, char *argv[])
{
	KAboutData aboutData("kaffeine", 0, ki18n("Kaffeine"), "1.0-svn-" __DATE__,
		ki18n("A media player for KDE with digital TV support."),
		KAboutData::License_GPL_V2, ki18n("(C) 2007-2009 The Kaffeine Authors"),
		KLocalizedString(), "http://kaffeine.kde.org");

	aboutData.addAuthor(ki18n("Christoph Pfister"), ki18n("Maintainer"),
		"christophpfister@gmail.com");

	KCmdLineArgs::init(argc, argv, &aboutData);
	KCmdLineArgs::addCmdLineOptions(Kaffeine::cmdLineOptions());

	KaffeineApplication app;
	return app.exec();
}
