/*
 * xinemediawidget.h
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

#ifndef XINEMEDIAWIDGET_H
#define XINEMEDIAWIDGET_H

#include <QWidget>

class KUrl;
class XineChildMarshaller;
class XineProcess;

class XineMediaWidget : public QWidget
{
	friend class XineProcess;
	Q_OBJECT
public:
	explicit XineMediaWidget(QWidget *parent);
	~XineMediaWidget();

	enum AspectRatio {
		AspectRatioAuto,
		AspectRatio4_3,
		AspectRatio16_9,
		AspectRatioWidget
	};

	void setMuted(bool muted);
	void setVolume(int volume); // [0 - 100]
	void setAspectRatio(AspectRatio aspectRatio);

	void playUrl(const KUrl &url);
	void playAudioCd(const QString &device);
	void playVideoCd(const QString &device);
	void playDvd(const QString &device);
	void playDvb(const KUrl &url);
	void stop();

	bool isPlaying() const;
	int getCurrentTime() const; // milliseconds
	int getTotalTime() const; // milliseconds
	QStringList getAudioChannels() const;
	QStringList getSubtitles() const;
	int getCurrentAudioChannel() const; // first audio channel = 0
	int getCurrentSubtitle() const; // first subtitle = 0

	void setPaused(bool paused);
	void setCurrentAudioChannel(int currentAudioChannel_);
	void setCurrentSubtitle(int currentSubtitle_);
	void toggleMenu();
	void setCurrentTitle(int currentTitle_); // first title = 1
	void setCurrentChapter(int currentChapter_); // first chapter = 1
	void setCurrentAngle(int currentAngle_); // first angle = 1
	bool playPreviousTitle();
	bool playNextTitle();

public slots:
	void seek(int time);

signals:
	void playbackChanged(bool playing);
	void seekableChanged(bool seekable);
	void totalTimeChanged(int totalTime);
	void currentTimeChanged(int currentTime);
	void metadataChanged(); // FIXME
	void audioChannelsChanged(const QStringList &audioChannels, int currentAudioChannel);
	void currentAudioChannelChanged(int currentAudioChannel);
	void subtitlesChanged(const QStringList &subtitles, int currentSubtitle);
	void currentSubtitleChanged(int currentSubtitle);
	void dvdPlaybackChanged(bool playingDvd);
	void titlesChanged(int titleCount, int currentTitle);
	void currentTitleChanged(int currentTitle);
	void chaptersChanged(int chapterCount, int currentChapter);
	void currentChapterChanged(int currentChapter);
	void anglesChanged(int angleCount, int currentAngle);
	void currentAngleChanged(int currentAngle);
	void dvbPlaybackFinished();
	void playbackFinished();

public:
	enum StateFlag {
		SourceChanged			= (1 <<  0),
		Playing				= (1 <<  1),
		Seekable			= (1 <<  2),
		TotalTimeChanged		= (1 <<  3),
		CurrentTimeChanged		= (1 <<  4),
		AudioChannelsChanged		= (1 <<  5),
		CurrentAudioChannelChanged	= (1 <<  6),
		SubtitlesChanged		= (1 <<  7),
		CurrentSubtitleChanged		= (1 <<  8),
		PlayingDvd			= (1 <<  9),
		TitleCountChanged		= (1 << 10),
		CurrentTitleChanged		= (1 << 11),
		ChapterCountChanged		= (1 << 12),
		CurrentChapterChanged		= (1 << 13),
		AngleCountChanged		= (1 << 14),
		CurrentAngleChanged		= (1 << 15),
		PlayingDvb			= (1 << 16),
		PlaybackFinished		= (1 << 17)
	};

	Q_DECLARE_FLAGS(StateFlags, StateFlag)

private:
	enum PlayOption {
		Normal,
		PlayDvd,
		PlayDvb
	};

	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void paintEvent(QPaintEvent *event);
	void resizeEvent(QResizeEvent *event);

	void initFailed(const QString &errorMessage);
	void sync(unsigned int sequenceNumber_);
	void playbackFailed(const QString &errorMessage);
	void playbackFinishedInternal();
	void updateSeekable(bool seekable);
	void updateCurrentTotalTime(int currentTime_, int totalTime_);
	void updateAudioChannels(const QByteArray &audioChannels_, int currentAudioChannel_);
	void updateSubtitles(const QByteArray &subtitles_, int currentSubtitle_);
	void updateTitles(int titleCount_, int currentTitle_);
	void updateChapters(int chapterCount_, int currentChapter_);
	void updateAngles(int angleCount_, int currentAngle_);
	void updateMouseTracking(bool mouseTrackingEnabled);
	void updateMouseCursor(bool pointingMouseCursor);

	void playEncodedUrl(const QByteArray &encodedUrl, PlayOption option = Normal);
	void stateChanged();
	void customEvent(QEvent *event);

	XineProcess *process;
	XineChildMarshaller *childProcess;
	unsigned int sequenceNumber;
	bool ready;
	bool eventPosted;
	StateFlags previousState;
	StateFlags currentState;
	QByteArray encodedDvdUrl;
	bool synchronized;
	int currentTime;
	int totalTime;
	QByteArray rawAudioChannels;
	QStringList audioChannels;
	int currentAudioChannel;
	QByteArray rawSubtitles;
	QStringList subtitles;
	int currentSubtitle;
	int titleCount;
	int currentTitle;
	int chapterCount;
	int currentChapter;
	int angleCount;
	int currentAngle;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(XineMediaWidget::StateFlags)

#endif /* XINEMEDIAWIDGET_H */
