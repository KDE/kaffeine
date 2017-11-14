/*
 * iso-codes.cpp
 *
 * Copyright (C) 2017 Mauro Carvalho Chehab <mchehab@s-opensource.com>
 * Copyright (C) 2017 Pino Toscano <pino@kde.org>
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

#include <KLocalizedString>
#include <QDebug>
#include <QFile>
#include <QLocale>
#include <QStandardPaths>
#include <QXmlStreamReader>

namespace IsoCodes
{
static void load(QHash<QString, QString> &hash,
		 QString file, QString main_key,
		 QString entry_key, QString code_key,
		 QString name_key)
{
	if (!hash.isEmpty()) {
		return;
	}

	const QString fileName = QStandardPaths::locate(QStandardPaths::GenericDataLocation, file);
	if (fileName.isEmpty()) {
		qInfo() << "Could not locate" << file << "(is iso-codes installed?)";
		return;
	}

	QFile f(fileName);
	if (!f.open(QIODevice::ReadOnly)) {
		qWarning() << "Could not open" << fileName << f.errorString();
		return;
	}

	QXmlStreamReader r(&f);
	bool inDoc = false;
	while (!r.atEnd()) {
		const QXmlStreamReader::TokenType t = r.readNext();
		QStringRef name;
		switch (t) {
		case QXmlStreamReader::StartElement:
			name = r.name();
			if (inDoc && name == entry_key) {
				const QXmlStreamAttributes attrs = r.attributes();
				const QString code = attrs.value(code_key).toString().toUpper();
				const QString lang = attrs.value(name_key).toString();
				hash.insert(code, lang);
			} else if (name == main_key) {
				inDoc = true;
			}
			break;
		case QXmlStreamReader::EndElement:
			name = r.name();
			if (inDoc && name == main_key) {
				inDoc = false;
			}
			break;
		case QXmlStreamReader::NoToken:
		case QXmlStreamReader::Invalid:
		case QXmlStreamReader::StartDocument:
		case QXmlStreamReader::EndDocument:
		case QXmlStreamReader::Characters:
		case QXmlStreamReader::Comment:
		case QXmlStreamReader::DTD:
		case QXmlStreamReader::EntityReference:
		case QXmlStreamReader::ProcessingInstruction:
			break;
		}
	}
}

/*
 * ISO 639-2 language codes
 * Loaded and translated at runtime from iso-codes.
 */
static QHash<QString, QString> iso639_2_codes;

bool getLanguage(const QString &code, QString *language)
{
	load(iso639_2_codes,
	     QString("xml/iso-codes/iso_639-2.xml"),
	     QLatin1String("iso_639_entries"),
	     QLatin1String("iso_639_entry"),
	     QLatin1String("iso_639_2B_code"),
	     QLatin1String("name"));

	QHash<QString, QString>::ConstIterator it = iso639_2_codes.constFind(code);
	if (it == iso639_2_codes.constEnd()) {
		return false;
	}

	if (language) {
		*language = i18nd("iso_639-2", it.value().toUtf8().constData());
	}
	return true;
}

}
