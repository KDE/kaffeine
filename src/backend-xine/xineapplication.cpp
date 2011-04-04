/*
 * xineapplication.cpp
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

#include "xineapplication.h"

#include <QFile>
#include <KAboutData>
#include <KApplication>
#include <KCmdLineArgs>
#include <KDebug>
#include <KStandardDirs>
#include <X11/Xlib.h>
#include "xinecommands.h"

class XineParent : public XineParentMarshaller
{
public:
	XineParent(int fd, QObject *parent) : writer(fd, parent)
	{
		XineParentMarshaller::writer = &writer;
	}

	~XineParent() { }

private:
	XinePipeWriter writer;
};

XineObject::XineObject() : engine(NULL), audioOutput(NULL), videoOutput(NULL), stream(NULL),
	eventQueue(NULL), widgetSize(0), overrideTitleCount(0), overrideCurrentTitle(0),
	dirtyFlags(NotReady), aspectRatio(XineAspectRatioAuto), videoSize(0)
{
	reader = new XinePipeReader(3, this);
	parentProcess = new XineParent(4, this);
	positionTimer.setInterval(500);
	connect(&positionTimer, SIGNAL(timeout()), this, SLOT(updatePosition()));
}

XineObject::~XineObject()
{
	if (eventQueue != NULL) {
		xine_event_dispose_queue(eventQueue);
	}

	if (stream != NULL) {
		xine_dispose(stream);
	}

	if (deinterlacer != NULL) {
		xine_post_dispose(engine, deinterlacer);
	}

	if (visualization != NULL) {
		xine_post_dispose(engine, visualization);
	}

	if (videoOutput != NULL) {
		xine_close_video_driver(engine, videoOutput);
	}

	if (audioOutput != NULL) {
		xine_close_audio_driver(engine, audioOutput);
	}

	if (engine != NULL) {
		xine_config_save(engine, QFile::encodeName(
			KStandardDirs::locateLocal("data", "kaffeine/xine-config")));
		xine_exit(engine);
	}

	delete reader;
	delete parentProcess;
}

const char *XineObject::version()
{
	return xine_get_version_string();
}

void XineObject::readyRead()
{
	reader->readyRead();

	while (true) {
		int command = reader->nextCommand();

		switch (command) {
		case -1:
			return;
		case XineCommands::Quit:
			kapp->quit();
			dirtyFlags |= Quit;
			return;
		case XineCommands::Init: {
			quint64 windowId = reader->readLongLong();

			if (reader->isValid()) {
				init(windowId);
			}

			break;
		    }
		case XineCommands::Resize: {
			quint16 width = reader->readShort();
			quint16 height = reader->readShort();

			if (reader->isValid()) {
				widgetSize = ((width << 16) | height);
			}

			break;
		    }
		case XineCommands::SetMuted: {
			bool muted_ = reader->readBool();

			if (reader->isValid()) {
				muted = muted_;
				dirtyFlags |= SetMuted;
			}

			break;
		    }
		case XineCommands::SetVolume: {
			quint8 volume_ = reader->readChar();

			if (reader->isValid()) {
				volume = volume_;
				dirtyFlags |= SetVolume;
			}

			break;
		    }
		case XineCommands::SetAspectRatio: {
			qint8 aspectRatio_ = reader->readChar();

			if (reader->isValid()) {
				aspectRatio = aspectRatio_;
				dirtyFlags |= SetAspectRatio;
			}

			break;
		    }
		case XineCommands::SetDeinterlacing: {
			bool deinterlacing_ = reader->readBool();

			if (reader->isValid()) {
				deinterlacing = deinterlacing_;
				dirtyFlags |= SetDeinterlacing;
			}

			break;
		    }
		case XineCommands::PlayUrl: {
			quint32 sequenceNumber_ = reader->readInt();
			QByteArray encodedUrl_ = reader->readByteArray();
			
			if (reader->isValid()) {
				sequenceNumber = sequenceNumber_;
				encodedUrl = encodedUrl_;
				dirtyFlags &= ~(PlayStream | SetPaused | Seek | Repaint |
						MouseMoved | MousePressed |
						SetCurrentAudioChannel | SetCurrentSubtitle |
						ToggleMenu);
				dirtyFlags |= OpenStream;
			}

			break;
		    }
		case XineCommands::SetPaused: {
			bool paused_ = reader->readBool();

			if (reader->isValid()) {
				paused = paused_;
				dirtyFlags |= SetPaused;
			}

			break;
		    }
		case XineCommands::Seek: {
			qint32 seekTime_ = reader->readInt();

			if (reader->isValid()) {
				seekTime = seekTime_;
				dirtyFlags |= Seek;
			}

			break;
		    }
		case XineCommands::Repaint:
			dirtyFlags |= Repaint;
			break;
		case XineCommands::MouseMoved: {
			quint16 x = reader->readShort();
			quint16 y = reader->readShort();

			if (reader->isValid()) {
				mouseMovePosition = ((x << 16) | y);
				dirtyFlags |= MouseMoved;
			}

			break;
		    }
		case XineCommands::MousePressed: {
			quint16 x = reader->readShort();
			quint16 y = reader->readShort();

			if (reader->isValid()) {
				mousePressPosition = ((x << 16) | y);
				dirtyFlags |= MousePressed;
			}

			break;
		    }
		case XineCommands::SetCurrentAudioChannel: {
			qint8 currentAudioChannel_ = reader->readChar();

			if (reader->isValid()) {
				currentAudioChannel = currentAudioChannel_;
				dirtyFlags |= SetCurrentAudioChannel;
			}

			break;
		    }
		case XineCommands::SetCurrentSubtitle: {
			qint8 currentSubtitle_ = reader->readChar();

			if (reader->isValid()) {
				currentSubtitle = currentSubtitle_;
				dirtyFlags |= SetCurrentSubtitle;
			}

			break;
		    }
		case XineCommands::ToggleMenu:
			dirtyFlags |= ToggleMenu;
			break;
		default:
			kError() << "unknown command" << command;
			continue;
		}

		if (!reader->isValid()) {
			kError() << "wrong argument size for command" << command;
		}

		if (((dirtyFlags & (Quit | NotReady | ProcessEvent)) == 0) && (dirtyFlags != 0)) {
			QCoreApplication::postEvent(this, new QEvent(ReaderEvent));
			dirtyFlags |= ProcessEvent;
		}
	}
}

void XineObject::updatePosition()
{
	int relativePosition;
	int currentTime;
	int totalTime;

	if (xine_get_pos_length(stream, &relativePosition, &currentTime, &totalTime) == 1) {
		parentProcess->updateCurrentTotalTime(currentTime, totalTime);
	}
}

void XineObject::init(quint64 windowId)
{
	if (engine != NULL) {
		kError() << "xine engine is already initialised";
		return;
	}

	engine = xine_new();

	if (engine == NULL) {
		parentProcess->initFailed("Cannot create engine.");
		return;
	}

	xine_config_load(engine,
		QFile::encodeName(KStandardDirs::locateLocal("data", "kaffeine/xine-config")));
	xine_init(engine);

	QVector<const char *> audioDrivers;
	audioDrivers.append("auto");

	for (const char *const *it = xine_list_audio_output_plugins(engine); *it != NULL; ++it) {
		audioDrivers.append(*it);
	}

	audioDrivers.append(NULL);
	const char *audioDriver = audioDrivers.at(
		xine_config_register_enum(engine, "audio.driver", 0,
		const_cast<char **>(audioDrivers.constData()), "audio driver", NULL, 10,
		&audio_driver_cb, this));
	audioDrivers.clear();

	QVector<const char *> videoDrivers;
	videoDrivers.append("auto");

	for (const char *const *it = xine_list_video_output_plugins(engine); *it != NULL; ++it) {
		videoDrivers.append(*it);
	}

	videoDrivers.append(NULL);
	const char *videoDriver = videoDrivers.at(
		xine_config_register_enum(engine, "video.driver", 0,
		const_cast<char **>(videoDrivers.constData()), "video driver", NULL, 10,
		&video_driver_cb, this));
	videoDrivers.clear();

	QLatin1String pixelAspectRatioString(xine_config_register_string(engine,
		"video.pixel_aspect_ratio", "", "override pixel aspect ratio", NULL, 10,
		&pixel_aspect_ratio_cb, this));

	bool ok;
	pixelAspectRatio = QString(pixelAspectRatioString).toDouble(&ok);

	if (!ok || (pixelAspectRatio < 0.1) || (pixelAspectRatio > 10)) {
		if (qstrlen(pixelAspectRatioString.latin1()) != 0) {
			kWarning() << "invalid pixel aspect ratio" << pixelAspectRatioString;
		}

		pixelAspectRatio = static_cast<double>(QX11Info::appDpiY()) / QX11Info::appDpiX();

		if ((pixelAspectRatio >= 0.96) || (pixelAspectRatio <= 1.04)) {
			pixelAspectRatio = 1;
		}
	}

	audioOutput = xine_open_audio_driver(engine, audioDriver, NULL);

	if (audioOutput == NULL) {
		kWarning() << "cannot create audio output" << QLatin1String(audioDriver);
		audioOutput = xine_open_audio_driver(engine, NULL, NULL);

		if (audioOutput == NULL) {
			parentProcess->initFailed("Cannot create audio output.");
			return;
		}
	}

	x11_visual_t videoOutputData;
	memset(&videoOutputData, 0, sizeof(videoOutputData));
	videoOutputData.display = QX11Info::display();
	videoOutputData.screen = QX11Info::appScreen();
	videoOutputData.d = windowId;
	videoOutputData.user_data = this;
	videoOutputData.dest_size_cb = &dest_size_cb;
	videoOutputData.frame_output_cb = &frame_output_cb;

	videoOutput = xine_open_video_driver(engine, videoDriver, XINE_VISUAL_TYPE_X11,
		&videoOutputData);

	if (videoOutput == NULL) {
		kWarning() << "cannot create video output" << QLatin1String(videoDriver);
		videoOutput = xine_open_video_driver(engine, NULL, XINE_VISUAL_TYPE_X11,
			&videoOutputData);

		if (videoOutput == NULL) {
			parentProcess->initFailed("Cannot create video output.");
			return;
		}
	}

	stream = xine_stream_new(engine, audioOutput, videoOutput);

	if (stream == NULL) {
		parentProcess->initFailed("Cannot create stream.");
		return;
	}

	eventQueue = xine_event_new_queue(stream);

	if (eventQueue == NULL) {
		parentProcess->initFailed("Cannot create event queue.");
		return;
	}

	xine_event_create_listener_thread(eventQueue, &event_listener_cb, this);
	dirtyFlags &= ~NotReady;

	visualization = NULL; // xine_post_init(engine, "goom", 0, &audioOutput, &videoOutput);

	if (visualization == NULL) {
		// kWarning() << "cannot create audio visualization plugin";
	} else {
		xine_post_in_t *input = xine_post_input(visualization, "audio in");

		if (input == NULL) {
			kWarning() << "cannot connect audio visualization plugin";
		} else {
			xine_post_wire(xine_get_audio_source(stream), input);
		}
	}

	deinterlacer = xine_post_init(engine, "tvtime", 1, 0, &videoOutput);

	if (deinterlacer == NULL) {
		kWarning() << "cannot create deinterlace plugin";
		return;
	}

	xine_post_in_t *input = xine_post_input(deinterlacer, "parameters");

	if (input == NULL) {
		kWarning() << "cannot configure deinterlace plugin";
		return;
	}

	xine_post_api_t *deinterlacerApi = static_cast<xine_post_api_t *>(input->data);

	if (deinterlacerApi == NULL) {
		kWarning() << "cannot configure deinterlace plugin";
		return;
	}

	xine_post_api_descr_t *deinterlacerParameters = deinterlacerApi->get_param_descr();

	if (deinterlacerParameters == NULL) {
		kWarning() << "cannot configure deinterlace plugin";
		return;
	}

	for (int i = 0; deinterlacerParameters->parameter[i].type != POST_PARAM_TYPE_LAST; ++i) {
		xine_post_api_parameter_t &parameter = deinterlacerParameters->parameter[i];

		if ((parameter.type == POST_PARAM_TYPE_INT) &&
		    (strcmp(parameter.name, "method") == 0)) {
			QByteArray parameterData;
			parameterData.resize(deinterlacerParameters->struct_size);
			char *data = parameterData.data();
			deinterlacerApi->get_parameters(deinterlacer, data);
			int *method = reinterpret_cast<int *>(data + parameter.offset);

			for (char **value = parameter.enum_values; *value != NULL; ++value) {
				if (strcmp(*value, "Greedy2Frame") == 0) {
					*method = value - parameter.enum_values;
					break;
				}
			}

			deinterlacerApi->set_parameters(deinterlacer, data);
			break;
		}
	}

	// FIXME gapless playback?
}

void XineObject::customEvent(QEvent *event)
{
	if (event->type() == XineEvent) {
		QMutexLocker locker(&mutex);

		if ((dirtyFlags & (Quit | OpenStream)) != 0) {
			errorMessage.clear();
			xineDirtyFlags = 0;
		}

		while (xineDirtyFlags != 0) {
			int lowestDirtyFlag = xineDirtyFlags & (~(xineDirtyFlags - 1));
			xineDirtyFlags &= ~lowestDirtyFlag;

			switch (lowestDirtyFlag) {
			case PlaybackFailed:
				parentProcess->playbackFailed(errorMessage);
				errorMessage.clear();
				xineDirtyFlags = CloseStream;
				break;
			case PlaybackFinished:
				parentProcess->playbackFinished();
				xineDirtyFlags = CloseStream;
				break;
			case CloseStream:
				positionTimer.stop();
				xine_close(stream);
				break;
			case UpdateStreamInfo: {
				QString metadata;
				metadata.append(QChar(XineMetadataTitle));
				metadata.append(QString::fromUtf8(xine_get_meta_info(stream,
					XINE_META_INFO_TITLE)));
				metadata.append(QChar('\0'));
				metadata.append(QChar(XineMetadataArtist));
				metadata.append(QString::fromUtf8(xine_get_meta_info(stream,
					XINE_META_INFO_ARTIST)));
				metadata.append(QChar('\0'));
				metadata.append(QChar(XineMetadataAlbum));
				metadata.append(QString::fromUtf8(xine_get_meta_info(stream,
					XINE_META_INFO_ALBUM)));
				metadata.append(QChar('\0'));
				metadata.append(QChar(XineMetadataTrackNumber));
				metadata.append(QString::fromUtf8(xine_get_meta_info(stream,
					XINE_META_INFO_TRACK_NUMBER)));
				metadata.append(QChar('\0'));
				parentProcess->updateMetadata(metadata);

				bool seekable = (xine_get_stream_info(stream,
					XINE_STREAM_INFO_SEEKABLE) != 0);
				parentProcess->updateSeekable(seekable);

				QByteArray audioChannels;
				int audioChannelCount = xine_get_stream_info(stream,
					XINE_STREAM_INFO_MAX_AUDIO_CHANNEL);
				int currentAudioChannel = xine_get_param(stream,
					XINE_PARAM_AUDIO_CHANNEL_LOGICAL);

				for (int i = 0; i < audioChannelCount; ++i) {
					char langBuffer[XINE_LANG_MAX];

					if (xine_get_audio_lang(stream, i, langBuffer) == 1) {
						audioChannels.append(langBuffer);
					}

					audioChannels.append('\0');
				}

				parentProcess->updateAudioChannels(audioChannels,
					currentAudioChannel);

				QByteArray subtitles;
				int subtitleCount = xine_get_stream_info(stream,
					XINE_STREAM_INFO_MAX_SPU_CHANNEL);
				int currentSubtitle = xine_get_param(stream,
					XINE_PARAM_SPU_CHANNEL);

				for (int i = 0; i < subtitleCount; ++i) {
					char langBuffer[XINE_LANG_MAX];

					if (xine_get_spu_lang(stream, i, langBuffer) == 1) {
						subtitles.append(langBuffer);
					}

					subtitles.append('\0');
				}

				parentProcess->updateSubtitles(subtitles, currentSubtitle);

				int titleCount = xine_get_stream_info(stream,
					XINE_STREAM_INFO_DVD_TITLE_COUNT);
				int currentTitle = xine_get_stream_info(stream,
					XINE_STREAM_INFO_DVD_TITLE_NUMBER);

				if ((titleCount <= 0) && (overrideTitleCount > 0)) {
					titleCount = overrideTitleCount;
				}

				if ((currentTitle <= 0) && (overrideCurrentTitle > 0)) {
					currentTitle = overrideCurrentTitle;
				}

				parentProcess->updateTitles(titleCount, currentTitle);

				int chapterCount = xine_get_stream_info(stream,
					XINE_STREAM_INFO_DVD_CHAPTER_COUNT);
				int currentChapter = xine_get_stream_info(stream,
					XINE_STREAM_INFO_DVD_CHAPTER_NUMBER);
				parentProcess->updateChapters(chapterCount, currentChapter);

				int angleCount = xine_get_stream_info(stream,
					XINE_STREAM_INFO_DVD_ANGLE_COUNT);
				int currentAngle = xine_get_stream_info(stream,
					XINE_STREAM_INFO_DVD_ANGLE_NUMBER);
				parentProcess->updateAngles(angleCount, currentAngle);
				break;
			    }
			case UpdateMouseTracking:
				parentProcess->updateMouseTracking(mouseTrackingEnabled);
				break;
			case UpdateMouseCursor:
				parentProcess->updateMouseCursor(pointingMouseCursor);
				break;
			case UpdateVideoSize:
				parentProcess->updateVideoSize(videoSize);
				break;
			case ProcessXineEvent:
				break;
			default:
				kWarning() << "unknown flag" << lowestDirtyFlag;
				break;
			}
		}

		return;
	}

	while (dirtyFlags != 0) {
		QCoreApplication::processEvents();
		int lowestDirtyFlag = dirtyFlags & (~(dirtyFlags - 1));
		dirtyFlags &= ~lowestDirtyFlag;

		switch (lowestDirtyFlag) {
		case Quit:
			return;
		case SetMuted:
			xine_set_param(stream, XINE_PARAM_AUDIO_AMP_MUTE, muted);
			break;
		case SetVolume:
			xine_set_param(stream, XINE_PARAM_AUDIO_AMP_LEVEL, volume);
			break;
		case SetAspectRatio: {
			int xineAspectRatio;

			switch (aspectRatio) {
			case XineAspectRatioAuto:
				xineAspectRatio = XINE_VO_ASPECT_AUTO;
				break;
			case XineAspectRatio4_3:
				xineAspectRatio = XINE_VO_ASPECT_4_3;
				break;
			case XineAspectRatio16_9:
				xineAspectRatio = XINE_VO_ASPECT_ANAMORPHIC;
				break;
			case XineAspectRatioWidget:
				// this is solved via xine frame callbacks
				xineAspectRatio = XINE_VO_ASPECT_SQUARE;
				break;
			default:
				kError() << "unknown aspect ratio" << aspectRatio;
				xineAspectRatio = XINE_VO_ASPECT_AUTO;
				break;
			}

			xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, xineAspectRatio);
			break;
		    }
		case SetDeinterlacing:
			if (deinterlacing && (deinterlacer != NULL)) {
				xine_post_in_t *input = xine_post_input(deinterlacer, "video");

				if (input != NULL) {
					xine_post_wire(xine_get_video_source(stream), input);
				}
			} else {
				xine_post_wire_video_port(xine_get_video_source(stream),
					videoOutput);
			}

			break;
		case OpenStream:
			parentProcess->sync(sequenceNumber);
			overrideTitleCount = 0;
			overrideCurrentTitle = 0;

			if (encodedUrl.startsWith("cdda:")) {
				xine_get_autoplay_input_plugin_ids(engine);
				xine_get_autoplay_mrls(engine, "CD", &overrideTitleCount);
				overrideCurrentTitle =
					encodedUrl.mid(encodedUrl.lastIndexOf('/') + 1).toInt();

				if (overrideCurrentTitle < 1) {
					overrideCurrentTitle = 1;
				}
			}

			if (!encodedUrl.isEmpty()) {
				if (xine_open(stream, encodedUrl.constData()) == 1) {
					mutex.lock();
					errorMessage.clear();
					xineDirtyFlags &= ~(PlaybackFailed | PlaybackFinished);
					mutex.unlock();
					dirtyFlags |= PlayStream;
				} else {
					int errorCode = xine_get_error(stream);
					// xine_close is called when dealing with PlaybackFailed
					mutex.lock();

					if (errorMessage.isEmpty()) {
						errorMessage = xineErrorString(errorCode);
					}

					xineDirtyFlags |= PlaybackFailed;
					postXineEvent();
					mutex.unlock();
				}

				encodedUrl.clear();
			} else {
				// xine_close is called when dealing with PlaybackFinished
				mutex.lock();
				xineDirtyFlags |= PlaybackFinished;
				postXineEvent();
				mutex.unlock();
			}

			break;
		case PlayStream:
			if (xine_play(stream, 0, 0) == 1) {
				mutex.lock();
				xineDirtyFlags |= UpdateStreamInfo;
				postXineEvent();
				mutex.unlock();
				positionTimer.start();
				updatePosition();
			} else {
				int errorCode = xine_get_error(stream);
				// xine_close is called when dealing with PlaybackFailed
				mutex.lock();

				if (errorMessage.isEmpty()) {
					errorMessage = xineErrorString(errorCode);
				}

				xineDirtyFlags |= PlaybackFailed;
				postXineEvent();
				mutex.unlock();
			}

			break;
		case SetPaused:
			if (paused) {
				xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
			} else {
				xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
			}

			break;
		case Seek:
			xine_play(stream, 0, seekTime);
			break;
		case Repaint: {
			XEvent event;
			memset(&event, 0, sizeof(event));
			event.xexpose.type = Expose;
			event.xexpose.width = (widgetSize >> 16);
			event.xexpose.height = (widgetSize & 0xffff);
			xine_port_send_gui_data(videoOutput, XINE_GUI_SEND_EXPOSE_EVENT, &event);
			break;
		    }
		case MouseMoved: {
			x11_rectangle_t rectangle;
			memset(&rectangle, 0, sizeof(rectangle));
			rectangle.x = (mouseMovePosition >> 16);
			rectangle.y = (mouseMovePosition & 0xffff);
			xine_port_send_gui_data(videoOutput, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
				&rectangle);
			xine_input_data_t input;
			memset(&input, 0, sizeof(input));
			input.event.type = XINE_EVENT_INPUT_MOUSE_MOVE;
			input.event.data = &input;
			input.event.data_length = sizeof(input);
			input.x = rectangle.x;
			input.y = rectangle.y;
			xine_event_send(stream, &input.event);
			break;
		    }
		case MousePressed: {
			x11_rectangle_t rectangle;
			memset(&rectangle, 0, sizeof(rectangle));
			rectangle.x = (mousePressPosition >> 16);
			rectangle.y = (mousePressPosition & 0xffff);
			xine_port_send_gui_data(videoOutput, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
				&rectangle);
			xine_input_data_t input;
			memset(&input, 0, sizeof(input));
			input.event.type = XINE_EVENT_INPUT_MOUSE_BUTTON;
			input.event.data = &input;
			input.event.data_length = sizeof(input);
			input.button = 1;
			input.x = rectangle.x;
			input.y = rectangle.y;
			xine_event_send(stream, &input.event);
			break;
		    }
		case SetCurrentAudioChannel:
			xine_set_param(stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL,
				currentAudioChannel);
			break;
		case SetCurrentSubtitle:
			xine_set_param(stream, XINE_PARAM_SPU_CHANNEL, currentSubtitle);
			break;
		case ToggleMenu: {
			xine_event_t event;
			memset(&event, 0, sizeof(event));
			event.type = XINE_EVENT_INPUT_MENU1;
			xine_event_send(stream, &event);
			break;
		    }
		case ProcessEvent:
			break;
		default:
			kWarning() << "unknown flag" << lowestDirtyFlag;
			break;
		}
	}
}

QString XineObject::xineErrorString(int errorCode) const
{
	switch (errorCode) {
	case XINE_ERROR_NO_INPUT_PLUGIN:
		return QString("Cannot find input plugin for MRL \"") + encodedUrl + "\".";
	case XINE_ERROR_INPUT_FAILED:
		return QString("Cannot open input plugin for MRL \"") + encodedUrl + "\".";
	case XINE_ERROR_NO_DEMUX_PLUGIN:
		return QString("Cannot find demux plugin for MRL \"") + encodedUrl + "\".";
	case XINE_ERROR_DEMUX_FAILED:
		return QString("Cannot open demux plugin for MRL \"") + encodedUrl + "\".";
	case XINE_ERROR_MALFORMED_MRL:
		return QString("Cannot parse MRL \"") + encodedUrl + "\".";
	case XINE_ERROR_NONE:
	default:
		return QString("Cannot open MRL \"") + encodedUrl + "\".";
	}
}

void XineObject::postXineEvent()
{
	if ((xineDirtyFlags & ProcessXineEvent) == 0) {
		QCoreApplication::postEvent(this, new QEvent(XineEvent));
		xineDirtyFlags |= ProcessXineEvent;
	}
}

void XineObject::audio_driver_cb(void *user_data, xine_cfg_entry_t *entry)
{
	Q_UNUSED(user_data)
	Q_UNUSED(entry)
}

void XineObject::video_driver_cb(void *user_data, xine_cfg_entry_t *entry)
{
	Q_UNUSED(user_data)
	Q_UNUSED(entry)
}

void XineObject::pixel_aspect_ratio_cb(void *user_data, xine_cfg_entry_t *entry)
{
	Q_UNUSED(user_data)
	Q_UNUSED(entry)
}

void XineObject::dest_size_cb(void *user_data, int video_width, int video_height,
	double video_pixel_aspect, int *dest_width, int *dest_height, double *dest_pixel_aspect)
{
	XineObject *instance = static_cast<XineObject *>(user_data);
	int widgetSize = instance->widgetSize;
	*dest_width = (widgetSize >> 16);
	*dest_height = (widgetSize & 0xffff);

	if (instance->aspectRatio != XineAspectRatioWidget) {
		*dest_pixel_aspect = instance->pixelAspectRatio;
	} else {
		*dest_pixel_aspect = video_pixel_aspect * video_width * (widgetSize & 0xffff) /
			(video_height * (widgetSize >> 16));
	}

	unsigned int videoSize = ((video_width << 16) | video_height);

	if (instance->videoSize != videoSize) {
		QMutexLocker locker(&instance->mutex);
		instance->videoSize = videoSize;
		instance->xineDirtyFlags |= UpdateVideoSize;
		instance->postXineEvent();
	}
}

void XineObject::frame_output_cb(void *user_data, int video_width, int video_height,
	double video_pixel_aspect, int *dest_x, int *dest_y, int *dest_width, int *dest_height,
	double *dest_pixel_aspect, int *win_x, int *win_y)
{
	XineObject *instance = static_cast<XineObject *>(user_data);
	*dest_x = 0;
	*dest_y = 0;
	int widgetSize = instance->widgetSize;
	*dest_width = (widgetSize >> 16);
	*dest_height = (widgetSize & 0xffff);
	*win_x = 0;
	*win_y = 0;

	if (instance->aspectRatio != XineAspectRatioWidget) {
		*dest_pixel_aspect = instance->pixelAspectRatio;
	} else {
		*dest_pixel_aspect = video_pixel_aspect * video_width * (widgetSize & 0xffff) /
			(video_height * (widgetSize >> 16));
	}

	unsigned int videoSize = ((video_width << 16) | video_height);

	if (instance->videoSize != videoSize) {
		QMutexLocker locker(&instance->mutex);
		instance->videoSize = videoSize;
		instance->xineDirtyFlags |= UpdateVideoSize;
		instance->postXineEvent();
	}
}

void XineObject::event_listener_cb(void *user_data, const xine_event_t *event)
{
	XineObject *instance = static_cast<XineObject *>(user_data);

	switch (event->type) {
	case XINE_EVENT_UI_PLAYBACK_FINISHED: {
		QMutexLocker locker(&instance->mutex);
		instance->xineDirtyFlags |= PlaybackFinished;
		instance->postXineEvent();
		break;
	    }
	case XINE_EVENT_UI_CHANNELS_CHANGED: {
		QMutexLocker locker(&instance->mutex);
		instance->xineDirtyFlags |= UpdateStreamInfo;
		instance->postXineEvent();
		break;
	    }
	case XINE_EVENT_UI_MESSAGE: {
		QMutexLocker locker(&instance->mutex);
		const xine_ui_message_data_t *messageData =
			static_cast<const xine_ui_message_data_t *>(event->data);

		if (messageData != NULL) {
			const char *data = messageData->messages;
			int size = 0;

			while (data[size + 1] != '\0') {
				++size;
				size += qstrlen(data + size);
			}

			instance->errorMessage = QString::fromLocal8Bit(data, size);
			instance->errorMessage.replace('\0', '\n');
		} else {
			instance->errorMessage.clear();
		}

		instance->xineDirtyFlags |= PlaybackFailed;
		instance->postXineEvent();
		break;
	    }
	case XINE_EVENT_UI_NUM_BUTTONS: {
		QMutexLocker locker(&instance->mutex);
		const xine_ui_data_t *uiData = static_cast<const xine_ui_data_t *>(event->data);

		if (uiData != NULL) {
			instance->mouseTrackingEnabled = (uiData->num_buttons != 0);
		} else {
			instance->mouseTrackingEnabled = false;
		}

		instance->xineDirtyFlags |= UpdateMouseTracking;
		instance->postXineEvent();
		break;
	    }
	case XINE_EVENT_SPU_BUTTON: {
		QMutexLocker locker(&instance->mutex);
		const xine_spu_button_t *spu_event =
			static_cast<const xine_spu_button_t *>(event->data);

		if (spu_event != NULL) {
			instance->pointingMouseCursor = (spu_event->direction == 1);
		} else {
			instance->pointingMouseCursor = false;
		}

		instance->xineDirtyFlags |= UpdateMouseCursor;
		instance->postXineEvent();
		break;
	    }
	}
}

int main(int argc, char *argv[])
{
	XInitThreads();

	bool ok = false;

	if ((argc == 2) && (strcmp(argv[1], "fU4eT3iN") == 0)) {
		ok = true;
		argc = 1;
	}

	KAboutData aboutData("kaffeine-xbu", "kaffeine", ki18n("Kaffeine"),
		QByteArray("1.3-svn ").append(XineObject::version()),
		ki18nc("program description", "Internal utility for Kaffeine."),
		KAboutData::License_GPL_V2, ki18n("(C) 2007-2011 The Kaffeine Authors"),
		KLocalizedString(), "http://kaffeine.kde.org");
	aboutData.addAuthor(ki18n("Christoph Pfister"), ki18n("Maintainer"),
		"christophpfister@gmail.com");
	KCmdLineArgs::init(argc, argv, &aboutData);

	KApplication application;

	if (!ok) {
		return 0;
	}

	XineObject xineObject;
	return application.exec();
}
