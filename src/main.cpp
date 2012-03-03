/*
 * main.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include <QPointer>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KUniqueApplication>
#include "mainwindow.h"
#include "sqlhelper.h"

class KaffeineApplication : public KUniqueApplication
{
public:
	KaffeineApplication();
	~KaffeineApplication();

private:
	int newInstance();

	QPointer<MainWindow> mainWindow;
};

KaffeineApplication::KaffeineApplication()
{
	if (!SqlHelper::createInstance()) {
		return;
	}

	mainWindow = new MainWindow();
}

KaffeineApplication::~KaffeineApplication()
{
	// unlike qt, kde sets Qt::WA_DeleteOnClose and needs it to work properly
	delete mainWindow; // QPointer; needed if kaffeine is closed via QCoreApplication::quit()
}

int KaffeineApplication::newInstance()
{
	if (mainWindow != NULL) {
		mainWindow->parseArgs();
	}

	return KUniqueApplication::newInstance();
}

int main(int argc, char *argv[])
{
	KAboutData aboutData("kaffeine", 0, ki18n("Kaffeine"), "1.3-git",
		ki18n("A media player for KDE with digital TV support."),
		KAboutData::License_GPL_V2, ki18n("(C) 2007-2011 The Kaffeine Authors"),
		KLocalizedString(), "http://kaffeine.kde.org");

	aboutData.addAuthor(ki18n("Christoph Pfister"), ki18n("Maintainer"),
		"christophpfister@gmail.com");

	KCmdLineArgs::init(argc, argv, &aboutData);
	KCmdLineArgs::addCmdLineOptions(MainWindow::cmdLineOptions());
	KCmdLineArgs::addTempFileOption();

	KaffeineApplication app;
	return app.exec();
}
