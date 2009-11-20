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

TableModel::TableModel(QObject *model) : QAbstractTableModel(model)
{
}

TableModel::~TableModel()
{
}

TableRow *TableModel::at(int row) const
{
	return rows.at(row).data();
}

void TableModel::append(TableRow *row)
{
	beginInsertRows(QModelIndex(), rows.size(), rows.size());
	rows.append(QExplicitlySharedDataPointer<TableRow>(row));
	endInsertRows();
}

void TableModel::rowUpdated(int row)
{
	emit dataChanged(index(row, 0), index(row, headerLabels.size() - 1));
}

void TableModel::remove(int row)
{
	beginRemoveRows(QModelIndex(), row, row);
	rows.removeAt(row);
	endRemoveRows();
}

int TableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return headerLabels.size();
}

int TableModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return rows.size();
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)) {
		return QVariant();
	}

	return headerLabels.at(section);
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
	return rows.at(index.row())->modelData(index.column(), role);
}

void TableModel::setHeaderLabels(const QStringList &headerLabels_)
{
	headerLabels = headerLabels_;
}

SqlTableModel::SqlTableModel(QObject *parent) : TableModel(parent), freeIndex(0),
	hasPendingStatements(false), createTable(false), sqlColumnCount(0)
{
	sqlHelper = SqlHelper::getInstance();
}

SqlTableModel::~SqlTableModel()
{
	if (hasPendingStatements) {
		sqlHelper->collectSubmissions();
	}
}

void SqlTableModel::append(SqlTableRow *row)
{
	if (!pendingDeletionIndexes.isEmpty()) {
		qint64 index = pendingDeletionIndexes.takeLast();
		QList<qint64>::ConstIterator it = qLowerBound(usedIndexes, index);
		Q_ASSERT((it == usedIndexes.constEnd()) || (*it != index));

		usedIndexes.insert(it - usedIndexes.constBegin(), index);
		row->sqlIndex = index;
		row->sqlPendingStatement = SqlTableRow::Update;
	} else {
		QList<qint64>::ConstIterator it = qLowerBound(usedIndexes, freeIndex);

		while ((it != usedIndexes.constEnd()) && (*it == freeIndex)) {
			++freeIndex;
			++it;
		}

		usedIndexes.insert(it - usedIndexes.constBegin(), freeIndex);
		row->sqlIndex = freeIndex;
		row->sqlPendingStatement = SqlTableRow::Insert;

		++freeIndex;
	}

	if (!hasPendingStatements) {
		hasPendingStatements = true;
		sqlHelper->requestSubmission(this);
	}

	TableModel::append(row);
}

void SqlTableModel::rowUpdated(int row)
{
	SqlTableRow *sqlRow = rows.at(row)->toSqlTableRow();
	Q_ASSERT(sqlRow != NULL);

	switch (sqlRow->sqlPendingStatement) {
	case SqlTableRow::Insert:
	case SqlTableRow::Update:
		break;
	case SqlTableRow::None:
		sqlRow->sqlPendingStatement = SqlTableRow::Update;

		if (!hasPendingStatements) {
			hasPendingStatements = true;
			sqlHelper->requestSubmission(this);
		}

		break;
	}

	TableModel::rowUpdated(row);
}

void SqlTableModel::rowUpdatedTrivially(int row)
{
	TableModel::rowUpdated(row);
}

void SqlTableModel::remove(int row)
{
	SqlTableRow *sqlRow = rows.at(row)->toSqlTableRow();
	Q_ASSERT(sqlRow != NULL);

	qint64 index = sqlRow->sqlIndex;
	QList<qint64>::ConstIterator it = qLowerBound(usedIndexes, index);
	Q_ASSERT((it != usedIndexes.constEnd()) && (*it == index));

	usedIndexes.removeAt(it - usedIndexes.constBegin());

	if (index < freeIndex) {
		freeIndex = index;
	}

	switch (sqlRow->sqlPendingStatement) {
	case SqlTableRow::Insert:
		break;
	case SqlTableRow::None:
	case SqlTableRow::Update:
		pendingDeletionIndexes.append(index);

		if (!hasPendingStatements) {
			hasPendingStatements = true;
			sqlHelper->requestSubmission(this);
		}

		break;
	}

	TableModel::remove(row);
}

void SqlTableModel::initSql(const QString &tableName, const QStringList &columnNames)
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
		qint64 index = query.value(0).toLongLong();
		SqlTableRow *row = createRow(query, 1);

		if (row == NULL) {
			pendingDeletionIndexes.append(index);
			continue;
		}

		QList<qint64>::ConstIterator it = qLowerBound(usedIndexes, index);

		if ((it != usedIndexes.constEnd()) && (*it == index)) {
			kError() << "SQL primary key isn't unique" << index;
			delete row;
			continue;
		}

		usedIndexes.insert(it - usedIndexes.constBegin(), index);
		row->sqlIndex = index;
		row->sqlPendingStatement = SqlTableRow::None;
		rows.append(QExplicitlySharedDataPointer<TableRow>(row));
	}
}

void SqlTableModel::customEvent(QEvent *)
{
	if (createTable) {
		createTable = false;
		sqlHelper->exec(createStatement);

		// queries can only be prepared if the table exists
		insertQuery = sqlHelper->prepare(insertStatement);
		updateQuery = sqlHelper->prepare(updateStatement);
		deleteQuery = sqlHelper->prepare(deleteStatement);
	}

	foreach (qint64 index, pendingDeletionIndexes) {
		deleteQuery.bindValue(0, index);
		sqlHelper->exec(deleteQuery);
	}

	pendingDeletionIndexes.clear();

	for (int i = 0; i < rows.size(); ++i) {
		SqlTableRow *row = rows.at(i)->toSqlTableRow();
		Q_ASSERT(row != NULL);

		switch (row->sqlPendingStatement) {
		case SqlTableRow::None:
			continue;
		case SqlTableRow::Insert:
			insertQuery.bindValue(0, row->sqlIndex);
			row->bindToSqlQuery(insertQuery, 1);
			sqlHelper->exec(insertQuery);
			break;
		case SqlTableRow::Update:
			row->bindToSqlQuery(updateQuery, 0);
			updateQuery.bindValue(sqlColumnCount, row->sqlIndex);
			sqlHelper->exec(updateQuery);
			break;
		}

		row->sqlPendingStatement = SqlTableRow::None;
	}

	hasPendingStatements = false;
}
