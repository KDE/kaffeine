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
class AbstractMediaWidget;
class OsdWidget;
class SeekSlider;

class MediaSource
{
public:
	MediaSource() : type(Url) { }
	~MediaSource() { }

	enum Type
	{
		Url,
		AudioCd,
		VideoCd,
		Dvd
	};

	Type type;
	KUrl url;
	KUrl subtitleUrl;
};

class MediaWidget : public QWidget
{
	Q_OBJECT
public:
	MediaWidget(KMenu *menu_, KToolBar *toolBar, KActionCollection *collection,
		QWidget *parent);
	~MediaWidget();

	static QString extensionFilter(); // usable for KFileDialog::setFilter()

	enum AspectRatio
	{
		AspectRatioAuto,
		AspectRatio4_3,
		AspectRatio16_9,
		AspectRatioWidget
	};

	enum DisplayMode
	{
		NormalMode,
		FullScreenMode,
		MinimalMode
	};

	enum MetadataType
	{
		Title,
		Artist,
		Album,
		TrackNumber
	};

	enum PlaybackStatus
	{
		Idle,
		Playing,
		Paused
	};

	enum ResizeFactor
	{
		ResizeOff,
		OriginalSize,
		DoubleSize
	};

	enum Source
	{
		Playlist,
		AudioCd,
		VideoCd,
		Dvd,
		Dvb,
		DvbTimeShift
	};

	DisplayMode getDisplayMode() const;
	void setDisplayMode(DisplayMode displayMode_);

	/*
	 * loads the media and starts playback
	 */

	void play(Source source_, const KUrl &url, const KUrl &subtitleUrl);
	void play(const KUrl &url, const KUrl &subtitleUrl = KUrl());
	void playDvb(Source source_, const KUrl &url, const QString &channelName);
	void playAudioCd(const QString &device);
	void playVideoCd(const QString &device);
	void playDvd(const QString &device);
	void updateExternalSubtitles(const QList<KUrl> &subtitles, int currentSubtitle);

	OsdWidget *getOsdWidget();

	// empty list = use audio channels / subtitles provided by the backend
	void updateDvbAudioChannels(const QStringList &dvbAudioChannels_,
		int currentDvbAudioChannel_);
	void updateDvbSubtitles(const QStringList &dvbSubtitles_, int currentDvbSubtitle_);

	PlaybackStatus getPlaybackStatus() const;
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

public:
	void playbackFinished();
	void updatePlaybackStatus();
	void currentTotalTimeChanged();
	void updateMetadata();
	void updateSeekable();
	void updateAudioChannels();
	void updateSubtitles();
	void updateTitles();
	void updateChapters();
	void updateAngles();
	void updateDvdMenu();
	void updateVideoSize();

signals:
	void displayModeChanged();
	void changeCaption(const QString &caption);
	void resizeToVideo(MediaWidget::ResizeFactor resizeFactor);

	void playlistPrevious();
	void playlistPlay();
	void playlistNext();
	void playlistUrlsDropped(const QList<KUrl> &urls);
	void playlistTrackLengthChanged(int length);
	void playlistTrackMetadataChanged(
		const QMap<MediaWidget::MetadataType, QString> &metadata);

	void osdKeyPressed(int key);
	void dvbStopped();
	void dvbPrepareTimeShift();
	void dvbStartTimeShift();
	void dvbSetCurrentAudioChannel(int index);
	void dvbSetCurrentSubtitle(int index);
	void dvbPreviousChannel();
	void dvbNextChannel();

private slots:
	void checkScreenSaver();

	void mutedChanged();
	void volumeChanged(int volume);
	void seek(int position);
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
	void shortSkipDurationChanged(int shortSkipDuration);
	void longSkipDurationChanged(int longSkipDuration);

private:
	void updateSeekableUi();
	void updateAudioChannelUi();
	void updateSubtitleUi();

	void contextMenuEvent(QContextMenuEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void resizeEvent(QResizeEvent *event);
	void wheelEvent(QWheelEvent *event);

	KMenu *menu;
	AbstractMediaWidget *backend;
	OsdWidget *osdWidget;

	KAction *actionPrevious;
	KAction *actionPlayPause;
	QString textPlay;
	QString textPause;
	KIcon iconPlay;
	KIcon iconPause;
	KAction *actionStop;
	KAction *actionNext;
	KAction *fullScreenAction;
	KAction *minimalModeAction;
	KComboBox *audioChannelBox;
	KComboBox *subtitleBox;
	QString textSubtitlesOff;
	KAction *muteAction;
	KIcon mutedIcon;
	KIcon unmutedIcon;
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
	KAction *jumpToPositionAction;
	QPushButton *timeButton;

	DisplayMode displayMode;
	ResizeFactor automaticResize;
	Source source;
	bool blockBackendUpdates;
	bool muted;
	bool screenSaverSuspended;

	bool showElapsedTime;

	QStringList dvbAudioChannels;
	int currentDvbAudioChannel; // first audio channel = 0

	QStringList dvbSubtitles;
	QList<KUrl> externalSubtitles;
	int currentDvbSubtitle; // first subtitle = 0
	int currentExternalSubtitle; // first subtitle = 0
};

#endif /* MEDIAWIDGET_H */
