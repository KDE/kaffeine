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
		case XineCommands::UpdateCurrentTotalTime: {
			qint32 currentTime = reader->readInt();
			qint32 totalTime = reader->readInt();

			if (reader->isValid()) {
				parent->updateCurrentTotalTime(currentTime, totalTime);
			}

			break;
		    }
		case XineCommands::UpdateMetadata: {
			QString metadata = reader->readString();

			if (reader->isValid()) {
				parent->updateMetadata(metadata);
			}

			break;
		    }
		case XineCommands::UpdateSeekable: {
			bool seekable = reader->readBool();

			if (reader->isValid()) {
				parent->updateSeekable(seekable);
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
		case XineCommands::UpdateVideoSize: {
			quint32 videoSize = reader->readInt();

			if (reader->isValid()) {
				parent->updateVideoSize(videoSize);
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
	currentTime(0), totalTime(0), seekable(false), currentAudioChannel(-1),
	currentSubtitle(-1), titleCount(0), currentTitle(0), chapterCount(0), currentChapter(0),
	angleCount(0), currentAngle(0), videoSize(0)
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

void XineMediaWidget::setAspectRatio(MediaWidget::AspectRatio aspectRatio)
{
	XineAspectRatio xineAspectRatio;

	switch (aspectRatio) {
	case MediaWidget::AspectRatioAuto:
		xineAspectRatio = XineAspectRatioAuto;
		break;
	case MediaWidget::AspectRatio4_3:
		xineAspectRatio = XineAspectRatio4_3;
		break;
	case MediaWidget::AspectRatio16_9:
		xineAspectRatio = XineAspectRatio16_9;
		break;
	case MediaWidget::AspectRatioWidget:
		xineAspectRatio = XineAspectRatioWidget;
		break;
	default:
		kError() << "unknown aspect ratio" << aspectRatio;
		return;
	}

	childProcess->setAspectRatio(xineAspectRatio);
}

void XineMediaWidget::setDeinterlacing(bool deinterlacing)
{
	childProcess->setDeinterlacing(deinterlacing);
}

void XineMediaWidget::playUrl(const KUrl &url, const KUrl &subtitleUrl)
{
	if (url.toLocalFile().endsWith(QLatin1String(".iso"), Qt::CaseInsensitive)) {
		currentUrl = QByteArray("dvd://").append(url.encodedPath());
		playEncodedUrl(currentUrl, PlayingDvd);
	} else {
		currentUrl = url.toEncoded();
		QByteArray encodedUrl = currentUrl;

		if (subtitleUrl.isValid()) {
			encodedUrl.append("#subtitle:");
			encodedUrl.append(subtitleUrl.encodedPath());
		}

		playEncodedUrl(encodedUrl, EmitPlaybackFinished);
	}
}

void XineMediaWidget::playAudioCd(const QString &device)
{
	currentUrl = QByteArray("cdda://").append(device.toUtf8().toPercentEncoding("/"));
	playEncodedUrl(currentUrl);
}

void XineMediaWidget::playVideoCd(const QString &device)
{
	currentUrl = QByteArray("vcd://").append(device.toUtf8().toPercentEncoding("/"));
	playEncodedUrl(currentUrl);
}

void XineMediaWidget::playDvd(const QString &device)
{
	currentUrl = QByteArray("dvd://").append(device.toUtf8().toPercentEncoding("/"));
	playEncodedUrl(currentUrl, PlayingDvd);
}

void XineMediaWidget::setExternalSubtitle(const KUrl &subtitleUrl)
{
	QByteArray encodedUrl = currentUrl;
	int position = currentTime;
	int audioChannel = currentAudioChannel;

	if (subtitleUrl.isValid()) {
		encodedUrl.append("#subtitle:");
		encodedUrl.append(subtitleUrl.encodedPath());
	}

	playEncodedUrl(encodedUrl, EmitPlaybackFinished);
	seek(position);
	setCurrentAudioChannel(audioChannel);
}

void XineMediaWidget::stop()
{
	currentUrl.clear();
	playEncodedUrl(QByteArray());
}

bool XineMediaWidget::isPlaying() const
{
	return ((currentState & Playing) != 0);
}

bool XineMediaWidget::isSeekable() const
{
	return seekable;
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
	childProcess->setCurrentAudioChannel(currentAudioChannel_);
}

void XineMediaWidget::setCurrentSubtitle(int currentSubtitle_)
{
	childProcess->setCurrentSubtitle(currentSubtitle_);
}

void XineMediaWidget::toggleMenu()
{
	childProcess->toggleMenu();
}

void XineMediaWidget::setCurrentTitle(int currentTitle_)
{
	if (currentUrl.startsWith("cdda:") || currentUrl.startsWith("dvd:")) {
		QByteArray encodedUrl = currentUrl;

		if (!encodedUrl.endsWith('/')) {
			encodedUrl.append('/');
		}

		encodedUrl.append(QByteArray::number(currentTitle_));

		if (currentUrl.startsWith("dvd:")) {
			playEncodedUrl(encodedUrl, PlayingDvd);
		} else {
			playEncodedUrl(encodedUrl);
		}
	} else if (currentUrl.startsWith("vcd:")) {
		playEncodedUrl(currentUrl + '@' + QByteArray::number(currentTitle_));
	}
}

void XineMediaWidget::setCurrentChapter(int currentChapter_)
{
	if ((currentState & PlayingDvd) != 0) {
		if (!encodedDvdUrl.endsWith('/')) {
			encodedDvdUrl.append('/');
		}

		QByteArray encodedUrl = encodedDvdUrl;
		encodedUrl.append(QByteArray::number(currentTitle));
		encodedUrl.append('.');
		encodedUrl.append(QByteArray::number(currentChapter_));
		playEncodedUrl(encodedUrl, PlayingDvd);
	}
}

void XineMediaWidget::setCurrentAngle(int currentAngle_)
{
	Q_UNUSED(currentAngle_) // not possible :-(
}

bool XineMediaWidget::playPreviousTitle()
{
	if (currentTitle > 1) {
		setCurrentTitle(currentTitle - 1);
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
	if (isPlaying() && (time != currentTime)) {
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

QSize XineMediaWidget::sizeHint() const
{
	return QSize(videoSize >> 16, videoSize & 0xffff);
}

void XineMediaWidget::initFailed(const QString &errorMessage)
{
	if ((currentState & NotReady) == 0) {
		currentUrl.clear();
		playEncodedUrl(QByteArray(), NotReady);
		KMessageBox::queuedMessageBox(this, KMessageBox::Sorry, errorMessage); // FIXME
	}
}

void XineMediaWidget::sync(unsigned int sequenceNumber_)
{
	if ((sequenceNumber == sequenceNumber_) && isPlaying()) {
		currentState |= Synchronized;
	}
}

void XineMediaWidget::playbackFailed(const QString &errorMessage)
{
	if ((currentState & Synchronized) != 0) {
		if ((currentState & PlayingDvd) != 0) {
			dirtyFlags |= PlayingDvdChanged;
		}

		currentState = 0;
		dirtyFlags |= (ResetState | PlaybackStopped | PlaybackChanged);
		stateChanged();
		KMessageBox::queuedMessageBox(this, KMessageBox::Sorry, errorMessage); // FIXME
	}
}

void XineMediaWidget::playbackFinishedInternal()
{
	if ((currentState & Synchronized) != 0) {
		if ((currentState & PlayingDvd) != 0) {
			dirtyFlags |= PlayingDvdChanged;
		}

		if ((currentState & EmitPlaybackFinished) != 0) {
			dirtyFlags |= PlaybackFinished;
		} else {
			dirtyFlags |= PlaybackStopped;
		}

		currentState = 0;
		dirtyFlags |= (ResetState | PlaybackChanged);
		stateChanged();
	}
}

void XineMediaWidget::updateCurrentTotalTime(int currentTime_, int totalTime_)
{
	if ((currentState & Synchronized) != 0) {
		if (currentTime_ < 0) {
			currentTime_ = 0;
		}

		if (currentTime != currentTime_) {
			currentTime = currentTime_;
			dirtyFlags |= CurrentTimeChanged;
		}

		if (totalTime_ < currentTime) {
			totalTime_ = currentTime;
		}

		if (totalTime != totalTime_) {
			totalTime = totalTime_;
			dirtyFlags |= TotalTimeChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateMetadata(const QString &metadata_)
{
	if (((currentState & Synchronized) != 0) && (rawMetadata != metadata_)) {
		rawMetadata = metadata_;
		metadata.clear();

		for (int i = 0; i < rawMetadata.size(); ++i) {
			int type = rawMetadata.at(i).unicode();
			++i;
			int end = i;

			while ((end < rawMetadata.size()) && (rawMetadata.at(end) != '\0')) {
				++end;
			}

			if (i == end) {
				continue;
			}

			QString content = rawMetadata.mid(i, end - i);
			i = end;

			switch (type) {
			case XineMetadataTitle:
				metadata.insert(MediaWidget::Title, content);
				break;
			case XineMetadataArtist:
				metadata.insert(MediaWidget::Artist, content);
				break;
			case XineMetadataAlbum:
				metadata.insert(MediaWidget::Album, content);
				break;
			case XineMetadataTrackNumber:
				metadata.insert(MediaWidget::TrackNumber, content);
				break;
			default:
				kError() << "unknown metadata type" << type;
				break;
			}
		}

		dirtyFlags |= MetadataChanged;
		stateChanged();
	}
}

void XineMediaWidget::updateSeekable(bool seekable_)
{
	if (((currentState & Synchronized) != 0) && (seekable != seekable_)) {
		seekable = seekable_;
		dirtyFlags |= SeekableChanged;
		stateChanged();
	}
}

void XineMediaWidget::updateAudioChannels(const QByteArray &audioChannels_,
	int currentAudioChannel_)
{
	if ((currentState & Synchronized) != 0) {
		if (rawAudioChannels != audioChannels_) {
			rawAudioChannels = audioChannels_;
			audioChannels.clear();
			const char *rawData = rawAudioChannels.constData();

			for (int i = 0; i < rawAudioChannels.size(); ++i) {
				QString audioChannel = QString::fromLatin1(rawData + i);
				audioChannels.append(audioChannel);
				i += audioChannel.size();

				if (audioChannel.isEmpty()) {
					audioChannel = QString::number(i + 1);
				}
			}

			dirtyFlags |= AudioChannelsChanged;
		}

		if ((currentAudioChannel_ < 0) || (currentAudioChannel_ >= audioChannels.size())) {
			currentAudioChannel_ = -1;
		}

		if (currentAudioChannel != currentAudioChannel_) {
			currentAudioChannel = currentAudioChannel_;
			dirtyFlags |= CurrentAudioChannelChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateSubtitles(const QByteArray &subtitles_, int currentSubtitle_)
{
	if ((currentState & Synchronized) != 0) {
		if (rawSubtitles != subtitles_) {
			rawSubtitles = subtitles_;
			subtitles.clear();
			const char *rawData = rawSubtitles.constData();

			for (int i = 0; i < rawSubtitles.size(); ++i) {
				QString subtitle = QString::fromLatin1(rawData + i);
				subtitles.append(subtitle);
				i += subtitle.size();

				if (subtitle.isEmpty()) {
					subtitle = QString::number(i + 1);
				}
			}

			dirtyFlags |= SubtitlesChanged;
		}

		if ((currentSubtitle_ < 0) || (currentSubtitle_ >= subtitles.size())) {
			currentSubtitle_ = -1;
		}

		if (currentSubtitle != currentSubtitle_) {
			currentSubtitle = currentSubtitle_;
			dirtyFlags |= CurrentSubtitleChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateTitles(int titleCount_, int currentTitle_)
{
	if ((currentState & Synchronized) != 0) {
		if (titleCount_ < 0) {
			titleCount_ = 0;
		}

		if (titleCount != titleCount_) {
			titleCount = titleCount_;
			dirtyFlags |= TitleCountChanged;
		}

		if ((currentTitle_ < 0) || (currentTitle_ > titleCount)) {
			currentTitle_ = 0;
		}

		if (currentTitle != currentTitle_) {
			currentTitle = currentTitle_;
			dirtyFlags |= CurrentTitleChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateChapters(int chapterCount_, int currentChapter_)
{
	if ((currentState & Synchronized) != 0) {
		if (chapterCount_ < 0) {
			chapterCount_ = 0;
		}

		if (chapterCount != chapterCount_) {
			chapterCount = chapterCount_;
			dirtyFlags |= ChapterCountChanged;
		}

		if ((currentChapter_ < 0) || (currentChapter_ > chapterCount)) {
			currentChapter_ = 0;
		}

		if (currentChapter != currentChapter_) {
			currentChapter = currentChapter_;
			dirtyFlags |= CurrentChapterChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateAngles(int angleCount_, int currentAngle_)
{
	if ((currentState & Synchronized) != 0) {
		if (angleCount_ < 0) {
			angleCount_ = 0;
		}

		if (angleCount != angleCount_) {
			angleCount = angleCount_;
			dirtyFlags |= AngleCountChanged;
		}

		if ((currentAngle_ < 0) || (currentAngle_ > angleCount)) {
			currentAngle_ = 0;
		}

		if (currentAngle != currentAngle_) {
			currentAngle = currentAngle_;
			dirtyFlags |= CurrentAngleChanged;
		}

		stateChanged();
	}
}

void XineMediaWidget::updateMouseTracking(bool mouseTrackingEnabled)
{
	if ((currentState & Synchronized) != 0) {
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
	if ((currentState & Synchronized) != 0) {
		if (pointingMouseCursor && hasMouseTracking()) {
			setCursor(Qt::PointingHandCursor);
		} else {
			unsetCursor();
		}
	}
}

void XineMediaWidget::updateVideoSize(unsigned int videoSize_)
{
	if (((currentState & Synchronized) != 0) && (videoSize != videoSize_)) {
		videoSize = videoSize_;
		dirtyFlags |= VideoSizeChanged;
		stateChanged();
	}
}

void XineMediaWidget::playEncodedUrl(const QByteArray &encodedUrl, StateFlags stateFlags)
{
	++sequenceNumber;
	childProcess->playUrl(sequenceNumber, encodedUrl);

	if (!encodedUrl.isEmpty() && ((currentState & NotReady) == 0)) {
		stateFlags |= Playing;
		dirtyFlags |= (SourceChanged | ResetState);
	} else {
		stateFlags = ((currentState | stateFlags) & NotReady);
		dirtyFlags |= (PlaybackStopped | ResetState);
	}

	StateFlags difference = currentState ^ stateFlags;
	currentState = stateFlags;

	if ((difference & Playing) != 0) {
		dirtyFlags |= PlaybackChanged;
	}

	if ((difference & PlayingDvd) != 0) {
		dirtyFlags |= PlayingDvdChanged;
	}

	stateChanged();
}

void XineMediaWidget::stateChanged()
{
	while (dirtyFlags != 0) {
		int lowestDirtyFlag = dirtyFlags & (~(dirtyFlags - 1));
		dirtyFlags &= ~lowestDirtyFlag;

		switch (lowestDirtyFlag) {
		case ResetState:
			if (currentTime != 0) {
				currentTime = 0;
				dirtyFlags |= CurrentTimeChanged;
			}

			if (totalTime != 0) {
				totalTime = 0;
				dirtyFlags |= TotalTimeChanged;
			}

			if (!metadata.isEmpty()) {
				rawMetadata.clear();
				metadata.clear();
				dirtyFlags |= MetadataChanged;
			}

			if (seekable != isPlaying()) {
				seekable = isPlaying();
				dirtyFlags |= SeekableChanged;
			}

			if (!audioChannels.isEmpty()) {
				rawAudioChannels.clear();
				audioChannels.clear();
				dirtyFlags |= AudioChannelsChanged;
			}

			if (currentAudioChannel != -1) {
				currentAudioChannel = -1;
				dirtyFlags |= CurrentAudioChannelChanged;
			}

			if (!subtitles.isEmpty()) {
				rawSubtitles.clear();
				subtitles.clear();
				dirtyFlags |= SubtitlesChanged;
			}

			if (currentSubtitle != -1) {
				currentSubtitle = -1;
				dirtyFlags |= CurrentSubtitleChanged;
			}

			if ((currentState & PlayingDvd) == 0) {
				encodedDvdUrl.clear();
			}

			if (titleCount != 0) {
				titleCount = 0;
				dirtyFlags |= TitleCountChanged;
			}

			if (currentTitle != 0) {
				currentTitle = 0;
				dirtyFlags |= CurrentTitleChanged;
			}

			if (chapterCount != 0) {
				chapterCount = 0;
				dirtyFlags |= ChapterCountChanged;
			}

			if (currentChapter != 0) {
				currentChapter = 0;
				dirtyFlags |= CurrentChapterChanged;
			}

			if (angleCount != 0) {
				angleCount = 0;
				dirtyFlags |= AngleCountChanged;
			}

			if (currentAngle != 0) {
				currentAngle = 0;
				dirtyFlags |= CurrentAngleChanged;
			}

			unsetCursor();
			setMouseTracking(false);
			break;
		case SourceChanged:
			emit sourceChanged();
			break;
		case PlaybackFinished:
			emit playbackFinished();
			break;
		case PlaybackStopped:
			emit playbackStopped();
			break;
		case PlaybackChanged:
			if (isPlaying()) {
				show();
				emit playbackChanged(true);
			} else {
				hide();
				parentWidget()->update();
				emit playbackChanged(false);
			}

			break;
		case TotalTimeChanged:
			emit totalTimeChanged(totalTime);
			break;
		case CurrentTimeChanged:
			emit currentTimeChanged(currentTime);
			break;
		case MetadataChanged:
			emit metadataChanged(metadata);
			break;
		case SeekableChanged:
			emit seekableChanged(seekable);
			break;
		case AudioChannelsChanged:
			emit audioChannelsChanged(audioChannels, currentAudioChannel);
			dirtyFlags &= ~CurrentAudioChannelChanged;
			break;
		case CurrentAudioChannelChanged:
			emit currentAudioChannelChanged(currentAudioChannel);
			break;
		case SubtitlesChanged:
			emit subtitlesChanged(subtitles, currentSubtitle);
			dirtyFlags &= ~CurrentSubtitleChanged;
			break;
		case CurrentSubtitleChanged:
			emit currentSubtitleChanged(currentSubtitle);
			break;
		case PlayingDvdChanged:
			emit dvdPlaybackChanged((currentState & PlayingDvd) != 0);
			break;
		case TitleCountChanged:
			emit titlesChanged(titleCount, currentTitle);
			dirtyFlags &= ~CurrentTitleChanged;
			break;
		case CurrentTitleChanged:
			emit currentTitleChanged(currentTitle);
			break;
		case ChapterCountChanged:
			emit chaptersChanged(chapterCount, currentChapter);
			dirtyFlags &= ~CurrentChapterChanged;
			break;
		case CurrentChapterChanged:
			emit currentChapterChanged(currentChapter);
			break;
		case AngleCountChanged:
			emit anglesChanged(angleCount, currentAngle);
			dirtyFlags &= ~CurrentAngleChanged;
			break;
		case CurrentAngleChanged:
			emit currentAngleChanged(currentAngle);
			break;
		case VideoSizeChanged:
			updateGeometry();
			emit videoSizeChanged();
			break;
		default:
			kWarning() << "unknown flag" << lowestDirtyFlag;
			break;
		}
	}
}
