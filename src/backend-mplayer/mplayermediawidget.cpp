/*
 * mplayermediawidget.cpp
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

#include "mplayermediawidget.h"

#include <KLocalizedString>
#include <KMessageBox>
#include "mplayervideowidget.h"

MPlayerMediaWidget::MPlayerMediaWidget(QWidget *parent) : AbstractMediaWidget(parent),
	muted(false), volume(0), aspectRatio(MediaWidget::AspectRatioAuto), deinterlacing(false),
	timerId(0), videoWidth(0), videoHeight(0), videoAspectRatio(1)
{
	videoWidget = new MPlayerVideoWidget(this);
	standardError.open(stderr, QIODevice::WriteOnly);
	connect(&process, SIGNAL(error(QProcess::ProcessError)),
		this, SLOT(error(QProcess::ProcessError)));
	connect(&process, SIGNAL(readyReadStandardOutput()), this, SLOT(readStandardOutput()));
	connect(&process, SIGNAL(readyReadStandardError()), this, SLOT(readStandardError()));
	process.start(QString("mplayer -idle -osdlevel 0 -quiet -slave -softvol -vf yadif "
		"-volume 0 -wid %1").arg(videoWidget->winId()));
}

MPlayerMediaWidget::~MPlayerMediaWidget()
{
	sendCommand(Quit);
	process.waitForFinished(10000);
}

AbstractMediaWidget *MPlayerMediaWidget::createMPlayerMediaWidget(QWidget *parent)
{
	return new MPlayerMediaWidget(parent);
}

void MPlayerMediaWidget::setMuted(bool muted_)
{
	muted = muted_;
	sendCommand(SetVolume);
}

void MPlayerMediaWidget::setVolume(int volume_)
{
	volume = volume_;
	sendCommand(SetVolume);
}

void MPlayerMediaWidget::setAspectRatio(MediaWidget::AspectRatio aspectRatio_)
{
	aspectRatio = aspectRatio_;
	updateVideoWidgetGeometry();
}

void MPlayerMediaWidget::setDeinterlacing(bool deinterlacing_)
{
	deinterlacing = deinterlacing_;
	sendCommand(SetDeinterlacing);
}

void MPlayerMediaWidget::play(const MediaSource &source)
{
	resetState();
	QByteArray url = source.getUrl().toEncoded();

	switch (source.getType()) {
	case MediaSource::Url:
		if (url.endsWith(".iso")) {
			// FIXME use dvd://, dvdnav:// ?
			updateDvdMenu(true);
		}

		if (source.getUrl().isLocalFile()) {
			// mplayer can't deal with urls like "file:///tmp/te%20st.m2t"
			url = QFile::encodeName(source.getUrl().toLocalFile());
			url.replace(' ', "\\ ");
		}

		break;
	case MediaSource::AudioCd:
		if (url.size() >= 7) {
			// e.g. cdda:////dev/sr0
			url.replace(0, 5, "cdda:/");
		} else {
			url = "cdda://";
		}

		break;
	case MediaSource::VideoCd:
		if (url.size() >= 7) {
			// e.g. vcd:////dev/sr0
			url.replace(0, 5, "vcd:/");
		} else {
			url = "vcd://";
		}

		break;
	case MediaSource::Dvd:
		if (url.size() >= 7) {
			// e.g. dvdnav:////dev/sr0
			url.replace(0, 5, "dvdnav:/");
		} else {
			url = "dvdnav://";
		}

		updateDvdMenu(true);
		break;
	case MediaSource::Dvb:
		if (source.getUrl().isLocalFile()) {
			// mplayer can't deal with urls like "file:///tmp/te%20st.m2t"
			url = QFile::encodeName(source.getUrl().toLocalFile());
			url.replace(' ', "\\ ");
		}

		break;
	}

	updatePlaybackStatus(MediaWidget::Playing);
	updateSeekable(true);
	process.write("loadfile " + url + '\n');
	process.write("pausing_keep_force get_property path\n");
	sendCommand(SetDeinterlacing);
	sendCommand(SetVolume);
	timerId = startTimer(500);
}

void MPlayerMediaWidget::stop()
{
	resetState();
	sendCommand(Stop);
}

void MPlayerMediaWidget::setPaused(bool paused)
{
	switch (getPlaybackStatus()) {
	case MediaWidget::Idle:
		break;
	case MediaWidget::Playing:
		if (paused) {
			updatePlaybackStatus(MediaWidget::Paused);
			sendCommand(TogglePause);
		}

		break;
	case MediaWidget::Paused:
		if (!paused) {
			updatePlaybackStatus(MediaWidget::Playing);
			sendCommand(TogglePause);
		}

		break;
	}
}

void MPlayerMediaWidget::seek(int time)
{
	process.write("pausing_keep_force set_property time_pos " +
		QByteArray::number(float(time) / 1000) + '\n');
}

void MPlayerMediaWidget::setCurrentAudioStream(int currentAudioStream)
{
	if ((currentAudioStream >= 0) && (currentAudioStream < audioIds.size())) {
		process.write("pausing_keep_force set_property switch_audio " +
			QByteArray::number(audioIds.at(currentAudioStream)) +
			"\npausing_keep_force get_property switch_audio\n");
	}
}

void MPlayerMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	process.write("pausing_keep_force set_property sub " +
		QByteArray::number(currentSubtitle) +
		"\npausing_keep_force get_property sub\n");
}

void MPlayerMediaWidget::setExternalSubtitle(const KUrl &subtitleUrl)
{
	// FIXME
	Q_UNUSED(subtitleUrl)
}

void MPlayerMediaWidget::setCurrentTitle(int currentTitle)
{
	process.write("pausing_keep_force set_property switch_title " +
		QByteArray::number(currentTitle) +
		"\npausing_keep_force get_property switch_title\n"
		"pausing_keep_force get_property chapter\n");
}

void MPlayerMediaWidget::setCurrentChapter(int currentChapter)
{
	process.write("pausing_keep_force set_property chapter " +
		QByteArray::number(currentChapter) +
		"\npausing_keep_force get_property switch_title\n"
		"pausing_keep_force get_property chapter\n");
}

void MPlayerMediaWidget::setCurrentAngle(int currentAngle)
{
	process.write("pausing_keep_force set_property switch_angle " +
		QByteArray::number(currentAngle) + '\n');
}

bool MPlayerMediaWidget::jumpToPreviousChapter()
{
	if ((getCurrentChapter() - 1) >= 0) {
		setCurrentChapter(getCurrentChapter() - 1);
		return true;
	}

	if ((getCurrentTitle() - 1) >= 0) {
		setCurrentTitle(getCurrentTitle() - 1);
		return true;
	}

	return false;
}

bool MPlayerMediaWidget::jumpToNextChapter()
{
	if ((getCurrentChapter() + 1) < getChapterCount()) {
		setCurrentChapter(getCurrentChapter() + 1);
		return true;
	}

	if ((getCurrentTitle() + 1) < getTitleCount()) {
		setCurrentTitle(getCurrentTitle() + 1);
		return true;
	}

	return false;
}

void MPlayerMediaWidget::showDvdMenu()
{
	sendCommand(ShowDvdMenu);
}

void MPlayerMediaWidget::error(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	KMessageBox::queuedMessageBox(this, KMessageBox::Error,
		i18n("Cannot start mplayer process."));
}

void MPlayerMediaWidget::readStandardOutput()
{
	QByteArray data = process.readAllStandardOutput();
	standardError.write(data); // forward
	standardError.flush();

	if ((data == "\n") || (data.indexOf("\n\n") >= 0)) {
		process.write("pausing_keep_force get_property path\n");
	}

	bool videoPropertiesChanged = false;
	QStringList audioStreams = getAudioStreams();
	bool audioStreamsChanged = false;
	QStringList subtitles = getSubtitles();
	bool subtitlesChanged = false;

	foreach (const QByteArray &line, data.split('\n')) {
		if (line.startsWith("VO: ")) {
			videoPropertiesChanged = true;
			continue;
		}

		if (line.startsWith("audio stream: ")) {
			int begin = 14;
			int end = line.indexOf(' ', begin);

			if (end < 0) {
				end = line.size();
			}

			int audioStreamIndex = line.mid(begin, end - begin).toInt();

			while (audioStreams.size() < audioStreamIndex) {
				audioStreams.append(QString::number(audioStreams.size() + 1));
			}

			while (audioIds.size() < audioStreamIndex) {
				audioIds.append(-1);
			}

			audioStreams.erase(audioStreams.begin() + audioStreamIndex,
				audioStreams.end());
			audioIds.erase(audioIds.begin() + audioStreamIndex, audioIds.end());
			QString audioStream;
			begin = line.indexOf("language: ");

			if (begin >= 0) {
				begin += 10;
				end = line.indexOf(' ', begin);

				if (end < 0) {
					end = line.size();
				}

				audioStream = line.mid(begin, end - begin);
			}

			if (audioStream.isEmpty()) {
				audioStream = QString::number(audioStreams.size() + 1);
			}

			int audioId = -1;
			begin = line.indexOf("aid: ");

			if (begin >= 0) {
				begin += 5;
				end = line.indexOf('.', begin);

				if (end < 0) {
					end = line.size();
				}

				audioId = line.mid(begin, end - begin).toInt();
			}

			audioStreams.append(audioStream);
			audioIds.append(audioId);
			audioStreamsChanged = true;
			continue;
		}

		if (line.startsWith("subtitle ")) {
			int begin = line.indexOf("( sid ): ");

			if (begin < 0) {
				continue;
			}

			begin += 9;
			int end = line.indexOf(' ', begin);

			if (end < 0) {
				end = line.size();
			}

			int subtitleIndex = line.mid(begin, end - begin).toInt();

			while (subtitles.size() < subtitleIndex) {
				subtitles.append(QString::number(subtitles.size() + 1));
			}

			subtitles.erase(subtitles.begin() + subtitleIndex, subtitles.end());
			QString subtitle;
			begin = line.indexOf("language: ");

			if (begin >= 0) {
				begin += 10;
				end = line.indexOf(' ', begin);

				if (end < 0) {
					end = line.size();
				}

				subtitle = line.mid(begin, end - begin);
			}

			if (subtitle.isEmpty()) {
				subtitle = QString::number(subtitles.size() + 1);
			}

			subtitles.append(subtitle);
			subtitlesChanged = true;
			continue;
		}

		if (line == "ANS_path=(null)") {
			switch (getPlaybackStatus()) {
			case MediaWidget::Idle:
				break;
			case MediaWidget::Playing:
			case MediaWidget::Paused:
				playbackFinished();
				break;
			}

			resetState();
			continue;
		}

		if (line.startsWith("ANS_length=")) {
			int totalTime = (line.mid(11).toFloat() * 1000 + 0.5);
			updateCurrentTotalTime(getCurrentTime(), totalTime);
			continue;
		}

		if (line.startsWith("ANS_time_pos=")) {
			int currentTime = (line.mid(13).toFloat() * 1000 + 0.5);
			updateCurrentTotalTime(currentTime, getTotalTime());
			continue;
		}

		if (line.startsWith("ANS_width=")) {
			videoWidth = line.mid(10).toInt();

			if (videoWidth < 0) {
				videoWidth = 0;
			}

			continue;
		}

		if (line.startsWith("ANS_height=")) {
			videoHeight = line.mid(11).toInt();

			if (videoHeight < 0) {
				videoHeight = 0;
			}

			continue;
		}

		if (line.startsWith("ANS_aspect=")) {
			videoAspectRatio = line.mid(11).toFloat();

			if ((videoAspectRatio > 0.01) && (videoAspectRatio < 100)) {
				// ok
			} else {
				videoAspectRatio = (videoWidth / float(videoHeight));

				if ((videoAspectRatio > 0.01) && (videoAspectRatio < 100)) {
					// ok
				} else {
					videoAspectRatio = 1;
				}
			}

			updateVideoWidgetGeometry();
			continue;
		}

		if (line.startsWith("ANS_switch_audio=")) {
			int audioId = line.mid(17).toInt();
			updateCurrentAudioStream(audioIds.indexOf(audioId));
			continue;
		}

		if (line.startsWith("ANS_sub=")) {
			int currentSubtitle = line.mid(8).toInt();
			updateCurrentSubtitle(currentSubtitle);
			continue;
		}
	}

	if (videoPropertiesChanged) {
		process.write("pausing_keep_force get_property width\n"
			"pausing_keep_force get_property height\n"
			"pausing_keep_force get_property aspect\n");
	}

	if (audioStreamsChanged) {
		updateAudioStreams(audioStreams);
		process.write("pausing_keep_force get_property switch_audio\n");
	}

	if (subtitlesChanged) {
		updateSubtitles(subtitles);
		process.write("pausing_keep_force get_property sub\n");
	}
}

void MPlayerMediaWidget::readStandardError()
{
	QByteArray data = process.readAllStandardError();
	standardError.write(data); // forward
	standardError.flush();
}

void MPlayerMediaWidget::mouseMoved(int x, int y)
{
	process.write("set_mouse_pos " + QByteArray::number(x) + ' ' + QByteArray::number(y) +
		'\n');
}

void MPlayerMediaWidget::mouseClicked()
{
	process.write("dvdnav mouse\n");
}

void MPlayerMediaWidget::resetState()
{
	resetBaseState();

	if (timerId != 0) {
		killTimer(timerId);
		timerId = 0;
	}

	audioIds.clear();
	videoWidth = 0;
	videoHeight = 0;
	videoAspectRatio = 1;
	updateVideoWidgetGeometry();
}

void MPlayerMediaWidget::resizeEvent(QResizeEvent *event)
{
	updateVideoWidgetGeometry();
	AbstractMediaWidget::resizeEvent(event);
}

void MPlayerMediaWidget::sendCommand(Command command)
{
	switch (command) {
	case SetDeinterlacing:
		if (getPlaybackStatus() == MediaWidget::Idle) {
			// only works if media is loaded
			break;
		}

		if (deinterlacing) {
			process.write("pausing_keep_force set_property deinterlace 1\n");
		} else {
			process.write("pausing_keep_force set_property deinterlace 0\n");
		}

		break;
	case SetVolume: {
		if (getPlaybackStatus() == MediaWidget::Idle) {
			// only works if media is loaded
			break;
		}

		int realVolume = volume;

		if (muted) {
			realVolume = 0;
		}

		process.write("pausing_keep_force set_property volume " +
			QByteArray::number(realVolume) + '\n');
		break;
	    }
	case ShowDvdMenu:
		process.write("dvdnav menu\n");
		break;
	case Stop:
		process.write("stop\n");
		break;
	case TogglePause:
		process.write("pause\n");
		break;
	case Quit:
		process.write("quit\n");
		break;
	}
}

void MPlayerMediaWidget::timerEvent(QTimerEvent *event)
{
	Q_UNUSED(event)
	process.write("pausing_keep_force get_property length\n"
		"pausing_keep_force get_property time_pos\n");
}

void MPlayerMediaWidget::updateVideoWidgetGeometry()
{
	float effectiveAspectRatio = videoAspectRatio;

	switch (aspectRatio) {
	case MediaWidget::AspectRatioAuto:
		break;
	case MediaWidget::AspectRatio4_3:
		effectiveAspectRatio = (4.0 / 3.0);
		break;
	case MediaWidget::AspectRatio16_9:
		effectiveAspectRatio = (16.0 / 9.0);
		break;
	case MediaWidget::AspectRatioWidget:
		effectiveAspectRatio = -1;
		break;
	}

	QRect geometry(QPoint(0, 0), size());

	if (getPlaybackStatus() == MediaWidget::Idle) {
		geometry.setSize(QSize(0, 0));
	} else if (effectiveAspectRatio > 0) {
		int newWidth = (geometry.height() * effectiveAspectRatio + 0.5);

		if (newWidth <= geometry.width()) {
			geometry.setX((geometry.width() - newWidth) / 2);
			geometry.setWidth(newWidth);
		} else {
			int newHeight = (geometry.width() / effectiveAspectRatio + 0.5);
			geometry.setY((geometry.height() - newHeight) / 2);
			geometry.setHeight(newHeight);
		}
	}

	if (videoWidget->geometry() != geometry) {
		videoWidget->setGeometry(geometry);
	}
}
