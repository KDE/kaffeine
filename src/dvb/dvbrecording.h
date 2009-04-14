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

class QDateTimeEdit;
class QTimeEdit;
class KComboBox;
class KLineEdit;
class DvbChannelModel;
class DvbManager;
class DvbRecording;
class DvbRecordingEditor;
class ProxyTreeView;

class DvbRecordingModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit DvbRecordingModel(DvbManager *manager_);
	~DvbRecordingModel();

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	const DvbRecording *getRecording(int row);
	void appendRecording(DvbRecording *recording);
	void removeRecording(int row);
	void updateRecording(int row, DvbRecordingEditor *editor);

	void startInstantRecording(const QString &name, const QString &channel);
	void stopInstantRecording(); // stops the last started instant recording

signals:
	void instantRecordingRemoved(); // not emitted by stopInstantRecording()

private slots:
	void checkStatus();

private:
	DvbManager *manager;
	QList<DvbRecording *> recordings;
	int instantRecordingRow;
	QTimer *checkStatusTimer;
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
	DvbRecordingEditor(const DvbRecording *recording, DvbChannelModel *channelModel,
		QWidget *parent);
	~DvbRecordingEditor();

	void updateRecording(DvbRecording *recording) const;

public slots:
	void beginChanged(const QDateTime &dateTime);
	void durationChanged(const QTime &time);
	void endChanged(const QDateTime &dateTime);
	void checkValid();

private:
	KLineEdit *nameEdit;
	KComboBox *channelBox;
	QDateTimeEdit *beginEdit;
	QTimeEdit *durationEdit;
	QDateTimeEdit *endEdit;
};

#endif /* DVBRECORDING_H */
