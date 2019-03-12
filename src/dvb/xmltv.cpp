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

	QString current = r->name().toString();
	while (!r->atEnd()) {
		const QXmlStreamReader::TokenType t = r->readNext();

		if (t == QXmlStreamReader::EndElement) {
			if (r->name() == current)
				break;
		}

		if (t != QXmlStreamReader::StartElement)
			continue;

		QStringRef name = r->name();
		if (name == "display-name") {
			QString display = r->readElementText();
			list.append(display);
		} else if (name != "icon") {
			static QString lastNotFound("");
			if (name.toString() != lastNotFound) {
				qCWarning(logDvb,
					"Ignoring unknown channel tag '%s'",
					qPrintable(name.toString()));
				lastNotFound = name.toString();
			}
		}
	}

	channelMap.insert(channelName.toString(), list);
	return true;
}

void XmlTv::parseKeyValues(QHash<QString, QString> &keyValues)
{
	QXmlStreamAttributes attrs;
	QHash<QString, QString>::ConstIterator it;

	QString current = r->name().toString();
	while (!r->atEnd()) {
		const QXmlStreamReader::TokenType t = r->readNext();

		if (t == QXmlStreamReader::EndElement) {
			if (r->name() == current)
				return;
		}

		if (t != QXmlStreamReader::StartElement)
			continue;

		attrs = r->attributes();
		QString key = r->name().toString();
		QString value;

		it = keyValues.constFind(key);
		if (it != keyValues.constEnd())
			value = *it + ", ";

		value += r->readElementText();

		keyValues.insert(key, value);
	}
}

void XmlTv::ignoreTag(void)
{
	QXmlStreamAttributes attrs;

	QString current = r->name().toString();
	while (!r->atEnd()) {
		const QXmlStreamReader::TokenType t = r->readNext();
		if (t == QXmlStreamReader::EndElement) {
			if (r->name() == current)
				return;
		}
	}
}

QString XmlTv::getValue(QHash<QString, QString> &keyValues, QString key)
{
	QHash<QString, QString>::ConstIterator it;

	it = keyValues.constFind(key);
	if (it == keyValues.constEnd())
		return QString("");

	return *it;
}

