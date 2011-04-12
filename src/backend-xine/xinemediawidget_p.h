/*
 * xinemediawidget_p.h
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

#ifndef XINEMEDIAWIDGET_P_H
#define XINEMEDIAWIDGET_P_H

#include <QProcess>
#include "xinecommands.h"

class XineMediaWidget;

class XineProcess : public QProcess
{
	Q_OBJECT
public:
	explicit XineProcess(XineMediaWidget *parent_);
	~XineProcess();

	XineChildMarshaller *getChildProcess()
	{
		return &childProcess;
	}

private slots:
	void readyRead();

private:
	void setupChildProcess();

	XineMediaWidget *parent;
	int pipeToChild[2];
	int pipeFromChild[2];
	XinePipeReader *reader;
	XinePipeWriterBase *writer;
	XineChildMarshaller childProcess;
};

#endif /* XINEMEDIAWIDGET_P_H */
