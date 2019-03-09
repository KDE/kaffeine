/*
 * vlcmediawidget.cpp
 *
 * Copyright (C) 2010-2012 Christoph Pfister <christophpfister@gmail.com>
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

#include "../log.h"

#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QTimer>
#include <QMap>
#include <vlc/libvlc_version.h>

#include "../configuration.h"
#include "vlcmediawidget.h"

const char *vlcEventName(int event)
{
	switch (event) {
	case libvlc_MediaMetaChanged:
		return "MediaMetaChanged";
	case libvlc_MediaPlayerEncounteredError:
		return "MediaPlayerEncounteredError";
	case libvlc_MediaPlayerEndReached:
		return "MediaPlayerEndReached";
	case libvlc_MediaPlayerLengthChanged:
		return "MediaPlayerLengthChanged";
	case libvlc_MediaPlayerSeekableChanged:
		return "MediaPlayerSeekableChanged";
	case libvlc_MediaPlayerStopped:
		return "MediaPlayerStopped";
	case libvlc_MediaPlayerTimeChanged:
		return "MediaPlayerTimeChanged";
	case libvlc_MediaSubItemAdded:
		return "MediaSubItemAdded";
	case libvlc_MediaDurationChanged:
		return "MediaDurationChanged";
	case libvlc_MediaParsedChanged:
		return "MediaParsedChanged";
	case libvlc_MediaFreed:
		return "MediaFreed";
	case libvlc_MediaStateChanged:
		return "MediaStateChanged";
	case libvlc_MediaSubItemTreeAdded:
		return "MediaSubItemTreeAdded";
	case libvlc_MediaPlayerMediaChanged:
		return "MediaPlayerMediaChanged";
	case libvlc_MediaPlayerNothingSpecial:
		return "MediaPlayerNothingSpecial";
	case libvlc_MediaPlayerOpening:
		return "MediaPlayerOpening";
	case libvlc_MediaPlayerBuffering:
		return "MediaPlayerBuffering";
	case libvlc_MediaPlayerPlaying:
		return "MediaPlayerPlaying";
	case libvlc_MediaPlayerPaused:
		return "MediaPlayerPaused";
	case libvlc_MediaPlayerForward:
		return "MediaPlayerForward";
	case libvlc_MediaPlayerBackward:
		return "MediaPlayerBackward";
	case libvlc_MediaPlayerPositionChanged:
		return "MediaPlayerPositionChanged";
	case libvlc_MediaPlayerPausableChanged:
		return "MediaPlayerPausableChanged";
	case libvlc_MediaPlayerTitleChanged:
		return "MediaPlayerTitleChanged";
	case libvlc_MediaPlayerSnapshotTaken:
		return "MediaPlayerSnapshotTaken";
	case libvlc_MediaPlayerVout:
		return "MediaPlayerVout";
	case libvlc_MediaPlayerScrambledChanged:
		return "MediaPlayerScrambledChanged";
	case libvlc_MediaPlayerUncorked:
		return "MediaPlayerUncorked";
	case libvlc_MediaPlayerMuted:
		return "MediaPlayerMuted";
	case libvlc_MediaListItemAdded:
		return "MediaListItemAdded";
	case libvlc_MediaListWillAddItem:
		return "MediaListWillAddItem";
	case libvlc_MediaListItemDeleted:
		return "MediaListItemDeleted";
	case libvlc_MediaListWillDeleteItem:
		return "MediaListWillDeleteItem";
	case libvlc_MediaListViewItemAdded:
		return "MediaListViewItemAdded";
	case libvlc_MediaListViewWillAddItem:
		return "MediaListViewWillAddItem";
	case libvlc_MediaListViewItemDeleted:
		return "MediaListViewItemDeleted";
	case libvlc_MediaListViewWillDeleteItem:
		return "MediaListViewWillDeleteItem";
	case libvlc_MediaListPlayerPlayed:
		return "MediaListPlayerPlayed";
	case libvlc_MediaListPlayerNextItemSet:
		return "MediaListPlayerNextItemSet";
	case libvlc_MediaListPlayerStopped:
		return "MediaListPlayerStopped";
	case libvlc_RendererDiscovererItemAdded:
		return "RendererDiscovererItemAdded";
	case libvlc_RendererDiscovererItemDeleted:
		return "RendererDiscovererItemDeleted";
	case libvlc_VlmMediaAdded:
		return "VlmMediaAdded";
	case libvlc_VlmMediaRemoved:
		return "VlmMediaRemoved";
	case libvlc_VlmMediaChanged:
		return "VlmMediaChanged";
	case libvlc_VlmMediaInstanceStarted:
		return "VlmMediaInstanceStarted";
	case libvlc_VlmMediaInstanceStopped:
		return "VlmMediaInstanceStopped";
	case libvlc_VlmMediaInstanceStatusInit:
		return "VlmMediaInstanceStatusInit";
	case libvlc_VlmMediaInstanceStatusOpening:
		return "VlmMediaInstanceStatusOpening";
	case libvlc_VlmMediaInstanceStatusPlaying:
		return "VlmMediaInstanceStatusPlaying";
	case libvlc_VlmMediaInstanceStatusPause:
		return "VlmMediaInstanceStatusPause";
	case libvlc_VlmMediaInstanceStatusEnd:
		return "VlmMediaInstanceStatusEnd";
	case libvlc_VlmMediaInstanceStatusError:
		return "VlmMediaInstanceStatusError";
#if LIBVLC_VERSION_MAJOR > 2
	case libvlc_MediaPlayerAudioVolume:
		return "MediaPlayerAudioVolume";
	case libvlc_MediaPlayerAudioDevice:
		return "MediaPlayerAudioDevice";
	case libvlc_MediaListEndReached:
		return "MediaListEndReached";
	case libvlc_MediaPlayerChapterChanged:
		return "MediaPlayerChapterChanged";
	case libvlc_MediaPlayerESAdded:
		return "MediaPlayerESAdded";
	case libvlc_MediaPlayerESDeleted:
		return "MediaPlayerESDeleted";
	case libvlc_MediaPlayerESSelected:
		return "MediaPlayerESSelected";
	case libvlc_MediaPlayerCorked:
		return "MediaPlayerCorked";
	case libvlc_MediaPlayerUnmuted:
		return "MediaPlayerUnmuted";
#endif

	default:
		return "Unknown";
	}
}

VlcMediaWidget::VlcMediaWidget(QWidget *parent) : AbstractMediaWidget(parent),
    timer(NULL), vlcInstance(NULL), vlcMedia(NULL), vlcMediaPlayer(NULL),
    isPaused(false), playingDvd(false), urlIsAudioCd(false),
    typeOfDevice(""), trackNumber(1), numTracks(1)
{
	libvlc_event_e events[] = {
		libvlc_MediaMetaChanged,
		libvlc_MediaPlayerEncounteredError,
		libvlc_MediaPlayerEndReached,
		libvlc_MediaPlayerLengthChanged,
		libvlc_MediaPlayerSeekableChanged,
		libvlc_MediaPlayerStopped,
#if LIBVLC_VERSION_MAJOR > 2
		libvlc_MediaPlayerESAdded,
		libvlc_MediaPlayerESDeleted,
#endif
		libvlc_MediaPlayerTimeChanged,
#if 0 // all other possible events
		libvlc_MediaSubItemAdded,
		libvlc_MediaDurationChanged,
		libvlc_MediaParsedChanged,
		libvlc_MediaFreed,
		libvlc_MediaStateChanged,
		libvlc_MediaSubItemTreeAdded,
		libvlc_MediaPlayerMediaChanged,
		libvlc_MediaPlayerNothingSpecial,
		libvlc_MediaPlayerOpening,
		libvlc_MediaPlayerBuffering,
		libvlc_MediaPlayerPlaying,
		libvlc_MediaPlayerPaused,
		libvlc_MediaPlayerForward,
		libvlc_MediaPlayerBackward,
		libvlc_MediaPlayerPositionChanged,
		libvlc_MediaPlayerPausableChanged,
		libvlc_MediaPlayerTitleChanged,
		libvlc_MediaPlayerSnapshotTaken,
		libvlc_MediaPlayerVout,
		libvlc_MediaPlayerScrambledChanged,
		libvlc_MediaPlayerUncorked,
		libvlc_MediaPlayerMuted,
		libvlc_MediaListItemAdded,
		libvlc_MediaListWillAddItem,
		libvlc_MediaListItemDeleted,
		libvlc_MediaListWillDeleteItem,
		libvlc_MediaListViewItemAdded,
		libvlc_MediaListViewWillAddItem,
		libvlc_MediaListViewItemDeleted,
		libvlc_MediaListViewWillDeleteItem,
		libvlc_MediaListPlayerPlayed,
		libvlc_MediaListPlayerNextItemSet,
		libvlc_MediaListPlayerStopped,
		libvlc_VlmMediaAdded,
		libvlc_VlmMediaRemoved,
		libvlc_VlmMediaChanged,
		libvlc_VlmMediaInstanceStarted,
		libvlc_VlmMediaInstanceStopped,
		libvlc_VlmMediaInstanceStatusInit,
		libvlc_VlmMediaInstanceStatusOpening,
		libvlc_VlmMediaInstanceStatusPlaying,
		libvlc_VlmMediaInstanceStatusPause,
		libvlc_VlmMediaInstanceStatusEnd,
		libvlc_VlmMediaInstanceStatusError,
#if LIBVLC_VERSION_MAJOR > 2
		libvlc_MediaListEndReached,
		libvlc_MediaPlayerAudioDevice,
		libvlc_MediaPlayerAudioVolume,
		libvlc_MediaPlayerChapterChanged,
		libvlc_MediaPlayerESSelected,
		libvlc_MediaPlayerCorked,
		libvlc_MediaPlayerUnmuted,
		libvlc_RendererDiscovererItemAdded,
		libvlc_RendererDiscovererItemDeleted,
#endif /*LIBVLC_VERSION_MAJOR */
#endif
	};

	for (uint i = 0; i < (sizeof(events) / sizeof(events[0])); ++i)
		eventType.append(events[i]);
}

