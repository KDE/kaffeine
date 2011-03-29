/*
 * dvbrecordingdialog_p.h
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

#ifndef DVBRECORDINGDIALOG_P_H
#define DVBRECORDINGDIALOG_P_H

#include <KDialog>
#include "../tablemodel.h"
#include "dvbrecording.h"

class QCheckBox;
class KComboBox;
class KLineEdit;
class DateTimeEdit;
class DurationEdit;

class DvbRecordingLessThan
{
public:
	DvbRecordingLessThan()
	{
		// specify each column exactly once
		sortOrder[0] = BeginAscending;
		sortOrder[1] = ChannelAscending;
		sortOrder[2] = NameAscending;
		sortOrder[3] = DurationAscending;
	}

	~DvbRecordingLessThan() { }

	enum SortOrder
	{
		NameAscending = 0,
		NameDescending = 1,
		ChannelAscending = 2,
		ChannelDescending = 3,
		BeginAscending = 4,
		BeginDescending = 5,
		DurationAscending = 6,
		DurationDescending = 7
	};

	SortOrder getSortOrder() const
	{
		return sortOrder[0];
	}

	void setSortOrder(SortOrder sortOrder_);

	bool operator()(const DvbSharedRecording &x, const DvbSharedRecording &y) const;

private:
	SortOrder sortOrder[4];
};

class DvbRecordingTableModelHelper
{
public:
	DvbRecordingTableModelHelper() { }
	~DvbRecordingTableModelHelper() { }

	typedef DvbSharedRecording ItemType;
	typedef DvbRecordingLessThan LessThanType;

	int columnCount() const
	{
		return 4;
	}

	bool filterAcceptsItem(const DvbSharedRecording &recording) const
	{
		Q_UNUSED(recording)
		return true;
	}
};

class DvbRecordingTableModel : public TableModel<DvbRecordingTableModelHelper>
{
	Q_OBJECT
public:
	explicit DvbRecordingTableModel(QObject *parent);
	~DvbRecordingTableModel();

	void setRecordingModel(DvbRecordingModel *recordingModel_);

	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	void sort(int column, Qt::SortOrder order);

private slots:
	void recordingAdded(const DvbSharedRecording &recording);
	void recordingAboutToBeUpdated(const DvbSharedRecording &recording);
	void recordingUpdated(const DvbSharedRecording &recording);
	void recordingRemoved(const DvbSharedRecording &recording);

private:
	DvbRecordingModel *recordingModel;
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
