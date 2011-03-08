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

#ifndef DVBRECORDINGDIALOG_P_H
#define DVBRECORDINGDIALOG_P_H

#include <QFile>
#include <QTimer>
#include <KDialog>
#include "../sqltablemodel.h"
#include "dvbchannel.h"
#include "dvbrecording.h"
#include "dvbsi.h"

class QCheckBox;
class KComboBox;
class KLineEdit;
class DateTimeEdit;
class DurationEdit;
class DvbDevice;

class DvbRecordingLessThan
{
public:
	DvbRecordingLessThan() { }
	~DvbRecordingLessThan() { }

	bool operator()(const DvbRecording &x, const DvbRecording &y) const
	{
		if (x.begin != y.begin) {
			return (x.begin < y.begin);
		}

		if (x.channel->name != y.channel->name) {
			return (x.channel->name.localeAwareCompare(x.channel->name) < 0);
		}

		if (x.name != y.name) {
			return (x.name < x.name);
		}

		if (x.duration != y.duration) {
			return (x.duration < y.duration);
		}

		return (x < y);
	}

	bool operator()(const DvbRecording &x, const DvbSharedRecording &y) const
	{
		return (*this)(x, *y);
	}

	bool operator()(const DvbSharedRecording &x, const DvbRecording &y) const
	{
		return (*this)(*x, y);
	}

	bool operator()(const DvbSharedRecording &x, const DvbSharedRecording &y) const
	{
		return (*this)(*x, *y);
	}
};

class DvbRecordingTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DvbRecordingTableModel(DvbManager *manager_, QObject *parent);
	~DvbRecordingTableModel();

	DvbSharedRecording getRecording(const QModelIndex &index) const;

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private slots:
	void recordingAdded(const DvbSharedRecording &recording);
	void recordingUpdated(const DvbSharedRecording &recording,
		const DvbRecording &oldRecording);
	void recordingRemoved(const DvbSharedRecording &recording);

private:
	DvbManager *manager;
	QList<DvbSharedRecording> recordings;
};

class DvbRecordingEditor : public KDialog
{
	Q_OBJECT
public:
	DvbRecordingEditor(DvbManager *manager_, const DvbSharedRecording &recording_,
		QWidget *parent);
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

	DvbManager *manager;
	DvbSharedRecording recording;
	KLineEdit *nameEdit;
	KComboBox *channelBox;
	DateTimeEdit *beginEdit;
	DurationEdit *durationEdit;
	DateTimeEdit *endEdit;
	QCheckBox *dayCheckBoxes[7];
};

#endif /* DVBRECORDINGDIALOG_P_H */
