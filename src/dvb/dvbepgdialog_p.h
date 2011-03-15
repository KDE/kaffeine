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

class DvbEpgChannelTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbEpgChannelTableModel(DvbManager *manager, QObject *parent);
	~DvbEpgChannelTableModel();

	const DvbSharedChannel &getChannel(int row) const;

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private slots:
	void epgChannelAdded(const DvbSharedChannel &channel);
	void epgChannelRemoved(const DvbSharedChannel &channel);

private:
	QList<DvbSharedChannel> channels;
	DvbChannelLessThan lessThan;
};

class DvbEpgTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbEpgTableModel(DvbEpgModel *epgModel_, QObject *parent);
	~DvbEpgTableModel();

	const DvbEpgEntry *getEntry(int row) const;
	void setChannelFilter(const DvbSharedChannel &channel);

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
	void setContentFilter(const QString &pattern);

private slots:
	void entryAdded(const DvbEpgEntry *entry);
	void entryUpdated(const DvbEpgEntry *entry, const DvbEpgEntry &oldEntry);
	void entryRemoved(const DvbEpgEntry *entry);

private:
	void customEvent(QEvent *event);

	class LessThan
	{
	public:
		bool operator()(const DvbEpgEntry &x, const DvbEpgEntry &y) const;

		bool operator()(const DvbEpgEntry &x, const DvbEpgEntry *y) const
		{
			return (*this)(x, *y);
		}

		bool operator()(const DvbEpgEntry *x, const DvbEpgEntry &y) const
		{
			return (*this)(*x, y);
		}

		bool operator()(const DvbEpgEntry *x, const DvbEpgEntry *y) const
		{
			return (*this)(*x, *y);
		}
	};

	DvbEpgModel *epgModel;
	QList<const DvbEpgEntry *> entries;
	DvbSharedChannel channelFilter;
	QStringMatcher contentFilter;
	bool contentFilterEventPending;
};

#endif /* DVBEPGDIALOG_P_H */
