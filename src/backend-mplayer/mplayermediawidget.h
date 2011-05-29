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

#include <QFile>
#include <QProcess>
#include "../abstractmediawidget.h"

class MPlayerMediaWidget : public AbstractMediaWidget
{
	Q_OBJECT
private:
	explicit MPlayerMediaWidget(QWidget *parent);

public:
	~MPlayerMediaWidget();

	// returns NULL if init fails
	static AbstractMediaWidget *createMPlayerMediaWidget(QWidget *parent);

	void setMuted(bool muted);
	void setVolume(int volume); // [0 - 200]
	void setAspectRatio(MediaWidget::AspectRatio aspectRatio);
	void setDeinterlacing(bool deinterlacing);
	void play(const MediaSource &source);
	void stop();
	void setPaused(bool paused);
	void seek(int time); // milliseconds
	void setCurrentAudioStream(int currentAudioStream);
	void setCurrentSubtitle(int currentSubtitle);
	void setExternalSubtitle(const KUrl &subtitleUrl);
	void setCurrentTitle(int currentTitle);
	void setCurrentChapter(int currentChapter);
	void setCurrentAngle(int currentAngle);
	bool jumpToPreviousChapter();
	bool jumpToNextChapter();
	void showDvdMenu();

	void mouseMoved(int x, int y);
	void mouseClicked();

private slots:
	void error(QProcess::ProcessError error);
	void readStandardOutput();
	void readStandardError();

private:
	enum Command
	{
		SetDeinterlacing,
		SetVolume,
		ShowDvdMenu,
		Stop,
		TogglePause,
		Quit
	};

	void resetState();
	void resizeEvent(QResizeEvent *event);
	void sendCommand(Command command);
	void timerEvent(QTimerEvent *event);
	void updateVideoWidgetGeometry();

	QWidget *videoWidget;
	QFile standardError;
	QProcess process;
	bool muted;
	int volume;
	MediaWidget::AspectRatio aspectRatio;
	bool deinterlacing;
	int timerId;
	QList<int> audioIds;
	int videoWidth;
	int videoHeight;
	float videoAspectRatio;
};

#endif /* MPLAYERMEDIAWIDGET_H */