bool VlcMediaWidget::init()
{
	QString args = Configuration::instance()->getLibVlcArguments();
	QStringList argList;
	int argc = 0, size;

	argList = args.split(' ', QString::SkipEmptyParts);
	size = argList.size();

	const char **argv = new const char *[size];
	QVector<QByteArray> str(size);

	for (int i = 0; i < size; i++) {
		str[i] = argList.at(i).toUtf8();
		argv[argc++] = str[i];
	}

	vlcInstance = libvlc_new(argc, argv);
	if (!vlcInstance) {
		qCWarning(logMediaWidget, "libVLC: failed to use extra args: %s", qPrintable(args));
		argc = 0;
		vlcInstance = libvlc_new(0, NULL);
		if (vlcInstance)
			qCInfo(logMediaWidget, "Using libVLC without arguments");
	}

	if (vlcInstance == NULL) {
		qFatal("Cannot create vlc instance %s", qPrintable(libvlc_errmsg()));
		delete[] argv;
		return false;
	}

	if (argc) {
		QString log = "Using libVLC with args:";
		for (int i = 0; i < argc; i++)
			log += ' ' + QLatin1String(argv[i]);

		qCDebug(logVlc, "%s", qPrintable(log));
	}
	delete[] argv;

	vlcMediaPlayer = libvlc_media_player_new(vlcInstance);

	if (vlcMediaPlayer == NULL) {
		qFatal("Cannot create vlc media player %s", qPrintable(libvlc_errmsg()));
		return false;
	}

	eventManager = libvlc_media_player_event_manager(vlcMediaPlayer);

	if (!registerEvents())
		return false;

	timer = new QTimer();
	connect(timer, SIGNAL(timeout()), this, SLOT(hideMouse()));

	libvlc_media_player_set_xwindow(vlcMediaPlayer, quint32(winId()));
	setAttribute(Qt::WA_NativeWindow);

	libvlc_audio_set_mute(vlcMediaPlayer, false);

	// This is broken on qt5: the kernel/qwidget.cpp tries to repaint
	// on a wrong place, causing this warning:
	//	QWidget::paintEngine: Should no longer be called
//	setAttribute(Qt::WA_PaintOnScreen);
	return true;
}

