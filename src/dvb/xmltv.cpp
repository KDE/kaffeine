/*
 * xmltv.cpp
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

#include "log.h"

#include <KLocalizedString>
#include <QFile>
#include <QLocale>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include "dvbchannel.h"
#include "dvbepg.h"
#include "dvbmanager.h"
#include "iso-codes.h"
#include "xmltv.h"

#include <QEventLoop>

XmlTv::XmlTv(DvbManager *manager_) : manager(manager_), r(NULL)
{
	channelModel = manager->getChannelModel();
	epgModel = manager->getEpgModel();

	connect(&watcher, &QFileSystemWatcher::fileChanged,
		this, &XmlTv::load);

//	exec();
};

void XmlTv::addFile(QString file)
{
	if (file == "")
		return;

	load(file);

	watcher.addPath(file);
};

void XmlTv::clear()
{
	if (watcher.files().empty())
		return;
	channelMap.clear();
	watcher.removePaths(watcher.files());
};


// This function is very close to the one at dvbepg.cpp
DvbEpgLangEntry *XmlTv::getLangEntry(DvbEpgEntry &epgEntry,
				     QString &code)
{
	DvbEpgLangEntry *langEntry;

	if (!epgEntry.langEntry.contains(code)) {
		DvbEpgLangEntry e;
		epgEntry.langEntry.insert(code, e);
		if (!manager->languageCodes.contains(code)) {
			manager->languageCodes[code] = true;
			emit epgModel->languageAdded(code);
		}
	}
	langEntry = &epgEntry.langEntry[code];

	return langEntry;
}

bool XmlTv::parseChannel(void)
{
	const QString emptyString("");
	QStringRef empty(&emptyString);

	const QXmlStreamAttributes attrs = r->attributes();
	QStringRef channelName = attrs.value("id");
	QList<QString>list;

	while (!r->atEnd()) {
		const QXmlStreamReader::TokenType t = r->readNext();
		QStringRef name;
		switch (t) {
		case QXmlStreamReader::StartElement:
			name = r->name();
			if (name == "display-name") {
				QString display = r->readElementText();
				list.append(display);
			}
			break;
		case QXmlStreamReader::EndElement:
			channelMap.insert(channelName.toString(), list);
			return true;
		default:
			break;
		}
	}
	channelMap.insert(channelName.toString(), list);
	return false;
}

bool XmlTv::parseProgram(void)
{
	const QString emptyString("");
	QStringRef empty(&emptyString);

	QXmlStreamAttributes attrs = r->attributes();
	QStringRef channelName = attrs.value("channel");
	QHash<QString, QList<QString>>::ConstIterator it;

	it = channelMap.constFind(channelName.toString());
	if (it == channelMap.constEnd()) {
		qCWarning(logDvb,
			  "Error parsing program: channel %s not found",
			  qPrintable(channelName.toString()));
		return false;
	}

	QList<QString>list = it.value();
	QList<QString>::iterator name;
	bool has_channel = false;

	for (name = list.begin(); name != list.end(); name++) {
		if (channelModel->hasChannelByName(*name)) {
			has_channel = true;
			break;
		}
	}

	if (!has_channel) {
#if 0 // This can be too noisy to keep enabled
		qCWarning(logDvb,
			  "Error: channel %s not found at transponders",
			  qPrintable(channelName.toString()));
#endif
		return true; // Not a parsing error
	}

	DvbSharedChannel channel = channelModel->findChannelByName(*name);
	DvbEpgEntry epgEntry;
	DvbEpgLangEntry *langEntry;
	QString start = attrs.value("start").toString();
	QString stop = attrs.value("stop").toString();

	/* Place "-", ":" and spaces to date formats for Qt::ISODate parser */
	start.replace(QRegularExpression("^(\\d...)(\\d)"), "\\1-\\2");
	start.replace(QRegularExpression("^(\\d...-\\d.)(\\d)"), "\\1-\\2");
	start.replace(QRegularExpression("^(\\d...-\\d.-\\d.)(\\d)"), "\\1 \\2");
	start.replace(QRegularExpression("^(\\d...-\\d.-\\d. \\d.)(\\d)"), "\\1:\\2");
	start.replace(QRegularExpression("^(\\d...-\\d.-\\d. \\d.:\\d.)(\\d)"), "\\1:\\2");

	stop.replace(QRegularExpression("^(\\d...)(\\d)"), "\\1-\\2");
	stop.replace(QRegularExpression("^(\\d...-\\d.)(\\d)"), "\\1-\\2");
	stop.replace(QRegularExpression("^(\\d...-\\d.-\\d.)(\\d)"), "\\1 \\2");
	stop.replace(QRegularExpression("^(\\d...-\\d.-\\d. \\d.)(\\d)"), "\\1:\\2");
	stop.replace(QRegularExpression("^(\\d...-\\d.-\\d. \\d.:\\d.)(\\d)"), "\\1:\\2");

	/* Convert formats to QDateTime */
	epgEntry.begin = QDateTime::fromString(start, Qt::ISODate);
	QDateTime end = QDateTime::fromString(stop, Qt::ISODate);
	epgEntry.duration = QTime(0, 0, 0).addSecs(epgEntry.begin.secsTo(end));
	epgEntry.channel = channel;

	while (!r->atEnd()) {
		const QXmlStreamReader::TokenType t = r->readNext();
		QStringRef element;
		QString lang;
		switch (t) {
		case QXmlStreamReader::StartElement:
			element = r->name();
			if (element == "title") {
				attrs = r->attributes();
				lang = IsoCodes::code2Convert(attrs.value("lang").toString());
				langEntry = getLangEntry(epgEntry, lang);
				langEntry->title += r->readElementText();
			} else if (element == "desc") {
				attrs = r->attributes();
				lang = IsoCodes::code2Convert(attrs.value("lang").toString());
				langEntry = getLangEntry(epgEntry, lang);
				langEntry->details += r->readElementText();
			} else if (element == "rating") {
				attrs = r->attributes();
				lang = IsoCodes::code2Convert(attrs.value("lang").toString());
				QString system = attrs.value("system").toString();
				langEntry = getLangEntry(epgEntry, lang);
				epgEntry.parental += "System: " + system + " = " + r->readElementText();
			}
			/* FIXME: add support for other fields:
			 * credits, date, country, episode-num, video
			 * rating, star-rating
			 */
			break;
		case QXmlStreamReader::EndElement:
			epgModel->addEntry(epgEntry);

			/*
			 * It is not uncommon to have the same xmltv channel
			 * associated with multiple DVB channels. It happens,
			 * for example, when there is a SD, HD, 4K video
			 * streams associated with the same programs.
			 * So, add entries also for the other channels.
			 */
			for (; name != list.end(); name++) {
				if (channelModel->hasChannelByName(*name)) {
					epgEntry.channel = channel;
					epgModel->addEntry(epgEntry);
				}
			}

			return true;
		default:
			break;
		}
	}
	return false;
}

bool XmlTv::load(QString file)
{
	bool parseError = false;

	if (file.isEmpty()) {
		qCInfo(logDvb,
			"Could not locate %s",
			qPrintable(file));
		return false;
	}

	QFile f(file);
	if (!f.open(QIODevice::ReadOnly)) {
		qCWarning(logDvb,
				"Could not open %s (%s)",
				qPrintable(file),
				qPrintable(f.errorString()));
		return false;
	}

	qCInfo(logDvb, "Reading XMLTV file from %s", qPrintable(file));

	r = new QXmlStreamReader(&f);
	while (!r->atEnd()) {
		const QXmlStreamReader::TokenType t = r->readNext();
		QStringRef name;
		switch (t) {
		case QXmlStreamReader::StartElement:
			name = r->name();
			if (name == "channel") {
				if (!parseChannel())
					parseError = true;
			} else if (name == "programme") {
				if (!parseProgram())
					parseError = true;
			}
			break;
		case QXmlStreamReader::EndElement:
			break;
		default:
			break;
		}
		if (parseError)
			break;
	}
	f.close();
	return parseError;
}
