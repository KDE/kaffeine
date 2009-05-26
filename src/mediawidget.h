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
#include <Phonon/Global>
#include <Phonon/ObjectDescription>
#include <KIcon>

class QActionGroup;
class QPushButton;
class QSlider;
namespace Phonon
{
class AudioOutput;
class MediaController;
class MediaObject;
class VideoWidget;
}
class KAction;
class KActionCollection;
class KComboBox;
class KMenu;
class KToolBar;
class KUrl;
class DvbFeed;
class OsdWidget;

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

	void playDvb(const QString &channelName); // starts dvb mode
	void writeDvbData(const QByteArray &data);

	// empty list = use audio channels / subtitles provided by phonon
	void updateDvbAudioChannels(const QStringList &audioChannels, int currentIndex);
	void updateDvbSubtitles(const QStringList &subtitles, int currentIndex);

public slots:
	void stop();
	void stopDvb();

signals:
	void changeCaption(const QString &caption);
	void toggleFullScreen();

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

private slots:
	void stateChanged(Phonon::State state);
	void playbackFinished();
	void previous();
	void playPause(bool paused);
	void next();
	void changeAudioChannel(int index);
	void changeSubtitle(int index);
	void toggleMuted();
	void mutedChanged(bool muted);
	void changeVolume(int volume);
	void volumeChanged(qreal volume);
	void increaseVolume();
	void decreaseVolume();
	void aspectRatioAuto();
	void aspectRatio4_3();
	void aspectRatio16_9();
	void aspectRatioWidget();
	void updateTitleMenu();
	void updateChapterMenu();
	void updateAngleMenu();
	void changeTitle(QAction *action);
	void changeChapter(QAction *action);
	void changeAngle(QAction *action);
	void updateSeekable();
	void longSkipBackward();
	void skipBackward();
	void skipForward();
	void longSkipForward();
	void jumpToPosition();
	void timeButtonClicked();
	void updateTimeButton();
	void updateCaption();

	void titleCountChanged(int count);
	void chapterCountChanged(int count);
	void angleCountChanged(int count);
	void audioChannelsChanged();
	void subtitlesChanged();

	void checkScreenSaver();

private:
	void contextMenuEvent(QContextMenuEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
	void resizeEvent(QResizeEvent *event);
	void wheelEvent(QWheelEvent *event);

	void updateAudioChannelBox();
	void updateSubtitleBox();

	KMenu *menu;
	Phonon::MediaObject *mediaObject;
	Phonon::AudioOutput *audioOutput;
	Phonon::VideoWidget *videoWidget;
	OsdWidget *osdWidget;
	Phonon::MediaController *mediaController;
	DvbFeed *dvbFeed;
	bool playing;

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
	QList<Phonon::AudioChannelDescription> audioChannels;
	QList<Phonon::SubtitleDescription> subtitles;
	KAction *muteAction;
	KIcon mutedIcon;
	KIcon unmutedIcon;
	QSlider *volumeSlider;
	KMenu *titleMenu;
	KMenu *chapterMenu;
	KMenu *angleMenu;
	QActionGroup *titleGroup;
	QActionGroup *chapterGroup;
	QActionGroup *angleGroup;
	int titleCount;
	int chapterCount;
	int angleCount;
	KMenu *navigationMenu;
	KAction *jumpToPositionAction;
	QPushButton *timeButton;
	bool showElapsedTime;
};

#endif /* MEDIAWIDGET_H */