VlcMediaWidget::~VlcMediaWidget()
{
	metadata.clear();
	audioStreams.clear();
	subtitles.clear();
	subtitleId.clear();

	if (timer != NULL) {
		timer->stop();
		delete timer;
	}

	unregisterEvents();

	if (vlcMedia != NULL)
		libvlc_media_release(vlcMedia);

	if (vlcInstance != NULL)
		libvlc_release(vlcInstance);

	if (vlcMediaPlayer != NULL)
		libvlc_media_player_release(vlcMediaPlayer);
}

VlcMediaWidget *VlcMediaWidget::createVlcMediaWidget(QWidget *parent)
{
	QScopedPointer<VlcMediaWidget> vlcMediaWidget(new VlcMediaWidget(parent));

	if (!vlcMediaWidget->init()) {
		return NULL;
	}

	return vlcMediaWidget.take();
}

QStringList VlcMediaWidget::getAudioDevices()
{
	libvlc_audio_output_device_t *vlcAudioOutput, *i;
	QStringList audioDevices;

	// Get audio device list
	vlcAudioOutput = libvlc_audio_output_device_enum(vlcMediaPlayer);
	if (!vlcAudioOutput)
		return audioDevices;

	for (i = vlcAudioOutput; i != NULL; i = i->p_next) {
		QString device = QString::fromUtf8(i->psz_description);
		audioDevices.append(device);
	}
	libvlc_audio_output_device_list_release(vlcAudioOutput);

	return audioDevices;
}

