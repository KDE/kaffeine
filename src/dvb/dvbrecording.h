/*
 * dvbrecording.h
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

#ifndef DVBRECORDING_H
#define DVBRECORDING_H

#include <QAbstractTableModel>
#include <QDateTime>

class DvbManager;
class DvbRecording;
class SqlTableModelInterface;

class DvbRecordingKey
{
public:
	explicit DvbRecordingKey(quint32 key_ = 0) : key(key_) { }
	~DvbRecordingKey() { }

	bool isValid() const
	{
		return (key != 0);
	}

	bool operator==(const DvbRecordingKey &other) const
	{
		return (key == other.key);
	}

	bool operator<(const DvbRecordingKey &other) const
	{
		return (key < other.key);
	}

	quint32 key;
};

class DvbRecordingEntry
{
public:
	DvbRecordingEntry() : repeat(0), isRunning(false) { }
	~DvbRecordingEntry() { }

	QString name;
	QString channelName;
	QDateTime begin; // UTC
	QTime duration;
	int repeat; // 1 (monday) | 2 (tuesday) | 4 (wednesday) | etc
	bool isRunning;
};

class DvbRecordingModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbRecordingModel(DvbManager *manager_, QObject *parent);
	~DvbRecordingModel();

	QMap<DvbRecordingKey, DvbRecordingEntry> listProgramSchedule();
	DvbRecordingKey scheduleProgram(const DvbRecordingEntry &entry);
	void removeProgram(const DvbRecordingKey &key);

	enum ItemDataRole
	{
		SortRole = Qt::UserRole,
		DvbRecordingEntryRole = Qt::UserRole + 1
	};

	QAbstractItemModel *createProxyModel(QObject *parent);
	void showDialog(QWidget *parent);
	void mayCloseApplication(bool *ok, QWidget *parent);

signals:
	void programRemoved(const DvbRecordingKey &key);

private slots:
	void checkStatus();

private:
	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool removeRows(int row, int count, const QModelIndex &parent);
	bool setData(const QModelIndex &modelIndex, const QVariant &value, int role);

	DvbManager *manager;
	QList<DvbRecording *> recordings;
	SqlTableModelInterface *sqlInterface;
	bool hasActiveRecordings;
};

#endif /* DVBRECORDING_H */
