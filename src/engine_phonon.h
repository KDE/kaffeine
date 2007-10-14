/*
 * engine_phonon.h
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

#ifndef ENGINE_PHONON_H
#define ENGINE_PHONON_H

#include <Phonon/AbstractMediaStream>

#include "engine.h"

namespace Phonon
{
class AudioOutput;
class MediaObject;
class VideoWidget;
};

class DvbLiveFeedPhonon : public Phonon::AbstractMediaStream
{
	Q_OBJECT
public:
	explicit DvbLiveFeedPhonon(QObject *parent) : Phonon::AbstractMediaStream(parent) 
	{
		setStreamSize(-1);
	}

	~DvbLiveFeedPhonon() { }

	void reset() { }

	void needData() { }

public slots:
	void writeData(const QByteArray &data)
	{
		Phonon::AbstractMediaStream::writeData(data);
	}
};

class EnginePhonon : public Engine
{
	Q_OBJECT
public:
	explicit EnginePhonon(QObject *parent);
	~EnginePhonon();

	QWidget *videoWidget();
	QWidget *positionSlider();
	QWidget *volumeSlider();

	/*
	 * loads the media and starts playback
	 */

	void play(const KUrl &url);
	void playAudioCd();
	void playVideoCd();
	void playDvd();

	/*
	 * ownership isn't taken; you have to delete it
	 * manually after DvbLiveFeed::stop() has been called
	 */

	void playDvb(DvbLiveFeed *feed);

	/*
	 * pauses / continues playback
	 */

	void setPaused(bool paused);

	/*
	 * stops playback
	 */

	void stop();

	/*
	 * sets volume (0 - 100)
	 */

	void setVolume(int volume);

	/*
	 * gets position (0 - 65535)
	 */

	int getPosition();

	/*
	 * sets position (0 - 65535)
	 */

	void setPosition(int position);

private slots:
	void stateChanged(Phonon::State state);

private:
	Phonon::MediaObject *mediaObject;
	Phonon::AudioOutput *audioOutput;

	DvbLiveFeed *liveFeed;
	DvbLiveFeedPhonon *liveFeedPhonon;
};

#endif /* ENGINE_PHONON_H */