QString XmlTv::getAllValues(QHash<QString, QString> &keyValues)
{
	QString values;

	foreach(const QString &key, keyValues.keys())
		values += key + "(s): " + keyValues.value(key) + "\n";

	return values;
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
		static QString lastNotFound("");
		if (channelName.toString() != lastNotFound) {
			qCWarning(logDvb,
				"Error: channel %s not found at transponders",
				qPrintable(channelName.toString()));
			lastNotFound = channelName.toString();
		}
#endif
		ignoreTag();

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

	epgEntry.begin.setTimeSpec(Qt::UTC);
	epgEntry.channel = channel;

	QString starRating, credits, category, date;
	QString current = r->name().toString();
	while (!r->atEnd()) {
		const QXmlStreamReader::TokenType t = r->readNext();

		if (t == QXmlStreamReader::EndElement) {
			if (r->name() == current)
				break;
		}

		if (t != QXmlStreamReader::StartElement)
			continue;

		QString lang;
		QStringRef element = r->name();
		if (element == "title") {
			attrs = r->attributes();
			lang = IsoCodes::code2Convert(attrs.value("lang").toString());
			langEntry = getLangEntry(epgEntry, lang);
			if (langEntry->title != "")
				langEntry->title += " ";
			langEntry->title += r->readElementText();
		} else if (element == "sub-title") {
			attrs = r->attributes();
			lang = IsoCodes::code2Convert(attrs.value("lang").toString());
			langEntry = getLangEntry(epgEntry, lang);
			if (langEntry->subheading != "")
				langEntry->subheading += " ";
			langEntry->subheading += r->readElementText();
		} else if (element == "desc") {
			attrs = r->attributes();
			lang = IsoCodes::code2Convert(attrs.value("lang").toString());
			langEntry = getLangEntry(epgEntry, lang);
			if (langEntry->details != "")
				langEntry->details += " ";
			langEntry->details += r->readElementText();
		} else if (element == "rating") {
			QHash<QString, QString> keyValues;

			attrs = r->attributes();
			parseKeyValues(keyValues);

			QString system = attrs.value("system").toString();

			QString value = getValue(keyValues, "value");

			if (system == "" || value == "")
				continue;

			if (epgEntry.parental != "")
				epgEntry.parental += ", ";

			epgEntry.parental += i18n("System: ") + system + i18n(", rating: ") + value;
		} else if (element == "star-rating") {
			QHash<QString, QString> keyValues;

			parseKeyValues(keyValues);

			QString value = getValue(keyValues, "value");

			if (value != "")
				starRating += value;
		} else if (element == "category") {
			attrs = r->attributes();
			lang = IsoCodes::code2Convert(attrs.value("lang").toString());
			langEntry = getLangEntry(epgEntry, lang);
			if (category != "")
				category += ", ";
			category += r->readElementText();
		} else if (element == "credits") {
			QHash<QString, QString> keyValues;

			// FIXME: the disadvantage of this simple parsing is
			// that it won't allow translations
			parseKeyValues(keyValues);

			credits = getAllValues(keyValues);
		} else if ((element == "date")) {
			QString rawdate = r->readElementText();
			date = rawdate.mid(0, 4);
			QString month = rawdate.mid(5, 2);
			QString day = rawdate.mid(7, 2);
			if (month != "")
				date += "-" + month;
			if (day != "") {
				date += "-" + day;

				QDate d = QDate::fromString(date, Qt::ISODate);
				date = d.toString();
			}
		} else if ((element == "aspect") ||
			   (element == "audio") ||
			   (element == "episode-num") ||
			   (element == "quality") ||
			   (element == "stereo") ||
			   (element == "url") ||
			   (element == "video")) {
			ignoreTag();
		} else {
			static QString lastNotFound("");
			if (element.toString() != lastNotFound) {
				qCWarning(logDvb,
					"Ignoring unknown programme tag '%s'",
					qPrintable(element.toString()));
				lastNotFound = element.toString();
			}
		}
	}

	/* Those extra fields are not language-specific data */
	if (starRating != "") {
		if (epgEntry.content != "")
			epgEntry.content += "\n";
		epgEntry.content += i18n("Star rating: ") + starRating;
	}

	if (date != "") {
		if (epgEntry.content != "")
			epgEntry.content += "\n";
		epgEntry.content += i18n("Date: ") + date;
	}

	if (category != "") {
		if (epgEntry.content != "")
			epgEntry.content += "\n";
		epgEntry.content += i18n("Category: ") + category;
	}

	if (credits != "") {
		if (epgEntry.content != "")
			epgEntry.content += "\n";
		epgEntry.content += credits;
	}

	epgEntry.content.replace(QRegularExpression("\\n+$"), "");
	epgEntry.content.replace(QRegularExpression("\\n"), "<p/>");

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
			channel = channelModel->findChannelByName(*name);
			epgEntry.channel = channel;
			epgModel->addEntry(epgEntry);
		}
	}
	return true;
}

bool XmlTv::load(QString file)
{
	bool parseError = false;

	watcher.removePath(file);
	if (file.isEmpty()) {
		qCInfo(logDvb, "File to load not specified");
		return false;
	}

	QFile f(file);
	if (!f.open(QIODevice::ReadOnly)) {
		qCWarning(logDvb,
				"Error opening %s: %s. Will stop monitoring it",
				qPrintable(file),
				qPrintable(f.errorString()));
		return false;
	}

	qCInfo(logDvb, "Reading XMLTV file from %s", qPrintable(file));

	r = new QXmlStreamReader(&f);
	while (!r->atEnd()) {
		if (r->readNext() != QXmlStreamReader::StartElement)
			continue;

		QStringRef name = r->name();

		if (name == "channel") {
			if (!parseChannel())
				parseError = true;
		} else if (name == "programme") {
			if (!parseProgram())
				parseError = true;
		} else if (name != "tv") {
			static QString lastNotFound("");
			if (name.toString() != lastNotFound) {
				qCWarning(logDvb,
					"Ignoring unknown main tag '%s'",
					qPrintable(r->qualifiedName().toString()));
				lastNotFound = name.toString();
			}
		}
	}

	if (r->error()) {
		qCWarning(logDvb, "XMLTV: error: %s",
			  qPrintable(r->errorString()));
	}

	if (parseError) {
		qCWarning(logDvb, "XMLTV: parsing error");
	}

	f.close();
	watcher.addPath(file);
	return parseError;
}
