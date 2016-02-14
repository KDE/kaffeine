/*
 * dbusobjects.cpp
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

#include "dbusobjects.h"

#include <QDBusMetaType>
#include <K4AboutData>
#include <KApplication>
#include "dvb/dvbmanager.h"
#include "dvb/dvbtab.h"
#include "playlist/playlisttab.h"

static QDBusArgument &operator<<(QDBusArgument &argument, const MprisStatusStruct &statusStruct)
{
	argument.beginStructure();
	argument << statusStruct.state << statusStruct.random << statusStruct.repeatTrack <<
		statusStruct.repeatPlaylist;
	argument.endStructure();
	return argument;
}

static const QDBusArgument &operator>>(const QDBusArgument &argument, MprisStatusStruct &statusStruct)
{
	argument.beginStructure();
	argument >> statusStruct.state >> statusStruct.random >> statusStruct.repeatTrack >>
		statusStruct.repeatPlaylist;
	argument.endStructure();
	return argument;
}

static QDBusArgument &operator<<(QDBusArgument &argument, const MprisVersionStruct &versionStruct)
{
	argument.beginStructure();
	argument << versionStruct.major << versionStruct.minor;
	argument.endStructure();
	return argument;
}

static const QDBusArgument &operator>>(const QDBusArgument &argument, MprisVersionStruct &versionStruct)
{
	argument.beginStructure();
	argument >> versionStruct.major >> versionStruct.minor;
	argument.endStructure();
	return argument;
}

static QDBusArgument &operator<<(QDBusArgument &argument, const TelevisionScheduleEntryStruct &entry)
{
	argument.beginStructure();
	argument << entry.key << entry.name << entry.channel << entry.begin << entry.duration <<
		entry.repeat << entry.isRunning;
	argument.endStructure();
	return argument;
}

static const QDBusArgument &operator>>(const QDBusArgument &argument,
	TelevisionScheduleEntryStruct &entry)
{
	argument.beginStructure();
	argument >> entry.key >> entry.name >> entry.channel >> entry.begin >> entry.duration >>
		entry.repeat >> entry.isRunning;
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
	const K4AboutData *aboutData = KGlobal::mainComponent().aboutData();
	return aboutData->programName() + QLatin1Char(' ' )+ aboutData->version();
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

MprisPlayerObject::MprisPlayerObject(MainWindow *mainWindow_, MediaWidget *mediaWidget_,
	PlaylistTab *playlistTab_, QObject *parent) : QObject(parent), mainWindow(mainWindow_),
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
	Q_UNUSED(repeat)
	// FIXME track repeat not implemented yet
}

MprisStatusStruct MprisPlayerObject::GetStatus()
{
	MprisStatusStruct statusStruct;
	statusStruct.state = 0;

	switch (mediaWidget->getPlaybackStatus()) {
	case MediaWidget::Idle:
		statusStruct.state = 2;
		break;
	case MediaWidget::Playing:
		statusStruct.state = 0;
		break;
	case MediaWidget::Paused:
		statusStruct.state = 1;
		break;
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
	return QVariantMap(); // FIXME metadata handling not implemented yet
}

int MprisPlayerObject::GetCaps()
{
	int capabilities = (1 << 0) | // CAN_GO_NEXT // FIXME check availability
			   (1 << 1) | // CAN_GO_PREV // FIXME check availability
			   (1 << 3) | // CAN_PLAY // FIXME check availability
			   (1 << 4) | // CAN_SEEK // FIXME check availability
			   (1 << 6);  // CAN_HAS_TRACKLIST

	switch (mediaWidget->getPlaybackStatus()) {
	case MediaWidget::Idle:
		break;
	case MediaWidget::Playing:
	case MediaWidget::Paused:
		capabilities |= (1 << 2); // CAN_PAUSE
		break;
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

void MprisPlayerObject::IncreaseVolume()
{
	mediaWidget->increaseVolume();
}

void MprisPlayerObject::DecreaseVolume()
{
	mediaWidget->decreaseVolume();
}

void MprisPlayerObject::ToggleMuted()
{
	mediaWidget->toggleMuted();
}

void MprisPlayerObject::ToggleFullScreen()
{
	mediaWidget->toggleFullScreen();
}

void MprisPlayerObject::ShortSkipBackward()
{
	mediaWidget->shortSkipBackward();
}

void MprisPlayerObject::ShortSkipForward()
{
	mediaWidget->shortSkipForward();
}

void MprisPlayerObject::LongSkipBackward()
{
	mediaWidget->longSkipBackward();
}

void MprisPlayerObject::LongSkipForward()
{
	mediaWidget->longSkipForward();
}

MprisTrackListObject::MprisTrackListObject(PlaylistTab *playlistTab_, QObject *parent) :
	QObject(parent), playlistTab(playlistTab_)
{
}

MprisTrackListObject::~MprisTrackListObject()
{
}

QVariantMap MprisTrackListObject::GetMetadata(int index)
{
	Q_UNUSED(index)
	return QVariantMap(); // FIXME metadata handling not implemented yet
}

int MprisTrackListObject::GetCurrentTrack()
{
	return playlistTab->getCurrentTrack();
}

int MprisTrackListObject::GetLength()
{
	return playlistTab->getTrackCount();
}

int MprisTrackListObject::AddTrack(const QString &url, bool playImmediately)
{
	playlistTab->appendToCurrentPlaylist(QList<QUrl>() << url, playImmediately);
	return 0;
}

void MprisTrackListObject::DelTrack(int index)
{
	playlistTab->removeTrack(index);
}

void MprisTrackListObject::SetLoop(bool loop)
{
	playlistTab->setRepeat(loop);
}

void MprisTrackListObject::SetRandom(bool random)
{
	playlistTab->setRandom(random);
}

#if HAVE_DVB == 1

DBusTelevisionObject::DBusTelevisionObject(DvbTab *dvbTab_, QObject *parent) : QObject(parent),
	dvbTab(dvbTab_)
{
	qDBusRegisterMetaType<TelevisionScheduleEntryStruct>();
	qDBusRegisterMetaType<QList<TelevisionScheduleEntryStruct> >();
}

DBusTelevisionObject::~DBusTelevisionObject()
{
}

void DBusTelevisionObject::DigitPressed(int digit)
{
	if ((digit >= 0) && (digit <= 9)) {
		dvbTab->osdKeyPressed(Qt::Key_0 + digit);
	}
}

void DBusTelevisionObject::PlayChannel(const QString &nameOrNumber)
{
	dvbTab->playChannel(nameOrNumber);
}

void DBusTelevisionObject::PlayLastChannel()
{
	dvbTab->playLastChannel();
}

void DBusTelevisionObject::ToggleInstantRecord()
{
	dvbTab->toggleInstantRecord();
}

void DBusTelevisionObject::ToggleOsd()
{
	dvbTab->toggleOsd();
}

QList<TelevisionScheduleEntryStruct> DBusTelevisionObject::ListProgramSchedule()
{
	QList<TelevisionScheduleEntryStruct> entries;
	DvbRecordingModel *recordingModel = dvbTab->getManager()->getRecordingModel();

	foreach (const DvbSharedRecording &recording, recordingModel->getRecordings()) {
		TelevisionScheduleEntryStruct entry;
		entry.key = recording->sqlKey;
		entry.name = recording->name;
		entry.channel = recording->channel->name;
		entry.begin = (recording->begin.toString(Qt::ISODate) + QLatin1Char('Z'));
		entry.duration = recording->duration.toString(Qt::ISODate);
		entry.repeat = recording->repeat;
		entry.isRunning = (recording->status != DvbRecording::Inactive);
		entries.append(entry);
	}

	return entries;
}

quint32 DBusTelevisionObject::ScheduleProgram(const QString &name, const QString &channel,
	const QString &begin, const QString &duration, int repeat)
{
	DvbRecording recording;
	recording.name = name;
	recording.channel = dvbTab->getManager()->getChannelModel()->findChannelByName(channel);
	recording.begin = QDateTime::fromString(begin, Qt::ISODate).toUTC();
	recording.duration = QTime::fromString(duration, Qt::ISODate);
	recording.repeat = (repeat & ((1 << 7) - 1));
	DvbSharedRecording newRecording =
		dvbTab->getManager()->getRecordingModel()->addRecording(recording);

	if (newRecording.isValid()) {
		return newRecording->sqlKey;
	}

	return 0;
}

void DBusTelevisionObject::RemoveProgram(quint32 key)
{
	DvbRecordingModel *recordingModel = dvbTab->getManager()->getRecordingModel();
	SqlKey sqlKey;
	sqlKey.sqlKey = key;
	DvbSharedRecording recording = recordingModel->getRecordings().value(sqlKey);

	if (recording.isValid()) {
		recordingModel->removeRecording(recording);
	}
}

#endif /* HAVE_DVB == 1 */
