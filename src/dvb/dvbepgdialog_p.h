/*
 * dvbepgdialog_p.h
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

#ifndef DVBEPGDIALOG_P_H
#define DVBEPGDIALOG_P_H

#include <QStringMatcher>
#include "dvbepg.h"

class DvbEpgChannelModel : public QAbstractTableModel
{
public:
	explicit DvbEpgChannelModel(QObject *parent);
	~DvbEpgChannelModel();

	void insertChannelName(const QString &channelName);
	void removeChannelName(const QString &channelName);

private:
	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	QList<QString> channelNames;
};

class DvbEpgTableModelEntry
{
public:
	explicit DvbEpgTableModelEntry(const DvbEpgEntry *entry_) : entry(entry_) { }
	~DvbEpgTableModelEntry() { }

	const DvbEpgEntry *constData() const
	{
		return entry;
	}

	const DvbEpgEntry *operator->() const
	{
		return entry;
	}

	bool operator<(const DvbEpgTableModelEntry &other) const
	{
		return (*entry < *other.entry);
	}

private:
	const DvbEpgEntry *entry;
};

Q_DECLARE_TYPEINFO(DvbEpgTableModelEntry, Q_MOVABLE_TYPE);

class DvbEpgTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbEpgTableModel(DvbEpgModel *epgModel_, QObject *parent);
	~DvbEpgTableModel();

	QAbstractItemModel *createEpgProxyChannelModel(QObject *parent);
	const DvbEpgEntry *getEntry(int row) const;
	void setChannelNameFilter(const QString &channelName);
	void scheduleProgram(int row, int extraSecondsBefore, int extraSecondsAfter);

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
	void setContentFilter(const QString &pattern);

private slots:
	void entryAdded(const DvbEpgEntry *entry);
	void entryChanged(const DvbEpgEntry *entry, const DvbEpgEntry &oldEntry);
	void entryAboutToBeRemoved(const DvbEpgEntry *entry);

private:
	void customEvent(QEvent *event);

	DvbEpgModel *epgModel;
	QList<DvbEpgTableModelEntry> entries;
	QHash<QString, int> channelNameCount;
	DvbEpgChannelModel epgChannelModel;
	QString channelNameFilter;
	QStringMatcher contentFilter;
	bool contentFilterEventPending;
};

#endif /* DVBEPGDIALOG_P_H */
