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

SqlTableHandler::SqlTableHandler(SqlTableModelBase *model_) : model(model_), freeKey(0),
	hasPendingStatements(false), createTable(false), sqlColumnCount(0)
{
	setParent(model->getModel());
	sqlHelper = SqlHelper::getInstance();
}

SqlTableHandler::~SqlTableHandler()
{
	if (hasPendingStatements) {
		kError() << "pending statements at destruction";
	}
}

void SqlTableHandler::init(const QString &tableName, const QStringList &columnNames)
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
	} else {
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

			int row = model->insertFromSqlQuery(query, 1);

			if (row >= 0) {
				usedKeys.insert(it - usedKeys.constBegin(), key);

				SqlTableRow sqlRow;
				sqlRow.key = key;
				sqlRow.pendingStatement = SqlTableRow::None;
				sqlRow.ident = model->getRowIdent(row);
				rows.insert(row, sqlRow);
			} else {
				pendingDeletionKeys.append(key);
			}
		}
	}

	QObject *itemModel = model->getModel();

	connect(itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
		this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(itemModel, SIGNAL(layoutChanged()), this, SLOT(layoutChanged()));
	connect(itemModel, SIGNAL(modelReset()), this, SLOT(modelReset()));
	connect(itemModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
		this, SLOT(rowsInserted(QModelIndex,int,int)));
	connect(itemModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
		this, SLOT(rowsRemoved(QModelIndex,int,int)));
}

void SqlTableHandler::flush()
{
	if (hasPendingStatements) {
		sqlHelper->collectSubmissions();
	}
}

void SqlTableHandler::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
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

void SqlTableHandler::layoutChanged()
{
	qSort(rows);

	for (int row = 0; row < rows.size(); ++row) {
		int index = qBinaryFind(rows.constBegin() + row, rows.constEnd(),
			model->getRowIdent(row)) - rows.constBegin();

		if (index != row) {
			rows.move(index, row);
		}
	}
}

void SqlTableHandler::modelReset()
{
	rowsRemoved(QModelIndex(), 0, rows.size());
	rowsInserted(QModelIndex(), 0, model->getRowCount());
}

void SqlTableHandler::rowsInserted(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent);

	for (int row = start; row <= end; ++row) {
		SqlTableRow sqlRow;

		if (!pendingDeletionKeys.isEmpty()) {
			qint64 key = pendingDeletionKeys.takeLast();
			QList<qint64>::ConstIterator it = qLowerBound(usedKeys, key);
			Q_ASSERT((it == usedKeys.constEnd()) || (*it != key));
			usedKeys.insert(it - usedKeys.constBegin(), key);

			sqlRow.key = key;
			sqlRow.pendingStatement = SqlTableRow::Update;
		} else {
			QList<qint64>::ConstIterator it = qLowerBound(usedKeys, freeKey);

			while ((it != usedKeys.constEnd()) && (*it == freeKey)) {
				++freeKey;
				++it;
			}

			usedKeys.insert(it - usedKeys.constBegin(), freeKey);
			sqlRow.key = freeKey;
			sqlRow.pendingStatement = SqlTableRow::Insert;
			++freeKey;
		}

		sqlRow.ident = model->getRowIdent(row);
		rows.insert(row, sqlRow);
	}

	if (!hasPendingStatements) {
		hasPendingStatements = true;
		sqlHelper->requestSubmission(this);
	}
}

void SqlTableHandler::rowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent);

	for (int row = end; row >= start; --row) {
		SqlTableRow sqlRow = rows.takeAt(row);

		QList<qint64>::ConstIterator it = qLowerBound(usedKeys, sqlRow.key);
		Q_ASSERT((it != usedKeys.constEnd()) && (*it == sqlRow.key));
		usedKeys.removeAt(it - usedKeys.constBegin());

		if (sqlRow.key < freeKey) {
			freeKey = sqlRow.key;
		}

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

void SqlTableHandler::submit()
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
			insertQuery.bindValue(0, sqlRow.key);
			model->bindToSqlQuery(row, insertQuery, 1);
			sqlHelper->exec(insertQuery);
			break;
		case SqlTableRow::Update:
			model->bindToSqlQuery(row, updateQuery, 0);
			updateQuery.bindValue(sqlColumnCount, sqlRow.key);
			sqlHelper->exec(updateQuery);
			break;
		}

		rows[row].pendingStatement = SqlTableRow::None;
	}

	hasPendingStatements = false;
}
