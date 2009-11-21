/*
 * tablemodel.cpp
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#include "tablemodel.h"

#include <KDebug>
#include "sqlhelper.h"

SqlTableHelper::SqlTableHelper(SqlTableModelBase *model_) : model(model_), freeKey(0),
	hasPendingStatements(false), createTable(false), sqlColumnCount(0)
{
	sqlHelper = SqlHelper::getInstance();
}

SqlTableHelper::~SqlTableHelper()
{
	if (hasPendingStatements) {
		sqlHelper->collectSubmissions();
	}
}

void SqlTableHelper::append()
{
	SqlTableRow row;

	if (!pendingDeletionKeys.isEmpty()) {
		qint64 key = pendingDeletionKeys.takeLast();
		QList<qint64>::ConstIterator it = qLowerBound(usedKeys, key);
		Q_ASSERT((it == usedKeys.constEnd()) || (*it != key));

		usedKeys.insert(it - usedKeys.constBegin(), key);
		row.sqlKey = key;
		row.sqlPendingStatement = SqlTableRow::Update;
	} else {
		QList<qint64>::ConstIterator it = qLowerBound(usedKeys, freeKey);

		while ((it != usedKeys.constEnd()) && (*it == freeKey)) {
			++freeKey;
			++it;
		}

		usedKeys.insert(it - usedKeys.constBegin(), freeKey);
		row.sqlKey = freeKey;
		row.sqlPendingStatement = SqlTableRow::Insert;

		++freeKey;
	}

	rows.append(row);

	if (!hasPendingStatements) {
		hasPendingStatements = true;
		sqlHelper->requestSubmission(this);
	}
}

void SqlTableHelper::rowUpdated(int row)
{
	SqlTableRow &sqlRow = rows[row];

	switch (sqlRow.sqlPendingStatement) {
	case SqlTableRow::Insert:
	case SqlTableRow::Update:
		break;
	case SqlTableRow::None:
		sqlRow.sqlPendingStatement = SqlTableRow::Update;

		if (!hasPendingStatements) {
			hasPendingStatements = true;
			sqlHelper->requestSubmission(this);
		}

		break;
	}
}

void SqlTableHelper::remove(int row)
{
	SqlTableRow sqlRow = rows.takeAt(row);

	qint64 key = sqlRow.sqlKey;
	QList<qint64>::ConstIterator it = qLowerBound(usedKeys, key);
	Q_ASSERT((it != usedKeys.constEnd()) && (*it == key));

	usedKeys.removeAt(it - usedKeys.constBegin());

	if (key < freeKey) {
		freeKey = key;
	}

	switch (sqlRow.sqlPendingStatement) {
	case SqlTableRow::Insert:
		break;
	case SqlTableRow::None:
	case SqlTableRow::Update:
		pendingDeletionKeys.append(key);

		if (!hasPendingStatements) {
			hasPendingStatements = true;
			sqlHelper->requestSubmission(this);
		}

		break;
	}
}

void SqlTableHelper::init(const QString &tableName, const QStringList &columnNames)
{
	QString existsStatement = "SELECT name FROM sqlite_master WHERE name='" +
		tableName + "' AND type = 'table'";
	createStatement = "CREATE TABLE " + tableName + " (Id INTEGER PRIMARY KEY";
	QString selectStatement = "SELECT Id";
	insertStatement = "INSERT INTO " + tableName + " (Id";
	updateStatement = "UPDATE " + tableName + " SET ";
	deleteStatement = "DELETE FROM " + tableName + " WHERE Id = ?";

	sqlColumnCount = columnNames.size();

	for (int i = 0; i < sqlColumnCount; ++i) {
		const QString &columnName = columnNames.at(i);

		createStatement.append(", ");
		createStatement.append(columnName);
		selectStatement.append(", ");
		selectStatement.append(columnName);
		insertStatement.append(", ");
		insertStatement.append(columnName);
		updateStatement.append(columnName);
		updateStatement.append(" = ?, ");
	}

	createStatement.append(')');
	selectStatement.append(" FROM ");
	selectStatement.append(tableName);
	insertStatement.append(") VALUES (?");
	updateStatement.chop(2);
	updateStatement.append(" WHERE Id = ?");

	for (int i = 0; i < sqlColumnCount; ++i) {
		insertStatement.append(", ?");
	}

	insertStatement.append(')');

	if (!sqlHelper->exec(existsStatement).next()) {
		createTable = true;

		if (!hasPendingStatements) {
			hasPendingStatements = true;
			sqlHelper->requestSubmission(this);
		}

		return;
	}

	// queries can only be prepared if the table exists
	insertQuery = sqlHelper->prepare(insertStatement);
	updateQuery = sqlHelper->prepare(updateStatement);
	deleteQuery = sqlHelper->prepare(deleteStatement);

	QSqlQuery query = sqlHelper->exec(selectStatement);

	while (query.next()) {
		qint64 key = query.value(0).toLongLong();
		QList<qint64>::ConstIterator it = qLowerBound(usedKeys, key);

		if ((it != usedKeys.constEnd()) && (*it == key)) {
			kError() << "SQL primary key isn't unique" << key;
			continue;
		}

		usedKeys.insert(it - usedKeys.constBegin(), key);
		model->handleRow(key, query, 1);
	}
}

void SqlTableHelper::insert(qint64 key, bool successful)
{
	if (successful) {
		SqlTableRow row;
		row.sqlKey = key;
		row.sqlPendingStatement = SqlTableRow::None;
		rows.append(row);
	} else {
		QList<qint64>::ConstIterator it = qLowerBound(usedKeys, key);
		Q_ASSERT((it != usedKeys.constEnd()) && (*it == key));

		usedKeys.removeAt(it - usedKeys.constBegin());
		pendingDeletionKeys.append(key);
	}
}

void SqlTableHelper::submit()
{
	if (createTable) {
		createTable = false;
		sqlHelper->exec(createStatement);

		// queries can only be prepared if the table exists
		insertQuery = sqlHelper->prepare(insertStatement);
		updateQuery = sqlHelper->prepare(updateStatement);
		deleteQuery = sqlHelper->prepare(deleteStatement);
	}

	foreach (qint64 key, pendingDeletionKeys) {
		deleteQuery.bindValue(0, key);
		sqlHelper->exec(deleteQuery);
	}

	pendingDeletionKeys.clear();

	for (int i = 0; i < rows.size(); ++i) {
		SqlTableRow &row = rows[i];

		switch (row.sqlPendingStatement) {
		case SqlTableRow::None:
			continue;
		case SqlTableRow::Insert:
			insertQuery.bindValue(0, row.sqlKey);
			model->bindToSqlQuery(i, insertQuery, 1);
			sqlHelper->exec(insertQuery);
			break;
		case SqlTableRow::Update:
			model->bindToSqlQuery(i, updateQuery, 0);
			updateQuery.bindValue(sqlColumnCount, row.sqlKey);
			sqlHelper->exec(updateQuery);
			break;
		}

		row.sqlPendingStatement = SqlTableRow::None;
	}

	hasPendingStatements = false;
}
