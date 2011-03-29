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

#include "dvbchanneldialog.h"
#include "dvbepg.h"

class DvbEpgEntryLessThan
{
public:
	DvbEpgEntryLessThan() { }
	~DvbEpgEntryLessThan() { }

	bool operator()(const DvbSharedEpgEntry &x, const DvbSharedEpgEntry &y) const;
};

class DvbEpgChannelTableModelHelper
{
public:
	DvbEpgChannelTableModelHelper() { }
	~DvbEpgChannelTableModelHelper() { }

	typedef DvbSharedChannel ItemType;
	typedef DvbChannelLessThan LessThanType;

	int columnCount() const
	{
		return 1;
	}

	bool filterAcceptsItem(const DvbSharedChannel &channel) const
	{
		Q_UNUSED(channel)
		return true;
	}
};

class DvbEpgChannelTableModel : public TableModel<DvbEpgChannelTableModelHelper>
{
	Q_OBJECT
public:
	explicit DvbEpgChannelTableModel(QObject *parent);
	~DvbEpgChannelTableModel();

	void setManager(DvbManager *manager);

	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private slots:
	void epgChannelAdded(const DvbSharedChannel &channel);
	void epgChannelRemoved(const DvbSharedChannel &channel);
};

class DvbEpgTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbEpgTableModel(DvbEpgModel *epgModel_, QObject *parent);
	~DvbEpgTableModel();

	DvbSharedEpgEntry getEntry(int row) const;
	void setChannelFilter(const DvbSharedChannel &channel);

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
	void setContentFilter(const QString &pattern);

private slots:
	void entryAdded(const DvbSharedEpgEntry &entry);
	void entryAboutToBeUpdated(const DvbSharedEpgEntry &entry);
	void entryUpdated(const DvbSharedEpgEntry &entry);
	void entryRemoved(const DvbSharedEpgEntry &entry);

private:
	void customEvent(QEvent *event);

	DvbEpgModel *epgModel;
	QList<DvbSharedEpgEntry> entries;
	DvbSharedChannel channelFilter;
	QStringMatcher contentFilter;
	int updatingRow;
	bool contentFilterEventPending;
};

#endif /* DVBEPGDIALOG_P_H */
