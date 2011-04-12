/*
 * sqlinterface.h
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef SQLINTERFACE_H
#define SQLINTERFACE_H

#include <QExplicitlySharedDataPointer>
#include <QMap>
#include <QSqlQuery>

class SqlHelper;

class SqlKey
{
public:
	explicit SqlKey(quint32 key = 0) : sqlKey(key) { }
	~SqlKey() { }

	bool isSqlKeyValid() const
	{
		return (sqlKey != 0);
	}

	void setSqlKey(const SqlKey &other)
	{
		sqlKey = other.sqlKey;
	}

	bool operator==(const SqlKey &other) const
	{
		return (sqlKey == other.sqlKey);
	}

	bool operator!=(const SqlKey &other) const
	{
		return (sqlKey != other.sqlKey);
	}

	bool operator<(const SqlKey &other) const
	{
		return (sqlKey < other.sqlKey);
	}

	quint32 sqlKey;
};

Q_DECLARE_TYPEINFO(SqlKey, Q_MOVABLE_TYPE);

class SqlInterface
{
public:
	SqlInterface();
	virtual ~SqlInterface();

	void sqlInit(const QString &tableName, const QStringList &columnNames);
	void sqlInsert(SqlKey key);
	void sqlUpdate(SqlKey key);
	void sqlRemove(SqlKey key);
	void sqlFlush();

	/* for SqlHelper */
	void sqlSubmit();

	template<class Container> SqlKey sqlFindFreeKey(const Container &container) const
	{
		SqlKey sqlKey(1);

		if (!container.isEmpty()) {
			sqlKey = SqlKey((container.constEnd() - 1).key().sqlKey + 1);

			while (container.contains(sqlKey) || !sqlKey.isSqlKeyValid()) {
				sqlKey = SqlKey(qrand());
			}
		}

		return sqlKey;
	}

protected:
	virtual void bindToSqlQuery(SqlKey sqlKey, QSqlQuery &query, int index) const = 0;
	virtual bool insertFromSqlQuery(SqlKey sqlKey, const QSqlQuery &query, int index) = 0;

private:
	enum PendingStatement {
		Nothing,
		RemoveAndInsert,
		Insert,
		Update,
		Remove
	};

	void requestSubmission();

	QExplicitlySharedDataPointer<SqlHelper> sqlHelper;
	QMap<SqlKey, PendingStatement> pendingStatements;
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

#endif /* SQLINTERFACE_H */
