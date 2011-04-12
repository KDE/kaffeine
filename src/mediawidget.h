/*
 * mediawidget.h
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef MEDIAWIDGET_H
#define MEDIAWIDGET_H

#include <QWidget>
#include <KIcon>
#include <KUrl>

class QActionGroup;
class QPushButton;
class QSlider;
class KAction;
class KActionCollection;
class KComboBox;
class KMenu;
class KToolBar;
class DvbFeed;
class OsdWidget;
class SeekSlider;
class XineMediaWidget;

class MediaWidget : public QWidget
{
	Q_OBJECT
public:
	MediaWidget(KMenu *menu_, KToolBar *toolBar, KActionCollection *collection,
		QWidget *parent);
	~MediaWidget();

	static QString extensionFilter(); // usable for KFileDialog::setFilter()

	enum AspectRatio {
		AspectRatioAuto,
		AspectRatio4_3,
		AspectRatio16_9,
		AspectRatioWidget
	};

	enum DisplayMode {
		NormalMode,
		FullScreenMode,
		MinimalMode
	};

	enum MetadataType {
		Title,
		Artist,
		Album,
		TrackNumber
	};

	DisplayMode getDisplayMode() const;
	void setDisplayMode(DisplayMode displayMode_);

	/*
	 * loads the media and starts playback
	 */

	void play(const KUrl &url, const KUrl &subtitleUrl = KUrl());
	void playAudioCd(const QString &device);
	void playVideoCd(const QString &device);
	void playDvd(const QString &device);
	void updateExternalSubtitles(const QList<KUrl> &subtitles, int currentSubtitle);

	OsdWidget *getOsdWidget();

	void playDvb(const QString &channelName); // starts dvb mode
	void writeDvbData(const QByteArray &data);

	// empty list = use audio channels / subtitles provided by phonon
	void updateDvbAudioChannels(const QStringList &audioChannels, int currentAudioChannel);
	void updateDvbSubtitles(const QStringList &subtitles, int currentSubtitle);

	int getShortSkipDuration() const;
	int getLongSkipDuration() const;
	void setShortSkipDuration(int duration);
	void setLongSkipDuration(int duration);

	bool isPlaying() const;
	bool isPaused() const;
	int getPosition() const; // milliseconds
	int getVolume() const; // 0 - 100

	void play(); // (re-)starts the current media
	void togglePause();
	void setPosition(int position); // milliseconds
	void setVolume(int volume); // 0 - 100
	void toggleMuted();

public slots:
	void previous();
	void next();
	void stop();
	void increaseVolume();
	void decreaseVolume();
	void toggleFullScreen();
	void toggleMinimalMode();
	void shortSkipBackward();
	void shortSkipForward();
	void longSkipBackward();
	void longSkipForward();

signals:
	void displayModeChanged();
	void changeCaption(const QString &caption);
	void resizeToVideo(int factor);

	void playlistPrevious();
	void playlistPlay();
	void playlistNext();
	void playlistUrlsDropped(const QList<KUrl> &urls);
	void playlistTrackLengthChanged(int length);
	void playlistTrackMetadataChanged(
		const QMap<MediaWidget::MetadataType, QString> &metadata);

	void previousDvbChannel();
	void nextDvbChannel();
	void prepareDvbTimeShift();
	void startDvbTimeShift();
	void changeDvbAudioChannel(int index);
	void changeDvbSubtitle(int index);
	void dvbStopped();
	void osdKeyPressed(int key);

private slots:
	void sourceChanged();
	void playbackFinished();
	void playbackStopped();
	void playbackChanged(bool playing);
	void totalTimeChanged(int totalTime);
	void currentTimeChanged(int currentTime);
	void setMetadata(const QMap<MediaWidget::MetadataType, QString> &metadata);
	void seekableChanged(bool seekable);
	void audioChannelsChanged(const QStringList &audioChannels, int currentAudioChannel);
	void setCurrentAudioChannel(int currentAudioChannel);
	void subtitlesChanged(const QStringList &subtitles, int currentSubtitle);
	void setCurrentSubtitle(int currentSubtitle);
	void dvdPlaybackChanged(bool playingDvd);
	void titlesChanged(int titleCount, int currentTitle);
	void setCurrentTitle(int currentTitle);
	void chaptersChanged(int chapterCount, int currentChapter);
	void setCurrentChapter(int currentChapter);
	void anglesChanged(int angleCount, int currentAngle);
	void setCurrentAngle(int currentAngle);
	void videoSizeChanged();
	void checkScreenSaver();

	void mutedChanged();
	void volumeChanged(int volume);
	void deinterlacingChanged(bool deinterlacing);
	void aspectRatioChanged(QAction *action);
	void autoResizeTriggered(QAction *action);
	void pausedChanged(bool paused);
	void timeButtonClicked();
	void jumpToPosition();
	void currentAudioChannelChanged(int currentAudioChannel);
	void currentSubtitleChanged(int currentSubtitle);
	void toggleMenu();
	void currentTitleChanged(QAction *action);
	void currentChapterChanged(QAction *action);
	void currentAngleChanged(QAction *action);

private:
	void updateTimeButton();
	void stopDvbPlayback();

	void contextMenuEvent(QContextMenuEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void resizeEvent(QResizeEvent *event);
	void wheelEvent(QWheelEvent *event);

	KMenu *menu;
	XineMediaWidget *backend;
	OsdWidget *osdWidget;
	DvbFeed *dvbFeed;

	QString currentSourceName;
	KAction *actionPrevious;
	KAction *actionPlayPause;
	QString textPlay;
	QString textPause;
	KIcon iconPlay;
	KIcon iconPause;
	KAction *actionStop;
	KAction *actionNext;
	DisplayMode displayMode;
	KAction *fullScreenAction;
	KAction *minimalModeAction;
	KComboBox *audioChannelBox;
	KComboBox *subtitleBox;
	QString textSubtitlesOff;
	QList<KUrl> externalSubtitles;
	KUrl currentExternalSubtitle;
	bool audioChannelsReady;
	bool subtitlesReady;
	int autoResizeFactor;
	KAction *muteAction;
	KIcon mutedIcon;
	KIcon unmutedIcon;
	bool isMuted;
	QSlider *volumeSlider;
	SeekSlider *seekSlider;
	KAction *longSkipBackwardAction;
	KAction *shortSkipBackwardAction;
	KAction *shortSkipForwardAction;
	KAction *longSkipForwardAction;
	KAction *deinterlaceAction;
	KAction *menuAction;
	KMenu *titleMenu;
	KMenu *chapterMenu;
	KMenu *angleMenu;
	QActionGroup *titleGroup;
	QActionGroup *chapterGroup;
	QActionGroup *angleGroup;
	KMenu *navigationMenu;
	int shortSkipDuration;
	int longSkipDuration;
	KAction *jumpToPositionAction;
	QPushButton *timeButton;
	bool showElapsedTime;
	bool screenSaverSuspended;
};

#endif /* MEDIAWIDGET_H */
