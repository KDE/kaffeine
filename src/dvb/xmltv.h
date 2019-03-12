/*
 * xmltv.h
 *
 * Copyright (C) 2019 Mauro Carvalho Chehab <mchehab+samsung@kernel.org>
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
 */

#ifndef XMLTV_H
#define XMLTV_H

#include <QFileSystemWatcher>
#include <QThread>

class DvbChannelModel;
class DvbManager;
class QXmlStreamReader;

class XmlTv : public QObject
{
Q_OBJECT

private:
	DvbManager *manager;
	DvbChannelModel *channelModel;
	DvbEpgModel *epgModel;
	QXmlStreamReader *r;

	// Maps display name into XmlTV channel name
	QHash<QString, QList<QString>> channelMap;

	bool parseChannel(void);
	bool parseProgram(void);
	void ignoreTag(void);
	void parseKeyValues(QHash<QString, QString> &keyValues);
	QString getValue(QHash<QString, QString> &keyValues, QString key);
	QString getAllValues(QHash<QString, QString> &keyValues);
	DvbEpgLangEntry *getLangEntry(DvbEpgEntry &epgEntry,
				      QString &code);

	QFileSystemWatcher watcher;

private slots:
	bool load(QString file);

public:
	XmlTv(DvbManager *manager);
	void addFile(QString file);
	void clear();
};

#endif
