/*
 * dvbepg.h
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

#ifndef DVBEPG_H
#define DVBEPG_H

#include "dvbrecording.h"

class DvbChannel;
class DvbDevice;
class DvbEitEntry;
class DvbEitFilter;
class DvbEpgChannelModel;
class DvbEpgProxyModel;

class DvbEpgEntry
{
public:
	DvbEpgEntry() { }
	~DvbEpgEntry() { }

	QString channelName;
	QDateTime begin; // UTC
	QTime duration;
	QString title;
	QString subheading;
	QString details;
	DvbRecordingKey recordingKey;

	int compare(const DvbEpgEntry &other) const;

	bool operator<(const DvbEpgEntry &other) const
	{
		return (compare(other) < 0);
	}

	bool operator!=(const DvbEpgEntry &other) const
	{
		return (compare(other) != 0);
	}
};

class DvbEpgModel : public QObject
{
	Q_OBJECT
public:
	DvbEpgModel(DvbManager *manager_, QObject *parent);
	~DvbEpgModel();

	QList<DvbEpgEntry> getCurrentNext(const QString &channelName) const;
	void startEventFilter(DvbDevice *device, const DvbChannel *channel);
	void stopEventFilter(DvbDevice *device, const DvbChannel *channel);

	void showDialog(const QString &currentChannelName, QWidget *parent);
	QAbstractItemModel *createEpgProxyChannelModel(QObject *parent);
	DvbEpgProxyModel *createEpgProxyModel(QObject *parent);
	void scheduleProgram(const DvbEpgEntry &constEntry, int extraSecondsBefore,
		int extraSecondsAfter);
	QString findChannelNameByDvbEitEntry(const DvbEitEntry &eitEntry);
	void addEntry(const DvbEpgEntry &epgEntry);

signals:
	void entryAdded(const DvbEpgEntry *entry);
	void entryRecordingKeyChanged(const DvbEpgEntry *entry);

private slots:
	void channelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
	void channelLayoutChanged();
	void channelModelReset();
	void channelRowsInserted(const QModelIndex &parent, int start, int end);
	void channelRowsRemoved(const QModelIndex &parent, int start, int end);
	void programRemoved(const DvbRecordingKey &recordingKey);

private:
	DvbManager *manager;
	QList<DvbEpgEntry> entries;
	QMap<DvbRecordingKey, const DvbEpgEntry *> recordingKeyMapping;
	DvbEpgChannelModel *epgChannelModel;
	QList<QExplicitlySharedDataPointer<const DvbChannel> > channels;
	QHash<DvbEitEntry, const DvbChannel *> dvbEitMapping;
	QList<DvbEitFilter> dvbEitFilters;
};

#endif /* DVBEPG_H */
