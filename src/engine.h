/*
 * engine.h
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef ENGINE_H
#define ENGINE_H

#include <QWidget>
#include <KUrl>

class DvbLiveFeed : public QObject
{
	friend class EnginePhonon;
	Q_OBJECT
public:
	DvbLiveFeed() { }
	virtual ~DvbLiveFeed() { }

signals:
	void livePaused(bool paused);
	void liveStopped();
	void writeData(const QByteArray &data);
};

class Engine : public QObject
{
	Q_OBJECT
public:
	explicit Engine(QObject *parent) : QObject(parent) { }
	virtual ~Engine() { }

	virtual QWidget *videoWidget() = 0;
	virtual QWidget *positionSlider() = 0;
	virtual QWidget *volumeSlider() = 0;

	/*
	 * loads the media and starts playback
	 */

	virtual void play(const KUrl &url) = 0;
	virtual void playAudioCd() = 0;
	virtual void playVideoCd() = 0;
	virtual void playDvd() = 0;

	/*
	 * ownership isn't taken; you have to delete it
	 * manually after DvbLiveFeed::stop() has been called
	 */

	virtual void playDvb(DvbLiveFeed *feed) = 0;

	/*
	 * pauses / continues playback
	 */

	virtual void setPaused(bool paused) = 0;

	/*
	 * stops playback
	 */

	virtual void stop() = 0;

	/*
	 * sets volume (0 - 100)
	 */

	virtual void setVolume(int volume) = 0;

	/*
	 * gets position (0 - 65535)
	 */

	virtual int getPosition() = 0;

	/*
	 * sets position (0 - 65535)
	 */

	virtual void setPosition(int position) = 0;

signals:
	/*
	 * playback finished successfully
	 */

	void playbackFinished();

	/*
	 * playback failed
	 */

	void playbackFailed(const QString &errorMessage);
};

#endif /* ENGINE_H */
