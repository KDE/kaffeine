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

template<class T> class TableModel : public QAbstractTableModel
{
public:
	explicit TableModel(QObject *parent) : QAbstractTableModel(parent)
	{
		headerLabels = T::modelHeaderLabels();
	}

	~TableModel()
	{
	}

	const typename T::Type *at(int row) const
	{
		return rows.at(row).constData();
	}

	typename T::Type *writableAt(int row) const
	{
		return rows.at(row).data();
	}

	void append(typename T::Type *row)
	{
		beginInsertRows(QModelIndex(), rows.size(), rows.size());
		rows.append(typename T::StorageType(row));
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
		return T::modelData(rows.at(index.row()).constData(), index.column(), role);
	}

protected:
	void rawAppend(typename T::Type *row)
	{
		rows.append(typename T::StorageType(row));
	}

private:
	QList<typename T::StorageType> rows;
	QStringList headerLabels;
};

class SqlTableModelBase
{
public:
	SqlTableModelBase() { }

	virtual int insertFromSqlQuery(const QSqlQuery &query, int index) = 0;
	virtual void bindToSqlQuery(int row, QSqlQuery &query, int index) const = 0;

protected:
	~SqlTableModelBase() { }
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

	qint64 key;
	PendingStatement pendingStatement;
};

class SqlModelAdaptor : public QObject
{
	Q_OBJECT
	friend class SqlHelper;
public:
	SqlModelAdaptor(QAbstractItemModel *model_, SqlTableModelBase *sqlModel_);
	~SqlModelAdaptor();

	void init(const QString &tableName, const QStringList &columnNames);
	void flush();

private slots:
	void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
	void layoutChanged();
	void modelReset();
	void rowsInserted(const QModelIndex &parent, int start, int end);
	void rowsRemoved(const QModelIndex &parent, int start, int end);

private:
	void submit();

	QAbstractItemModel *model;
	SqlTableModelBase *sqlModel;
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

#endif /* TABLEMODEL_H */
