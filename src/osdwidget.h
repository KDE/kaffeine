/*
 * osdwidget.h
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

#ifndef OSDWIDGET_H
#define OSDWIDGET_H

#include <QWidget>

class OsdObject
{
public:
	OsdObject() { }
	virtual ~OsdObject() { }

	virtual QPixmap paintOsd(QRect &rect, const QFont &font,
		Qt::LayoutDirection direction) = 0;
};

class OsdWidget : public QWidget
{
	Q_OBJECT
public:
	explicit OsdWidget(QWidget *parent);
	~OsdWidget();

	void showText(const QString &text, int duration); // duration: msecs

	void showObject(OsdObject *osdObject_, int duration); // duration: msecs
	void hideObject();

protected:
	void paintEvent(QPaintEvent *);

private slots:
	void hideOsd();

private:
	QRect rect;
	QPixmap pixmap;
	OsdObject *osdObject;
	QTimer *timer;
};

#endif /* OSDWIDGET_H */