void VlcMediaWidget::setAudioDevice(QString device)
{
	libvlc_audio_output_device_t *vlcAudioOutput, *i;
	vlcAudioOutput = libvlc_audio_output_device_enum(vlcMediaPlayer);

	if (!vlcAudioOutput)
		return;

	for (i = vlcAudioOutput; i != NULL; i = i->p_next) {
		if (device.compare(QString::fromUtf8(i->psz_description)))
			continue;
		qCDebug(logVlc, "Setting audio output to: %s", qPrintable(i->psz_device));

		libvlc_audio_output_device_set(vlcMediaPlayer, NULL, i->psz_device);
	}
	libvlc_audio_output_device_list_release(vlcAudioOutput);
}

void VlcMediaWidget::setMuted(bool muted)
{
	libvlc_audio_set_mute(vlcMediaPlayer, muted);
}

void VlcMediaWidget::setVolume(int volume)
{
	// 0 <= volume <= 200
	if (libvlc_audio_set_volume(vlcMediaPlayer, volume) != 0) {
		qCWarning(logMediaWidget, "cannot set volume %i", volume);
	}
}

void VlcMediaWidget::setAspectRatio(MediaWidget::AspectRatio aspectRatio)
{
	const char *vlcAspectRatio = "";

	switch (aspectRatio) {
	case MediaWidget::AspectRatioAuto:
		break;
	case MediaWidget::AspectRatio1_1:
		vlcAspectRatio = "1:1";
		break;
	case MediaWidget::AspectRatio4_3:
		vlcAspectRatio = "4:3";
		break;
	case MediaWidget::AspectRatio5_4:
		vlcAspectRatio = "5:4";
		break;
	case MediaWidget::AspectRatio16_9:
		vlcAspectRatio = "16:9";
		break;
	case MediaWidget::AspectRatio16_10:
		vlcAspectRatio = "16:10";
		break;
	case MediaWidget::AspectRatio221_100:
		vlcAspectRatio = "221:100";
		break;
	case MediaWidget::AspectRatio235_100:
		vlcAspectRatio = "235:100";
		break;
	case MediaWidget::AspectRatio239_100:
		vlcAspectRatio = "239:100";
		break;
	}

	libvlc_video_set_aspect_ratio(vlcMediaPlayer, vlcAspectRatio);
}

void VlcMediaWidget::resizeToVideo(float resizeFactor)
{
	qCDebug(logMediaWidget, "video resized to %.1f", resizeFactor);
	libvlc_video_set_scale(vlcMediaPlayer, resizeFactor);
}

void VlcMediaWidget::setDeinterlacing(MediaWidget::DeinterlaceMode deinterlacing)
{
	const char *vlcDeinterlaceMode;

	switch (deinterlacing) {
	case MediaWidget::DeinterlaceDiscard:
		vlcDeinterlaceMode = "discard";
		break;
	case MediaWidget::DeinterlaceBob:
		vlcDeinterlaceMode = "bob";
		break;
	case MediaWidget::DeinterlaceLinear:
		vlcDeinterlaceMode = "linear";
		break;
	case MediaWidget::DeinterlaceYadif:
		vlcDeinterlaceMode = "yadif";
		break;
	case MediaWidget::DeinterlaceYadif2x:
		vlcDeinterlaceMode = "yadif2x";
		break;
	case MediaWidget::DeinterlacePhosphor:
		vlcDeinterlaceMode = "phosphor";
		break;
	case MediaWidget::DeinterlaceX:
		vlcDeinterlaceMode = "x";
		break;
	case MediaWidget::DeinterlaceMean:
		vlcDeinterlaceMode = "mean";
		break;
	case MediaWidget::DeinterlaceBlend:
		vlcDeinterlaceMode = "blend";
		break;
	case MediaWidget::DeinterlaceIvtc:
		vlcDeinterlaceMode = "ivtc";
		break;
	case MediaWidget::DeinterlaceDisabled:
	default:
		vlcDeinterlaceMode = NULL;
	}


	libvlc_video_set_deinterlace(vlcMediaPlayer, vlcDeinterlaceMode);
}

