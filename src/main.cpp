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

#include "log.h"

#include <KAboutData>
#include <KDBusService>
#include <KStartupInfo>
#include <KWindowSystem>
#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QPointer>
#include <QStandardPaths>
#include <QString>

#include <iostream>

#include <config-kaffeine.h>

extern "C" {
  #include <unistd.h>
  #include <string.h>
}

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
		"[Info    ] "};
	static const QString color[] {
		"\x1b[0;32m",	// Debug
		"\x1b[0;33m",	// Warning
		"\x1b[0;31m",	// Critical
		"\x1b[1;31m",	// Fatal
		"\x1b[0;37m"};	// Info
	QString contextString, file = context.file;
	QByteArray localMsg = msg.toLocal8Bit();
	QString log;

	file.remove(QRegExp(".*/kaffeine/"));

	if (context.line && QLoggingCategory::defaultCategory()->isEnabled(QtDebugMsg))
		contextString = QStringLiteral("%1#%2: %3: ")
						.arg(file)
						.arg(context.line)
						.arg(context.function);

	QString timeStr(QDateTime::currentDateTime().toString("dd-MM-yy HH:mm:ss.zzz "));

	log.append(timeStr);
	if (type <= 4)
		log.append(typeStr[type]);

	if (isatty(STDERR_FILENO) && (type <= 4)) {
		if (context.category && strcmp(context.category, "default"))
			log.append(QStringLiteral("\x1b[1;37m%1:\x1b[0;37m ") .arg(context.category));
		std::cerr << color[type].toLocal8Bit().constData();
	} else {
		if (context.category && strcmp(context.category, "default"))
			log.append(QStringLiteral("%1: ") .arg(context.category));
	}
	std::cerr << log.toLocal8Bit().constData();

	if (!contextString.isEmpty()) {
		if (isatty(STDERR_FILENO))
			std::cerr << "\x1b[0;33m";
		std::cerr << contextString.toLocal8Bit().constData();
	}

	if (isatty(STDERR_FILENO))
		std::cerr << "\x1b[0m";

	std::cerr << localMsg.constData() << "\n";

	if (type == QtFatalMsg)
		abort();

	log.append(localMsg.constData());
	log.append("\n");

	Log newLog;
	newLog.storeLog(log);
}

class KaffeineApplication : public QApplication
{
public:
	KaffeineApplication(int &argc, char **argv);
	~KaffeineApplication();
	void startWindow();

	QCommandLineParser parser;
	KAboutData aboutData;

public slots:
	void activateRequested(const QStringList &arguments,
			       const QString &workingDirectory);

private:
	QPointer<MainWindow> mainWindow;
};

KaffeineApplication::KaffeineApplication(int &argc, char **argv) : QApplication(argc, argv),
	aboutData(
		// Program name
		QStringLiteral("kaffeine"),
		i18n("Kaffeine"),
		// Version
		QStringLiteral(KAFFEINE_VERSION),
		// Short description
		i18n("A media player by KDE with digital TV support."),
		// License
		KAboutLicense::GPL_V2,
		// Copyright statement
		i18n("(C) 2007-2017 The Kaffeine Authors"),
		// Optional additional text
		"",
		// Home page
		QStringLiteral("http://kaffeine.kde.org")
	)
{
	QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

	QDir dir(path);
	if (!dir.exists())
		dir.mkpath(path);

	if (!SqlHelper::createInstance()) {
		return;
	}

	aboutData.addAuthor("Mauro Carvalho Chehab",
		i18n("KF5 port"),
		QStringLiteral("mchehab@infradead.org"));
	aboutData.addAuthor("Christoph Pfister", "",
		QStringLiteral("christophpfister@gmail.com"));
	aboutData.addAuthor("Lasse Lindqvist",
		i18n("Maintainer (for KDE4)"),
		QStringLiteral("lasse.k.lindqvist@gmail.com"));
	aboutData.addAuthor("Christophe Thommeret");
	aboutData.addAuthor(QString::fromUtf8("Jürgen Kofler"));
	aboutData.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"),
				i18nc("EMAIL OF TRANSLATORS", "Your emails"));

	KAboutData::setApplicationData(aboutData);

	mainWindow = new MainWindow(&aboutData, &parser);
}

void KaffeineApplication::activateRequested(const QStringList &arguments,
					    const QString &workingDirectory)
{
	if (arguments.isEmpty())
		return;

	parser.parse(arguments);
	KStartupInfo::setNewStartupId(mainWindow, KStartupInfo::startupId());
	KWindowSystem::forceActiveWindow(mainWindow->winId());
	mainWindow->parseArgs(workingDirectory);
}

void KaffeineApplication::startWindow()
{
	mainWindow->run();
}

KaffeineApplication::~KaffeineApplication()
{
	// unlike qt, kde sets Qt::WA_DeleteOnClose and needs it to work properly
	delete mainWindow; // QPointer; needed if kaffeine is closed via QCoreApplication::quit()
}

int main(int argc, char *argv[])
{
	qInstallMessageHandler(verboseMessageHandler);

	KLocalizedString::setApplicationDomain("kaffeine");

	KaffeineApplication app(argc, argv);

	KAboutData *aboutData = &app.aboutData;

	app.setApplicationName("kaffeine");
	app.setOrganizationDomain("kde.org");
	app.setWindowIcon(QIcon::fromTheme(QLatin1String("kaffeine"), QIcon(":kaffeine")));

	app.parser.addVersionOption();
	app.parser.addHelpOption();
	aboutData->setupCommandLine(&app.parser);

	app.parser.process(app);

	aboutData->processCommandLine(&app.parser);

	const KDBusService service(KDBusService::Unique);

	QObject::connect(&service, &KDBusService::activateRequested,
			 &app, &KaffeineApplication::activateRequested);

	app.startWindow();

	return app.exec();
}
