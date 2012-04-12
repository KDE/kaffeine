/*
 * updatesource.cpp
 *
 * Copyright (C) 2011 Christoph Pfister <christophpfister@gmail.com>
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
#include <QDir>

int main(int argc, char *argv[])
{
	// QCoreApplication is needed for proper file name handling
	QCoreApplication application(argc, argv);

	QStringList paths;
	paths.append(".");

	for (int i = 0; i < paths.size(); ++i) {
		QString path = paths.at(i);

		if (QFileInfo(path).isDir()) {
			QStringList entries = QDir(path).entryList(QDir::AllEntries |
				QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

			foreach (const QString &entry, entries) {
				if (path == ".") {
					if (entry != ".git") {
						paths.append(entry);
					}
				} else {
					paths.append(path + '/' + entry);
				}
			}

			paths.removeAt(i);
			--i;
		}
	}

	qSort(paths);

	foreach (const QString &path, paths) {
		QFile file(path);

		if (!file.open(QIODevice::ReadOnly)) {
			qCritical() << "cannot open file" << file.fileName();
			return 1;
		}

		QByteArray data = file.readAll();
		file.close();

		if (data.isEmpty()) {
			qCritical() << "file is empty" << file.fileName();
			return 1;
		}

		if (path.endsWith(".png") || path.endsWith(".svgz")) {
			continue;
		}

		QString content = QString::fromUtf8(data.constData());

		if (data != content.toUtf8()) {
			qCritical() << "non-utf8 characters in file" << file.fileName();
			return 1;
		}

		// FIXME make src/scanfile.dvb ascii-only

		if ((path == "README") || (path == "src/scanfile.dvb") ||
		    path.endsWith(".desktop")) {
			continue;
		}

		for (int i = 0; i < content.size(); ++i) {
			int value = content.at(i).unicode();

			if ((value > 0x7f) ||
			    ((value < 0x20) && (value != '\n') && (value != '\t'))) {
				qCritical() << "non-ascii character" << content.at(i) <<
					"in file" << file.fileName();
				return 1;
			}
		}

		if (path.startsWith(QLatin1String("include/")) || path.startsWith(QLatin1String("tools/"))) {
			continue;
		}

		QStringList lines = content.split('\n');
		bool ignoreLineLength = false;
		QRegExp logRegExp("Log[^0-9A-Za-z]*[(][^0-9A-Za-z]*\"");
		QString logFunctionName;
		int bracketLevel = 0;

		for (int i = 0; i < lines.size(); ++i) {
			const QString &line = lines.at(i);

			if ((line == "// everything below this line is automatically generated") &&
			    ((path == "src/dvb/dvbsi.cpp") || (path == "src/dvb/dvbsi.h"))) {
				ignoreLineLength = true;
			}

			if (!ignoreLineLength &&
			    (QString(line).replace('\t', "        ").size() > 99)) {
				qWarning() << file.fileName() << "line" << (i + 1) <<
					"is longer than 99 characters";
			}

			int logIndex = logRegExp.indexIn(line);

			if (logIndex >= 0) {
				logIndex = (line.indexOf('"', logIndex) + 1);

				if (!line.mid(logIndex).startsWith(logFunctionName)) {
					int cutIndex = (line.indexOf(": ", logIndex) + 2);

					if (cutIndex > logIndex) {
						lines[i].remove(logIndex, cutIndex - logIndex);
					}

					lines[i].insert(logIndex, logFunctionName);
				}
			}

			bracketLevel = (bracketLevel + line.count('{') - line.count('}'));

			if (bracketLevel < 0) {
				qWarning() << file.fileName() << "line" << (i + 1) <<
					"brackets do not match";
			}

			if ((bracketLevel == 0) && !line.startsWith('\t')) {
				QRegExp logFunctionRegExp("[0-9A-Za-z:~]*[(]");
				int index = logFunctionRegExp.indexIn(line);

				if (index >= 0) {
					logFunctionName =
						line.mid(index, logFunctionRegExp.matchedLength());
					logFunctionName.chop(1);
					logFunctionName.append(": ");
				}
			}
		}

		if (bracketLevel != 0) {
			qWarning() << file.fileName() << "brackets do not match";
		}

		if (content != lines.join("\n")) {
			if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				qCritical() << "cannot open file" << file.fileName();
				return 1;
			}

			file.write(lines.join("\n").toUtf8());
		}
	}

	return 0;
}
