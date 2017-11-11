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

#include <QDialog>
#include "dvbchannel.h"

class QLabel;
class QModelIndex;
class QTreeView;
class DvbEpgChannelTableModel;
class DvbEpgTableModel;
class DvbManager;

class DvbEpgDialog : public QDialog
{
	Q_OBJECT
public:
	DvbEpgDialog(DvbManager *manager_, QWidget *parent);
	~DvbEpgDialog();

	void setCurrentChannel(const DvbSharedChannel &channel);

private slots:
	void languageChanged(const QString);
	void languageAdded(const QString);
	void channelActivated(const QModelIndex &index);
	void entryActivated(const QModelIndex &index);
	void checkEntry();
	void scheduleProgram();

private:
	DvbManager *manager;
	DvbEpgChannelTableModel *epgChannelTableModel;
	DvbEpgTableModel *epgTableModel;
	QString currentLanguage;
	QTreeView *channelView;
	QTreeView *epgView;
	QLabel *contentLabel;
	QComboBox *languageBox;
	QLabel *languageLabel;
};

#endif /* DVBEPGDIALOG_H */
