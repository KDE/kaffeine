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

#include <QMouseEvent>
#include <KLocalizedString>
#include <KMessageBox>
#include "../log.h"
#include "mplayervideowidget.h"

MPlayerMediaWidget::MPlayerMediaWidget(QWidget *parent) : AbstractMediaWidget(parent), volume(0),
	muted(false), aspectRatio(MediaWidget::AspectRatioAuto), playbackStatus(MediaWidget::Idle),
	playingDvd(false), totalTime(0), currentTime(0), videoWidth(0), videoHeight(0),
	videoAspectRatio(1)
{
	videoWidget = new MPlayerVideoWidget(this);
	standardError.open(stderr, QIODevice::WriteOnly);
	connect(&process, SIGNAL(error(QProcess::ProcessError)),
		this, SLOT(error(QProcess::ProcessError)));
	connect(&process, SIGNAL(readyReadStandardOutput()), this, SLOT(readStandardOutput()));
	connect(&process, SIGNAL(readyReadStandardError()), this, SLOT(readStandardError()));
	process.start(QString("mplayer -idle -quiet -slave -softvol -volume 0 -wid %1").arg(videoWidget->winId()));
}

MPlayerMediaWidget::~MPlayerMediaWidget()
{
	sendCommand(Quit);
	process.waitForFinished(10000);
}

MPlayerMediaWidget *MPlayerMediaWidget::createMPlayerMediaWidget(QWidget *parent)
{
	return new MPlayerMediaWidget(parent);
}

MediaWidget::PlaybackStatus MPlayerMediaWidget::getPlaybackStatus()
{
	return playbackStatus;
}

int MPlayerMediaWidget::getTotalTime()
{
	return totalTime;
}

int MPlayerMediaWidget::getCurrentTime()
{
	return currentTime;
}

bool MPlayerMediaWidget::isSeekable()
{
	return false;
	// FIXME
}

QMap<MediaWidget::MetadataType, QString> MPlayerMediaWidget::getMetadata()
{
	QMap<MediaWidget::MetadataType, QString> metadata;
	return metadata;
	// FIXME
}

QStringList MPlayerMediaWidget::getAudioChannels()
{
	QStringList audioChannels;
	return audioChannels;
	// FIXME
}

int MPlayerMediaWidget::getCurrentAudioChannel()
{
	return 0;
	// FIXME
}

QStringList MPlayerMediaWidget::getSubtitles()
{
	QStringList subtitles;
	return subtitles;
	// FIXME
}

int MPlayerMediaWidget::getCurrentSubtitle()
{
	return 0;
	// FIXME
}

int MPlayerMediaWidget::getTitleCount()
{
	return 0;
	// FIXME
}

int MPlayerMediaWidget::getCurrentTitle()
{
	return -1;
	// FIXME
}

int MPlayerMediaWidget::getChapterCount()
{
	return 0;
	// FIXME
}

int MPlayerMediaWidget::getCurrentChapter()
{
	return -1;
	// FIXME
}

int MPlayerMediaWidget::getAngleCount()
{
	return 0;
	// FIXME
}

int MPlayerMediaWidget::getCurrentAngle()
{
	return -1;
	// FIXME
}

bool MPlayerMediaWidget::hasMenu()
{
	return playingDvd;
}

QSize MPlayerMediaWidget::getVideoSize()
{
	return QSize();
	// FIXME
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

void MPlayerMediaWidget::setDeinterlacing(bool deinterlacing)
{
	// FIXME
	Q_UNUSED(deinterlacing)
}

void MPlayerMediaWidget::play(const MediaSource &source)
{
	QByteArray url = source.url.toEncoded();
	playbackStatus = MediaWidget::Playing;
	playingDvd = false;
	totalTime = 0;
	currentTime = 0;
	videoWidth = 0;
	videoHeight = 0;
	videoAspectRatio = 1;

	switch (source.type) {
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
			url.replace(0, 5, "dvdnav:/");
		} else {
			url = "dvdnav:///";
		}

		playingDvd = true;
		break;
	}

	process.write("loadfile " + url + '\n');
	sendCommand(SetVolume);
}

void MPlayerMediaWidget::stop()
{
	playbackStatus = MediaWidget::Idle;
	playingDvd = false;
	totalTime = 0;
	currentTime = 0;
	videoWidth = 0;
	videoHeight = 0;
	videoAspectRatio = 1;
	sendCommand(Stop);
}

