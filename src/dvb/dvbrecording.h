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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef DVBRECORDING_H
#define DVBRECORDING_H

#include <QAbstractTableModel>
#include <KDialog>

class QAbstractItemModel;
class QComboBox;
class QDateTimeEdit;
class QLineEdit;
class QTimeEdit;
class DvbManager;
class DvbRecording;
class ProxyTreeView;

class DvbRecordingModel : public QAbstractTableModel
{
public:
	explicit DvbRecordingModel(DvbManager *manager_);
	~DvbRecordingModel();

	DvbRecording *at(int i);
	void append(DvbRecording *recording);
	void remove(int i);
	void updated(int i);

	int columnCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	int rowCount(const QModelIndex &parent) const;

private:
	void timerEvent(QTimerEvent *event);

	void checkStatus();

	DvbManager *manager;
	QList<DvbRecording *> recordings;
	int timerId;
};

class DvbRecordingDialog : public KDialog
{
	Q_OBJECT
public:
	DvbRecordingDialog(DvbManager *manager_, QWidget *parent);
	~DvbRecordingDialog();

private slots:
	void newRecording();
	void editRecording();
	void removeRecording();

private:
	DvbManager *manager;
	DvbRecordingModel *model;
	ProxyTreeView *treeView;
};

class DvbRecordingEditor : public KDialog
{
	Q_OBJECT
public:
	DvbRecordingEditor(QAbstractItemModel *channels, const DvbRecording *recording,
		QWidget *parent);
	~DvbRecordingEditor() { }

	void updateRecording(DvbRecording *recording) const;

public slots:
	void beginChanged(const QDateTime &dateTime);
	void durationChanged(const QTime &time);
	void endChanged(const QDateTime &dateTime);

private:
	QLineEdit *nameEdit;
	QComboBox *channelBox;
	QDateTimeEdit *beginEdit;
	QTimeEdit *durationEdit;
	QDateTimeEdit *endEdit;
};

#endif /* DVBRECORDING_H */
