/*
 * mplayervideowidget.cpp
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

#include "mplayervideowidget.h"

#include <QMouseEvent>
#include "mplayermediawidget.h"

MPlayerVideoWidget::MPlayerVideoWidget(MPlayerMediaWidget *mediaWidget_) : QWidget(mediaWidget_),
	mediaWidget(mediaWidget_)
{
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_PaintOnScreen);
	setMouseTracking(true);
}

MPlayerVideoWidget::~MPlayerVideoWidget()
{
}

void MPlayerVideoWidget::mouseMoveEvent(QMouseEvent *event)
{
	mediaWidget->mouseMoved(event->x(), event->y());
	QWidget::mouseMoveEvent(event);
}

void MPlayerVideoWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		mediaWidget->mouseMoved(event->x(), event->y());
		mediaWidget->mouseClicked();
	}

	QWidget::mousePressEvent(event);
}
