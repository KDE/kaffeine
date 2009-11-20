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
#include <QSharedData>
#include <QSqlQuery>
#include <QStringList>

class DvbRecording;
class SqlHelper;
class SqlTableRow;

class TableRow : public QSharedData
{
public:
	TableRow() { }
	virtual ~TableRow() { }

	virtual DvbRecording *toDvbRecording()
	{
		return NULL;
	}

	virtual SqlTableRow *toSqlTableRow()
	{
		return NULL;
	}

	virtual QVariant modelData(int column, int row) const = 0;
};

Q_DECLARE_TYPEINFO(QExplicitlySharedDataPointer<TableRow>, Q_MOVABLE_TYPE);

class TableModel : public QAbstractTableModel
{
	friend class SqlTableModel;
public:
	explicit TableModel(QObject *parent);
	~TableModel();

	TableRow *at(int row) const;
	void append(TableRow *row);
	void rowUpdated(int row);
	void remove(int row);

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;

protected:
	void setHeaderLabels(const QStringList &headerLabels_);

private:
	QList<QExplicitlySharedDataPointer<TableRow> > rows;
	QStringList headerLabels;
};

class SqlTableRow : public TableRow
{
	friend class SqlTableModel;
public:
	SqlTableRow() { }
	~SqlTableRow() { }

	SqlTableRow *toSqlTableRow()
	{
		return this;
	}

	virtual void bindToSqlQuery(QSqlQuery &query, int index) const = 0;

private:
	Q_DISABLE_COPY(SqlTableRow);

	enum PendingStatement {
		None,
		Insert,
		Update
	};

	qint64 sqlIndex;
	PendingStatement sqlPendingStatement;
};

class SqlTableModel : public TableModel
{
public:
	explicit SqlTableModel(QObject *parent);
	~SqlTableModel();

	void append(SqlTableRow *row);
	void rowUpdated(int row);
	void rowUpdatedTrivially(int row); // doesn't trigger an SQL update
	void remove(int row);

protected:
	void initSql(const QString &tableName, const QStringList &columnNames);
	virtual SqlTableRow *createRow(const QSqlQuery &query, int index) const = 0;

private:
	void append(TableRow *row); // hide function
	void customEvent(QEvent *);

	SqlHelper *sqlHelper;
	QList<qint64> usedIndexes;
	QList<qint64> pendingDeletionIndexes;
	qint64 freeIndex;
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
