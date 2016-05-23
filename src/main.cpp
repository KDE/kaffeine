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

#include <QDebug>
#include <QString>
#include <QDateTime>
#include <iostream>
#include <QPointer>
#include <KAboutData>

#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <KLocalizedString>
#include <QCommandLineParser>
#include "mainwindow.h"
#include "sqlhelper.h"

void verboseMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	static const char* typeStr[] = {
		"[Debug   ]",
		"[Warning ]",
		"[Critical]",
		"[Fatal   ]",
		"[System  ]"};
	QString contextString, file = context.file;
	QByteArray localMsg = msg.toLocal8Bit();

	file.remove(QRegExp(".*/kaffeine/"));

	if (context.line)
		contextString = QStringLiteral("%1#%2: ")
						.arg(file)
						.arg(context.line);

	QString timeStr(QDateTime::currentDateTime().toString("dd-MM-yy HH:mm:ss.zzz "));

	std::cerr << timeStr.toLocal8Bit().constData();
	if (type <= 4)
		std::cerr << typeStr[type] << " ";
	if (!contextString.isEmpty())
		std::cerr << contextString.toLocal8Bit().constData();
	std::cerr << localMsg.constData() << std::endl;

	if (type == QtFatalMsg)
		abort();
}

class KaffeineApplication : public QApplication
{
public:
	KaffeineApplication(int &argc, char **argv);
	~KaffeineApplication();
	QCommandLineParser parser;

private:
//	int newInstance();

	QPointer<MainWindow> mainWindow;
};

KaffeineApplication::KaffeineApplication(int &argc, char **argv) : QApplication(argc, argv)
{
	QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

	QDir dir(path);
	if (!dir.exists())
		dir.mkpath(path);

	if (!SqlHelper::createInstance()) {
		return;
	}

	mainWindow = new MainWindow();

	mainWindow->cmdLineOptions(&parser);
}

KaffeineApplication::~KaffeineApplication()
{
	// unlike qt, kde sets Qt::WA_DeleteOnClose and needs it to work properly
	delete mainWindow; // QPointer; needed if kaffeine is closed via QCoreApplication::quit()
}

//int KaffeineApplication::newInstance()
//{
//	if (mainWindow != NULL) {
//		mainWindow->parseArgs();
//	}
//
//	return QApplication::newInstance();
//}

// The icon Kaffeine needs are either at breeze or oxygen themes
static void iconThemeFunc()

{
	if ((QIcon::themeName() != QLatin1String("breeze")
	    && QIcon::themeName() != QLatin1String("oxgen"))
	    || QIcon::themeName().isEmpty()) {
		foreach(const QString &path, QIcon::themeSearchPaths()) {
			QDir d(path);
			if (d.exists(QLatin1String("breeze"))) {
				QIcon::setThemeName(QLatin1String("breeze"));
				return;
			}
		}
		foreach(const QString &path, QIcon::themeSearchPaths()) {
			QDir d(path);
			if (d.exists(QLatin1String("oxygen"))) {
				QIcon::setThemeName(QLatin1String("oxygen"));
				return;
			}
		}
	}
}

Q_COREAPP_STARTUP_FUNCTION(iconThemeFunc)

int main(int argc, char *argv[])
{
    qInstallMessageHandler(verboseMessageHandler);
//    qSetMessagePattern("%{file}(%{line}): %{message}");

    KaffeineApplication app(argc, argv);

    KAboutData aboutData(
		// Program name
		QStringLiteral("kaffeine"),
		i18n("Kaffeine"),
		// Version
		QStringLiteral("1.3-git"),
		// Short description
		i18n("A media player for KDE with digital TV support."),
		// License
		KAboutLicense::GPL_V2,
		// Copyright statement
		i18n("(C) 2007-2016 The Kaffeine Authors"),
		// Optional additional text
		"",
		// Home page
		QStringLiteral("http://kaffeine.kde.org")
    );

    aboutData.addAuthor(i18n("Christoph Pfister"), i18n("Maintainer"),
		QStringLiteral("christophpfister@gmail.com"));

    KAboutData::setApplicationData(aboutData);

    app.setWindowIcon(QIcon::fromTheme(QLatin1String("kaffeine")));

    app.parser.addVersionOption();
    app.parser.addHelpOption();

    aboutData.setupCommandLine(&app.parser);

    app.parser.process(app);

    aboutData.processCommandLine(&app.parser);

//    KCmdLineArgs::addTempFileOption();

    return app.exec();
}
