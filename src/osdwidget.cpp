/*
 * osdwidget.cpp
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#include "osdwidget.h"

#include <QPainter>
#include <QTimer>

OsdWidget::OsdWidget(QWidget *parent) : QWidget(parent)
{
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(hideOsd()));

	hide();
}

OsdWidget::~OsdWidget()
{
}

void OsdWidget::showText(const QString &text, int duration)
{
	QFont osdFont = font();
	osdFont.setPointSize(25);

	rect = QFontMetrics(osdFont).boundingRect(text);
	rect.moveTo(0, 0);
	rect.adjust(0, 0, 10, 0);

	pixmap = QPixmap(rect.size());

	{
		QPainter painter(&pixmap);
		painter.fillRect(rect, Qt::black);
		painter.setFont(osdFont);
		painter.setPen(Qt::white);
		painter.drawText(rect, Qt::AlignCenter, text);
	}

	update();
	show();

	timer->start(duration);
}

void OsdWidget::paintEvent(QPaintEvent *)
{
	if (layoutDirection() == Qt::LeftToRight) {
		QPainter(this).drawPixmap(20, 20, pixmap);
		rect.moveTo(20, 20);
	} else {
		int x = width() - pixmap.width() - 20;
		QPainter(this).drawPixmap(x, 20, pixmap);
		rect.moveTo(x, 20);
	}

	setMask(rect);
}

void OsdWidget::hideOsd()
{
	timer->stop();
	hide();
}
