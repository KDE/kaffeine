/*
 * iso639.cpp
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

namespace Iso639
{

/*
 * ISO 639-2 language codes as defined on 2017-11-08 at:
 *	https://www.loc.gov/standards/iso639-2/ISO-639-2_utf-8.txt
 * Loaded and translated at runtime from iso-codes.
 */
static QHash<QString, QString> iso639_2_codes;

static void load()
{
	if (!iso639_2_codes.isEmpty()) {
		return;
	}

	const QString fileName = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "xml/iso-codes/iso_639-2.xml");
	if (fileName.isEmpty()) {
		qInfo() << "Could not locate iso_639-2.xml (is iso-codes installed?)";
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
			if (inDoc && name == QLatin1String("iso_639_entry")) {
				const QXmlStreamAttributes attrs = r.attributes();
				const QString code = attrs.value("iso_639_2B_code").toString().toUpper();
				const QString lang = attrs.value("name").toString();
				iso639_2_codes.insert(code, lang);
			} else if (name == QLatin1String("iso_639_entries")) {
				inDoc = true;
			}
			break;
		case QXmlStreamReader::EndElement:
			name = r.name();
			if (inDoc && name == QLatin1String("iso_639_entries")) {
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


bool lookupCode(const QString &code, QString *language)
{
	load();

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
