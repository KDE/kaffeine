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

#include <KLocalizedString>
#include <QDebug>
#if QT_VERSION < 0x050500
# define qInfo qDebug
#endif

#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QTimer>
#include <QMap>
#include <vlc/vlc.h>

#include "../configuration.h"
#include "vlcmediawidget.h"

VlcMediaWidget::VlcMediaWidget(QWidget *parent) : AbstractMediaWidget(parent), vlcInstance(NULL),
	vlcMediaPlayer(NULL), playingDvd(false)
{
}

bool VlcMediaWidget::init()
{
	QString args = Configuration::instance()->getLibVlcArguments();
	QStringList argList;
	int argc = 0, size;

	argList = args.split(' ', QString::SkipEmptyParts);
	size = argList.size();

	const char *argv[size];

	QByteArray str[size];
	for (int i = 0; i < size; i++) {
		str[i] = argList.at(i).toUtf8();
		argv[argc++] = str[i];
	}

	vlcInstance = libvlc_new(argc, argv);
	if (!vlcInstance) {
		qWarning("libVLC: failed to use extra args: %s", qPrintable(args));
		argc = 0;
		vlcInstance = libvlc_new(0, NULL);
		if (vlcInstance)
			qInfo("Using libVLC without arguments");
	}

	if (vlcInstance == NULL) {
		qFatal("Cannot create vlc instance %s", qPrintable(libvlc_errmsg()));
		return false;
	}

	if (argc) {
		QString log = "Using libVLC with args:";
		for (int i = 0; i < argc; i++)
			log += " " + QLatin1String(argv[i]);

		qDebug("%s", qPrintable(log));
	}

	vlcMediaPlayer = libvlc_media_player_new(vlcInstance);

	if (vlcMediaPlayer == NULL) {
		qFatal("Cannot create vlc media player %s", qPrintable(libvlc_errmsg()));
		return false;
	}

	libvlc_event_manager_t *eventManager = libvlc_media_player_event_manager(vlcMediaPlayer);
	libvlc_event_e eventTypes[] = { libvlc_MediaPlayerEncounteredError,
		libvlc_MediaPlayerEndReached, libvlc_MediaPlayerLengthChanged,
		libvlc_MediaPlayerSeekableChanged, libvlc_MediaPlayerStopped,
		libvlc_MediaPlayerTimeChanged };

	for (uint i = 0; i < (sizeof(eventTypes) / sizeof(eventTypes[0])); ++i) {
		if (libvlc_event_attach(eventManager, eventTypes[i], vlcEventHandler, this) != 0) {
			qCritical("Cannot attach event handler %s", qPrintable(eventTypes[i]));
			return false;
		}
	}

	timer = new QTimer();
	connect(timer, SIGNAL(timeout()), this, SLOT(hideMouse()));

	libvlc_media_player_set_xwindow(vlcMediaPlayer, quint32(winId()));
	setAttribute(Qt::WA_NativeWindow);
	// This is broken on qt5: the kernel/qwidget.cpp tries to repaint
	// on a wrong place, causing this warning:
	//	QWidget::paintEngine: Should no longer be called
//	setAttribute(Qt::WA_PaintOnScreen);
	return true;
}

VlcMediaWidget::~VlcMediaWidget()
{
	if (vlcMediaPlayer != NULL) {
		libvlc_media_player_release(vlcMediaPlayer);
	}

	if (vlcInstance != NULL) {
		libvlc_release(vlcInstance);
	}
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
		qDebug("Setting audio output to: %s", qPrintable(i->psz_device));

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
		qWarning("cannot set volume %i", volume);
	}
}

void VlcMediaWidget::setAspectRatio(MediaWidget::AspectRatio aspectRatio)
{
	// "", "1:1", "4:3", "5:4", 16:9", "16:10", "221:100", "235:100", "239:100"
	const char *vlcAspectRatio = "";
	int vlcScaleFactor = 1;

	switch (aspectRatio) {
	case MediaWidget::AspectRatioAuto:
		break;
	case MediaWidget::AspectRatio4_3:
		vlcAspectRatio = "4:3";
		break;
	case MediaWidget::AspectRatio16_9:
		vlcAspectRatio = "16:9";
		break;
	case MediaWidget::AspectRatioWidget:
		// zero = adjust video to window
		vlcScaleFactor = 0;
		break;
	}

	libvlc_video_set_aspect_ratio(vlcMediaPlayer, vlcAspectRatio);
	libvlc_video_set_scale(vlcMediaPlayer, vlcScaleFactor);
}

void VlcMediaWidget::setDeinterlacing(bool deinterlacing)
{
	// "", "blend", "bob", "discard", "ivtc", "linear",
	// "mean", "phosphor", "x", "yadif", "yadif2x"
	const char *vlcDeinterlaceMode = "";

	if (deinterlacing) {
		vlcDeinterlaceMode = "yadif";
	}

	libvlc_video_set_deinterlace(vlcMediaPlayer, vlcDeinterlaceMode);
}

