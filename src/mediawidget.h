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

class DvbLiveFeed; // defined in engine.h
class Engine;
class Manager;

class MediaWidget : public QWidget
{
	Q_OBJECT
public:
	MediaWidget(Manager *manager_);
	~MediaWidget() { }

	QWidget *newPositionSlider();
	QWidget *newVolumeSlider();

	void setFullscreen(bool fullscreen);
	void setPaused(bool paused);

	/*
	 * loads the media and starts playback
	 */

	void play(const KUrl &url);
	void playAudioCd();
	void playVideoCd();
	void playDvd();

	/*
	 * ownership isn't taken; you have to delete it
	 * manually after DvbStream::stop() has been called
	 */

	void playDvb(DvbLiveFeed *feed);

	void switchEngine(Engine *newEngine);

public slots:
	void stop();

private slots:
	void playbackFinished();
	void playbackFailed(const QString &errorMessage);

private:
	void mouseDoubleClickEvent(QMouseEvent *);

	Manager *manager;
	Engine *engine;
};

#endif /* MEDIAWIDGET_H */
