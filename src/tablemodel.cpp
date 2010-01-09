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

SqlModelAdaptor::SqlModelAdaptor(QAbstractItemModel *model_, SqlTableModelBase *sqlModel_) :
	QObject(model_), model(model_), sqlModel(sqlModel_), hasPendingStatements(false),
	createTable(false), sqlColumnCount(0)
{
	sqlHelper = SqlHelper::getInstance();
}

SqlModelAdaptor::~SqlModelAdaptor()
{
	if (hasPendingStatements) {
		kError() << "pending statements at destruction";
	}
}

void SqlModelAdaptor::init(const QString &tableName, const QStringList &columnNames)
{
	QString existsStatement = "SELECT name FROM sqlite_master WHERE name='" +
		tableName + "' AND type = 'table'";
	createStatement = "CREATE TABLE " + tableName + " (Id INTEGER PRIMARY KEY, ";
	QString selectStatement = "SELECT Id, ";
	insertStatement = "INSERT INTO " + tableName + " (";
	updateStatement = "UPDATE " + tableName + " SET ";
	deleteStatement = "DELETE FROM " + tableName + " WHERE Id = ?";

	sqlColumnCount = columnNames.size();

	for (int i = 0; i < sqlColumnCount; ++i) {
		if (i != 0) {
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

	for (int i = 0; i < (sqlColumnCount - 1); ++i) {
		insertStatement.append(", ?");
	}

	insertStatement.append(')');

	if (!sqlHelper->exec(existsStatement).next()) {
		createTable = true;

		if (!hasPendingStatements) {
			hasPendingStatements = true;
			sqlHelper->requestSubmission(this);
		}
	} else {
		// queries can only be prepared if the table exists
		insertQuery = sqlHelper->prepare(insertStatement);
		updateQuery = sqlHelper->prepare(updateStatement);
		deleteQuery = sqlHelper->prepare(deleteStatement);

		QSqlQuery query = sqlHelper->exec(selectStatement);

		while (query.next()) {
			qint64 key = query.value(0).toLongLong();
			int row = sqlModel->insertFromSqlQuery(query, 1);

			if (row >= 0) {
				SqlTableRow sqlRow;
				sqlRow.key = key;
				sqlRow.pendingStatement = SqlTableRow::None;
				rows.insert(row, sqlRow);
			} else {
				pendingDeletionKeys.append(key);
			}
		}
	}

	connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
		this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(model, SIGNAL(layoutChanged()), this, SLOT(layoutChanged()));
	connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
	connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
		this, SLOT(rowsInserted(QModelIndex,int,int)));
	connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
		this, SLOT(rowsRemoved(QModelIndex,int,int)));
}

void SqlModelAdaptor::flush()
{
	if (hasPendingStatements) {
		sqlHelper->collectSubmissions();
	}
}

void SqlModelAdaptor::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
		const SqlTableRow &sqlRow = rows.at(row);

		switch (sqlRow.pendingStatement) {
		case SqlTableRow::Insert:
		case SqlTableRow::Update:
			break;
		case SqlTableRow::None:
			rows[row].pendingStatement = SqlTableRow::Update;

			if (!hasPendingStatements) {
				hasPendingStatements = true;
				sqlHelper->requestSubmission(this);
			}

			break;
		}
	}
}

void SqlModelAdaptor::layoutChanged()
{
	// FIXME not implemented; but not used either
	Q_ASSERT(false);
}

void SqlModelAdaptor::modelReset()
{
	rowsRemoved(QModelIndex(), 0, rows.size() - 1);
	rowsInserted(QModelIndex(), 0, model->rowCount(QModelIndex()) - 1);
}

void SqlModelAdaptor::rowsInserted(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)

	for (int row = start; row <= end; ++row) {
		if (!pendingDeletionKeys.isEmpty()) {
			SqlTableRow sqlRow;
			sqlRow.key = pendingDeletionKeys.takeLast();
			sqlRow.pendingStatement = SqlTableRow::Update;
			rows.insert(row, sqlRow);
		} else {
			SqlTableRow sqlRow;
			sqlRow.key = 0;
			sqlRow.pendingStatement = SqlTableRow::Insert;
			rows.insert(row, sqlRow);
		}
	}

	if (!hasPendingStatements) {
		hasPendingStatements = true;
		sqlHelper->requestSubmission(this);
	}
}

void SqlModelAdaptor::rowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent)

	for (int row = end; row >= start; --row) {
		SqlTableRow sqlRow = rows.takeAt(row);

		switch (sqlRow.pendingStatement) {
		case SqlTableRow::Insert:
			break;
		case SqlTableRow::None:
		case SqlTableRow::Update:
			pendingDeletionKeys.append(sqlRow.key);

			if (!hasPendingStatements) {
				hasPendingStatements = true;
				sqlHelper->requestSubmission(this);
			}

			break;
		}
	}
}

void SqlModelAdaptor::submit()
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

	for (int row = 0; row < rows.size(); ++row) {
		const SqlTableRow &sqlRow = rows.at(row);

		switch (sqlRow.pendingStatement) {
		case SqlTableRow::None:
			continue;
		case SqlTableRow::Insert:
			sqlModel->bindToSqlQuery(row, insertQuery, 0);
			sqlHelper->exec(insertQuery);
			rows[row].key = insertQuery.lastInsertId().toLongLong();
			break;
		case SqlTableRow::Update:
			sqlModel->bindToSqlQuery(row, updateQuery, 0);
			updateQuery.bindValue(sqlColumnCount, sqlRow.key);
			sqlHelper->exec(updateQuery);
			break;
		}

		rows[row].pendingStatement = SqlTableRow::None;
	}

	hasPendingStatements = false;
}
