/*
 * dvbrecordingdialog.h
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

#ifndef DVBRECORDINGDIALOG_H
#define DVBRECORDINGDIALOG_H

#include <KDialog>

class QTreeView;
class DvbManager;
class DvbRecordingTableModel;

class DvbRecordingDialog : public KDialog
{
	Q_OBJECT
public:
	DvbRecordingDialog(DvbManager *manager_, QWidget *parent);
	~DvbRecordingDialog();

	static void showDialog(DvbManager *manager_, QWidget *parent);

private slots:
	void newRecording();
	void editRecording();
	void removeRecording();

private:
	DvbManager *manager;
	DvbRecordingTableModel *model;
	QTreeView *treeView;
};

#endif /* DVBRECORDINGDIALOG_H */