void VlcMediaWidget::play(const MediaSource &source)
{
	addPendingUpdates(PlaybackStatus | DvdMenu);
	QByteArray url = source.getUrl().toEncoded();
	playingDvd = false;

	trackNumber = 1;
	urlIsAudioCd = false;

	switch (source.getType()) {
	case MediaSource::Url:
		if (url.endsWith(".iso")) {
			playingDvd = true;
		}

		break;
	case MediaSource::AudioCd:
		urlIsAudioCd = true;

		if (url.size() >= 7) {
			url.replace(0, 4, "cdda");
		} else {
			url = "cdda://";
		}

		break;
	case MediaSource::VideoCd:
		if (url.size() >= 7) {
			url.replace(0, 4, "vcd");
		} else {
			url = "vcd://";
		}

		break;
	case MediaSource::Dvd:
		if (url.size() >= 7) {
			url.replace(0, 4, "dvd");
		} else {
			url = "dvd://";
		}

		playingDvd = true;
		break;
	case MediaSource::Dvb:
		break;
	}

	typeOfDevice = url.constData();

	if (vlcMedia != NULL) {
		libvlc_media_player_stop(vlcMediaPlayer);
		libvlc_media_release(vlcMedia);
	}

	vlcMedia = libvlc_media_new_location(vlcInstance, typeOfDevice);
	if (urlIsAudioCd)
		libvlc_media_add_option(vlcMedia, "cdda-track=1");

	if (makePlay() < 0) {
		stop();
		return;
	}

	setCursor(Qt::BlankCursor);
	setCursor(Qt::ArrowCursor);
	timer->start(1000);
	setMouseTracking(true);
}

void VlcMediaWidget::unregisterEvents()
{
	for (int i = 0; i < eventType.size(); ++i)
		libvlc_event_detach(eventManager, eventType.at(i),
				    vlcEventHandler, this);
}

bool VlcMediaWidget::registerEvents()
{
	for (int i = 0; i < eventType.size(); ++i) {
		if (libvlc_event_attach(eventManager, eventType.at(i), vlcEventHandler, this) != 0) {
			qCCritical(logMediaWidget, "Cannot attach event handler %d", eventType.at(i));
			return false;
		}
	}
	return true;
}

int VlcMediaWidget::makePlay()
{
	if (vlcMedia == NULL) {
		libvlc_media_player_stop(vlcMediaPlayer);
		return -1;
	}

	libvlc_media_player_set_media(vlcMediaPlayer, vlcMedia);

	/*
	 * FIXME: This is mostly a boilerplate as, at least with vlc 3,
	 * this function always return -1
         */
	if (urlIsAudioCd)
		numTracks = libvlc_audio_get_track_count(vlcMediaPlayer);

	return libvlc_media_player_play(vlcMediaPlayer);
}

void VlcMediaWidget::playDirection(int direction)
{
	QString strBuf = "cdda-track=";
	libvlc_state_t state;
	int oldTrackNumber = trackNumber;

	if (direction == -1)
		trackNumber--;
	else
		trackNumber++;

	if (numTracks > 0 && trackNumber > numTracks)
		trackNumber = numTracks;
	if (trackNumber < 1)
		trackNumber = 1;

	strBuf += QString::number(trackNumber);

	if (vlcMedia != NULL) {
		libvlc_media_player_stop(vlcMediaPlayer);
		libvlc_media_release(vlcMedia);
	}

	vlcMedia = libvlc_media_new_location(vlcInstance, typeOfDevice);
	libvlc_media_add_option(vlcMedia, strBuf.toUtf8());

	if (makePlay() < 0)
		stop();

	do {
		state = libvlc_media_player_get_state (vlcMediaPlayer);
	} while(state != libvlc_Playing &&
		state != libvlc_Error &&
		state != libvlc_Ended );

	if (state != libvlc_Playing) {
		stop();
		trackNumber = oldTrackNumber;
	}
}

void VlcMediaWidget::stop()
{
	libvlc_media_player_stop(vlcMediaPlayer);

	if (vlcMedia != NULL) {
		libvlc_media_release(vlcMedia);
		vlcMedia = NULL;
	}

	timer->stop();
	setCursor(Qt::BlankCursor);
	setCursor(Qt::ArrowCursor);
	addPendingUpdates(PlaybackStatus | Metadata |
			  Subtitles | AudioStreams);
}

