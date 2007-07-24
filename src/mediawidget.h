/*
 * mediawidget.h
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#include <KUrl>

#include <Phonon/Global>

#include "manager.h"

namespace Phonon
{
class AudioOutput;
class AudioPath;
class MediaObject;
class VideoPath;
class VideoWidget;
};

class MediaWidget : public QWidget
{
	Q_OBJECT
	
public:
	MediaWidget(Manager *manager_);
	~MediaWidget() { }
	void play( const KUrl &url );

	QWidget *newPositionSlider();
	QWidget *newVolumeSlider();

public slots:
	void play();
	void playAudioCd();
	void playVideoCd();
	void playDvd();
	void togglePause( bool b );
	void stop();

private:
	void mouseDoubleClickEvent(QMouseEvent *);

private:
	Phonon::VideoWidget *vw;
	Phonon::VideoPath *vp;
	Phonon::AudioOutput *ao;
	Phonon::AudioPath *ap;
	Phonon::MediaObject *media;

	KUrl currentUrl;

	Manager *manager;

private slots:
//	void newLength( qint64 );
	void playbackFinished();
	void stateChanged( Phonon::State, Phonon::State );

};

#endif /* MEDIAWIDGET_H */
