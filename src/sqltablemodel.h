/*
 * sqltablemodel.h
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

#ifndef SQLTABLEMODEL_H
#define SQLTABLEMODEL_H

#include <QHash>
#include <QMap>
#include <QSqlQuery>

class QAbstractItemModel;
class QModelIndex;
class SqlHelper;

class SqlTableModelInterface : public QObject
{
	Q_OBJECT
	friend class SqlHelper;
public:
	explicit SqlTableModelInterface(QObject *parent);
	virtual ~SqlTableModelInterface();

	void flush();
	quint32 keyForRow(int row) const;
	int rowForKey(quint32 key) const;

protected:
	void init(QAbstractItemModel *model, const QString &tableName,
		const QStringList &columnNames);

	virtual int insertFromSqlQuery(const QSqlQuery &query, int index) = 0;
	virtual void bindToSqlQuery(QSqlQuery &query, int index, int row) const = 0;

private slots:
	void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
	void layoutChanged();
	void modelReset();
	void rowsInserted(const QModelIndex &parent, int start, int end);
	void rowsRemoved(const QModelIndex &parent, int start, int end);

private:
	enum PendingStatement {
		None,
		RemoveAndInsert,
		Insert,
		Update,
		Remove
	};

	void requestSubmission();
	void submit();

	SqlHelper *sqlHelper;
	QList<quint32> rowToKeyMapping;
	QHash<quint32, int> keyToRowMapping;
	QMap<quint32, PendingStatement> pendingStatements;
	bool createTable;
	bool hasPendingStatements;

	int sqlColumnCount;
	QString createStatement;
	QString insertStatement;
	QString updateStatement;
	QString deleteStatement;
	QSqlQuery insertQuery;
	QSqlQuery updateQuery;
	QSqlQuery deleteQuery;
};

#endif /* SQLTABLEMODEL_H */
