/*
 * xinemediawidget.cpp
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

#include "xinemediawidget.h"
#include "xinemediawidget_p.h"

#include <QCoreApplication>
#include <QResizeEvent>
#include <KDebug>
#include <KMessageBox>
#include <KUrl>
#include <config-kaffeine.h>
#include <errno.h>
#include <unistd.h>

static QString binInstallPath()
{
	return QString::fromUtf8(KAFFEINE_BIN_INSTALL_DIR "/");
}

XineProcess::XineProcess(XineMediaWidget *parent_) : QProcess(parent_), parent(parent_)
{
	memset(pipeToChild, 0, sizeof(pipeToChild));
	memset(pipeFromChild, 0, sizeof(pipeFromChild));

	if ((pipe(pipeToChild) != 0) || (pipe(pipeFromChild) != 0)) {
		kError() << "pipe failed";
		reader = NULL;
		writer = new XinePipeWriterBase();
		childProcess.writer = writer;
		return;
	}

	reader = new XinePipeReader(pipeFromChild[0], this);
	writer = new XinePipeWriter(pipeToChild[1], this);
	childProcess.writer = writer;

	setProcessChannelMode(ForwardedChannels);
	start(binInstallPath() + "kaffeine-xbu", QStringList() << "fU4eT3iN");
}

XineProcess::~XineProcess()
{
	if (state() != QProcess::NotRunning) {
		childProcess.quit();

		if (!waitForFinished(3000)) {
			kill();
		}
	}

	delete reader;
	delete writer;
}

void XineProcess::readyRead()
{
	reader->readyRead();

	while (true) {
		int command = reader->nextCommand();

		switch (command) {
		case -1:
			return;
		case XineCommands::InitFailed: {
			QString errorMessage = reader->readString();

			if (reader->isValid()) {
				parent->initFailed(errorMessage);
			}

			break;
		    }
		case XineCommands::Sync: {
			quint32 sequenceNumber = reader->readInt();

			if (reader->isValid()) {
				parent->sync(sequenceNumber);
			}

			break;
		    }
		case XineCommands::PlaybackFailed: {
			QString errorMessage = reader->readString();

			if (reader->isValid()) {
				parent->playbackFailed(errorMessage);
			}

			break;
		    }
		case XineCommands::PlaybackFinished:
			parent->playbackFinishedInternal();
			break;
		case XineCommands::UpdateSeekable: {
			bool seekable = reader->readBool();

			if (reader->isValid()) {
				parent->updateSeekable(seekable);
			}

			break;
		    }
		case XineCommands::UpdateCurrentTotalTime: {
			qint32 currentTime = reader->readInt();
			qint32 totalTime = reader->readInt();

			if (reader->isValid()) {
				parent->updateCurrentTotalTime(currentTime, totalTime);
			}

			break;
		    }
		case XineCommands::UpdateAudioChannels: {
			qint8 currentAudioChannel = reader->readChar();
			QByteArray audioChannels = reader->readByteArray();

			if (reader->isValid()) {
				parent->updateAudioChannels(audioChannels, currentAudioChannel);
			}

			break;
		    }
		case XineCommands::UpdateSubtitles: {
			qint8 currentSubtitle = reader->readChar();
			QByteArray subtitles = reader->readByteArray();

			if (reader->isValid()) {
				parent->updateSubtitles(subtitles, currentSubtitle);
			}

			break;
		    }
		case XineCommands::UpdateTitles: {
			qint8 titleCount = reader->readChar();
			qint8 currentTitle = reader->readChar();

			if (reader->isValid()) {
				parent->updateTitles(titleCount, currentTitle);
			}

			break;
		    }
		case XineCommands::UpdateChapters: {
			qint8 chapterCount = reader->readChar();
			qint8 currentChapter = reader->readChar();

			if (reader->isValid()) {
				parent->updateChapters(chapterCount, currentChapter);
			}

			break;
		    }
		case XineCommands::UpdateAngles: {
			qint8 angleCount = reader->readChar();
			qint8 currentAngle = reader->readChar();

			if (reader->isValid()) {
				parent->updateAngles(angleCount, currentAngle);
			}

			break;
		    }
		case XineCommands::UpdateMouseTracking: {
			bool mouseTrackingEnabled = reader->readBool();

			if (reader->isValid()) {
				parent->updateMouseTracking(mouseTrackingEnabled);
			}

			break;
		    }
		case XineCommands::UpdateMouseCursor: {
			bool pointingMouseCursor = reader->readBool();

			if (reader->isValid()) {
				parent->updateMouseCursor(pointingMouseCursor);
			}

			break;
		    }
		default:
			kError() << "unknown command" << command;
			continue;
		}

		if (!reader->isValid()) {
			kError() << "wrong argument size for command" << command;
		}
	}
}

void XineProcess::setupChildProcess()
{
	while (true) {
		if (dup2(pipeToChild[0], 3) < 0) {
			if (errno == EINTR) {
				continue;
			}

			kError() << "dup2 failed";
		}

		break;
	}

	while (true) {
		if (dup2(pipeFromChild[1], 4) < 0) {
			if (errno == EINTR) {
				continue;
			}

			kError() << "dup2 failed";
		}

		break;
	}
}

XineMediaWidget::XineMediaWidget(QWidget *parent) : QWidget(parent), sequenceNumber(0x71821680),
	ready(true), eventPosted(false), synchronized(false), currentTime(0), totalTime(0),
	currentAudioChannel(-1), currentSubtitle(-1), titleCount(0), currentTitle(0),
	chapterCount(0), currentChapter(0), angleCount(0), currentAngle(0)
{
	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_PaintOnScreen);

	QPalette palette = QWidget::palette();
	palette.setColor(backgroundRole(), Qt::black);
	setPalette(palette);
	setAutoFillBackground(true);

	process = new XineProcess(this);
	childProcess = process->getChildProcess();
	childProcess->init(winId());
	hide();
}

XineMediaWidget::~XineMediaWidget()
{
}

void XineMediaWidget::setMuted(bool muted)
{
	childProcess->setMuted(muted);
}

void XineMediaWidget::setVolume(int volume)
{
	childProcess->setVolume(volume);
}

void XineMediaWidget::setAspectRatio(AspectRatio aspectRatio)
{
	XineAspectRatio xineAspectRatio;

	switch (aspectRatio) {
	case AspectRatioAuto:
		xineAspectRatio = XineAspectRatioAuto;
		break;
	case AspectRatio4_3:
		xineAspectRatio = XineAspectRatio4_3;
		break;
	case AspectRatio16_9:
		xineAspectRatio = XineAspectRatio16_9;
		break;
	case AspectRatioWidget:
		xineAspectRatio = XineAspectRatioWidget;
		break;
	default:
		kError() << "unknown aspect ratio" << aspectRatio;
		return;
	}

	childProcess->setAspectRatio(xineAspectRatio);
}

void XineMediaWidget::playUrl(const KUrl &url)
{
	if (url.toLocalFile().endsWith(QLatin1String(".iso"), Qt::CaseInsensitive)) {
		encodedDvdUrl = QByteArray("dvd://").append(url.encodedPath());
		playEncodedUrl(encodedDvdUrl, PlayDvd);
	} else {
		playEncodedUrl(url.toEncoded());
	}
}

void XineMediaWidget::playAudioCd(const QString &device)
{
	playEncodedUrl(QByteArray("cdda://").append(device.toUtf8().toPercentEncoding("/")));
}

void XineMediaWidget::playVideoCd(const QString &device)
{
	playEncodedUrl(QByteArray("vcd://").append(device.toUtf8().toPercentEncoding("/")));
}

void XineMediaWidget::playDvd(const QString &device)
{
	encodedDvdUrl = QByteArray("dvd://").append(device.toUtf8().toPercentEncoding("/"));
	playEncodedUrl(encodedDvdUrl, PlayDvd);
}

void XineMediaWidget::playDvb(const KUrl &url)
{
	playEncodedUrl(url.toEncoded(), PlayDvb);
}

void XineMediaWidget::stop()
{
	playEncodedUrl(QByteArray());
}

bool XineMediaWidget::isPlaying() const
{
	return ((currentState & Playing) != 0);
}

int XineMediaWidget::getCurrentTime() const
{
	return currentTime;
}

int XineMediaWidget::getTotalTime() const
{
	return totalTime;
}

QStringList XineMediaWidget::getAudioChannels() const
{
	return audioChannels;
}

QStringList XineMediaWidget::getSubtitles() const
{
	return subtitles;
}

int XineMediaWidget::getCurrentAudioChannel() const
{
	return currentAudioChannel;
}

int XineMediaWidget::getCurrentSubtitle() const
{
	return currentSubtitle;
}

void XineMediaWidget::setPaused(bool paused)
{
	childProcess->setPaused(paused);
}

void XineMediaWidget::setCurrentAudioChannel(int currentAudioChannel_)
{
	Q_UNUSED(currentAudioChannel_) // FIXME
}

void XineMediaWidget::setCurrentSubtitle(int currentSubtitle_)
{
	Q_UNUSED(currentSubtitle_) // FIXME
}

void XineMediaWidget::toggleMenu()
{
	childProcess->toggleMenu();
}

void XineMediaWidget::setCurrentTitle(int currentTitle_)
{
	if (!encodedDvdUrl.isEmpty()) {
		if (!encodedDvdUrl.endsWith('/')) {
			encodedDvdUrl.append('/');
		}

		QByteArray encodedUrl = encodedDvdUrl;
		encodedUrl.append(QByteArray::number(currentTitle_));
		playEncodedUrl(encodedUrl, PlayDvd);
	}
}

void XineMediaWidget::setCurrentChapter(int currentChapter_)
{
	if (!encodedDvdUrl.isEmpty()) {
		if (!encodedDvdUrl.endsWith('/')) {
			encodedDvdUrl.append('/');
		}

		QByteArray encodedUrl = encodedDvdUrl;
		encodedUrl.append(QByteArray::number(currentTitle));
		encodedUrl.append('.');
		encodedUrl.append(QByteArray::number(currentChapter_));
		playEncodedUrl(encodedUrl, PlayDvd);
	}
}

void XineMediaWidget::setCurrentAngle(int currentAngle_)
{
	Q_UNUSED(currentAngle_) // not possible :-(
}

bool XineMediaWidget::playPreviousTitle()
{
	if (currentTitle > 1) {
		setCurrentTitle(--currentTitle);
		return true;
	}

	return false;
}

bool XineMediaWidget::playNextTitle()
{
	if (currentTitle < titleCount) {
		setCurrentTitle(currentTitle + 1);
		return true;
	}

	return false;
}

void XineMediaWidget::seek(int time)
{
	if (((currentState & Seekable) != 0) && (time != currentTime)) {
		childProcess->seek(time);
	}
}

void XineMediaWidget::mouseMoveEvent(QMouseEvent *event)
{
	QWidget::mouseMoveEvent(event);
	childProcess->mouseMoved(event->x(), event->y());
}

void XineMediaWidget::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);

	if (event->button() == Qt::LeftButton) {
		childProcess->mousePressed(event->x(), event->y());
	}
}

void XineMediaWidget::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);
	childProcess->repaint();
}

void XineMediaWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	childProcess->resize(event->size().width(), event->size().height());
}

void XineMediaWidget::initFailed(const QString &errorMessage)
{
	if (ready) {
		ready = false;
		currentState = 0;
		previousState &= ~PlaybackFinished;
		previousState |= SourceChanged;
		stateChanged();
		KMessageBox::queuedMessageBox(this, KMessageBox::Sorry, errorMessage); // FIXME
	}
}

void XineMediaWidget::sync(unsigned int sequenceNumber_)
{
	synchronized = ((sequenceNumber == sequenceNumber_) && isPlaying() && ready);
}

void XineMediaWidget::playbackFailed(const QString &errorMessage)
{
	if (synchronized) {
		currentState = 0;
		previousState &= ~PlaybackFinished;
		previousState |= SourceChanged;
		stateChanged();
		KMessageBox::queuedMessageBox(this, KMessageBox::Sorry, errorMessage); // FIXME
	}
}

void XineMediaWidget::playbackFinishedInternal()
{
	if (synchronized) {
		currentState = 0;
		previousState |= SourceChanged;
		stateChanged();
	}
}

void XineMediaWidget::updateSeekable(bool seekable)
{
	if (synchronized) {
		if (seekable) {
			currentState |= Seekable;
		} else {
			currentState &= ~Seekable;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateCurrentTotalTime(int currentTime_, int totalTime_)
{
	if (synchronized && ((currentState & PlayingDvb) == 0)) {
		if (currentTime_ < 0) {
			currentTime_ = 0;
		}

		if (currentTime != currentTime_) {
			currentTime = currentTime_;
			previousState |= CurrentTimeChanged;
		}

		if (totalTime_ < currentTime_) {
			totalTime_ = currentTime_;
		}

		if (totalTime != totalTime_) {
			totalTime = totalTime_;
			previousState |= TotalTimeChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateAudioChannels(const QByteArray &audioChannels_,
	int currentAudioChannel_)
{
	if (synchronized) {
		if (rawAudioChannels != audioChannels_) {
			rawAudioChannels = audioChannels_;
			audioChannels.clear();
			const char *rawData = rawAudioChannels.constData();

			for (int i = 0; i < rawAudioChannels.size(); ++i) {
				QString audioChannel = QString::fromLatin1(rawData + i);
				i += (audioChannel.size() + 1);
				audioChannels.append(audioChannel);
			}

			previousState |= AudioChannelsChanged;
		}

		if ((currentAudioChannel_ < 0) || (currentAudioChannel_ >= audioChannels.size())) {
			currentAudioChannel_ = -1;
		}

		if (currentAudioChannel != currentAudioChannel_) {
			currentAudioChannel = currentAudioChannel_;
			previousState |= CurrentAudioChannelChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateSubtitles(const QByteArray &subtitles_, int currentSubtitle_)
{
	if (synchronized) {
		if (rawSubtitles != subtitles_) {
			rawSubtitles = subtitles_;
			subtitles.clear();
			const char *rawData = rawSubtitles.constData();

			for (int i = 0; i < rawSubtitles.size(); ++i) {
				QString subtitle = QString::fromLatin1(rawData + i);
				i += (subtitle.size() + 1);
				subtitles.append(subtitle);
			}

			previousState |= SubtitlesChanged;
		}

		if ((currentSubtitle_ < 0) || (currentSubtitle_ >= subtitles.size())) {
			currentSubtitle_ = -1;
		}

		if (currentSubtitle != currentSubtitle_) {
			currentSubtitle = currentSubtitle_;
			previousState |= CurrentSubtitleChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateTitles(int titleCount_, int currentTitle_)
{
	if (synchronized) {
		if (titleCount_ < 0) {
			titleCount_ = 0;
		}

		if (titleCount != titleCount_) {
			titleCount = titleCount_;
			previousState |= TitleCountChanged;
		}

		if ((currentTitle_ < 0) || (currentTitle_ > titleCount)) {
			currentTitle_ = 0;
		}

		if (currentTitle != currentTitle_) {
			currentTitle = currentTitle_;
			previousState |= CurrentTitleChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateChapters(int chapterCount_, int currentChapter_)
{
	if (synchronized) {
		if (chapterCount_ < 0) {
			chapterCount_ = 0;
		}

		if (chapterCount != chapterCount_) {
			chapterCount = chapterCount_;
			previousState |= ChapterCountChanged;
		}

		if ((currentChapter_ < 0) || (currentChapter_ > chapterCount)) {
			currentChapter_ = 0;
		}

		if (currentChapter != currentChapter_) {
			currentChapter = currentChapter_;
			previousState |= CurrentChapterChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateAngles(int angleCount_, int currentAngle_)
{
	if (synchronized) {
		if (angleCount_ < 0) {
			angleCount_ = 0;
		}

		if (angleCount != angleCount_) {
			angleCount = angleCount_;
			previousState |= AngleCountChanged;
		}

		if ((currentAngle_ < 0) || (currentAngle_ > angleCount)) {
			currentAngle_ = 0;
		}

		if (currentAngle != currentAngle_) {
			currentAngle = currentAngle_;
			previousState |= CurrentAngleChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateMouseTracking(bool mouseTrackingEnabled)
{
	if (synchronized) {
		if (mouseTrackingEnabled) {
			setMouseTracking(true);
		} else {
			unsetCursor();
			setMouseTracking(false);
		}
	}
}

void XineMediaWidget::updateMouseCursor(bool pointingMouseCursor)
{
	if (synchronized) {
		if (pointingMouseCursor && hasMouseTracking()) {
			setCursor(Qt::PointingHandCursor);
		} else {
			unsetCursor();
		}
	}
}

void XineMediaWidget::playEncodedUrl(const QByteArray &encodedUrl, PlayOption option)
{
	++sequenceNumber;
	childProcess->playUrl(sequenceNumber, encodedUrl);

	if (ready && !encodedUrl.isEmpty()) {
		switch (option) {
		case Normal:
			currentState = (Playing | Seekable | PlaybackFinished);
			previousState |= (SourceChanged | PlaybackFinished);
			break;
		case PlayDvd:
			currentState = (Playing | Seekable | PlayingDvd);
			previousState |= SourceChanged;
			break;
		case PlayDvb:
			currentState = (Playing | PlayingDvb);
			previousState |= (SourceChanged | PlayingDvb);
			break;
		}
	} else {
		// emit dvbPlaybackFinished() immediately if necessary
		// don't emit playbackFinished() because stop() was called
		currentState &= ~(PlayingDvb | PlaybackFinished);

		if (option == PlayDvb) {
			// immediately emit dvbPlaybackFinished() if necessary
			previousState |= PlayingDvb;
		}
	}

	stateChanged();
}

void XineMediaWidget::stateChanged()
{
	if ((previousState != currentState) && !eventPosted) {
		QCoreApplication::postEvent(this, new QEvent(QEvent::User));
		eventPosted = true;
	}
}

void XineMediaWidget::customEvent(QEvent *event)
{
	Q_UNUSED(event)

	while (previousState != currentState) {
		StateFlags difference = previousState ^ currentState;
		int lowestFlag = difference & (~(difference - 1));
		previousState = (previousState & (~lowestFlag)) | (currentState & lowestFlag);

		switch (lowestFlag) {
		case SourceChanged:
			if ((currentState & PlayingDvd) == 0) {
				encodedDvdUrl.clear();
			}

			synchronized = false;

			if (currentTime != 0) {
				currentTime = 0;
				previousState |= CurrentTimeChanged;
			}

			if (totalTime != 0) {
				totalTime = 0;
				previousState |= TotalTimeChanged;
			}

			if (!audioChannels.isEmpty()) {
				rawAudioChannels.clear();
				audioChannels.clear();
				previousState |= AudioChannelsChanged;
			}

			if (currentAudioChannel != -1) {
				currentAudioChannel = -1;
				previousState |= CurrentAudioChannelChanged;
			}

			if (!subtitles.isEmpty()) {
				rawSubtitles.clear();
				subtitles.clear();
				previousState |= SubtitlesChanged;
			}

			if (currentSubtitle != -1) {
				currentSubtitle = -1;
				previousState |= CurrentSubtitleChanged;
			}

			if (titleCount != 0) {
				titleCount = 0;
				previousState |= TitleCountChanged;
			}

			if (currentTitle != 0) {
				currentTitle = 0;
				previousState |= CurrentTitleChanged;
			}

			if (chapterCount != 0) {
				chapterCount = 0;
				previousState |= ChapterCountChanged;
			}

			if (currentChapter != 0) {
				currentChapter = 0;
				previousState |= CurrentChapterChanged;
			}

			if (angleCount != 0) {
				angleCount = 0;
				previousState |= AngleCountChanged;
			}

			if (currentAngle != 0) {
				currentAngle = 0;
				previousState |= CurrentAngleChanged;
			}

			unsetCursor();
			setMouseTracking(false);
			break;
		case Playing:
			if (isPlaying()) {
				show();
				emit playbackChanged(true);
			} else {
				synchronized = false;
				hide();
				parentWidget()->update();
				emit playbackChanged(false);
			}

			break;
		case Seekable:
			emit seekableChanged((currentState & Seekable) != 0);
			break;
		case TotalTimeChanged:
			emit totalTimeChanged(totalTime);
			break;
		case CurrentTimeChanged:
			emit currentTimeChanged(currentTime);
			break;
		case AudioChannelsChanged:
			emit audioChannelsChanged(audioChannels, currentAudioChannel);
			previousState &= ~CurrentAudioChannelChanged;
			break;
		case CurrentAudioChannelChanged:
			emit currentAudioChannelChanged(currentAudioChannel);
			break;
		case SubtitlesChanged:
			emit subtitlesChanged(subtitles, currentSubtitle);
			previousState &= ~CurrentSubtitleChanged;
			break;
		case CurrentSubtitleChanged:
			emit currentSubtitleChanged(currentSubtitle);
			break;
		case PlayingDvd:
			emit dvdPlaybackChanged((currentState & PlayingDvd) != 0);
			break;
		case TitleCountChanged:
			emit titlesChanged(titleCount, currentTitle);
			previousState &= ~CurrentTitleChanged;
			break;
		case CurrentTitleChanged:
			emit currentTitleChanged(currentTitle);
			break;
		case ChapterCountChanged:
			emit chaptersChanged(chapterCount, currentChapter);
			previousState &= ~CurrentChapterChanged;
			break;
		case CurrentChapterChanged:
			emit currentChapterChanged(currentChapter);
			break;
		case AngleCountChanged:
			emit anglesChanged(angleCount, currentAngle);
			previousState &= ~CurrentAngleChanged;
			break;
		case CurrentAngleChanged:
			emit currentAngleChanged(currentAngle);
			break;
		case PlayingDvb:
			emit dvbPlaybackFinished();
			break;
		case PlaybackFinished:
			emit playbackFinished();
		default:
			kWarning() << "unknown flag" << lowestFlag;
			break;
		}
	}

	eventPosted = false;
}
