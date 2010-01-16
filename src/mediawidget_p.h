/*
 * mediawidget_p.h
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef MEDIAWIDGET_P_H
#define MEDIAWIDGET_P_H

#include <QSlider>
#include <KDialog>

class QSocketNotifier;
class QTimeEdit;
class MediaWidget;

class DvbFeed : public QObject
{
	Q_OBJECT
public:
	explicit DvbFeed(QObject *parent);
	~DvbFeed();

	KUrl getUrl() const;
	void writeData(const QByteArray &data);
	void endOfData();

	bool timeShiftActive;
	bool ignoreSourceChange;
	QStringList audioChannels;
	QStringList subtitles;

private slots:
	void readyWrite();

private:
	bool flush();

	KUrl url;
	int readFd;
	int writeFd;
	QSocketNotifier *notifier;
	QList<QByteArray> buffers;
};

class JumpToPositionDialog : public KDialog
{
public:
	explicit JumpToPositionDialog(MediaWidget *mediaWidget_);
	~JumpToPositionDialog();

private:
	void accept();

	MediaWidget *mediaWidget;
	QTimeEdit *timeEdit;
};

class SeekSlider : public QSlider
{
public:
	explicit SeekSlider(QWidget *parent) : QSlider(parent) { }
	~SeekSlider() { }

private:
	void mousePressEvent(QMouseEvent *event);
};

#endif /* MEDIAWIDGET_P_H */