void VlcMediaWidget::setPaused(bool paused)
{
	isPaused = paused;
	libvlc_media_player_set_pause(vlcMediaPlayer, paused);
	// we don't monitor playing / buffering / paused state changes
	addPendingUpdates(PlaybackStatus);
}

void VlcMediaWidget::seek(int time)
{
	if (!seekable)
		return;

	libvlc_media_player_set_time(vlcMediaPlayer, time);
}

void VlcMediaWidget::setCurrentAudioStream(int _currentAudioStream)
{
	if (_currentAudioStream < 0 ||
	    _currentAudioStream > libvlc_audio_get_track(vlcMediaPlayer))
		_currentAudioStream = 0;
	// skip the 'deactivate' audio channel
	libvlc_audio_set_track(vlcMediaPlayer, _currentAudioStream + 1);

	currentAudioStream = _currentAudioStream;
}

void VlcMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	libvlc_track_description_t *tr, *track;
	int requestedSubtitle = -1;

	QMap<int, int>::const_iterator i = subtitleId.constBegin();
	while (i != subtitleId.constEnd()) {
		qCDebug(logVlc, "Subtitle #%d, key: %d", i.value(), i.key());
		if (i.value() == currentSubtitle) {
			requestedSubtitle = i.key();
			break;
		}
		i++;
	}

	qCDebug(logVlc, "Try to set subtitle #%d, id %d", currentSubtitle, requestedSubtitle);
	libvlc_video_set_spu(vlcMediaPlayer, requestedSubtitle);

	/* Print what it was actually selected */

	tr = libvlc_video_get_spu_description(vlcMediaPlayer);
	track = tr;
	while (track != NULL) {
		QString subtitle = QString::fromUtf8(track->psz_name);

		if (subtitle.isEmpty()) {
			subtitle = i18n("Subtitle %1", track->i_id);
		}

		if (track->i_id == requestedSubtitle)
			qCDebug(logVlc, "Subtitle set to id %d: %s", track->i_id, qPrintable(subtitle));
		track = track->p_next;
	}
	libvlc_track_description_list_release(tr);

}

void VlcMediaWidget::setExternalSubtitle(const QUrl &url)
{
	QString fname = url.toLocalFile();

#if LIBVLC_VERSION_MAJOR > 2
	if (libvlc_media_player_add_slave(vlcMediaPlayer,
					  libvlc_media_slave_type_subtitle,
					  url.toEncoded().constData(),
					  true) == 0)
		qCWarning(logMediaWidget, "Cannot set subtitle file %s", qPrintable(fname));
#else
	if (libvlc_video_set_subtitle_file(vlcMediaPlayer,
					   fname.toLocal8Bit().constData()) == 0)
		qCWarning(logMediaWidget, "Cannot set subtitle file %s", qPrintable(fname));
#endif
}

void VlcMediaWidget::setCurrentTitle(int currentTitle)
{
	libvlc_media_player_set_title(vlcMediaPlayer, currentTitle);
}

void VlcMediaWidget::setCurrentChapter(int currentChapter)
{
	libvlc_media_player_set_chapter(vlcMediaPlayer, currentChapter);
}

void VlcMediaWidget::setCurrentAngle(int currentAngle)
{
	Q_UNUSED(currentAngle)
	// FIXME
}

bool VlcMediaWidget::jumpToPreviousChapter()
{
	int currentTitle = libvlc_media_player_get_title(vlcMediaPlayer);
	int currentChapter = libvlc_media_player_get_chapter(vlcMediaPlayer);
	if (urlIsAudioCd)
		playDirection(-1);
	else
		libvlc_media_player_previous_chapter(vlcMediaPlayer);

	if ((libvlc_media_player_get_title(vlcMediaPlayer) != currentTitle) ||
	    (libvlc_media_player_get_chapter(vlcMediaPlayer) != currentChapter)) {
		return true;
	}

	return false;
}

bool VlcMediaWidget::jumpToNextChapter()
{
	int currentTitle = libvlc_media_player_get_title(vlcMediaPlayer);
	int currentChapter = libvlc_media_player_get_chapter(vlcMediaPlayer);
	if (urlIsAudioCd)
		playDirection(1);
	else
		libvlc_media_player_next_chapter(vlcMediaPlayer);

	if ((libvlc_media_player_get_title(vlcMediaPlayer) != currentTitle) ||
	    (libvlc_media_player_get_chapter(vlcMediaPlayer) != currentChapter)) {
		return true;
	}

	return false;
}

void VlcMediaWidget::showDvdMenu()
{
	if (playingDvd) {
		libvlc_media_player_set_title(vlcMediaPlayer, 0);
	}
}

