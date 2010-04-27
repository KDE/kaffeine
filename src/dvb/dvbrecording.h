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
#include <QTimer>

class KDialog;
class DvbManager;
class DvbRecording;
class DvbSqlRecordingModelInterface;
class ProxyTreeView;

class DvbRecordingIndex
{
public:
	DvbRecordingIndex() : key(0) { }
	explicit DvbRecordingIndex(quint32 key_) : key(key_) { }
	~DvbRecordingIndex() { }

	bool operator==(const DvbRecordingIndex &other) const
	{
		return (key == other.key);
	}

	quint32 key;
};

class DvbRecordingModel : public QAbstractTableModel
{
	Q_OBJECT
	friend class DvbRecordingEditor;
	friend class DvbSqlRecordingModelInterface;
public:
	explicit DvbRecordingModel(DvbManager *manager_);
	~DvbRecordingModel();

	DvbRecordingIndex scheduleProgram(const QString &name, const QString &channel,
		const QDateTime &begin, const QTime &duration); // begin must be local time!
	void removeProgram(const DvbRecordingIndex &index);
	void showDialog(QWidget *parent);

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;

signals:
	void programRemoved(const DvbRecordingIndex &index);

protected:
	DvbRecordingIndex insert(DvbRecording *recording);
	void update(int row);
	void remove(int row);

private slots:
	void checkStatus();
	void newRecording();
	void editRecording();
	void removeRecording();

private:
	DvbManager *manager;
	QList<DvbRecording *> recordings;
	DvbSqlRecordingModelInterface *sqlInterface;
	QTimer checkStatusTimer;
	KDialog *scheduleDialog;
	ProxyTreeView *treeView;
};

#endif /* DVBRECORDING_H */
