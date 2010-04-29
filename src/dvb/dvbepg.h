/*
 * dvbepg.h
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

#ifndef DVBEPG_H
#define DVBEPG_H

#include <QAbstractTableModel>
#include <KDialog>
#include "dvbrecording.h"
#include "dvbsi.h"

class QLabel;
class QListView;
class QStringListModel;
class QTreeView;
class DvbEitEntry;
class DvbManager;

class DvbEpgEntry
{
public:
	QString channel;
	QDateTime begin;
	QTime duration;
	QDateTime end;
	QString title;
	QString subheading;
	QString details;
	DvbRecordingIndex recordingIndex;

	bool operator<(const DvbEpgEntry &x) const
	{
		if (channel != x.channel) {
			return channel < x.channel;
		}

		return begin < x.begin;
	}
};

class DvbEpgModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit DvbEpgModel(DvbManager *manager_);
	~DvbEpgModel();

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	void addEntry(const DvbEpgEntry &entry);
	bool contains(const QString &channel) const;
	void resetChannel();
	void setChannel(const QString &channel);
	const DvbEpgEntry *getEntry(int row) const;
	QList<DvbEpgEntry> getCurrentNext(const QString &channel) const;
	void scheduleProgram(int row, int extraSecondsBefore, int extraSecondsAfter);

private slots:
	void programRemoved(const DvbRecordingIndex &recordingIndex);

private:
	DvbManager *manager;
	QList<DvbEpgEntry> allEntries;
	QList<DvbEpgEntry *> filteredEntries;
	QMap<DvbRecordingIndex, DvbEpgEntry *> recordingIndexes;
};

class DvbEitFilter : public DvbSectionFilter
{
public:
	DvbEitFilter();
	~DvbEitFilter();

	void setManager(DvbManager *manager_);
	void setSource(const QString &source_);

private:
	static QTime bcdToTime(int bcd);

	void processSection(const QByteArray &data);

	DvbManager *manager;
	DvbEpgModel *model;
	QString source;
	QMap<DvbEitEntry, QString> channelMapping;
};

class DvbEpgDialog : public KDialog
{
	Q_OBJECT
public:
	DvbEpgDialog(DvbManager *manager_, const QString &currentChannel, QWidget *parent);
	~DvbEpgDialog();

private slots:
	void refresh();
	void channelActivated(const QModelIndex &index);
	void entryActivated(const QModelIndex &index);
	void scheduleProgram();

private:
	DvbManager *manager;
	QStringListModel *channelModel;
	QListView *channelView;
	DvbEpgModel *epgModel;
	QTreeView *epgView;
	QLabel *contentLabel;
};

#endif /* DVBEPG_H */
