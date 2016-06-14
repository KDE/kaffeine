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
#if QT_VERSION < 0x050500
# define qInfo qDebug
#endif

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QPointer>
#include <QStandardPaths>
#include <QString>

#include <iostream>

#include <config-kaffeine.h>

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
	KaffeineApplication(int &argc, char **argv);
	~KaffeineApplication();
	void startWindow();

	QCommandLineParser parser;
	KAboutData aboutData;

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
		i18n("(C) 2007-2016 The Kaffeine Authors"),
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

	KDBusService service(KDBusService::Unique);

	app.startWindow();

	return app.exec();
}
