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
#include <QIcon>
#include <QUrl>

class QActionGroup;
class QPushButton;
class QSlider;
class QStringListModel;
class KAction;
class KActionCollection;
class KComboBox;
class KMenu;
class KToolBar;
class AbstractMediaWidget;
class MediaSource;
class OsdWidget;
class SeekSlider;

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
		FullScreenReturnToMinimalMode,
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

	DisplayMode getDisplayMode() const;
	void setDisplayMode(DisplayMode displayMode_);

	/*
	 * loads the media and starts playback
	 */

	void play(MediaSource *source_);
	void play(const QUrl &url, const KUrl &subtitleUrl = KUrl());
	void playAudioCd(const QString &device);
	void playVideoCd(const QString &device);
	void playDvd(const QString &device);

	OsdWidget *getOsdWidget();

	PlaybackStatus getPlaybackStatus() const;
	int getPosition() const; // milliseconds
	int getVolume() const; // 0 - 100

	void play(); // (re-)starts the current media
	void togglePause();
	void setPosition(int position); // milliseconds
	void setVolume(int volume); // 0 - 100
	void toggleMuted();
	void mediaSourceDestroyed(MediaSource *mediaSource);

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
	void playbackStatusChanged();
	void currentTotalTimeChanged();
	void metadataChanged();
	void seekableChanged();
	void audioStreamsChanged();
	void subtitlesChanged();
	void titlesChanged();
	void chaptersChanged();
	void anglesChanged();
	void dvdMenuChanged();
	void videoSizeChanged();

signals:
	void displayModeChanged();
	void changeCaption(const QString &caption);
	void resizeToVideo(MediaWidget::ResizeFactor resizeFactor);

	void playlistPrevious();
	void playlistNext();
	void playlistUrlsDropped(const QList<QUrl> &urls);
	void playlistTrackLengthChanged(int length);
	void playlistTrackMetadataChanged(const QMap<MediaWidget::MetadataType, QString> &metadata);
	void osdKeyPressed(int key);

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
	void currentAudioStreamChanged(int currentAudioStream);
	void currentSubtitleChanged(int currentSubtitle);
	void toggleMenu();
	void currentTitleChanged(QAction *action);
	void currentChapterChanged(QAction *action);
	void currentAngleChanged(QAction *action);
	void shortSkipDurationChanged(int shortSkipDuration);
	void longSkipDurationChanged(int longSkipDuration);

private:
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
	QIcon iconPlay;
	QIcon iconPause;
	KAction *actionStop;
	KAction *actionNext;
	KAction *fullScreenAction;
	KAction *minimalModeAction;
	KComboBox *audioStreamBox;
	KComboBox *subtitleBox;
	QStringListModel *audioStreamModel;
	QStringListModel *subtitleModel;
	QString textSubtitlesOff;
	KAction *muteAction;
	QIcon mutedIcon;
	QIcon unmutedIcon;
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
	QScopedPointer<MediaSource> dummySource;
	MediaSource *source;
	bool blockBackendUpdates;
	bool muted;
	bool screenSaverSuspended;
	bool showElapsedTime;
};

class MediaSource
{
public:
	MediaSource() { }

	virtual ~MediaSource()
	{
		setMediaWidget(NULL);
	}

	enum Type
	{
		Url,
		AudioCd,
		VideoCd,
		Dvd,
		Dvb
	};

	virtual Type getType() const { return Url; }
	virtual QUrl getUrl() const { return KUrl(); }
	virtual bool hideCurrentTotalTime() const { return false; }
	virtual bool overrideAudioStreams() const { return false; }
	virtual bool overrideSubtitles() const { return false; }
	virtual QStringList getAudioStreams() const { return QStringList(); }
	virtual QStringList getSubtitles() const { return QStringList(); }
	virtual int getCurrentAudioStream() const { return -1; }
	virtual int getCurrentSubtitle() const { return -1; }
	virtual bool overrideCaption() const { return false; }
	virtual QString getDefaultCaption() const { return QString(); }
	virtual void setCurrentAudioStream(int ) { }
	virtual void setCurrentSubtitle(int ) { }
	virtual void trackLengthChanged(int ) { }
	virtual void metadataChanged(const QMap<MediaWidget::MetadataType, QString> &) { }
	virtual void playbackFinished() { }
	virtual void playbackStatusChanged(MediaWidget::PlaybackStatus ) { }
	virtual void replay() { }
	virtual void previous() { }
	virtual void next() { }

	void setMediaWidget(MediaWidget *mediaWidget)
	{
		MediaWidget *oldMediaWidget = weakMediaWidget.data();

		if (mediaWidget != oldMediaWidget) {
			if (oldMediaWidget != NULL) {
				oldMediaWidget->mediaSourceDestroyed(this);
			}

			weakMediaWidget = mediaWidget;
		}
	}

private:
	QWeakPointer<MediaWidget> weakMediaWidget;
};

#endif /* MEDIAWIDGET_H */
