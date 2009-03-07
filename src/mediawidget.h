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

#include <QPointer>
#include <QWidget>
#include <Phonon/Global>
#include <Phonon/ObjectDescription>
#include <KIcon>

namespace Phonon
{
class MediaController;
class MediaObject;
}
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
	void toggleFullscreen();
	void dvbStopped();

private slots:
	void stateChanged(Phonon::State state);
	void previous();
	void playPause(bool paused);
	void stop();
	void next();
	void changeAudioChannel(int index);
	void changeSubtitle(int index);

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
	Phonon::MediaController *mediaController;
	QPointer<DvbFeed> dvbFeed;
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

	int titleCount;
	int chapterCount;
	bool audioChannelsReady;
	bool subtitlesReady;
	QList<Phonon::AudioChannelDescription> audioChannels;
	QList<Phonon::SubtitleDescription> subtitles;
};

#endif /* MEDIAWIDGET_H */
