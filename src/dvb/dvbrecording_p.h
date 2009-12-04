/*
 * dvbrecording_p.h
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

#ifndef DVBRECORDING_P_H
#define DVBRECORDING_P_H

#include <QFile>
#include <KDialog>
#include "../tablemodel.h"
#include "dvbsi.h"

class QCheckBox;
class KComboBox;
class KLineEdit;
class DateTimeEdit;
class DurationEdit;
class DvbManager;
class DvbRecordingModel;

class DvbRecording : public QObject, public QSharedData, private DvbPidFilter
{
	Q_OBJECT
public:
	explicit DvbRecording(DvbManager *manager_);
	~DvbRecording();

	bool isRunning() const;
	void start();
	void stop();

	QString name;
	QString channelName;
	QDateTime begin; // local time
	QTime duration;
	QDateTime end; // local time
	int repeat; // 1 (monday) | 2 (tuesday) | 4 (wednesday) | etc

private slots:
	void deviceStateChanged();
	void pmtSectionChanged(const DvbPmtSection &pmtSection);
	void insertPatPmt();

private:
	void processData(const char data[188]);

	DvbManager *manager;
	QExplicitlySharedDataPointer<const DvbChannel> channel;
	QFile file;
	QList<QByteArray> buffers;
	DvbDevice *device;
	QList<int> pids;
	DvbPmtFilter pmtFilter;
	DvbSectionGenerator patGenerator;
	DvbSectionGenerator pmtGenerator;
	QTimer patPmtTimer;
	bool pmtValid;
};

Q_DECLARE_TYPEINFO(QExplicitlySharedDataPointer<DvbRecording>, Q_MOVABLE_TYPE);

class DvbRecordingEditor : public KDialog
{
	Q_OBJECT
public:
	DvbRecordingEditor(DvbManager *manager_, DvbRecordingModel *model_, int row_,
		QWidget *parent);
	~DvbRecordingEditor();

private slots:
	void beginChanged(const QDateTime &begin);
	void durationChanged(const QTime &duration);
	void endChanged(const QDateTime &end);
	void repeatNever();
	void repeatDaily();
	void checkValid();

private:
	void accept();

	DvbManager *manager;
	DvbRecordingModel *model;
	int row;
	KLineEdit *nameEdit;
	KComboBox *channelBox;
	DateTimeEdit *beginEdit;
	DurationEdit *durationEdit;
	DateTimeEdit *endEdit;
	QCheckBox *dayCheckBoxes[7];
};

class DvbRecordingHelper
{
public:
	typedef QExplicitlySharedDataPointer<DvbRecording> StorageType;
	typedef DvbRecording Type;

	static QStringList modelHeaderLabels();
	static QVariant modelData(const DvbRecording *recording, int column, int role);
};

class DvbRecordingModel : public TableModel<DvbRecordingHelper>, private SqlTableModelBase
{
public:
	DvbRecordingModel(DvbManager *manager_, QObject *parent);
	~DvbRecordingModel();

private:
	int insertFromSqlQuery(const QSqlQuery &query, int index);
	void bindToSqlQuery(int row, QSqlQuery &query, int index) const;

	DvbManager *manager;
	SqlModelAdaptor *sqlAdaptor;
};

#endif /* DVBRECORDING_P_H */
