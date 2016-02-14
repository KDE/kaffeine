/*
 * osdwidget.cpp
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

OsdWidget::OsdWidget(QWidget *parent) : QWidget(parent), osdObject(NULL)
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

	rect = QRect(QPoint(0, 0), QFontMetrics(osdFont).size(Qt::AlignLeft, text));
	rect.adjust(0, 0, 10, 0);

	pixmap = QPixmap(rect.size());

	{
		QPainter painter(&pixmap);
		painter.fillRect(rect, Qt::black);
		painter.setFont(osdFont);
		painter.setPen(Qt::white);
		painter.drawText(rect.adjusted(5, 0, 0, 0), Qt::AlignLeft, text);
	}

	osdObject = NULL;
	update();
	show();

	timer->start(duration);
}

void OsdWidget::showObject(OsdObject *osdObject_, int duration)
{
	osdObject = osdObject_;
	update();
	show();

	if (duration >= 0) {
		timer->start(duration);
	} else {
		timer->stop();
	}
}

void OsdWidget::hideObject()
{
	if (osdObject != NULL) {
		osdObject = NULL;
		timer->stop();
		hide();
	}
}

void OsdWidget::paintEvent(QPaintEvent *)
{
	if (osdObject == NULL) {
		if (layoutDirection() == Qt::LeftToRight) {
			QPainter(this).drawPixmap(20, 20, pixmap);
			rect.moveTo(20, 20);
		} else {
			int x = width() - pixmap.width() - 20;
			QPainter(this).drawPixmap(x, 20, pixmap);
			rect.moveTo(x, 20);
		}
	} else {
		rect = QWidget::rect();
		rect.adjust(0, 0, -40, -60);
		pixmap = osdObject->paintOsd(rect, font(), layoutDirection());

		int y = (4 * height()) / 5 - rect.height();

		if (y < 30) {
			y = 30;
		}

		rect.moveTo(20, y);
		QPainter(this).drawPixmap(rect.left(), rect.top(), pixmap);
	}

	setMask(rect);
}

void OsdWidget::hideOsd()
{
	timer->stop();
	hide();
}