void MPlayerMediaWidget::setPaused(bool paused)
{
	switch (playbackStatus) {
	case MediaWidget::Idle:
		break;
	case MediaWidget::Playing:
		if (paused) {
			playbackStatus = MediaWidget::Paused;
			sendCommand(TogglePause);
		}

		break;
	case MediaWidget::Paused:
		if (!paused) {
			playbackStatus = MediaWidget::Playing;
			sendCommand(TogglePause);
		}

		break;
	}
}

void MPlayerMediaWidget::seek(int time)
{
	// FIXME
	Q_UNUSED(time)
}

void MPlayerMediaWidget::setCurrentAudioChannel(int currentAudioChannel)
{
	// FIXME
	Q_UNUSED(currentAudioChannel)
}

void MPlayerMediaWidget::setCurrentSubtitle(int currentSubtitle)
{
	// FIXME
	Q_UNUSED(currentSubtitle)
}

void MPlayerMediaWidget::setExternalSubtitle(const KUrl &subtitleUrl)
{
	// FIXME
	Q_UNUSED(subtitleUrl)
}

void MPlayerMediaWidget::setCurrentTitle(int currentTitle)
{
	// FIXME
	Q_UNUSED(currentTitle)
}

void MPlayerMediaWidget::setCurrentChapter(int currentChapter)
{
	// FIXME
	Q_UNUSED(currentChapter)
}

void MPlayerMediaWidget::setCurrentAngle(int currentAngle)
{
	// FIXME
	Q_UNUSED(currentAngle)
}

bool MPlayerMediaWidget::jumpToPreviousChapter()
{
	// FIXME
	return false;
}

bool MPlayerMediaWidget::jumpToNextChapter()
{
	// FIXME
	return false;
}

void MPlayerMediaWidget::toggleMenu()
{
	sendCommand(ActivateMenu);
}

void MPlayerMediaWidget::error(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	KMessageBox::queuedMessageBox(this, KMessageBox::Error, i18n("Cannot start mplayer process."));
}

void MPlayerMediaWidget::readStandardOutput()
{
	QByteArray data = process.readAllStandardOutput();
	standardError.write(data); // forward
	standardError.flush();

	if ((data == "\n") || (data.indexOf("\n\n") >= 0)) {
		process.write("pausing_keep_force get_property path\n");
	}

	foreach (const QByteArray &line, data.split('\n')) {
		if (line.startsWith("VO: ")) {
			process.write("pausing_keep_force get_property width\n"
				"pausing_keep_force get_property height\n"
				"pausing_keep_force get_property aspect\n");
		}

		if (line == "ANS_path=(null)") {
			playbackStatus = MediaWidget::Idle;
			playingDvd = false;
			totalTime = 0;
			currentTime = 0;
			videoWidth = 0;
			videoHeight = 0;
			videoAspectRatio = 1;
			invalidateState();
			updateVideoWidgetGeometry();
		}

		if (line.startsWith("ANS_width=")) {
			videoWidth = line.mid(10).toInt();

			if (videoWidth < 0) {
				videoWidth = 0;
			}
		}

		if (line.startsWith("ANS_height=")) {
			videoHeight = line.mid(11).toInt();

			if (videoHeight < 0) {
				videoHeight = 0;
			}
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
		}
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
	process.write("set_mouse_pos " + QByteArray::number(x) + ' ' + QByteArray::number(y) + '\n');
}

void MPlayerMediaWidget::mouseClicked()
{
	process.write("dvdnav mouse\n");
}

void MPlayerMediaWidget::resizeEvent(QResizeEvent *event)
{
	updateVideoWidgetGeometry();
	AbstractMediaWidget::resizeEvent(event);
}

void MPlayerMediaWidget::sendCommand(Command command)
{
	switch (command) {
	case ActivateMenu:
		process.write("dvdnav menu\n");
		break;
	case SetVolume: {
		int realVolume = volume;

		if (muted) {
			realVolume = 0;
		}

		process.write("pausing_keep_force set_property volume " +
			QByteArray::number(realVolume) + '\n');
		break;
	    }
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

	if (playbackStatus == MediaWidget::Idle) {
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
