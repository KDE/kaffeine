/*
 * vlcmediawidget.cpp
 *
 * Copyright (C) 2010-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "vlcmediawidget.h"
#include "vlcmediawidget_p.h"

#include <QCoreApplication>
#include <vlc/vlc.h>
#include "../log.h"

AbstractMediaWidget::AbstractMediaWidget(QWidget *parent) : QWidget(parent)
{
}

AbstractMediaWidget::~AbstractMediaWidget()
{
}

AbstractMediaWidget *AbstractMediaWidget::createVlcMediaWidget(QWidget *parent)
{
	AbstractMediaWidget *mediaWidget = VlcMediaWidget::createVlcMediaWidget(parent);

	if (mediaWidget == NULL) {
		mediaWidget = new AbstractMediaWidget(parent);
	}

	return mediaWidget;
}

void AbstractMediaWidget::setMuted(bool muted)
{
	Q_UNUSED(muted)
}

void AbstractMediaWidget::setVolume(int volume)
{
	Q_UNUSED(volume)
}

void AbstractMediaWidget::setAspectRatio(MediaWidget::AspectRatio aspectRatio)
{
	Q_UNUSED(aspectRatio)
}

void AbstractMediaWidget::setDeinterlacing(bool deinterlacing)
{
	Q_UNUSED(deinterlacing)
}

void AbstractMediaWidget::play(MediaWidget::Source source, const KUrl &url,
	const KUrl &subtitleUrl)
{
	Q_UNUSED(source)
	Q_UNUSED(url)
	Q_UNUSED(subtitleUrl)
}

void AbstractMediaWidget::stop()
{
	emit updatePlaybackStatus(MediaWidget::Idle);
}

void AbstractMediaWidget::setPaused(bool paused)
{
	Q_UNUSED(paused)
}

void AbstractMediaWidget::seek(int time)
{
	Q_UNUSED(time)
}

void AbstractMediaWidget::setCurrentAudioChannel(int currentAudioChannel)
{
	Q_UNUSED(currentAudioChannel)
}

void AbstractMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	Q_UNUSED(currentSubtitle)
}

void AbstractMediaWidget::setExternalSubtitle(const KUrl &subtitleUrl)
{
	Q_UNUSED(subtitleUrl)
}

void AbstractMediaWidget::setCurrentTitle(int currentTitle)
{
	Q_UNUSED(currentTitle)
}

void AbstractMediaWidget::setCurrentChapter(int currentChapter)
{
	Q_UNUSED(currentChapter)
}

void AbstractMediaWidget::setCurrentAngle(int currentAngle)
{
	Q_UNUSED(currentAngle)
}

bool AbstractMediaWidget::jumpToPreviousChapter()
{
	return false;
}

bool AbstractMediaWidget::jumpToNextChapter()
{
	return false;
}

void AbstractMediaWidget::toggleMenu()
{
}

VlcMediaWidget::VlcMediaWidget(QWidget *parent) : AbstractMediaWidget(parent), vlcInstance(NULL),
	vlcMediaPlayer(NULL)
{
}

bool VlcMediaWidget::init()
{
	const char *arguments[] = { "--no-video-title-show" };
	vlcInstance = libvlc_new(sizeof(arguments) / sizeof(arguments[0]), arguments);

	if (vlcInstance == NULL) {
		Log("VlcMediaWidget::init: cannot create vlc instance") << libvlc_errmsg();
		return false;
	}

	vlcMediaPlayer = libvlc_media_player_new(vlcInstance);

	if (vlcMediaPlayer == NULL) {
		Log("VlcMediaWidget::init: cannot create vlc media player") << libvlc_errmsg();
		return false;
	}

	libvlc_event_manager_t *eventManager = libvlc_media_player_event_manager(vlcMediaPlayer);
	libvlc_event_e eventTypes[] = { libvlc_MediaPlayerEncounteredError,
		libvlc_MediaPlayerEndReached, libvlc_MediaPlayerLengthChanged,
		libvlc_MediaPlayerSeekableChanged, libvlc_MediaPlayerStopped,
		libvlc_MediaPlayerTimeChanged };

	for (uint i = 0; i < (sizeof(eventTypes) / sizeof(eventTypes[0])); ++i) {
		if (libvlc_event_attach(eventManager, eventTypes[i], eventHandler, this) != 0) {
			Log("VlcMediaWidget::init: cannot attach event handler") << eventTypes[i];
			return false;
		}
	}

	libvlc_media_player_set_xwindow(vlcMediaPlayer, winId());
	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_PaintOnScreen);
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

void VlcMediaWidget::setMuted(bool muted)
{
	libvlc_audio_set_mute(vlcMediaPlayer, muted);
}

void VlcMediaWidget::setVolume(int volume)
{
	// 0 <= volume <= 200
	if (libvlc_audio_set_volume(vlcMediaPlayer, volume) != 0) {
		Log("VlcMediaWidget::setVolume: cannot set volume") << volume;
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

void VlcMediaWidget::play(MediaWidget::Source source, const KUrl &url, const KUrl &subtitleUrl)
{
	QByteArray encodedUrl = url.toEncoded();

	switch (source) {
	case MediaWidget::Playlist:
	case MediaWidget::Dvb:
	case MediaWidget::DvbTimeShift:
		// FIXME how to treat ".iso" files?
		break;
	case MediaWidget::AudioCd:
		encodedUrl.replace(0, 4, "cdda");
		break;
	case MediaWidget::VideoCd:
		encodedUrl.replace(0, 4, "vcd");
		break;
	case MediaWidget::Dvd:
		encodedUrl.replace(0, 4, "dvd");
		break;
	}

	libvlc_media_t *vlcMedia = libvlc_media_new_location(vlcInstance, encodedUrl.constData());

	if (vlcMedia == NULL) {
		Log("VlcMediaWidget::play: cannot create media") << url.prettyUrl();
		stop();
		return;
	}

	libvlc_event_manager_t *eventManager = libvlc_media_event_manager(vlcMedia);
	libvlc_event_e eventTypes[] = { libvlc_MediaMetaChanged };

	for (uint i = 0; i < (sizeof(eventTypes) / sizeof(eventTypes[0])); ++i) {
		if (libvlc_event_attach(eventManager, eventTypes[i], eventHandler, this) != 0) {
			Log("VlcMediaWidget::play: cannot attach event handler") << eventTypes[i];
		}
	}

	libvlc_media_player_set_media(vlcMediaPlayer, vlcMedia);
	libvlc_media_release(vlcMedia);
	setDirtyFlags(UpdatePlaybackStatus | UpdateTotalTime | UpdateCurrentTime | UpdateSeekable |
		UpdateMetadata | UpdateAudioChannels | UpdateSubtitles | UpdateTitles |
		UpdateChapters | UpdateAngles);

	if (!subtitleUrl.isEmpty()) {
		if (libvlc_video_set_subtitle_file(vlcMediaPlayer,
		    subtitleUrl.toEncoded().constData()) == 0) {
			Log("VlcMediaWidget::play: cannot set subtitle file") <<
				subtitleUrl.prettyUrl();
		}
	}

	if (libvlc_media_player_play(vlcMediaPlayer) != 0) {
		Log("VlcMediaWidget::play: cannot play media") << url.prettyUrl();
		stop();
		return;
	}
}

void VlcMediaWidget::stop()
{
	libvlc_media_player_stop(vlcMediaPlayer);
	setDirtyFlags(UpdatePlaybackStatus | UpdateTotalTime | UpdateCurrentTime | UpdateSeekable |
		UpdateMetadata | UpdateAudioChannels | UpdateSubtitles | UpdateTitles |
		UpdateChapters | UpdateAngles);
}

void VlcMediaWidget::setPaused(bool paused)
{
	libvlc_media_player_set_pause(vlcMediaPlayer, paused);
	addDirtyFlags(UpdatePlaybackStatus);
}

void VlcMediaWidget::seek(int time)
{
	libvlc_media_player_set_time(vlcMediaPlayer, time);
}

void VlcMediaWidget::setCurrentAudioChannel(int currentAudioChannel)
{
	libvlc_audio_set_track(vlcMediaPlayer, currentAudioChannel);
}

void VlcMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	libvlc_video_set_spu(vlcMediaPlayer, currentSubtitle);
}

void VlcMediaWidget::setExternalSubtitle(const KUrl &subtitleUrl)
{
	if (libvlc_video_set_subtitle_file(vlcMediaPlayer,
	    subtitleUrl.toEncoded().constData()) == 0) {
		Log("VlcMediaWidget::setExternalSubtitle: cannot set subtitle file") <<
			subtitleUrl.prettyUrl();
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

void VlcMediaWidget::toggleMenu()
{
	// FIXME
}

QSize VlcMediaWidget::sizeHint() const
{
	// FIXME
	return QWidget::sizeHint();
}

void VlcMediaWidget::customEvent(QEvent *event)
{
	Q_UNUSED(event)

	while (true) {
		uint oldDirtyFlags = dirtyFlags;
		uint lowestDirtyFlag = (oldDirtyFlags & (~(oldDirtyFlags - 1)));
		uint newDirtyFlags = (oldDirtyFlags & (~lowestDirtyFlag));

		if (oldDirtyFlags == newDirtyFlags) {
			break;
		}

		if (!dirtyFlags.testAndSetRelaxed(oldDirtyFlags, newDirtyFlags)) {
			continue;
		}

		switch (lowestDirtyFlag) {
		case PlaybackFinished:
			emit playbackFinished();
			break;
		case UpdatePlaybackStatus: {
			MediaWidget::PlaybackStatus playbackStatus = MediaWidget::Playing;

			switch (libvlc_media_player_get_state(vlcMediaPlayer)) {
			case libvlc_NothingSpecial:
			case libvlc_Stopped:
			case libvlc_Ended:
			case libvlc_Error:
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
			}

			emit updatePlaybackStatus(playbackStatus);
			break;
		    }
		case UpdateTotalTime:
			emit updateTotalTime(libvlc_media_player_get_length(vlcMediaPlayer));
			break;
		case UpdateCurrentTime:
			emit updateCurrentTime(libvlc_media_player_get_time(vlcMediaPlayer));
			break;
		case UpdateSeekable:
			emit updateSeekable(libvlc_media_player_is_seekable(vlcMediaPlayer));
			break;
		case UpdateMetadata: {
			QMap<MediaWidget::MetadataType, QString> metadata;
			libvlc_media_t *media = libvlc_media_player_get_media(vlcMediaPlayer);

			if (media != NULL) {
				metadata.insert(MediaWidget::Title, QString::fromUtf8(
					libvlc_media_get_meta(media, libvlc_meta_Title)));
				metadata.insert(MediaWidget::Artist, QString::fromUtf8(
					libvlc_media_get_meta(media, libvlc_meta_Artist)));
				metadata.insert(MediaWidget::Album, QString::fromUtf8(
					libvlc_media_get_meta(media, libvlc_meta_Album)));
				metadata.insert(MediaWidget::TrackNumber, QString::fromUtf8(
					libvlc_media_get_meta(media, libvlc_meta_TrackNumber)));
			}

			emit updateMetadata(metadata);
			break;
		    }
		case UpdateAudioChannels: {
			QStringList audioChannels;
			libvlc_track_description_t *track =
				libvlc_audio_get_track_description(vlcMediaPlayer);

			while (track != NULL) {
				QString audioChannel = QString::fromUtf8(track->psz_name);

				if (audioChannel.isEmpty()) {
					audioChannel = QString::number(audioChannels.size() + 1);
				}

				audioChannels.append(audioChannel);
				track = track->p_next;
			}

			Log("VlcMediaWidget::customEvent: number of audio channels") << audioChannels.size();
			Log("XXX:") << libvlc_audio_get_track_count(vlcMediaPlayer);
			int currentAudioChannel = libvlc_audio_get_track(vlcMediaPlayer);
			emit updateAudioChannels(audioChannels, currentAudioChannel);
			break;
		    }
		case UpdateSubtitles: {
			QStringList subtitles;
			libvlc_track_description_t *track =
				libvlc_video_get_spu_description(vlcMediaPlayer);

			while (track != NULL) {
				QString subtitle = QString::fromUtf8(track->psz_name);

				if (subtitle.isEmpty()) {
					subtitle = QString::number(subtitles.size() + 1);
				}

				subtitles.append(subtitle);
				track = track->p_next;
			}

			int currentSubtitle = libvlc_video_get_spu(vlcMediaPlayer);
			emit updateSubtitles(subtitles, currentSubtitle);
			break;
		    }
		case UpdateTitles: {
			int titleCount = libvlc_media_player_get_title_count(vlcMediaPlayer);
			int currentTitle = libvlc_media_player_get_title(vlcMediaPlayer);
			emit updateTitles(titleCount, currentTitle);
			break;
		    }
		case UpdateChapters: {
			int chapterCount = libvlc_media_player_get_chapter_count(vlcMediaPlayer);
			int currentChapter = libvlc_media_player_get_chapter(vlcMediaPlayer);
			emit updateChapters(chapterCount, currentChapter);
			break;
		    }
		case UpdateAngles: {
			// FIXME
			break;
		    }
		case UpdateDvdPlayback: {
			// FIXME
			break;
		    }
		case UpdateVideoSize: {
			// FIXME
			break;
		    }
		default:
			Log("VlcMediaWidget::customEvent: unknown dirty flag") << lowestDirtyFlag;
			break;
		}
	}
}

void VlcMediaWidget::eventHandler(const libvlc_event_t *event, void *instance)
{
	uint changedDirtyFlags = 0;

	switch (event->type) {
	case libvlc_MediaMetaChanged:
		Log("VlcMediaWidget::eventHandler: called");
		changedDirtyFlags = (UpdateMetadata | UpdateAudioChannels | UpdateSubtitles |
			UpdateTitles | UpdateChapters | UpdateAngles);
		break;
	case libvlc_MediaPlayerEncounteredError:
		changedDirtyFlags = UpdatePlaybackStatus;
		break;
	case libvlc_MediaPlayerEndReached:
		changedDirtyFlags = (PlaybackFinished | UpdatePlaybackStatus);
		break;
	case libvlc_MediaPlayerLengthChanged:
		changedDirtyFlags = UpdateTotalTime;
		break;
	case libvlc_MediaPlayerSeekableChanged:
		changedDirtyFlags = UpdateSeekable;
		break;
	case libvlc_MediaPlayerStopped:
		changedDirtyFlags = UpdatePlaybackStatus;
		break;
	case libvlc_MediaPlayerTimeChanged:
		changedDirtyFlags = UpdateCurrentTime;
		break;
	}

	if (changedDirtyFlags != 0) {
		reinterpret_cast<VlcMediaWidget *>(instance)->addDirtyFlags(changedDirtyFlags);
	} else {
		Log("VlcMediaWidget::eventHandler: unknown event type") << event->type;
	}
}

void VlcMediaWidget::addDirtyFlags(uint changedDirtyFlags)
{
	while (true) {
		uint oldDirtyFlags = dirtyFlags;
		uint newDirtyFlags = (oldDirtyFlags | changedDirtyFlags);

		if (oldDirtyFlags == newDirtyFlags) {
			break;
		}

		if (!dirtyFlags.testAndSetRelaxed(oldDirtyFlags, newDirtyFlags)) {
			continue;
		}

		if (oldDirtyFlags == 0) {
			QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		}

		break;
	}
}

void VlcMediaWidget::setDirtyFlags(uint newDirtyFlags)
{
	if (dirtyFlags.fetchAndStoreRelaxed(newDirtyFlags) == 0) {
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
	}
}
