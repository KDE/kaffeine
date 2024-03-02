/*
 * sqlhelper.cpp
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include <KMessageBox>
#include <QSqlError>
#include <QStandardPaths>

#include "sqlhelper.h"
#include "sqlinterface.h"

SqlHelper::SqlHelper()
{
	database = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"), QLatin1String("kaffeine"));
	database.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QLatin1String("/sqlite.db"));

	timer.setInterval(5000);
	connect(&timer, SIGNAL(timeout()), this, SLOT(collectSubmissions()));
}

SqlHelper::~SqlHelper()
{
}

bool SqlHelper::createInstance()
{
	Q_ASSERT(instance == NULL);

	if (!QSqlDatabase::isDriverAvailable(QLatin1String("QSQLITE"))) {
		KMessageBox::error(NULL, i18nc("message box", "Please install the Qt SQLite plugin."));
		return false;
	}

	instance = new SqlHelper();

	if (!instance->database.open()) {
		QString details = instance->database.lastError().databaseText();

		if (!details.isEmpty() && !details.endsWith(QLatin1Char('\n'))) {
			details.append(QLatin1Char('\n'));
		}

		details.append(instance->database.lastError().driverText());
		KMessageBox::detailedError(NULL,
			i18nc("message box", "Cannot open the SQLite database."), details);
		delete instance;
		instance = NULL;
		return false;
	}

	return true;
}

SqlHelper *SqlHelper::getInstance()
{
	return instance;
}

QSqlQuery SqlHelper::prepare(const QString &statement)
{
	QSqlQuery query(database);
	query.setForwardOnly(true);

	if (!query.prepare(statement)) {
		qCWarning(logSql, "Error while preparing statement '%s'", qPrintable(query.lastError().text()));
	}

	return query;
}

QSqlQuery SqlHelper::exec(const QString &statement)
{
	QSqlQuery query(database);
	query.setForwardOnly(true);

	if (!query.exec(statement)) {
		qCWarning(logSql, "Error while executing statement '%s'", qPrintable(query.lastError().text()));
	}

	return query;
}

void SqlHelper::exec(QSqlQuery &query)
{
	if (!query.exec()) {
		qCWarning(logSql, "Error while executing statement '%s'", qPrintable(query.lastError().text()));
	}
}

void SqlHelper::requestSubmission(SqlInterface *object)
{
	if (!timer.isActive()) {
		timer.start();
	}

	objects.append(object);
}

void SqlHelper::collectSubmissions()
{
	exec(QLatin1String("BEGIN"));

	for (int i = 0; i < objects.size(); ++i) {
		objects.at(i)->sqlSubmit();
	}

	exec(QLatin1String("COMMIT"));
	timer.stop();
	objects.clear();
}

SqlHelper *SqlHelper::instance = NULL;

#include "moc_sqlhelper.cpp"
