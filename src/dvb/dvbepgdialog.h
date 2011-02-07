/*
 * dvbepgdialog.h
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

#ifndef DVBEPGDIALOG_H
#define DVBEPGDIALOG_H

#include <KDialog>

class QLabel;
class QModelIndex;
class QTreeView;
class DvbEpgModel;
class DvbManager;

class DvbEpgDialog : public KDialog
{
	Q_OBJECT
public:
	DvbEpgDialog(DvbManager *manager_, const QString &currentChannelName, QWidget *parent);
	~DvbEpgDialog();

	static void showDialog(DvbManager *manager_, const QString &currentChannelName,
		QWidget *parent);

private slots:
	void channelActivated(const QModelIndex &index);
	void entryActivated(const QModelIndex &index);
	void scheduleProgram();

private:
	DvbManager *manager;
	DvbEpgModel *epgModel;
	QTreeView *channelView;
	QTreeView *epgView;
	QLabel *contentLabel;
};

#endif /* DVBEPGDIALOG_H */
