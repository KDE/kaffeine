/*
 * mplayermediawidget.h
 *
 * Copyright (C) 2010-2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef MPLAYERMEDIAWIDGET_H
#define MPLAYERMEDIAWIDGET_H

#include "../abstractmediawidget.h"

#include <QFile>
#include <QProcess>

class MPlayerMediaWidget : public AbstractMediaWidget
{
	Q_OBJECT
	friend class MPlayerVideoWidget;
private:
	explicit MPlayerMediaWidget(QWidget *parent);

public:
	~MPlayerMediaWidget();

	static MPlayerMediaWidget *createMPlayerMediaWidget(QWidget *parent); // returns NULL if init fails

	void setMuted(bool muted);
	void setVolume(int volume); // [0 - 200]
	void setAspectRatio(MediaWidget::AspectRatio aspectRatio);
	void setDeinterlacing(bool deinterlacing);
	void play(const MediaSource &source);
	void stop();
	void setPaused(bool paused);
	void seek(int time); // milliseconds
	void setCurrentAudioChannel(int currentAudioChannel);
	void setCurrentSubtitle(int currentSubtitle);
	void setExternalSubtitle(const KUrl &subtitleUrl);
	void setCurrentTitle(int currentTitle);
	void setCurrentChapter(int currentChapter);
	void setCurrentAngle(int currentAngle);
	bool jumpToPreviousChapter();
	bool jumpToNextChapter();
	void showDvdMenu();

private slots:
	void error(QProcess::ProcessError error);
	void readStandardOutput();
	void readStandardError();

private:
	enum Command
	{
		SetVolume,
		ShowDvdMenu,
		Stop,
		TogglePause,
		Quit
	};

	void mouseMoved(int x, int y);
	void mouseClicked();
	void resizeEvent(QResizeEvent *event);
	void sendCommand(Command command);
	void updateVideoWidgetGeometry();

	QWidget *videoWidget;
	QProcess process;
	QFile standardError;
	bool muted;
	int volume;
	MediaWidget::AspectRatio aspectRatio;
	int videoWidth;
	int videoHeight;
	float videoAspectRatio;
};

#endif /* MPLAYERMEDIAWIDGET_H */