void VlcMediaWidget::play(const MediaSource &source)
{
	addPendingUpdates(PlaybackStatus | DvdMenu);
	QByteArray url = source.getUrl().toEncoded();
	playingDvd = false;

	switch (source.getType()) {
	case MediaSource::Url:
		if (url.endsWith(".iso")) {
			playingDvd = true;
		}

		break;
	case MediaSource::AudioCd:
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

	libvlc_media_t *vlcMedia = libvlc_media_new_location(vlcInstance, url.constData());

	if (vlcMedia == NULL) {
		libvlc_media_player_stop(vlcMediaPlayer);
		qWarning("Cannot create media %s", qPrintable(source.getUrl().toDisplayString()));
		return;
	}

	libvlc_event_manager_t *eventManager = libvlc_media_event_manager(vlcMedia);
	libvlc_event_e eventTypes[] = { libvlc_MediaMetaChanged };

	for (uint i = 0; i < (sizeof(eventTypes) / sizeof(eventTypes[0])); ++i) {
		if (libvlc_event_attach(eventManager, eventTypes[i], vlcEventHandler, this) != 0) {
			qWarning("Cannot attach event handler %s", qPrintable(eventTypes[i]));
		}
	}

	libvlc_media_player_set_media(vlcMediaPlayer, vlcMedia);
	libvlc_media_release(vlcMedia);

//	FIXME! subtitleUrl is only available for MediaSourceUrl private class
//	if (source.subtitleUrl.isValid())
//		setExternalSubtitle(source.subtitleUrl);

	if (libvlc_media_player_play(vlcMediaPlayer) != 0) {
		qWarning("Cannot play media %s", qPrintable(source.getUrl().toDisplayString()));
	}

	setCursor(Qt::BlankCursor);
	setCursor(Qt::ArrowCursor);
	timer->start(1000);
	setMouseTracking(true);
}

void VlcMediaWidget::stop()
{
	libvlc_media_player_stop(vlcMediaPlayer);

	timer->stop();
	setCursor(Qt::BlankCursor);
	setCursor(Qt::ArrowCursor);
	addPendingUpdates(PlaybackStatus);
}

void VlcMediaWidget::setPaused(bool paused)
{
	libvlc_media_player_set_pause(vlcMediaPlayer, paused);
	// we don't monitor playing / buffering / paused state changes
	addPendingUpdates(PlaybackStatus);
}

void VlcMediaWidget::seek(int time)
{
	libvlc_media_player_set_time(vlcMediaPlayer, time);
}

void VlcMediaWidget::setCurrentAudioStream(int currentAudioStream)
{
	// skip the 'deactivate' audio channel
	libvlc_audio_set_track(vlcMediaPlayer, currentAudioStream + 1);
}

void VlcMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	int requestedSubtitle = -1;

	QMap<int, int>::const_iterator i = subtitleId.constBegin();
	while (i != subtitleId.constEnd()) {
		if (i.value() == currentSubtitle) {
			requestedSubtitle = i.key();
			break;
		}
		i++;
	}

	libvlc_video_set_spu(vlcMediaPlayer, requestedSubtitle);
}

void VlcMediaWidget::setExternalSubtitle(const QUrl &subtitleUrl)
{
	if (libvlc_video_set_subtitle_file(vlcMediaPlayer,
	    subtitleUrl.toEncoded().constData()) == 0) {
		qWarning("Cannot set subtitle file %s", qPrintable(subtitleUrl.toDisplayString()));
	}
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
	case libvlc_Playing:
		playbackStatus = MediaWidget::Playing;
		break;
	case libvlc_Paused:
		playbackStatus = MediaWidget::Paused;
		break;
	case libvlc_Ended:
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
}

void VlcMediaWidget::updateAudioStreams()
{
	audioStreams.clear();
	libvlc_track_description_t *track = libvlc_audio_get_track_description(vlcMediaPlayer);

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

	// skip the 'deactivate' audio channel
	currentAudioStream = (libvlc_audio_get_track(vlcMediaPlayer) - 1);
}

void VlcMediaWidget::updateSubtitles()
{
	subtitles.clear();
	libvlc_track_description_t *track = libvlc_video_get_spu_description(vlcMediaPlayer);
	int i = 0;

	subtitleId.clear();
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
		track = track->p_next;
	}
	libvlc_track_description_list_release(track);

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

void VlcMediaWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		libvlc_media_player_navigate(vlcMediaPlayer, libvlc_navigate_activate);
	}

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
	mouseVisible = this->rect().contains(event->pos());

	if (!timer->isActive()) {
		setCursor(Qt::BlankCursor);
		setCursor(Qt::ArrowCursor);
	}
	if (mouseVisible)
		timer->start(1000);
	else
		timer->stop();

	AbstractMediaWidget::mouseMoveEvent(event);
}

void VlcMediaWidget::vlcEvent(const libvlc_event_t *event)
{
	PendingUpdates pendingUpdatesToBeAdded = 0;

	switch (event->type) {
	case libvlc_MediaMetaChanged:
		pendingUpdatesToBeAdded = Metadata | Subtitles;
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
		qWarning("Unknown libVLC event type %d", event->type);
	}
}

void VlcMediaWidget::vlcEventHandler(const libvlc_event_t *event, void *instance)
{
	reinterpret_cast<VlcMediaWidget *>(instance)->vlcEvent(event);
}
