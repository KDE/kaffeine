/*
 * dvbrecording.h
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

#ifndef DVBRECORDING_H
#define DVBRECORDING_H

#include <QPersistentModelIndex>
#include <QTimer>

class KDialog;
class DvbManager;
class DvbRecordingModel;
class ProxyTreeView;

class DvbRecordingManager : public QObject
{
	Q_OBJECT
public:
	explicit DvbRecordingManager(DvbManager *manager_);
	~DvbRecordingManager();

	void scheduleProgram(const QString &name, const QString &channel, const QDateTime &begin,
		const QTime &duration); // begin must be local time!
	void startInstantRecording(const QString &name, const QString &channel);
	void stopInstantRecording(); // stops the last started instant recording

	void showDialog(QWidget *parent);

signals:
	void instantRecordingRemoved(); // not emitted by stopInstantRecording()

private slots:
	void checkStatus();
	void checkInstantRecording();
	void newRecording();
	void editRecording();
	void removeRecording();

private:
	DvbManager *manager;
	DvbRecordingModel *model;
	QTimer checkStatusTimer;
	bool checkingStatus;
	bool instantRecordingActive;
	QPersistentModelIndex instantRecordingIndex;
	KDialog *scheduleDialog;
	ProxyTreeView *treeView;
};

#endif /* DVBRECORDING_H */
