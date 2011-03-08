/*
 * sqlinterface.cpp
 *
 * Copyright (C) 2009-2010 Christoph Pfister <christophpfister@gmail.com>
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
#include <KDebug>
#include "sqlhelper.h"

SqlInterface::SqlInterface() : createTable(false), hasPendingStatements(false),
	sqlColumnCount(0)
{
	sqlHelper = SqlHelper::getInstance();
}

SqlInterface::~SqlInterface()
{
	if (hasPendingStatements) {
		kError() << "pending statements at destruction";
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
	QString existsStatement = "SELECT name FROM sqlite_master WHERE name='" + tableName +
		"' AND type = 'table'";
	createStatement = "CREATE TABLE " + tableName + " (Id INTEGER PRIMARY KEY, ";
	QString selectStatement = "SELECT Id, ";
	insertStatement = "INSERT INTO " + tableName + " (Id, ";
	updateStatement = "UPDATE " + tableName + " SET ";
	deleteStatement = "DELETE FROM " + tableName + " WHERE Id = ?";

	sqlColumnCount = columnNames.size();

	for (int i = 0; i < sqlColumnCount; ++i) {
		if (i > 0) {
			createStatement.append(", ");
			selectStatement.append(", ");
			insertStatement.append(", ");
			updateStatement.append(" = ?, ");
		}

		const QString &columnName = columnNames.at(i);
		createStatement.append(columnName);
		selectStatement.append(columnName);
		insertStatement.append(columnName);
		updateStatement.append(columnName);
	}

	createStatement.append(')');
	selectStatement.append(" FROM ");
	selectStatement.append(tableName);
	insertStatement.append(") VALUES (?");
	updateStatement.append(" = ? WHERE Id = ?");

	for (int i = 0; i < sqlColumnCount; ++i) {
		insertStatement.append(", ?");
	}

	insertStatement.append(')');

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
			SqlKey sqlKey(fullKey);

			if (!sqlKey.isSqlKeyValid() || (sqlKey.sqlKey != fullKey)) {
				kWarning() << "invalid key" << fullKey;
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
	switch (pendingStatements.value(key, Nothing)) {
	case Nothing:
		pendingStatements.insert(key, Insert);
		requestSubmission();
		break;
	case Remove:
		pendingStatements.insert(key, RemoveAndInsert);
		requestSubmission();
		break;
	case RemoveAndInsert:
	case Insert:
	case Update:
		kError() << "invalid pending statement" << pendingStatements.value(key, Nothing);
		break;
	}
}

void SqlInterface::sqlUpdate(SqlKey key)
{
	switch (pendingStatements.value(key, Nothing)) {
	case Nothing:
		pendingStatements.insert(key, Update);
		requestSubmission();
		break;
	case RemoveAndInsert:
	case Insert:
	case Update:
		break;
	case Remove:
		kError() << "invalid pending statement" << pendingStatements.value(key, Nothing);
		break;
	}
}

void SqlInterface::sqlRemove(SqlKey key)
{
	switch (pendingStatements.value(key, Nothing)) {
	case Nothing:
	case RemoveAndInsert:
	case Update:
		pendingStatements.insert(key, Remove);
		requestSubmission();
		break;
	case Insert:
		pendingStatements.remove(key);
		break;
	case Remove:
		kError() << "invalid pending statement" << pendingStatements.value(key, Nothing);
		break;
	}
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
		switch (it.value()) {
		case Nothing:
			kError() << "invalid pending statement" << it.value();
			break;
		case RemoveAndInsert:
			deleteQuery.bindValue(0, it.key().sqlKey);
			sqlHelper->exec(deleteQuery);
			// fall through
		case Insert:
			bindToSqlQuery(it.key(), insertQuery, 1);
			insertQuery.bindValue(0, it.key().sqlKey);
			sqlHelper->exec(insertQuery);
			break;
		case Update:
			bindToSqlQuery(it.key(), updateQuery, 0);
			updateQuery.bindValue(sqlColumnCount, it.key().sqlKey);
			sqlHelper->exec(updateQuery);
			break;
		case Remove:
			deleteQuery.bindValue(0, it.key().sqlKey);
			sqlHelper->exec(deleteQuery);
			break;
		}
	}

	pendingStatements.clear();
	hasPendingStatements = false;
}
