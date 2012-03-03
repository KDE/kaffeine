/*
 * convertscanfiles.cpp
 *
 * Copyright (C) 2008-2011 Christoph Pfister <christophpfister@gmail.com>
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
#include <QDate>
#include <QDebug>
#include <QDir>
#include "../src/dvb/dvbtransponder.h"

class NumericalLessThan
{
public:
	bool operator()(const QString &x, const QString &y)
	{
		int i = 0;

		while (true) {
			if ((i == x.length()) || (i == y.length())) {
				return x.length() < y.length();
			}

			if (x.at(i) != y.at(i)) {
				break;
			}

			++i;
		}

		int xIndex = x.indexOf(' ', i);

		if (xIndex == -1) {
			xIndex = x.length();
		}

		int yIndex = y.indexOf(' ', i);

		if (yIndex == -1) {
			yIndex = y.length();
		}

		if (xIndex != yIndex) {
			return xIndex < yIndex;
		} else {
			return x.at(i) < y.at(i);
		}
	}
};

static QString parseLine(DvbTransponderBase::TransmissionType type, const QString &line, const QString &fileName)
{
	switch (type) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC: {
		DvbCTransponder transponder;

		if (!transponder.fromString(line)) {
			break;
		}

		if (transponder.modulation == DvbCTransponder::ModulationAuto) {
			qWarning() << "Warning: modulation == AUTO in file" << fileName;
		}

		if (transponder.fecRate != DvbCTransponder::FecNone) {
			qWarning() << "Warning: fec rate != NONE in file" << fileName;
		}

		return transponder.toString();
	    }
	case DvbTransponderBase::DvbS: {
		if (line.startsWith(QLatin1String("S "))) {
			DvbSTransponder transponder;

			if (!transponder.fromString(line)) {
				break;
			}

			if (transponder.fecRate == DvbSTransponder::FecNone) {
				qWarning() << "Warning: fec rate == NONE in file" << fileName;
			}

			// fecRate == AUTO is ok

			return transponder.toString();
		} else {
			DvbS2Transponder transponder;

			if (!transponder.fromString(line)) {
				break;
			}

			if (transponder.fecRate == DvbSTransponder::FecNone) {
				qWarning() << "Warning: fec rate == NONE in file" << fileName;
			}

			// fecRate == AUTO is ok

			return transponder.toString();
		}
	    }
	case DvbTransponderBase::DvbS2:
		break;
	case DvbTransponderBase::DvbT: {
		DvbTTransponder transponder;

		if (!transponder.fromString(line)) {
			break;
		}

		if (transponder.bandwidth == DvbTTransponder::BandwidthAuto) {
			qWarning() << "Warning: bandwidth == AUTO in file" << fileName;
		}

		if ((transponder.modulation == DvbTTransponder::ModulationAuto) && !fileName.startsWith(QLatin1String("auto-"))) {
			qWarning() << "Warning: modulation == AUTO in file" << fileName;
		}

		if (transponder.fecRateHigh == DvbTTransponder::FecNone) {
			qWarning() << "Warning: fec rate high == NONE in file" << fileName;
		}

		// fecRateHigh == AUTO is ok

		if (transponder.fecRateLow != DvbTTransponder::FecNone) {
			qWarning() << "Warning: fec rate low != NONE in file" << fileName;
		}

		if ((transponder.transmissionMode == DvbTTransponder::TransmissionModeAuto) && !fileName.startsWith(QLatin1String("auto-"))) {
			qWarning() << "Warning: transmission mode == AUTO in file" << fileName;
		}

		if ((transponder.guardInterval == DvbTTransponder::GuardIntervalAuto) && !fileName.startsWith(QLatin1String("auto-"))) {
			qWarning() << "Warning: guard interval == AUTO in file" << fileName;
		}

		if (transponder.hierarchy != DvbTTransponder::HierarchyNone) {
			qWarning() << "Warning: hierarchy != NONE in file" << fileName;
		}

		return transponder.toString();
	    }
	case DvbTransponderBase::Atsc: {
		AtscTransponder transponder;

		if (!transponder.fromString(line)) {
			break;
		}

		if (transponder.modulation == AtscTransponder::ModulationAuto) {
			qWarning() << "Warning: modulation == AUTO in file" << fileName;
		}

		return transponder.toString();
	    }
	}

	return QString();
}

static void readScanDirectory(QTextStream &out, const QString &path, DvbTransponderBase::TransmissionType type)
{
	QDir dir;

	switch (type) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC:
		dir.setPath(path + "/dvb-c");
		break;
	case DvbTransponderBase::DvbS:
		dir.setPath(path + "/dvb-s");
		break;
	case DvbTransponderBase::DvbS2:
		break;
	case DvbTransponderBase::DvbT:
		dir.setPath(path + "/dvb-t");
		break;
	case DvbTransponderBase::Atsc:
		dir.setPath(path + "/atsc");
		break;
	}

	if (!dir.exists()) {
		qCritical() << "Error: can't open directory" << dir.path();
		return;
	}

	foreach (const QString &fileName, dir.entryList(QDir::Files, QDir::Name)) {
		QFile file(dir.filePath(fileName));

		if (!file.open(QIODevice::ReadOnly)) {
			qCritical() << "Error: can't open file" << file.fileName();
			return;
		}

		QTextStream stream(&file);
		stream.setCodec("UTF-8");
		QList<QString> transponders;

		while (!stream.atEnd()) {
			QString line = stream.readLine();

			int pos = line.indexOf('#');

			if (pos != -1) {
				while ((pos > 0) && (line[pos - 1] == ' ')) {
					--pos;
				}

				line.truncate(pos);
			}

			if (line.isEmpty()) {
				continue;
			}

			QString string = parseLine(type, line, fileName);

			if (string.isEmpty()) {
				qCritical() << "Error: can't parse line" << line << "in file" << fileName;
				return;
			}

			// reduce multiple spaces to one space and remove leading zeros

			for (int i = 1; i < line.length(); ++i) {
				if (line.at(i - 1) != ' ') {
					continue;
				}

				if ((line.at(i) == ' ') || (line.at(i) == '0')) {
					line.remove(i, 1);
					--i;
				}
			}

			if (line != string) {
				qWarning() << "Warning: suboptimal representation" << line << "<-->" << string << "in file" << fileName;
			}

			transponders.append(string);
		}

		if (transponders.isEmpty()) {
			qWarning() << "Warning: no transponder found in file" << fileName;
			continue;
		}

		QString name = dir.dirName() + '/' + fileName;

		if (type == DvbSTransponder::DvbS) {
			// use upper case for orbital position
			name[name.size() - 1] = name.at(name.size() - 1).toUpper();

			QString source = name;
			source.remove(0, source.lastIndexOf('-') + 1);

			bool ok = false;

			if (source.endsWith('E')) {
				source.chop(1);
				source.toDouble(&ok);
			} else if (source.endsWith('W')) {
				source.chop(1);
				source.toDouble(&ok);
			}

			if (!ok) {
				qWarning() << "Warning: invalid orbital position for file" << fileName;
			}
		}

		out << '[' << name << "]\n";

		qSort(transponders.begin(), transponders.end(), NumericalLessThan());

		foreach (const QString &transponder, transponders) {
			out << transponder << '\n';
		}
	}
}

int main(int argc, char *argv[])
{
	// QCoreApplication is needed for proper file name handling
	QCoreApplication app(argc, argv);

	if (argc != 3) {
		qCritical() << "Syntax: convertscanfiles <scan file dir> <output file>";
		return 1;
	}

	QByteArray data;
	QTextStream out(&data);
	out.setCodec("UTF-8");

	out << "# this file is automatically generated from http://linuxtv.org/hg/dvb-apps\n";
	out << "[date]\n";
	out << QDate::currentDate().toString(Qt::ISODate) << '\n';

	QString path(argv[1]);

	readScanDirectory(out, path, DvbTransponderBase::DvbC);
	readScanDirectory(out, path, DvbTransponderBase::DvbS);
	readScanDirectory(out, path, DvbTransponderBase::DvbT);
	readScanDirectory(out, path, DvbTransponderBase::Atsc);

	out.flush();

	QFile file(argv[2]);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCritical() << "Error: can't open file" << file.fileName();
		return 1;
	}

	file.write(data);

	QFile compressedFile(QString(argv[2]) + ".qz");

	if (!compressedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCritical() << "Error: can't open file" << compressedFile.fileName();
		return 1;
	}

	compressedFile.write(qCompress(data, 9));

	return 0;
}
