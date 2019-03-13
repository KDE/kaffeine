/*
 * iso-codes.cpp
 *
 * Copyright (C) 2017 Mauro Carvalho Chehab <mchehab+samsung@kernel.org>
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

#include "log.h"

#include <KLocalizedString>
#include <QFile>
#include <QLocale>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include "iso-codes.h"

namespace IsoCodes
{
	static void load(QHash<QString, QString> *code_3letters,
			 QHash<QString, QString> *code_2letters,
			 QString file,
			 QString main_key,
			 QString entry_key,
			 QString code_3_key,
			 QString code_2_key,
			 QString name_key)
	{
		if (!code_3letters->isEmpty())
			return;

		const QString fileName = QStandardPaths::locate(QStandardPaths::GenericDataLocation, file);
		if (fileName.isEmpty()) {
			qCInfo(logConfig,
			       "Could not locate %s (is iso-codes installed?)",
			       qPrintable(file));
			return;
		}

		QFile f(fileName);
		if (!f.open(QIODevice::ReadOnly)) {
			qCWarning(logConfig,
				  "Could not open %s (%s)",
				  qPrintable(fileName),
				  qPrintable(f.errorString()));
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
					const QString code3 = attrs.value(code_3_key).toString().toUpper();
					const QString lang = attrs.value(name_key).toString();
					code_3letters->insert(code3, lang);
					if (code_2letters) {
						const QString code2 = attrs.value(code_2_key).toString().toUpper();
						if (code2 != "")
							code_2letters->insert(code2, code3);
					}
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
		if (code_3letters->isEmpty())
			qCWarning(logConfig,
				  "Error parsing %s: no entries found.",
				  qPrintable(fileName));
	}

	/*
	 * ISO 639-2 language codes
	 * Loaded and translated at runtime from iso-codes.
	 */
	static QHash<QString, QString> iso639_2_codes;

	/*
	 * ISO 639-1 to ISO 639-2 language code conversion
	 * Loaded and translated at runtime from iso-codes.
	 */
	static QHash<QString, QString> iso639_1_codes;

	bool getLanguage(const QString &iso_code, QString *language)
	{
		static bool first = true;

		QString code = iso_code.toUpper();

		if (code == "QAA") {
			*language = i18n("Original Language");
			return true;
		}

		if (first) {
			load(&iso639_2_codes,
			     &iso639_1_codes,
			     QString("xml/iso-codes/iso_639-2.xml"),
			     QLatin1String("iso_639_entries"),
			     QLatin1String("iso_639_entry"),
			     QLatin1String("iso_639_2B_code"),
			     QLatin1String("iso_639_1_code"),
			     QLatin1String("name"));
			first = false;
		}

		QHash<QString, QString>::ConstIterator it = iso639_2_codes.constFind(code);
		if (it == iso639_2_codes.constEnd()) {
			/*
			 * The ETSI EN 300 468 Annex F spec defines the
			 * original audio soundtrack code to be "QAA". Yet,
			 * TV bundle providers could use something else.
			 *
			 * At least here, my provider actually uses "ORG"
			 * instead. Currently, this is an unused ISO 639
			 * code, so we can safely accept it as well,
			 * at least as a fallback code while ISO doesn't
			 * define it.
			 */
			if (code == "ORG") {
				*language = i18n("Original Language");
				return true;
			}
			return false;
		}

		if (language) {
			*language = i18nd("iso_639-2", it.value().toUtf8().constData());
		}
		return true;
	}

	const QString code2Convert(const QString &code2)
	{
		static bool first = true;

		QString code = code2.toUpper();

		/* Ignore any embedded Country data */
		code.remove(QRegularExpression("_.*"));

		if (first) {
			load(&iso639_2_codes,
			     &iso639_1_codes,
			     QString("xml/iso-codes/iso_639-2.xml"),
			     QLatin1String("iso_639_entries"),
			     QLatin1String("iso_639_entry"),
			     QLatin1String("iso_639_2B_code"),
			     QLatin1String("iso_639_1_code"),
			     QLatin1String("name"));
			first = false;
		}

		QHash<QString, QString>::ConstIterator it = iso639_1_codes.constFind(code);
		if (it == iso639_1_codes.constEnd()) {
			return "QAA";
		}
		return it.value().toUtf8().constData();
	}

	/*
	* ISO 3166-1 Alpha 3 Country codes
	* Loaded and translated at runtime from iso-codes.
	*/
	static QHash<QString, QString> iso3166_1_codes, iso3166_2_codes;

	bool getCountry(const QString &_code, QString *country)
	{
		static bool first = true;
		QString code;

		if (first) {
			load(&iso3166_1_codes,
			     &iso3166_2_codes,
			     QString("xml/iso-codes/iso_3166-1.xml"),
			     QLatin1String("iso_3166_entries"),
			     QLatin1String("iso_3166_entry"),
			     QLatin1String("alpha_3_code"),
			     QString("alpha_2_code"),
			     QLatin1String("name"));
			first = false;
		}

		if (_code.size() == 2) {
			QHash<QString, QString>::ConstIterator it = iso3166_1_codes.constFind(code);
			if (it == iso3166_2_codes.constEnd())
				return false;
			code = it.value();
		} else {
			code = _code;
		}

		QHash<QString, QString>::ConstIterator it = iso3166_1_codes.constFind(code);
		if (it == iso3166_1_codes.constEnd()) {
			return false;
		}

		if (country) {
			*country = i18nd("iso_3166-1", it.value().toUtf8().constData());
		}
		return true;
	}
}