int VlcMediaWidget::updatePlaybackStatus()
{
	MediaWidget::PlaybackStatus oldPlaybackStatus = playbackStatus;

	switch (libvlc_media_player_get_state(vlcMediaPlayer)) {
	case libvlc_NothingSpecial:
	case libvlc_Stopped:
		playbackStatus = MediaWidget::Idle;
		break;
	case libvlc_Opening:
	case libvlc_Buffering:
		playbackStatus = MediaWidget::Playing;
		break;
	case libvlc_Playing:
		// The first time libVLC is set to pause, it reports status as playing
		if (isPaused)
			playbackStatus = MediaWidget::Paused;
		else
			playbackStatus = MediaWidget::Playing;
		break;
	case libvlc_Paused:
		playbackStatus = MediaWidget::Paused;
		break;
	case libvlc_Ended:
		playDirection(1);
		break;
	case libvlc_Error:
		playbackStatus = MediaWidget::Idle;
		// don't keep last picture shown
		libvlc_media_player_stop(vlcMediaPlayer);
		break;
	}

	if (playbackStatus == MediaWidget::Idle) {
		addPendingUpdates(DvdMenu);
		playingDvd = false;
	}

	// Report if the status has changed
	return (oldPlaybackStatus != playbackStatus);
}

void VlcMediaWidget::updateCurrentTotalTime()
{
	if (playbackStatus == MediaWidget::Idle)
		return;

	currentTime = int(libvlc_media_player_get_time(vlcMediaPlayer));
	totalTime = int(libvlc_media_player_get_length(vlcMediaPlayer));

	if (currentTime < 0) {
		currentTime = 0;
	}

	if (totalTime < 0) {
		totalTime = 0;
	}

	if (totalTime && currentTime > totalTime) {
		currentTime = totalTime;
	}
}

void VlcMediaWidget::updateSeekable()
{
	seekable = libvlc_media_player_is_seekable(vlcMediaPlayer);
}

void VlcMediaWidget::updateMetadata()
{
	metadata.clear();
	libvlc_media_t *media = libvlc_media_player_get_media(vlcMediaPlayer);

	if (media != NULL) {
		metadata.insert(MediaWidget::Title,
			QString::fromUtf8(libvlc_media_get_meta(media, libvlc_meta_Title)));
		metadata.insert(MediaWidget::Artist,
			QString::fromUtf8(libvlc_media_get_meta(media, libvlc_meta_Artist)));
		metadata.insert(MediaWidget::Album,
			QString::fromUtf8(libvlc_media_get_meta(media, libvlc_meta_Album)));
		metadata.insert(MediaWidget::TrackNumber,
			QString::fromUtf8(libvlc_media_get_meta(media, libvlc_meta_TrackNumber)));
	}

	if (urlIsAudioCd)
		metadata.insert(MediaWidget::Title,
				i18n("CD track ") + QString::number(trackNumber));
}

void VlcMediaWidget::updateAudioStreams()
{
	audioStreams.clear();
	libvlc_track_description_t *tr, *track;

	tr = libvlc_audio_get_track_description(vlcMediaPlayer);
	track = tr;

	if (track != NULL) {
		// skip the 'deactivate' audio channel
		track = track->p_next;
	}

	while (track != NULL) {
		QString audioStream = QString::fromUtf8(track->psz_name);
		int cutBegin = (audioStream.indexOf(QLatin1Char('[')) + 1);

		if (cutBegin > 0) {
			int cutEnd = audioStream.lastIndexOf(QLatin1Char(']'));

			if (cutEnd >= 0) {
				// remove unnecessary text
				audioStream = audioStream.mid(cutBegin, cutEnd - cutBegin);
			}
		}

		if (audioStream.isEmpty()) {
			audioStream = QString::number(audioStreams.size() + 1);
		}

		audioStreams.append(audioStream);
		track = track->p_next;
	}
	libvlc_track_description_list_release(tr);

	// skip the 'deactivate' audio channel
	currentAudioStream = (libvlc_audio_get_track(vlcMediaPlayer) - 1);
	if (currentAudioStream < 0)
		setCurrentAudioStream(0);
}

