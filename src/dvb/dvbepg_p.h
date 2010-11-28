/*
 * dvbepg_p.h
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

#ifndef DVBEPG_P_H
#define DVBEPG_P_H

#include <KDialog>
#include "dvbbackenddevice.h"
#include "dvbepg.h"
#include "dvbtransponder.h"

class QLabel;
class QTreeView;
class DvbDevice;

Q_DECLARE_METATYPE(const DvbEpgEntry *)

class DvbEpgEntryLessThan
{
public:
	DvbEpgEntryLessThan() { }
	~DvbEpgEntryLessThan() { }

	bool operator()(const DvbEpgEntry &x, const QString &channelName) const
	{
		return (x.channelName < channelName);
	}

	bool operator()(const DvbEpgEntry *x, const DvbEpgEntry *y) const
	{
		return (*x < *y);
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

class DvbEpgProxyModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbEpgProxyModel(DvbEpgModel *epgModel_, const QList<DvbEpgEntry> *epgModelEntries_,
		QObject *parent);
	~DvbEpgProxyModel();

	enum ItemDataRole
	{
		DvbEpgEntryRole = Qt::UserRole
	};

	void setChannelNameFilter(const QString &channelName);
	void scheduleProgram(int proxyRow, int extraSecondsBefore, int extraSecondsAfter);

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
	void setContentFilter(const QString &pattern);

private slots:
	void sourceEntryAdded(const DvbEpgEntry *entry);
	void sourceEntryRecordingKeyChanged(const DvbEpgEntry *entry);

private:
	void customEvent(QEvent *event);

	DvbEpgModel *epgModel;
	const QList<DvbEpgEntry> *epgModelEntries;
	QList<const DvbEpgEntry *> entries;
	QString channelNameFilter;
	QStringMatcher contentFilter;
	bool contentFilterEventPending;
};

class DvbEpgDialog : public KDialog
{
	Q_OBJECT
public:
	DvbEpgDialog(DvbManager *manager_, const QString &currentChannelName, QWidget *parent);
	~DvbEpgDialog();

private slots:
	void channelActivated(const QModelIndex &index);
	void entryActivated(const QModelIndex &index);
	void scheduleProgram();

private:
	DvbManager *manager;
	DvbEpgProxyModel *epgProxyModel;
	QTreeView *channelView;
	QTreeView *epgView;
	QLabel *contentLabel;
};

class DvbEitEntry
{
public:
	DvbEitEntry() : networkId(0), transportStreamId(0), serviceId(0) { }
	explicit DvbEitEntry(const DvbChannel *channel);
	~DvbEitEntry() { }

	bool operator==(const DvbEitEntry &other) const
	{
		return ((source == other.source) && (networkId == other.networkId) &&
			(transportStreamId == other.transportStreamId) &&
			(serviceId == other.serviceId));
	}

	QString source;
	int networkId;
	int transportStreamId;
	int serviceId;
};

class DvbEitFilter : public DvbSectionFilter
{
public:
	explicit DvbEitFilter(DvbEpgModel *epgModel_) : epgModel(epgModel_), device(NULL) { }
	~DvbEitFilter() { }

	DvbEpgModel *epgModel;
	DvbDevice *device;
	QString source;
	DvbTransponder transponder;

private:
	static QTime bcdToTime(int bcd);

	void processSection(const char *data, int size, int crc);
};

#endif /* DVBEPG_P_H */
