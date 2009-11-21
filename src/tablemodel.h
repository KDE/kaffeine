/*
 * tablemodel.h
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

#ifndef TABLEMODEL_H
#define TABLEMODEL_H

#include <QAbstractTableModel>
#include <QSqlQuery>
#include <QStringList>

class SqlHelper;
class SqlTableModelBase;

template<class T> class TableModel : public QAbstractTableModel
{
public:
	explicit TableModel(QObject *parent) : QAbstractTableModel(parent)
	{
		headerLabels = T::modelHeaderLabels();
	}

	~TableModel()
	{
		int size = rows.size();

		for (int i = 0; i < size; ++i) {
			T::deleteObject(rows.at(i));
		}
	}

	const typename T::Type &at(int row) const
	{
		return rows.at(row);
	}

	void append(const typename T::Type &row)
	{
		beginInsertRows(QModelIndex(), rows.size(), rows.size());
		rows.append(row);
		endInsertRows();
	}

	void rowUpdated(int row)
	{
		emit dataChanged(index(row, 0), index(row, headerLabels.size() - 1));
	}

	void remove(int row)
	{
		beginRemoveRows(QModelIndex(), row, row);
		rows.removeAt(row);
		endRemoveRows();
	}

	int columnCount(const QModelIndex &parent) const
	{
		if (parent.isValid()) {
			return 0;
		}

		return headerLabels.size();
	}

	int rowCount(const QModelIndex &parent) const
	{
		if (parent.isValid()) {
			return 0;
		}

		return rows.size();
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const
	{
		if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)) {
			return QVariant();
		}

		return headerLabels.at(section);
	}

	QVariant data(const QModelIndex &index, int role) const
	{
		return T::modelData(rows.at(index.row()), index.column(), role);
	}

protected:
	void rawAppend(const typename T::Type &row)
	{
		rows.append(row);
	}

private:
	QList<typename T::Type> rows;
	QStringList headerLabels;
};

class SqlTableRow
{
public:
	SqlTableRow() { }
	~SqlTableRow() { }

	enum PendingStatement {
		None,
		Insert,
		Update
	};

	qint64 sqlKey;
	PendingStatement sqlPendingStatement;
};

class SqlTableHelper
{
public:
	explicit SqlTableHelper(SqlTableModelBase *model_);
	~SqlTableHelper();

	void append();
	void rowUpdated(int row);
	void remove(int row);

	void init(const QString &tableName, const QStringList &columnNames);
	void insert(qint64 key, bool successful);
	void submit();

private:
	SqlTableModelBase *model;
	SqlHelper *sqlHelper;
	QList<SqlTableRow> rows;
	QList<qint64> usedKeys;
	QList<qint64> pendingDeletionKeys;
	qint64 freeKey;
	bool hasPendingStatements;
	bool createTable;

	int sqlColumnCount;
	QString createStatement;
	QString insertStatement;
	QString updateStatement;
	QString deleteStatement;
	QSqlQuery insertQuery;
	QSqlQuery updateQuery;
	QSqlQuery deleteQuery;
};

class SqlTableModelBase
{
public:
	SqlTableModelBase() { }

	virtual void handleRow(qint64 key, const QSqlQuery &query, int index) = 0;
	virtual void bindToSqlQuery(int row, QSqlQuery &query, int index) const = 0;

protected:
	~SqlTableModelBase() { }
};

template<class T> class SqlTableModel : public TableModel<T>, private SqlTableModelBase
{
public:
	explicit SqlTableModel(QObject *parent) : TableModel<T>(parent), helper(this)
	{
	}

	~SqlTableModel()
	{
	}

	void append(const typename T::Type &row)
	{
		helper.append();
		TableModel<T>::append(row);
	}

	void rowUpdated(int row)
	{
		helper.rowUpdated(row);
		TableModel<T>::rowUpdated(row);
	}

	void rowUpdatedTrivially(int row) // doesn't trigger an SQL update
	{
		TableModel<T>::rowUpdated(row);
	}

	void remove(int row)
	{
		helper.remove(row);
		TableModel<T>::remove(row);
	}

protected:
	void initSql()
	{
		helper.init(T::sqlTableName(), T::sqlColumnNames());
	}

	void insert(qint64 key, const typename T::Type &row)
	{
		helper.insert(key, row);

		if (row != NULL) {
			TableModel<T>::rawAppend(row);
		}
	}

private:
	void bindToSqlQuery(int row, QSqlQuery &query, int index) const
	{
		T::bindToSqlQuery(TableModel<T>::at(row), query, index);
	}

	SqlTableHelper helper;
};

#endif /* TABLEMODEL_H */