void VlcMediaWidget::updateSubtitles()
{
	libvlc_track_description_t *tr, *track;
	int i = 0;

	subtitles.clear();
	subtitleId.clear();

	tr = libvlc_video_get_spu_description(vlcMediaPlayer);
	track = tr;

	if (track != NULL) {
		// skip the 'deactivate' subtitle
		track = track->p_next;
	}

	while (track != NULL) {
		QString subtitle = QString::fromUtf8(track->psz_name);

		if (subtitle.isEmpty()) {
			subtitle = i18n("Subtitle %1", track->i_id);
		}

		// 0 is reserved for "disabled" at mediawidget. So, we should
		// Start counting from 1, to match the range expected for
		// currentSubtitle
		subtitleId[track->i_id] = ++i;
		subtitles.append(subtitle);
		qCDebug(logVlc, "Got subtitle id#%d: %s", track->i_id, qPrintable(subtitle));
		track = track->p_next;
	}
	libvlc_track_description_list_release(tr);

	// skip the 'deactivate' subtitle
	currentSubtitle = subtitleId.value(libvlc_video_get_spu(vlcMediaPlayer), -1);
}

void VlcMediaWidget::updateTitles()
{
	titleCount = libvlc_media_player_get_title_count(vlcMediaPlayer);
	currentTitle = libvlc_media_player_get_title(vlcMediaPlayer);
}

void VlcMediaWidget::updateChapters()
{
	chapterCount = libvlc_media_player_get_chapter_count(vlcMediaPlayer);
	currentChapter = libvlc_media_player_get_chapter(vlcMediaPlayer);
}

void VlcMediaWidget::updateAngles()
{
	// FIXME
}

void VlcMediaWidget::updateDvdMenu()
{
	dvdMenu = playingDvd;
}

void VlcMediaWidget::updateVideoSize()
{
	// FIXME
}

void VlcMediaWidget::dvdNavigate(int key)
{
	int event;

	switch (key){
	case Qt::Key_Return:
		event = libvlc_navigate_activate;
		break;
	case Qt::Key_Up:
		event = libvlc_navigate_up;
		break;
	case Qt::Key_Down:
		event = libvlc_navigate_down;
		break;
	case Qt::Key_Left:
		event = libvlc_navigate_left;
		break;
	case Qt::Key_Right:
		event = libvlc_navigate_right;
		break;
	default:
		return;
	}
	libvlc_media_player_navigate(vlcMediaPlayer, event);
}

void VlcMediaWidget::mousePressEvent(QMouseEvent *event)
{
	AbstractMediaWidget::mousePressEvent(event);
}

void VlcMediaWidget::hideMouse()
{
	timer->stop();

	setCursor(Qt::ArrowCursor);
	setCursor(Qt::BlankCursor);
}

void VlcMediaWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (!timer->isActive()) {
		setCursor(Qt::BlankCursor);
		setCursor(Qt::ArrowCursor);
	}
	if (this->underMouse())
		timer->start(1000);
	else
		timer->stop();

	AbstractMediaWidget::mouseMoveEvent(event);
}

void VlcMediaWidget::vlcEvent(const libvlc_event_t *event)
{
	PendingUpdates pendingUpdatesToBeAdded = 0;

	switch (event->type) {
#if LIBVLC_VERSION_MAJOR > 2
	case libvlc_MediaPlayerESAdded:
	case libvlc_MediaPlayerESDeleted:
#endif
	case libvlc_MediaMetaChanged:
		pendingUpdatesToBeAdded = Metadata | Subtitles | AudioStreams;
		break;
	case libvlc_MediaPlayerEncounteredError:
		pendingUpdatesToBeAdded = PlaybackStatus;
		break;
	case libvlc_MediaPlayerEndReached:
		pendingUpdatesToBeAdded = (PlaybackFinished | PlaybackStatus);
		break;
	case libvlc_MediaPlayerLengthChanged:
		pendingUpdatesToBeAdded = CurrentTotalTime;
		break;
	case libvlc_MediaPlayerSeekableChanged:
		pendingUpdatesToBeAdded = Seekable | Subtitles;
		break;
	case libvlc_MediaPlayerStopped:
		pendingUpdatesToBeAdded = PlaybackStatus;
		setMouseTracking(false);
		break;
	case libvlc_MediaPlayerTimeChanged:
		pendingUpdatesToBeAdded = CurrentTotalTime;
		break;
	}

	if (pendingUpdatesToBeAdded != 0) {
		addPendingUpdates(pendingUpdatesToBeAdded);
	} else {
		qCWarning(logMediaWidget, "Unhandled event %s (%d)",
			  vlcEventName(event->type), event->type);
	}
}

void VlcMediaWidget::vlcEventHandler(const libvlc_event_t *event, void *instance)
{
	reinterpret_cast<VlcMediaWidget *>(instance)->vlcEvent(event);
}
