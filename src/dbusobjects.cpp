/*
 * dbusobjects.cpp
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

#include "dbusobjects.h"

#include <QDBusMetaType>
#include <KAboutData>
#include <KApplication>
#include "playlist/playlisttab.h"
#include "mediawidget.h"

QDBusArgument &operator<<(QDBusArgument &argument, const MprisStatusStruct &statusStruct)
{
	argument.beginStructure();
	argument << statusStruct.state << statusStruct.random << statusStruct.repeatTrack <<
		statusStruct.repeatPlaylist;
	argument.endStructure();
	return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MprisStatusStruct &statusStruct)
{
	argument.beginStructure();
	argument >> statusStruct.state >> statusStruct.random >> statusStruct.repeatTrack >>
		statusStruct.repeatPlaylist;
	argument.endStructure();
	return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const MprisVersionStruct &versionStruct)
{
	argument.beginStructure();
	argument << versionStruct.major << versionStruct.minor;
	argument.endStructure();
	return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MprisVersionStruct &versionStruct)
{
	argument.beginStructure();
	argument >> versionStruct.major >> versionStruct.minor;
	argument.endStructure();
	return argument;
}

MprisRootObject::MprisRootObject(QObject *parent) : QObject(parent)
{
	 qDBusRegisterMetaType<MprisVersionStruct>();
}

MprisRootObject::~MprisRootObject()
{
}

QString MprisRootObject::Identity()
{
	const KAboutData *aboutData = KGlobal::mainComponent().aboutData();
	return aboutData->programName() + ' ' + aboutData->version();
}

void MprisRootObject::Quit()
{
	kapp->quit();
}

MprisVersionStruct MprisRootObject::MprisVersion()
{
	MprisVersionStruct versionStruct;
	versionStruct.major = 1;
	versionStruct.minor = 0;
	return versionStruct;
}

MprisPlayerObject::MprisPlayerObject(MediaWidget *mediaWidget_, PlaylistTab *playlistTab_) :
	mediaWidget(mediaWidget_), playlistTab(playlistTab_)
{
	qDBusRegisterMetaType<MprisStatusStruct>();
}

MprisPlayerObject::~MprisPlayerObject()
{
}

void MprisPlayerObject::Next()
{
	mediaWidget->next();
}

void MprisPlayerObject::Prev()
{
	mediaWidget->previous();
}

void MprisPlayerObject::Pause()
{
	mediaWidget->togglePause();
}

void MprisPlayerObject::Stop()
{
	mediaWidget->stop();
}

void MprisPlayerObject::Play()
{
	mediaWidget->play();
}

void MprisPlayerObject::Repeat(bool repeat)
{
	Q_UNUSED(repeat);
	// FIXME track repeat not implemented yet
}

MprisStatusStruct MprisPlayerObject::GetStatus()
{
	MprisStatusStruct statusStruct;

	if (mediaWidget->isPaused()) {
		statusStruct.state = 1;
	} else if (mediaWidget->isPlaying()) {
		statusStruct.state = 0;
	} else {
		statusStruct.state = 2;
	}

	if (playlistTab->getRandom()) {
		statusStruct.random = 1;
	} else {
		statusStruct.random = 0;
	}

	statusStruct.repeatTrack = 0; // FIXME track repeat not implemented yet

	if (playlistTab->getRepeat()) {
		statusStruct.repeatPlaylist = 1;
	} else {
		statusStruct.repeatPlaylist = 0;
	}

	return statusStruct;
}

QVariantMap MprisPlayerObject::GetMetadata()
{
	// FIXME metadata handling not implemented yet
	return QVariantMap();
}

int MprisPlayerObject::GetCaps()
{
	int capabilities = (1 << 0) | // CAN_GO_NEXT // FIXME check availability
			   (1 << 1) | // CAN_GO_PREV // FIXME check availability
			   (1 << 3) | // CAN_PLAY // FIXME check availability
			   (1 << 4) | // CAN_SEEK // FIXME check availability
			   (1 << 6);  // CAN_HAS_TRACKLIST

	if (mediaWidget->isPlaying()) {
		capabilities |= (1 << 2); // CAN_PAUSE
	}

	return capabilities; // FIXME metadata handling not implemented yet
}

void MprisPlayerObject::VolumeSet(int volume)
{
	mediaWidget->setVolume(volume);
}

int MprisPlayerObject::VolumeGet()
{
	return mediaWidget->getVolume();
}

void MprisPlayerObject::PositionSet(int position)
{
	mediaWidget->setPosition(position);
}

int MprisPlayerObject::PositionGet()
{
	return mediaWidget->getPosition();
}
