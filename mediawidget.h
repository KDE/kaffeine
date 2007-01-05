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

#include <config.h>

#include <QWidget>

#include <phonon/videopath.h>
#include <phonon/audiooutput.h>
#include <phonon/audiopath.h>
#include <phonon/mediaobject.h>
#include <phonon/ui/videowidget.h>
#include <phonon/ui/mediacontrols.h>
#include <kurl.h>

using namespace Phonon;

class MediaWidget : public QWidget
{
	Q_OBJECT
	
public:
	MediaWidget();
	~MediaWidget() { }
	void play( const KUrl &url );
	
public slots:
	void play();
	void togglePause( bool b );
	void stop();
	void setVolume( int val );
	
signals:
	void volumeHasChanged( int value );
	
private:
	VideoWidget *vw;
	VideoPath *vp;
	AudioOutput *ao;
	AudioPath *ap;
	MediaObject *media;
	
	KUrl currentUrl;
	
private slots:
	void volumeChanged( float val );
	void playbackFinished();

};

#endif /* MEDIAWIDGET_H */
