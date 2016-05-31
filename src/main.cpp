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

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QPointer>
#include <QStandardPaths>
#include <QString>

#include <iostream>

#include "configurationdialog.h"
#include "mainwindow.h"
#include "sqlhelper.h"

void verboseMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	static const QString typeStr[] = {
		"[Debug   ] ",
		"[Warning ] ",
		"[Critical] ",
		"[Fatal   ] ",
		"[System  ] "};
	QString contextString, file = context.file;
	QByteArray localMsg = msg.toLocal8Bit();
	QString log;

	file.remove(QRegExp(".*/kaffeine/"));

	if (context.line)
		contextString = QStringLiteral("%1#%2: ")
						.arg(file)
						.arg(context.line);

	QString timeStr(QDateTime::currentDateTime().toString("dd-MM-yy HH:mm:ss.zzz "));

	log.append(timeStr);
	if (type <= 4)
		log.append(typeStr[type]);
	if (!contextString.isEmpty())
		log.append(contextString);
	log.append(localMsg.constData());
	log.append("\n");

	std::cerr << log.toLocal8Bit().constData();

	Log newLog;
	newLog.storeLog(log);

	if (type == QtFatalMsg)
		abort();
}

class KaffeineApplication : public QApplication
{
public:
	KaffeineApplication(int &argc, char **argv, KAboutData *aboutData);
	~KaffeineApplication();
	QCommandLineParser parser;

private:
	QPointer<MainWindow> mainWindow;
};

KaffeineApplication::KaffeineApplication(int &argc, char **argv, KAboutData *aboutData) : QApplication(argc, argv)
{
	QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

	QDir dir(path);
	if (!dir.exists())
		dir.mkpath(path);

	if (!SqlHelper::createInstance()) {
		return;
	}

	mainWindow = new MainWindow(aboutData);

	mainWindow->cmdLineOptions(&parser);
}

KaffeineApplication::~KaffeineApplication()
{
	// unlike qt, kde sets Qt::WA_DeleteOnClose and needs it to work properly
	delete mainWindow; // QPointer; needed if kaffeine is closed via QCoreApplication::quit()
}

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

	KLocalizedString::setApplicationDomain("kaffeine");

	KAboutData aboutData(
		// Program name
		QStringLiteral("kaffeine"),
		i18n("Kaffeine"),
		// Version
		QStringLiteral("2.0.1"),
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

	aboutData.addAuthor("Mauro Carvalho Chehab",
		i18n("this KDE5 port"),
		QStringLiteral("mchehab@infradead.org"));
	aboutData.addAuthor("Christoph Pfister",
		i18n("Original author"),
		QStringLiteral("christophpfister@gmail.com"));
	aboutData.addAuthor("Lasse Lindqvist",
		i18n("Maintainer (for KDE4)"),
		QStringLiteral("lasse.k.lindqvist@gmail.com"));

	KaffeineApplication app(argc, argv, &aboutData);
	KAboutData::setApplicationData(aboutData);

	app.setApplicationName("kaffeine");
	app.setOrganizationDomain("kde.org");
	app.setWindowIcon(QIcon::fromTheme(QLatin1String("kaffeine")));

	app.parser.addVersionOption();
	app.parser.addHelpOption();

	aboutData.setupCommandLine(&app.parser);

	app.parser.process(app);

	aboutData.processCommandLine(&app.parser);

//	KCmdLineArgs::addTempFileOption();
	KDBusService service(KDBusService::Unique);

	return app.exec();
}
