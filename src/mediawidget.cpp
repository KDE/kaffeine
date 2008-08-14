/*
 * mediawidget.cpp
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

#include "mediawidget.h"

#include <QHBoxLayout>
#include <KMessageBox>
#include "engine_phonon.h"
#include "kaffeine.h"
#include "manager.h"

MediaWidget::MediaWidget(Manager *manager_) : manager(manager_), engine(NULL)
{
	QBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
	setLayout(layout);

	switchEngine(new EnginePhonon(this));
}

QWidget *MediaWidget::newPositionSlider()
{
	return engine->positionSlider();
}

QWidget *MediaWidget::newVolumeSlider()
{
	return engine->volumeSlider();
}

void MediaWidget::setFullscreen(bool fullscreen)
{
	if (fullscreen) {
		setWindowFlags((windowFlags() & (~Qt::WindowType_Mask)) | Qt::Dialog);
		showFullScreen();
	} else {
		setWindowFlags(windowFlags() & (~Qt::WindowType_Mask));
	}
}

void MediaWidget::setPaused(bool paused)
{
	engine->setPaused(paused);
}

void MediaWidget::play(const KUrl &url)
{
	engine->play(url);
	manager->setPlaying();
}

void MediaWidget::playAudioCd()
{
	engine->playAudioCd();
	manager->setPlaying();
}

void MediaWidget::playVideoCd()
{
	engine->playVideoCd();
	manager->setPlaying();
}

void MediaWidget::playDvd()
{
	engine->playDvd();
	manager->setPlaying();
}

void MediaWidget::playDvb(DvbLiveFeed *feed)
{
	engine->playDvb(feed);
	manager->setPlaying();
}

void MediaWidget::switchEngine(Engine *newEngine)
{
	delete engine;

	engine = newEngine;
	layout()->addWidget(engine->videoWidget());

	connect(engine, SIGNAL(playbackFinished()), this, SLOT(playbackFinished()));
	connect(engine, SIGNAL(playbackFailed(QString)), this, SLOT(playbackFailed(QString)));
}

void MediaWidget::stop()
{
	engine->stop();
}

void MediaWidget::playbackFinished()
{
	manager->setStopped();
}

void MediaWidget::playbackFailed(const QString &errorMessage)
{
	manager->setStopped();
	KMessageBox::error(manager->getKaffeine(), errorMessage);
}

void MediaWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	manager->toggleFullscreen();
}
