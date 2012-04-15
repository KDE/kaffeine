/*
 * sqlinterface.cpp
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

#include "sqlinterface.h"

#include <QAbstractItemModel>
#include <QStringList>
#include "log.h"
#include "sqlhelper.h"

SqlInterface::SqlInterface() : createTable(false), hasPendingStatements(false),
	sqlColumnCount(0)
{
	sqlHelper = SqlHelper::getInstance();
}

SqlInterface::~SqlInterface()
{
	if (hasPendingStatements) {
		Log("SqlInterface::~SqlInterface: pending statements at destruction");
		/* data isn't valid anymore */
		pendingStatements.clear();
		createTable = false;
		/* make sure we don't get called after destruction */
		sqlHelper->collectSubmissions();
	}
}

void SqlInterface::sqlFlush()
{
	if (hasPendingStatements) {
		sqlHelper->collectSubmissions();
	}
}

void SqlInterface::sqlInit(const QString &tableName, const QStringList &columnNames)
{
	QString existsStatement = QLatin1String("SELECT name FROM sqlite_master WHERE name='") + tableName +
		QLatin1String("' AND type = 'table'");
	createStatement = QLatin1String("CREATE TABLE ") + tableName + QLatin1String(" (Id INTEGER PRIMARY KEY, ");
	QString selectStatement = QLatin1String("SELECT Id, ");
	insertStatement = QLatin1String("INSERT INTO ") + tableName + QLatin1String(" (Id, ");
	updateStatement = QLatin1String("UPDATE ") + tableName + QLatin1String(" SET ");
	deleteStatement = QLatin1String("DELETE FROM ") + tableName + QLatin1String(" WHERE Id = ?");

	sqlColumnCount = columnNames.size();

	for (int i = 0; i < sqlColumnCount; ++i) {
		if (i > 0) {
			createStatement.append(QLatin1String(", "));
			selectStatement.append(QLatin1String(", "));
			insertStatement.append(QLatin1String(", "));
			updateStatement.append(QLatin1String(" = ?, "));
		}

		const QString &columnName = columnNames.at(i);
		createStatement.append(columnName);
		selectStatement.append(columnName);
		insertStatement.append(columnName);
		updateStatement.append(columnName);
	}

	createStatement.append(QLatin1Char(')'));
	selectStatement.append(QLatin1String(" FROM "));
	selectStatement.append(tableName);
	insertStatement.append(QLatin1String(") VALUES (?"));
	updateStatement.append(QLatin1String(" = ? WHERE Id = ?"));

	for (int i = 0; i < sqlColumnCount; ++i) {
		insertStatement.append(QLatin1String(", ?"));
	}

	insertStatement.append(QLatin1Char(')'));

	if (!sqlHelper->exec(existsStatement).next()) {
		createTable = true;
		requestSubmission();
	} else {
		// queries can only be prepared if the table exists
		insertQuery = sqlHelper->prepare(insertStatement);
		updateQuery = sqlHelper->prepare(updateStatement);
		deleteQuery = sqlHelper->prepare(deleteStatement);

		for (QSqlQuery query = sqlHelper->exec(selectStatement); query.next();) {
			qint64 fullKey = query.value(0).toLongLong();
			SqlKey sqlKey(static_cast<int>(fullKey));

			if (!sqlKey.isSqlKeyValid() || (sqlKey.sqlKey != fullKey)) {
				Log("SqlInterface::sqlInit: invalid key") << fullKey;
				continue;
			}

			if (!insertFromSqlQuery(sqlKey, query, 1)) {
				pendingStatements.insert(sqlKey, Remove);
				requestSubmission();
			}
		}
	}
}

void SqlInterface::sqlInsert(SqlKey key)
{
	PendingStatement pendingStatement = pendingStatements.value(key, Nothing);

	switch (pendingStatement) {
	case Nothing:
		pendingStatements.insert(key, Insert);
		requestSubmission();
		return;
	case Remove:
		pendingStatements.insert(key, RemoveAndInsert);
		requestSubmission();
		return;
	case RemoveAndInsert:
	case Insert:
	case Update:
		break;
	}

	Log("SqlInterface::sqlInsert: invalid pending statement") << pendingStatement;
}

void SqlInterface::sqlUpdate(SqlKey key)
{
	PendingStatement pendingStatement = pendingStatements.value(key, Nothing);

	switch (pendingStatement) {
	case Nothing:
		pendingStatements.insert(key, Update);
		requestSubmission();
		return;
	case RemoveAndInsert:
	case Insert:
	case Update:
		return;
	case Remove:
		break;
	}

	Log("SqlInterface::sqlUpdate: invalid pending statement") << pendingStatement;
}

void SqlInterface::sqlRemove(SqlKey key)
{
	PendingStatement pendingStatement = pendingStatements.value(key, Nothing);

	switch (pendingStatement) {
	case Nothing:
	case RemoveAndInsert:
	case Update:
		pendingStatements.insert(key, Remove);
		requestSubmission();
		return;
	case Insert:
		pendingStatements.remove(key);
		return;
	case Remove:
		break;
	}

	Log("SqlInterface::sqlRemove: invalid pending statement") << pendingStatement;
}

void SqlInterface::requestSubmission()
{
	if (!hasPendingStatements) {
		hasPendingStatements = true;
		sqlHelper->requestSubmission(this);
	}
}

void SqlInterface::sqlSubmit()
{
	if (createTable) {
		createTable = false;
		sqlHelper->exec(createStatement);

		// queries can only be prepared if the table exists
		insertQuery = sqlHelper->prepare(insertStatement);
		updateQuery = sqlHelper->prepare(updateStatement);
		deleteQuery = sqlHelper->prepare(deleteStatement);
	}

	for (QMap<SqlKey, PendingStatement>::ConstIterator it = pendingStatements.constBegin();
	     it != pendingStatements.constEnd(); ++it) {
		PendingStatement pendingStatement = it.value();

		switch (pendingStatement) {
		case Nothing:
			break;
		case RemoveAndInsert:
			deleteQuery.bindValue(0, it.key().sqlKey);
			sqlHelper->exec(deleteQuery);
			// fall through
		case Insert:
			bindToSqlQuery(it.key(), insertQuery, 1);
			insertQuery.bindValue(0, it.key().sqlKey);
			sqlHelper->exec(insertQuery);
			continue;
		case Update:
			bindToSqlQuery(it.key(), updateQuery, 0);
			updateQuery.bindValue(sqlColumnCount, it.key().sqlKey);
			sqlHelper->exec(updateQuery);
			continue;
		case Remove:
			deleteQuery.bindValue(0, it.key().sqlKey);
			sqlHelper->exec(deleteQuery);
			continue;
		}

		Log("SqlInterface::sqlSubmit: invalid pending statement") << pendingStatement;
	}

	pendingStatements.clear();
	hasPendingStatements = false;
}
