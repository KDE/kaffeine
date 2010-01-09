/*
 * xinecommands.h
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

#ifndef XINECOMMANDS_H
#define XINECOMMANDS_H

#include <QSocketNotifier>

namespace XineCommands
{
	enum CommandToChild {
		Quit		=  0,
		Init		=  1,
		Resize		=  2,
		SetMuted	=  3,
		SetVolume	=  4,
		SetAspectRatio	=  5,
		PlayUrl		=  6,
		SetPaused	=  7,
		Seek		=  8,
		Repaint		=  9,
		MouseMoved	= 10,
		MousePressed	= 11
	};

	enum CommandFromChild {
		InitFailed		=  0,
		Sync			=  1,
		PlaybackFailed		=  2,
		PlaybackFinished	=  3,
		UpdateSeekable		=  4,
		UpdateCurrentTotalTime	=  5,
		UpdateAudioChannels	=  6,
		UpdateSubtitles		=  7,
		UpdateTitles		=  8,
		UpdateChapters		=  9,
		UpdateAngles		= 10,
		UpdateMouseTracking	= 11,
		UpdateMouseCursor	= 12
	};
};

enum XineAspectRatio {
	XineAspectRatioAuto	= 0,
	XineAspectRatio4_3	= 1,
	XineAspectRatio16_9	= 2,
	XineAspectRatioWidget	= 3
};

class XinePipeReader
{
public:
	XinePipeReader(int fd_, QObject *parent);
	~XinePipeReader();

	void readyRead();
	int nextCommand();
	bool isValid() const;

	bool readBool();
	qint8 readChar();
	qint16 readShort();
	qint32 readInt();
	qint64 readLongLong();
	QByteArray readByteArray();
	QString readString();

private:
	int fd;
	QSocketNotifier notifier;
	QByteArray buffer;
	int bufferPosition;
	int bufferSize;
	const char *currentData;
	int currentSize;
};

class XinePipeWriterBase
{
	friend class XineChildMarshaller;
	friend class XineParentMarshaller;
public:
	XinePipeWriterBase() { }
	virtual ~XinePipeWriterBase() { }

private:
	virtual void write(qint8 command,
		const char *firstArg = NULL, unsigned int firstArgSize = 0,
		const char *secondArg = NULL, unsigned int secondArgSize = 0);
};

class XinePipeWriter : public QObject, public XinePipeWriterBase
{
	Q_OBJECT
public:
	XinePipeWriter(int fd_, QObject *parent);
	~XinePipeWriter();

private slots:
	void readyWrite();

private:
	void write(qint8 command, const char *firstArg, unsigned int firstArgSize,
		const char *secondArg, unsigned int secondArgSize);
	bool flush();

	int fd;
	QSocketNotifier notifier;
	QByteArray buffer;
	int bufferPosition;
};

class XineChildMarshaller
{
public:
	XineChildMarshaller() : writer(NULL) { }
	~XineChildMarshaller() { }

	void quit()
	{
		writer->write(XineCommands::Quit);
	}

	void init(quint64 windowId)
	{
		writer->write(XineCommands::Init, reinterpret_cast<const char *>(&windowId),
			sizeof(windowId));
	}

	void resize(quint16 width, quint16 height)
	{
		writer->write(XineCommands::Resize, reinterpret_cast<const char *>(&width),
			sizeof(width), reinterpret_cast<const char *>(&height), sizeof(height));
	}

	void setMuted(bool muted)
	{
		writer->write(XineCommands::SetMuted, reinterpret_cast<const char *>(&muted),
			sizeof(muted));
	}

	void setVolume(quint8 volume)
	{
		writer->write(XineCommands::SetVolume, reinterpret_cast<const char *>(&volume),
			sizeof(volume));
	}

	void setAspectRatio(qint8 aspectRatio)
	{
		writer->write(XineCommands::SetAspectRatio,
			reinterpret_cast<const char *>(&aspectRatio), sizeof(aspectRatio));
	}

	void playUrl(quint32 sequenceNumber, const QByteArray &encodedUrl)
	{
		writer->write(XineCommands::PlayUrl,
			reinterpret_cast<const char *>(&sequenceNumber), sizeof(sequenceNumber),
			encodedUrl.constData(), encodedUrl.size());
	}

	void setPaused(bool paused)
	{
		writer->write(XineCommands::SetPaused, reinterpret_cast<const char *>(&paused),
			sizeof(paused));
	}

	void seek(qint32 time)
	{
		writer->write(XineCommands::Seek, reinterpret_cast<const char *>(&time),
			sizeof(time));
	}

	void repaint()
	{
		writer->write(XineCommands::Repaint);
	}

	void mouseMoved(quint16 x, quint16 y)
	{
		writer->write(XineCommands::MouseMoved, reinterpret_cast<const char *>(&x),
			sizeof(x), reinterpret_cast<const char *>(&y), sizeof(y));
	}

	void mousePressed(quint16 x, quint16 y)
	{
		writer->write(XineCommands::MousePressed, reinterpret_cast<const char *>(&x),
			sizeof(x), reinterpret_cast<const char *>(&y), sizeof(y));
	}

	XinePipeWriterBase *writer;
};

class XineParentMarshaller
{
public:
	XineParentMarshaller() : writer(NULL) { }
	~XineParentMarshaller() { }

	void initFailed(const QString &errorMessage)
	{
		writer->write(XineCommands::InitFailed,
			reinterpret_cast<const char *>(errorMessage.constData()),
			static_cast<unsigned int>(errorMessage.size()) * sizeof(QChar));
	}

	void sync(quint32 sequenceNumber)
	{
		writer->write(XineCommands::Sync, reinterpret_cast<const char *>(&sequenceNumber),
			sizeof(sequenceNumber));
	}

	void playbackFailed(const QString &errorMessage)
	{
		writer->write(XineCommands::PlaybackFailed,
			reinterpret_cast<const char *>(errorMessage.constData()),
			static_cast<unsigned int>(errorMessage.size()) * sizeof(QChar));
	}

	void playbackFinished()
	{
		writer->write(XineCommands::PlaybackFinished);
	}

	void updateSeekable(bool seekable)
	{
		writer->write(XineCommands::UpdateSeekable,
			reinterpret_cast<const char *>(&seekable), sizeof(seekable));
	}

	void updateCurrentTotalTime(qint32 currentTime, qint32 totalTime)
	{
		writer->write(XineCommands::UpdateCurrentTotalTime,
			reinterpret_cast<const char *>(&currentTime), sizeof(currentTime),
			reinterpret_cast<const char *>(&totalTime), sizeof(totalTime));
	}

	void updateAudioChannels(const QByteArray &audioChannels, qint8 currentAudioChannel)
	{
		writer->write(XineCommands::UpdateAudioChannels,
			reinterpret_cast<const char *>(&currentAudioChannel),
			sizeof(currentAudioChannel),
			audioChannels.constData(), audioChannels.size());
	}

	void updateSubtitles(const QByteArray &subtitles, qint8 currentSubtitle)
	{
		writer->write(XineCommands::UpdateSubtitles,
			reinterpret_cast<const char *>(&currentSubtitle), sizeof(currentSubtitle),
			subtitles.constData(), subtitles.size());
	}

	void updateTitles(qint8 titleCount, qint8 currentTitle)
	{
		writer->write(XineCommands::UpdateTitles,
			reinterpret_cast<const char *>(&titleCount), sizeof(titleCount),
			reinterpret_cast<const char *>(&currentTitle), sizeof(currentTitle));
	}

	void updateChapters(qint8 chapterCount, qint8 currentChapter)
	{
		writer->write(XineCommands::UpdateChapters,
			reinterpret_cast<const char *>(&chapterCount), sizeof(chapterCount),
			reinterpret_cast<const char *>(&currentChapter), sizeof(currentChapter));
	}

	void updateAngles(qint8 angleCount, qint8 currentAngle)
	{
		writer->write(XineCommands::UpdateAngles,
			reinterpret_cast<const char *>(&angleCount), sizeof(angleCount),
			reinterpret_cast<const char *>(&currentAngle), sizeof(currentAngle));
	}

	void updateMouseTracking(bool mouseTrackingEnabled)
	{
		writer->write(XineCommands::UpdateMouseTracking,
			reinterpret_cast<const char *>(&mouseTrackingEnabled),
			sizeof(mouseTrackingEnabled));
	}

	void updateMouseCursor(bool pointingMouseCursor)
	{
		writer->write(XineCommands::UpdateMouseCursor,
			reinterpret_cast<const char *>(&pointingMouseCursor),
			sizeof(pointingMouseCursor));
	}

	XinePipeWriterBase *writer;
};

#endif /* XINECOMMANDS_H */
