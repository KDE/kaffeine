/*
 * sqltablemodel.cpp
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

#include "sqltablemodel.h"

#include <QAbstractItemModel>
#include <QStringList>
#include <KDebug>
#include "sqlhelper.h"

SqlTableModelInterface::SqlTableModelInterface(QObject *parent) : QObject(parent),
	createTable(false), hasPendingStatements(false), sqlColumnCount(0)
{
	sqlHelper = SqlHelper::getInstance();
}

SqlTableModelInterface::~SqlTableModelInterface()
{
	if (hasPendingStatements) {
		kError() << "pending statements at destruction";
	}
}

void SqlTableModelInterface::flush()
{
	if (hasPendingStatements) {
		sqlHelper->collectSubmissions();
	}
}

quint32 SqlTableModelInterface::keyForRow(int row) const
{
	return rowToKeyMapping.at(row);
}

int SqlTableModelInterface::rowForKey(quint32 key) const
{
	return keyToRowMapping.value(key, -1);
}

void SqlTableModelInterface::init(QAbstractItemModel *model_, const QString &tableName,
	const QStringList &columnNames)
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

		QSqlQuery query = sqlHelper->exec(selectStatement);

		while (query.next()) {
			qint64 fullKey = query.value(0).toLongLong();
			quint32 key = fullKey;

			if ((key == 0) || (key != fullKey)) {
				kWarning() << "invalid key" << fullKey;
				continue;
			}

			int row = insertFromSqlQuery(query, 1);

			if (row >= 0) {
				rowToKeyMapping.insert(row, key);
			} else {
				pendingStatements.insert(key, Remove);
				requestSubmission();
			}
		}

		for (int row = 0; row < rowToKeyMapping.size(); ++row) {
			keyToRowMapping.insert(rowToKeyMapping.at(row), row);
		}
	}

	model = model_;
	connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
		this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(model, SIGNAL(layoutChanged()), this, SLOT(layoutChanged()));
	connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
	connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
		this, SLOT(rowsInserted(QModelIndex,int,int)));
	connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
		this, SLOT(rowsRemoved(QModelIndex,int,int)));
}

void SqlTableModelInterface::dataChanged(const QModelIndex &topLeft,
	const QModelIndex &bottomRight)
{
	for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
		quint32 key = rowToKeyMapping.at(row);

		switch (pendingStatements.value(key, None)) {
		case None:
			pendingStatements.insert(key, Update);
			requestSubmission();
			break;
		case RemoveAndInsert:
		case Insert:
		case Update:
			break;
		case Remove:
			kError() << "invalid pending statement" <<
				pendingStatements.value(key, None);
			break;
		}
	}
}

void SqlTableModelInterface::layoutChanged()
{
	// not supported
	Q_ASSERT(false);
}

void SqlTableModelInterface::modelReset()
{
	rowsRemoved(QModelIndex(), 0, rowToKeyMapping.size() - 1);
	rowsInserted(QModelIndex(), 0, model->rowCount(QModelIndex()) - 1);
}

void SqlTableModelInterface::rowsInserted(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)

	for (int row = start; row <= end; ++row) {
		quint32 key = qrand();

		while (keyToRowMapping.contains(key) || (key == 0)) {
			++key;
		}

		switch (pendingStatements.value(key, None)) {
		case None:
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
			kError() << "invalid pending statement" <<
				pendingStatements.value(key, None);
			break;
		}

		rowToKeyMapping.insert(row, key);
	}

	for (int row = start; row < rowToKeyMapping.size(); ++row) {
		keyToRowMapping.insert(rowToKeyMapping.at(row), row);
	}
}

void SqlTableModelInterface::rowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)

	for (int row = end; row >= start; --row) {
		quint32 key = rowToKeyMapping.at(row);

		switch (pendingStatements.value(key, None)) {
		case None:
		case RemoveAndInsert:
		case Update:
			pendingStatements.insert(key, Remove);
			requestSubmission();
			break;
		case Insert:
			pendingStatements.remove(key);
			break;
		case Remove:
			kError() << "invalid pending statement" <<
				pendingStatements.value(key, None);
			break;
		}

		rowToKeyMapping.removeAt(row);
		keyToRowMapping.remove(key);
	}

	for (int row = start; row < rowToKeyMapping.size(); ++row) {
		keyToRowMapping.insert(rowToKeyMapping.at(row), row);
	}
}

void SqlTableModelInterface::requestSubmission()
{
	if (!hasPendingStatements) {
		hasPendingStatements = true;
		sqlHelper->requestSubmission(this);
	}
}

void SqlTableModelInterface::submit()
{
	if (createTable) {
		createTable = false;
		sqlHelper->exec(createStatement);

		// queries can only be prepared if the table exists
		insertQuery = sqlHelper->prepare(insertStatement);
		updateQuery = sqlHelper->prepare(updateStatement);
		deleteQuery = sqlHelper->prepare(deleteStatement);
	}

	for (QMap<quint32, PendingStatement>::const_iterator it = pendingStatements.constBegin();
	     it != pendingStatements.constEnd(); ++it) {
		switch (it.value()) {
		case None:
			kError() << "invalid pending statement" << it.value();
			break;
		case RemoveAndInsert:
			deleteQuery.bindValue(0, it.key());
			sqlHelper->exec(deleteQuery);
			// fall through
		case Insert:
			insertQuery.bindValue(0, it.key());
			bindToSqlQuery(insertQuery, 1, keyToRowMapping.value(it.key(), -1));
			sqlHelper->exec(insertQuery);
			break;
		case Update:
			bindToSqlQuery(updateQuery, 0, keyToRowMapping.value(it.key(), -1));
			updateQuery.bindValue(sqlColumnCount, it.key());
			sqlHelper->exec(updateQuery);
			break;
		case Remove:
			deleteQuery.bindValue(0, it.key());
			sqlHelper->exec(deleteQuery);
			break;
		}
	}

	pendingStatements.clear();
	hasPendingStatements = false;
}
