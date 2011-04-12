/*
 * updatemimetypes.cpp
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

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <KMimeType>

int main(int argc, char *argv[])
{
	// QCoreApplication is needed for proper file name handling
	QCoreApplication application(argc, argv);

	if (argc != 3) {
		qCritical() << "syntax:" << argv[0] << "<desktop file> <path to mediawidget.cpp>";
		return 1;
	}

	QFile file(argv[1]);
	QStringList lines;

	{
		if (!file.open(QFile::ReadOnly)) {
			qCritical() << "cannot open file" << file.fileName();
			return 1;
		}

		QTextStream stream(&file);
		stream.setCodec("UTF-8");

		while (!stream.atEnd()) {
			lines.append(stream.readLine());
		}

		file.close();
	}

	int mimeTypeIndex = -1;
	QStringList mimeTypes;

	for (int i = 0; i < lines.size(); ++i) {
		if (lines.at(i).startsWith(QLatin1String("MimeType="))) {
			if (mimeTypeIndex >= 0) {
				qCritical() << "more than one MimeType entry found in file" <<
					file.fileName();
				return 1;
			}

			mimeTypeIndex = i;
			mimeTypes = lines.at(i).mid(9).split(';', QString::SkipEmptyParts);
		}
	}

	if (mimeTypeIndex < 0) {
		qCritical() << "cannot find a MimeType entry in file" << file.fileName();
		return 1;
	}

	mimeTypes.removeDuplicates();
	mimeTypes.sort();

	QStringList realExtensions;

	for (int skipMimeType = -1; skipMimeType < mimeTypes.size(); ++skipMimeType) {
		QStringList extensions;

		for (int i = 0; i < mimeTypes.size(); ++i) {
			if (i != skipMimeType) {
				KMimeType::Ptr mimetype = KMimeType::mimeType(mimeTypes.at(i),
					KMimeType::DontResolveAlias);

				if (mimetype.isNull()) {
					qCritical() << "unknown mime type" << mimeTypes.at(i);
					return 1;
				}

				extensions.append(mimetype->patterns());
			}
		}

		QRegExp regExp("\\*\\.[a-z0-9+]+");

		for (int i = 0; i < extensions.size(); ++i) {
			if (extensions.at(i) == "*.anim[1-9j]") {
				extensions.removeAt(i);
				extensions.insert(i, "*.anim1");
				extensions.insert(i, "*.anim2");
				extensions.insert(i, "*.anim3");
				extensions.insert(i, "*.anim4");
				extensions.insert(i, "*.anim5");
				extensions.insert(i, "*.anim6");
				extensions.insert(i, "*.anim7");
				extensions.insert(i, "*.anim8");
				extensions.insert(i, "*.anim9");
				extensions.insert(i, "*.animj");
			}

			if (!regExp.exactMatch(extensions.at(i))) {
				qCritical() << "unknown extension syntax" << extensions.at(i);
				return 1;
			}
		}

		extensions.removeDuplicates();
		extensions.sort();

		if (realExtensions.isEmpty()) {
			realExtensions = extensions;
		} else if (extensions.size() == realExtensions.size()) {
			qWarning() << "possibly unneeded mime type found" <<
				mimeTypes.at(skipMimeType);
		}
	}

	{
		if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
			qCritical() << "cannot open file" << file.fileName();
			return 1;
		}

		QTextStream stream(&file);
		stream.setCodec("UTF-8");

		lines[mimeTypeIndex] = "MimeType=" + mimeTypes.join(";") + ';';

		foreach (const QString &line, lines) {
			stream << line << '\n';
		}

		file.close();
	}

	file.setFileName(argv[2]);
	lines.clear();

	{
		if (!file.open(QFile::ReadOnly)) {
			qCritical() << "cannot open file" << file.fileName();
			return 1;
		}

		QTextStream stream(&file);
		stream.setCodec("UTF-8");

		while (!stream.atEnd()) {
			lines.append(stream.readLine());
		}

		file.close();
	}

	{
		if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
			qCritical() << "cannot open file" << file.fileName();
			return 1;
		}

		QTextStream stream(&file);
		stream.setCodec("UTF-8");

		for (int i = 0; i < lines.size(); ++i) {
			stream << lines.at(i) << '\n';

			if (lines.at(i) == "\t\t// generated from kaffeine.desktop's mime types") {
				QString line = "\t\t\"";

				foreach (const QString &extension, realExtensions) {
					if ((line.size() + extension.size()) > 83) {
						stream << line << "\"\n";
						line = "\t\t\"";
					}

					line += extension;
					line += ' ';
				}

				stream << line << "\"\n";

				while ((i < lines.size()) &&
				       (lines.at(i) != "\t\t// manual entries")) {
					++i;
				}

				stream << lines.at(i) << '\n';
			}
		}

		file.close();
	}

	return 0;
}
