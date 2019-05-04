/*
 * mediawidget_p.h
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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
#include <QDialog>
#include "mediawidget.h"

class QTimeEdit;

class JumpToPositionDialog : public QDialog
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
	void wheelEvent(QWheelEvent *event);
};

class MediaSourceUrl : public MediaSource
{
public:
	MediaSourceUrl(const QUrl &url_, const QUrl &subtitleUrl_) : url(url_),
		subtitleUrl(subtitleUrl_) { }
	~MediaSourceUrl() { }

	Type getType() const { return Url; }
	QUrl getUrl() const { return url; }

	QUrl url;
	QUrl subtitleUrl;
};

class MediaSourceAudioCd : public MediaSource
{
public:
	explicit MediaSourceAudioCd(const QUrl &url_) : url(url_) { }
	~MediaSourceAudioCd() { }

	Type getType() const { return AudioCd; }
	QUrl getUrl() const { return url; }

	QUrl url;
};

class MediaSourceVideoCd : public MediaSource
{
public:
	explicit MediaSourceVideoCd(const QUrl &url_) : url(url_) { }
	~MediaSourceVideoCd() { }

	Type getType() const { return VideoCd; }
	QUrl getUrl() const { return url; }

	QUrl url;
};

class MediaSourceDvd : public MediaSource
{
public:
	explicit MediaSourceDvd(const QUrl &url_) : url(url_) { }
	~MediaSourceDvd() { }

	Type getType() const { return Dvd; }
	QUrl getUrl() const { return url; }

	QUrl url;
};

#endif /* MEDIAWIDGET_P_H */
