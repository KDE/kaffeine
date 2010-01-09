/*
 * xineapplication.h
 *
 * Copyright (C) 2010 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef XINEAPPLICATION_H
#define XINEAPPLICATION_H

#include <QEvent>
#include <QMutex>
#include <QTimer>
#include <xine.h>

class XineParent;
class XinePipeReader;

class XineObject : public QObject
{
	Q_OBJECT
public:
	XineObject();
	~XineObject();

	static const char *version();

private slots:
	void readyRead();
	void updatePosition();

private:
	enum DirtyFlag {
		Quit		= (1 <<  0),
		NotReady	= (1 <<  1),
		SetMuted	= (1 <<  2),
		SetVolume	= (1 <<  3),
		SetAspectRatio	= (1 <<  4),
		OpenStream	= (1 <<  5),
		PlayStream	= (1 <<  6),
		SetPaused	= (1 <<  7),
		Seek		= (1 <<  8),
		Repaint		= (1 <<  9),
		MouseMoved	= (1 << 10),
		MousePressed	= (1 << 11),
		ToggleMenu	= (1 << 12),
		ProcessEvent	= (1 << 30)
	};

	Q_DECLARE_FLAGS(DirtyFlags, DirtyFlag)

	enum XineDirtyFlag {
		PlaybackFailed		= (1 <<  0),
		PlaybackFinished	= (1 <<  1),
		CloseStream		= (1 <<  2),
		UpdateStreamInfo	= (1 <<  3),
		UpdateMouseTracking	= (1 <<  4),
		UpdateMouseCursor	= (1 <<  5),
		ProcessXineEvent	= (1 << 30)
	};

	Q_DECLARE_FLAGS(XineDirtyFlags, XineDirtyFlag)

	static const QEvent::Type ReaderEvent = static_cast<QEvent::Type>(QEvent::User + 1);
	static const QEvent::Type XineEvent = static_cast<QEvent::Type>(QEvent::User + 2);

	void init(quint64 windowId);
	void customEvent(QEvent *event);
	QString xineErrorString(int errorCode) const;

	void postXineEvent();
	static void dest_size_cb(void *user_data, int video_width, int video_height,
		double video_pixel_aspect, int *dest_width, int *dest_height,
		double *dest_pixel_aspect);
	static void frame_output_cb(void *user_data, int video_width, int video_height,
		double video_pixel_aspect, int *dest_x, int *dest_y, int *dest_width,
		int *dest_height, double *dest_pixel_aspect, int *win_x, int *win_y);
	static void event_listener_cb(void *user_data, const xine_event_t *event);

	XinePipeReader *reader;
	XineParent *parentProcess;

	xine_t *engine;
	xine_audio_port_t *audioOutput;
	xine_video_port_t *videoOutput;
	xine_stream_t *stream;
	xine_event_queue_t *eventQueue;
	double pixelAspectRatio;
	unsigned int widgetSize;
	QTimer positionTimer;

	DirtyFlags dirtyFlags;
	bool muted;
	bool paused;
	int aspectRatio;
	int seekTime;
	int volume;
	unsigned int mouseMovePosition;
	unsigned int mousePressPosition;
	unsigned int sequenceNumber;
	QByteArray encodedUrl;

	QMutex mutex;
	XineDirtyFlags xineDirtyFlags;
	bool mouseTrackingEnabled;
	bool pointingMouseCursor;
	QString errorMessage;
};

#endif /* XINEAPPLICATION_H */
