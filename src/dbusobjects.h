/*
 * dbusobjects.h
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

#ifndef DBUSOBJECTS_H
#define DBUSOBJECTS_H

#include <QVariantMap>

class MediaWidget;
class PlaylistTab;

struct MprisStatusStruct;
struct MprisVersionStruct;

class MprisRootObject : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.MediaPlayer")
public:
	explicit MprisRootObject(QObject *parent);
	~MprisRootObject();

public slots:
	QString Identity();
	void Quit();
	MprisVersionStruct MprisVersion();
};

class MprisPlayerObject : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.MediaPlayer")
public:
	MprisPlayerObject(MediaWidget *mediaWidget_, PlaylistTab *playlistTab_);
	~MprisPlayerObject();

public slots:
	void Next();
	void Prev();
	void Pause();
	void Stop();
	void Play();
	void Repeat(bool repeat);
	MprisStatusStruct GetStatus();
	QVariantMap GetMetadata();
	int GetCaps();
	void VolumeSet(int volume);
	int VolumeGet();
	void PositionSet(int position);
	int PositionGet();

signals:
	void TrackChange(const QVariantMap &metadata); // FIXME not emitted yet
	void StatusChange(const MprisStatusStruct &status); // FIXME not emitted yet
	void CapsChange(int capabilities); // FIXME not emitted yet

private:
	MediaWidget *mediaWidget;
	PlaylistTab *playlistTab;
};

struct MprisStatusStruct
{
	int state;
	int random;
	int repeatTrack;
	int repeatPlaylist;
};

Q_DECLARE_METATYPE(MprisStatusStruct)

struct MprisVersionStruct
{
	quint16 major;
	quint16 minor;
};

Q_DECLARE_METATYPE(MprisVersionStruct)

#endif /* DBUSOBJECTS_H */
