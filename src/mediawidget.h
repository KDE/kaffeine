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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef MEDIAWIDGET_H
#define MEDIAWIDGET_H

#include <QWidget>
#include <Phonon/Global>
#include <Phonon/ObjectDescription>
#include <KIcon>

namespace Phonon
{
class AudioOutput;
class MediaController;
class MediaObject;
}
class QPushButton;
class QSlider;
class KAction;
class KActionCollection;
class KComboBox;
class KToolBar;
class KUrl;
class DvbFeed;

class MediaWidget : public QWidget
{
	Q_OBJECT
public:
	MediaWidget(KToolBar *toolBar, KActionCollection *collection, QWidget *parent);
	~MediaWidget();

	/*
	 * loads the media and starts playback
	 */

	void play(const KUrl &url);
	void playAudioCd();
	void playVideoCd();
	void playDvd();

	/*
	 * start dvb mode
	 */

	void playDvb();

public slots:
	void writeDvbData(const QByteArray &data);
	void stopDvb();

signals:
	void changeCaption(const QString &caption);
	void toggleFullscreen();

	void playlistPrevious();
	void playlistPlay();
	void playlistNext();

	void prepareDvbTimeShift();
	void startDvbTimeShift();
	void dvbStopped();

private slots:
	void stateChanged(Phonon::State state);
	void playbackFinished();
	void previous();
	void playPause(bool paused);
	void stop();
	void next();
	void changeAudioChannel(int index);
	void changeSubtitle(int index);
	void toggleMuted();
	void mutedChanged(bool muted);
	void changeVolume(int volume);
	void volumeChanged(qreal volume);
	void increaseVolume();
	void decreaseVolume();
	void skipBackward();
	void skipForward();
	void timeButtonClicked();
	void updateCaption();
	void updateTimeButton();

	void titleCountChanged(int count);
	void chapterCountChanged(int count);
	void audioChannelsChanged();
	void subtitlesChanged();

private:
	void mouseDoubleClickEvent(QMouseEvent *);
	void updatePreviousNext();
	void updateAudioChannelBox();
	void updateSubtitleBox();

	Phonon::MediaObject *mediaObject;
	Phonon::AudioOutput *audioOutput;
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
	KAction *muteAction;
	KIcon mutedIcon;
	KIcon unmutedIcon;
	QSlider *volumeSlider;
	QPushButton *timeButton;
	bool showElapsedTime;

	int titleCount;
	int chapterCount;
	bool audioChannelsReady;
	bool subtitlesReady;
	QList<Phonon::AudioChannelDescription> audioChannels;
	QList<Phonon::SubtitleDescription> subtitles;
};

#endif /* MEDIAWIDGET_H */
