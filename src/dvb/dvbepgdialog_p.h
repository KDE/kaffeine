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

class DvbEpgEntryLessThan
{
public:
	DvbEpgEntryLessThan() { }
	~DvbEpgEntryLessThan() { }

	bool operator()(const DvbEpgEntry &x, const QString &channelName) const
	{
		return (x.channelName < channelName);
	}

	bool operator()(const QString &channelName, const DvbEpgEntry &y) const
	{
		return (channelName < y.channelName);
	}

	bool operator()(const DvbEpgEntry &x, const DvbEpgEntry &y) const
	{
		if (x.channelName != y.channelName) {
			return (x.channelName < y.channelName);
		}

		if (x.begin != y.begin) {
			return (x.begin < y.begin);
		}

		if (x.duration != y.duration) {
			return (x.duration < y.duration);
		}

		if (x.title != y.title) {
			return (x.title < y.title);
		}

		if (x.subheading != y.subheading) {
			return (x.subheading < y.subheading);
		}

		return (x.details < y.details);
	}

	bool operator()(const DvbEpgEntry *x, const DvbEpgEntry &y) const
	{
		return (*this)(*x, y);
	}

	bool operator()(const DvbEpgEntry &x, const DvbEpgEntry *y) const
	{
		return (*this)(x, *y);
	}
};

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

class DvbEpgModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbEpgModel(DvbEpg *epg_, QObject *parent);
	~DvbEpgModel();

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

	DvbEpg *epg;
	QList<const DvbEpgEntry *> entries;
	QHash<QString, int> channelNameCount;
	DvbEpgChannelModel epgChannelModel;
	QString channelNameFilter;
	QStringMatcher contentFilter;
	bool contentFilterEventPending;
};

#endif /* DVBEPGDIALOG_P_H */
