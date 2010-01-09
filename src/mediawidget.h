/*
 * mediawidget.h
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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

class QActionGroup;
class QPushButton;
class QSlider;
class KAction;
class KActionCollection;
class KComboBox;
class KMenu;
class KToolBar;
class KUrl;
class DvbFeed;
class OsdWidget;
class SeekSlider;
class XineMediaWidget;

class MediaWidget : public QWidget
{
	Q_OBJECT
public:
	MediaWidget(KMenu *menu_, KAction *fullScreenAction, KToolBar *toolBar,
		KActionCollection *collection, QWidget *parent);
	~MediaWidget();

	static QString extensionFilter(); // usable for KFileDialog::setFilter()

	/*
	 * loads the media and starts playback
	 */

	void play(const KUrl &url);
	void playAudioCd();
	void playVideoCd();
	void playDvd();

	OsdWidget *getOsdWidget();

	void playDvb(const QString &channelName); // starts dvb mode
	void writeDvbData(const QByteArray &data);

	// empty list = use audio channels / subtitles provided by phonon
	void updateDvbAudioChannels(const QStringList &audioChannels, int currentIndex);
	void updateDvbSubtitles(const QStringList &subtitles, int currentIndex);

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

signals:
	void changeCaption(const QString &caption);
	void toggleFullScreen();
	void resizeToVideo(int factor);

	void playlistPrevious();
	void playlistPlay();
	void playlistNext();
	void playlistUrlsDropped(const QList<KUrl> &urls);

	void previousDvbChannel();
	void nextDvbChannel();
	void prepareDvbTimeShift();
	void startDvbTimeShift();
	void changeDvbAudioChannel(int index);
	void changeDvbSubtitle(int index);
	void dvbStopped();
	void osdKeyPressed(int key);

private slots:
	void playbackChanged(bool playing);
	void seekableChanged(bool seekable);
	void totalTimeChanged(int totalTime);
	void currentTimeChanged(int currentTime);
	void metadataChanged(); // FIXME
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
	void dvbPlaybackFinished();
	void playbackFinished();
	void checkScreenSaver();

	void mutedChanged();
	void volumeChanged(int volume);
	void aspectRatioChanged(QAction *action);
	void autoResizeTriggered(QAction *action);
	void pausedChanged(bool paused);
	void timeButtonClicked();
	void longSkipBackward();
	void shortSkipBackward();
	void shortSkipForward();
	void longSkipForward();
	void jumpToPosition();
	void currentAudioChannelChanged(int currentAudioChannel);
	void currentSubtitleChanged(int currentSubtitle);
	void currentTitleChanged(QAction *action);
	void currentChapterChanged(QAction *action);
	void currentAngleChanged(QAction *action);

private:
	void updateTimeButton(int currentTime);

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

	KAction *actionPrevious;
	KAction *actionPlayPause;
	QString textPlay;
	QString textPause;
	KIcon iconPlay;
	KIcon iconPause;
	KAction *actionStop;
	KAction *actionNext;
	KComboBox *audioChannelBox;
	KComboBox *subtitleBox;
	QString textSubtitlesOff;
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
};

#endif /* MEDIAWIDGET_H */
