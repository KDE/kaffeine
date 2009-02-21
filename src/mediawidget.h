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
#include <KIcon>

namespace Phonon
{
class AbstractMediaStream;
class MediaController;
class MediaObject;
}
class KAction;
class KActionCollection;
class KToolBar;
class KUrl;

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
	 * you stop dvb playback by deleting the feed
	 * the feed is deleted by MediaWidget if the user stops playback
	 */

	void playDvb(Phonon::AbstractMediaStream *feed);

signals:
	void toggleFullscreen();

private slots:
	void stateChanged(Phonon::State state);
	void previous();
	void playPause(bool paused);
	void stop();
	void next();

	void titleCountChanged(int count);
	void chapterCountChanged(int count);

private:
	void mouseDoubleClickEvent(QMouseEvent *);
	void updatePreviousNext();

	Phonon::MediaObject *mediaObject;
	Phonon::MediaController *mediaController;
	QPointer<Phonon::AbstractMediaStream> dvbFeed;
	bool playing;

	KAction *actionPrevious;
	KAction *actionPlayPause;
	KAction *actionStop;
	KAction *actionNext;

	QString textPlay;
	QString textPause;
	KIcon iconPlay;
	KIcon iconPause;

	int titleCount;
	int chapterCount;
};

#endif /* MEDIAWIDGET_H */
