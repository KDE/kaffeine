/*
 * dvbrecording_p.h
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

#ifndef DVBRECORDING_P_H
#define DVBRECORDING_P_H

#include <QFile>
#include <KDialog>
#include "../sqltablemodel.h"
#include "dvbrecording.h"
#include "dvbsi.h"
#include "dvbsifilter.h"

class QCheckBox;
class QTreeView;
class KComboBox;
class KLineEdit;
class DateTimeEdit;
class DurationEdit;
class DvbChannel;

Q_DECLARE_METATYPE(const DvbRecordingEntry *)

class DvbRecording : public QObject, public DvbPidFilter
{
	Q_OBJECT
public:
	DvbRecording(DvbManager *manager_, const DvbRecordingEntry &entry_);
	~DvbRecording();

	const DvbRecordingEntry &getEntry() const;
	void setEntry(const DvbRecordingEntry &entry_);

	void start();
	void stop();

private slots:
	void deviceStateChanged();
	void pmtSectionChanged(const DvbPmtSection &pmtSection);
	void insertPatPmt();

private:
	void processData(const char data[188]);

	DvbManager *manager;
	DvbRecordingEntry entry;
	QExplicitlySharedDataPointer<const DvbChannel> channel;
	QFile file;
	QList<QByteArray> buffers;
	DvbDevice *device;
	QList<int> pids;
	DvbPmtFilter pmtFilter;
	DvbPmtSection pmtSection;
	DvbSectionGenerator patGenerator;
	DvbSectionGenerator pmtGenerator;
	QTimer patPmtTimer;
	bool pmtValid;
};

class DvbRecordingSqlInterface : public SqlTableModelInterface
{
public:
	DvbRecordingSqlInterface(DvbManager *manager_, QAbstractItemModel *model,
		QList<DvbRecording *> *recordings_, QObject *parent);
	~DvbRecordingSqlInterface();

private:
	int insertFromSqlQuery(const QSqlQuery &query, int index);
	void bindToSqlQuery(QSqlQuery &query, int index, int row) const;

	DvbManager *manager;
	QList<DvbRecording *> *recordings;
};

class DvbRecordingDialog : public KDialog
{
	Q_OBJECT
public:
	DvbRecordingDialog(DvbManager *manager_, DvbRecordingModel *recordingModel,
		QWidget *parent);
	~DvbRecordingDialog();

private slots:
	void newRecording();
	void editRecording();
	void removeRecording();

private:
	DvbManager *manager;
	QAbstractItemModel *proxyModel;
	QTreeView *treeView;
};

class DvbRecordingEditor : public KDialog
{
	Q_OBJECT
public:
	DvbRecordingEditor(DvbManager *manager, QAbstractItemModel *model_,
		const QModelIndex &modelIndex_, QWidget *parent);
	~DvbRecordingEditor();

private slots:
	void beginChanged(const QDateTime &begin);
	void durationChanged(const QTime &duration);
	void endChanged(const QDateTime &end);
	void repeatNever();
	void repeatDaily();
	void checkValidity();

private:
	void accept();

	QAbstractItemModel *model;
	QPersistentModelIndex persistentIndex;
	KLineEdit *nameEdit;
	KComboBox *channelBox;
	DateTimeEdit *beginEdit;
	DurationEdit *durationEdit;
	DateTimeEdit *endEdit;
	QCheckBox *dayCheckBoxes[7];
};

#endif /* DVBRECORDING_P_H */
