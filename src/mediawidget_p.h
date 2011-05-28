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
#include <KDialog>

class QTimeEdit;
class MediaWidget;

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

class MediaSourceUrl : public MediaSource
{
public:
	MediaSourceUrl(const KUrl &url_, const KUrl &subtitleUrl_) : url(url_),
		subtitleUrl(subtitleUrl_) { }
	~MediaSourceUrl() { }

	Type getType() const { return Url; }
	KUrl getUrl() const { return url; }

	KUrl url;
	KUrl subtitleUrl;
};

class MediaSourceAudioCd : public MediaSource
{
public:
	explicit MediaSourceAudioCd(const KUrl &url_) : url(url_) { }
	~MediaSourceAudioCd() { }

	Type getType() const { return AudioCd; }
	KUrl getUrl() const { return url; }

	KUrl url;
};

class MediaSourceVideoCd : public MediaSource
{
public:
	explicit MediaSourceVideoCd(const KUrl &url_) : url(url_) { }
	~MediaSourceVideoCd() { }

	Type getType() const { return VideoCd; }
	KUrl getUrl() const { return url; }

	KUrl url;
};

class MediaSourceDvd : public MediaSource
{
public:
	explicit MediaSourceDvd(const KUrl &url_) : url(url_) { }
	~MediaSourceDvd() { }

	Type getType() const { return Dvd; }
	KUrl getUrl() const { return url; }

	KUrl url;
};

#endif /* MEDIAWIDGET_P_H */
