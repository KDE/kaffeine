/*
 * convertscanfiles.cpp
 *
 * Copyright (C) 2008 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QtDebug>
#include "../src/dvb/dvbchannel.cpp"

class ScanFileReader
{
public:
	ScanFileReader(const QString &line_) : line(line_), pos(0), valid(true) { }
	~ScanFileReader() { }

	bool checkEnd() const
	{
		return pos >= line.size();
	}

	bool isValid() const
	{
		return valid;
	}

	template<class T> T readEnum(const QMap<QString, T> &map)
	{
		T value = map.value(readString(), static_cast<T>(-1));

		if (value == static_cast<T>(-1)) {
			valid = false;
		}

		return value;
	}

	int readInt()
	{
		QString string = readString();

		if (string.isEmpty()) {
			valid = false;
			return -1;
		}

		bool ok;
		int value = string.toInt(&ok);

		if (!ok || (value <= 0)) {
			valid = false;
		}

		return value;
	}

	QString readString()
	{
		int begin = pos;
		int end = pos;

		while ((end < line.size()) && (line[end] != ' ')) {
			++end;
		}

		pos = end + 1;

		while ((pos < line.size()) && (line[pos] == ' ')) {
			++pos;
		}

		if (end > begin) {
			return line.mid(begin, end - begin);
		} else {
			return QString();
		}
	}

private:
	QString line;
	int pos;
	bool valid;
};

class DvbCFileHelper
{
public:
	DvbCFileHelper();
	~DvbCFileHelper() { }

	DvbCTransponder *readTransponder(ScanFileReader &reader);

	bool operator()(DvbCTransponder *x, DvbCTransponder *y) const
	{
		return x->frequency < y->frequency;
	}

private:
	QMap<QString, DvbSTransponder::FecRate> fecMap;
	QMap<QString, DvbCTransponder::Modulation> modulationMap;
};

DvbCFileHelper::DvbCFileHelper()
{
	fecMap.insert("NONE", DvbSTransponder::FecNone);

	modulationMap.insert("QAM16", DvbCTransponder::Qam16);
	modulationMap.insert("QAM32", DvbCTransponder::Qam32);
	modulationMap.insert("QAM64", DvbCTransponder::Qam64);
	modulationMap.insert("QAM128", DvbCTransponder::Qam128);
	modulationMap.insert("QAM256", DvbCTransponder::Qam256);
	modulationMap.insert("AUTO", DvbCTransponder::ModulationAuto);
}

DvbCTransponder *DvbCFileHelper::readTransponder(ScanFileReader &reader)
{
	if (reader.readString() != "C") {
		return NULL;
	}

	DvbCTransponder *transponder = new DvbCTransponder();

	transponder->frequency = reader.readInt();
	transponder->symbolRate = reader.readInt();
	transponder->fecRate = reader.readEnum(fecMap);
	transponder->modulation = reader.readEnum(modulationMap);

	if (!reader.isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

class DvbSFileHelper
{
public:
	DvbSFileHelper();
	~DvbSFileHelper() { }

	DvbSTransponder *readTransponder(ScanFileReader &reader);

	bool operator()(DvbSTransponder *x, DvbSTransponder *y) const
	{
		if (x->frequency != y->frequency) {
			return x->frequency < y->frequency;
		}

		return x->polarization < y->polarization;
	}

private:
	QMap<QString, DvbSTransponder::Polarization> polarizationMap;
	QMap<QString, DvbSTransponder::FecRate> fecMap;
};

DvbSFileHelper::DvbSFileHelper()
{
	polarizationMap.insert("H", DvbSTransponder::Horizontal);
	polarizationMap.insert("V", DvbSTransponder::Vertical);

	fecMap.insert("1/2", DvbSTransponder::Fec1_2);
	fecMap.insert("2/3", DvbSTransponder::Fec2_3);
	fecMap.insert("3/4", DvbSTransponder::Fec3_4);
	fecMap.insert("4/5", DvbSTransponder::Fec4_5);
	fecMap.insert("5/6", DvbSTransponder::Fec5_6);
	fecMap.insert("6/7", DvbSTransponder::Fec6_7);
	fecMap.insert("7/8", DvbSTransponder::Fec7_8);
	fecMap.insert("8/9", DvbSTransponder::Fec8_9);
	fecMap.insert("AUTO", DvbSTransponder::FecAuto);
}

DvbSTransponder *DvbSFileHelper::readTransponder(ScanFileReader &reader)
{
	if (reader.readString() != "S") {
		return NULL;
	}

	DvbSTransponder *transponder = new DvbSTransponder();

	transponder->frequency = reader.readInt();
	transponder->polarization = reader.readEnum(polarizationMap);
	transponder->symbolRate = reader.readInt();
	transponder->fecRate = reader.readEnum(fecMap);

	if (!reader.isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

class DvbTFileHelper
{
public:
	DvbTFileHelper();
	~DvbTFileHelper() { }

	DvbTTransponder *readTransponder(ScanFileReader &reader);

	bool operator()(DvbTTransponder *x, DvbTTransponder *y) const
	{
		return x->frequency < y->frequency;
	}

private:
	QMap<QString, DvbTTransponder::Bandwidth> bandwidthMap;
	QMap<QString, DvbTTransponder::FecRate> fecHighMap;
	QMap<QString, DvbTTransponder::FecRate> fecLowMap;
	QMap<QString, DvbTTransponder::Modulation> modulationMap;
	QMap<QString, DvbTTransponder::TransmissionMode> transmissionModeMap;
	QMap<QString, DvbTTransponder::GuardInterval> guardIntervalMap;
	QMap<QString, DvbTTransponder::Hierarchy> hierarchyMap;
};

DvbTFileHelper::DvbTFileHelper()
{
	bandwidthMap.insert("6MHz", DvbTTransponder::Bandwidth6Mhz);
	bandwidthMap.insert("7MHz", DvbTTransponder::Bandwidth7Mhz);
	bandwidthMap.insert("8MHz", DvbTTransponder::Bandwidth8Mhz);
	bandwidthMap.insert("AUTO", DvbTTransponder::BandwidthAuto);

	fecHighMap.insert("1/2", DvbSTransponder::Fec1_2);
	fecHighMap.insert("2/3", DvbSTransponder::Fec2_3);
	fecHighMap.insert("3/4", DvbSTransponder::Fec3_4);
	fecHighMap.insert("4/5", DvbSTransponder::Fec4_5);
	fecHighMap.insert("5/6", DvbSTransponder::Fec5_6);
	fecHighMap.insert("6/7", DvbSTransponder::Fec6_7);
	fecHighMap.insert("7/8", DvbSTransponder::Fec7_8);
	fecHighMap.insert("8/9", DvbSTransponder::Fec8_9);
	fecHighMap.insert("AUTO", DvbSTransponder::FecAuto);

	fecLowMap.insert("NONE", DvbSTransponder::FecNone);

	modulationMap.insert("QPSK", DvbTTransponder::Qpsk);
	modulationMap.insert("QAM16", DvbTTransponder::Qam16);
	modulationMap.insert("QAM64", DvbTTransponder::Qam64);
	modulationMap.insert("AUTO", DvbTTransponder::ModulationAuto);

	transmissionModeMap.insert("2k", DvbTTransponder::TransmissionMode2k);
	transmissionModeMap.insert("8k", DvbTTransponder::TransmissionMode8k);
	transmissionModeMap.insert("AUTO", DvbTTransponder::TransmissionModeAuto);

	guardIntervalMap.insert("1/4", DvbTTransponder::GuardInterval1_4);
	guardIntervalMap.insert("1/8", DvbTTransponder::GuardInterval1_8);
	guardIntervalMap.insert("1/16", DvbTTransponder::GuardInterval1_16);
	guardIntervalMap.insert("1/32", DvbTTransponder::GuardInterval1_32);
	guardIntervalMap.insert("AUTO", DvbTTransponder::GuardIntervalAuto);

	hierarchyMap.insert("NONE", DvbTTransponder::HierarchyNone);
}

DvbTTransponder *DvbTFileHelper::readTransponder(ScanFileReader &reader)
{
	if (reader.readString() != "T") {
		return NULL;
	}

	DvbTTransponder *transponder = new DvbTTransponder();

	transponder->frequency = reader.readInt();
	transponder->bandwidth = reader.readEnum(bandwidthMap);
	transponder->fecRateHigh = reader.readEnum(fecHighMap);
	transponder->fecRateLow = reader.readEnum(fecLowMap);
	transponder->modulation = reader.readEnum(modulationMap);
	transponder->transmissionMode = reader.readEnum(transmissionModeMap);
	transponder->guardInterval = reader.readEnum(guardIntervalMap);
	transponder->hierarchy = reader.readEnum(hierarchyMap);

	if (!reader.isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

class AtscFileHelper
{
public:
	AtscFileHelper();
	~AtscFileHelper() { }

	AtscTransponder *readTransponder(ScanFileReader &reader);

	bool operator()(AtscTransponder *x, AtscTransponder *y) const
	{
		return x->frequency < y->frequency;
	}

private:
	QMap<QString, AtscTransponder::Modulation> modulationMap;
};

AtscFileHelper::AtscFileHelper()
{
	modulationMap.insert("8VSB", AtscTransponder::Vsb8);
	modulationMap.insert("16VSB", AtscTransponder::Vsb16);
	modulationMap.insert("QAM64", AtscTransponder::Qam64);
	modulationMap.insert("QAM256", AtscTransponder::Qam256);
	modulationMap.insert("AUTO", AtscTransponder::ModulationAuto);
}

AtscTransponder *AtscFileHelper::readTransponder(ScanFileReader &reader)
{
	if (reader.readString() != "A") {
		return NULL;
	}

	AtscTransponder *transponder = new AtscTransponder();

	transponder->frequency = reader.readInt();
	transponder->modulation = reader.readEnum(modulationMap);

	if (!reader.isValid()) {
		delete transponder;
		return NULL;
	}

	return transponder;
}

template<class T1, class T2> void readScanDirectory(QTextStream &out, const QString &dirName)
{
	QDir dir(dirName);

	if (!dir.exists()) {
		qCritical() << "Error: can't open directory" << dirName;
		return;
	}

	T2 scanFileHelper;

	foreach (const QString &fileName, dir.entryList(QDir::Files, QDir::Name)) {
		QFile file(dir.filePath(fileName));

		if (!file.open(QIODevice::ReadOnly)) {
			qCritical() << "Error: can't open file" << file.fileName();
			return;
		}

		QTextStream stream(&file);
		stream.setCodec("UTF-8");
		QList<T1 *> list;
		bool superfluousChar = false;

		while (!stream.atEnd()) {
			QString line = stream.readLine();

			int pos = line.indexOf('#');

			if (pos != -1) {
				while ((pos > 0) && (line[pos - 1] == ' ')) {
					--pos;
				}

				line = line.left(pos);
			}

			if (line.isEmpty()) {
				continue;
			}

			ScanFileReader reader(line);

			T1 *transponder = scanFileHelper.readTransponder(reader);

			if (transponder == NULL) {
				qCritical() << "Error: can't parse file" << file.fileName();
				qDeleteAll(list);
				return;
			}

			if (!reader.checkEnd()) {
				superfluousChar = true;
			}

			list.append(transponder);
		}

		if (superfluousChar) {
			qWarning() << "Warning: superfluous characters in file" << file.fileName();
		}

		if (!list.isEmpty()) {
			QString name = dir.dirName() + '/' + fileName;

			if (name.startsWith("dvb-s")) {
				// use upper case for orbital position
				name[name.size() - 1] = name.at(name.size() - 1).toUpper();
			}

			out << "[" << name << "]\n";

			qStableSort(list.begin(), list.end(), scanFileHelper);

			foreach (const T1 *transponder, list) {
				DvbLineWriter writer;
				writer.writeTransponder(transponder);
				out << writer.getLine();
			}
		} else {
			qWarning() << "Warning: no transponder found in file" << file.fileName();
		}

		qDeleteAll(list);
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

	QString baseDir(argv[1]);

	readScanDirectory<DvbCTransponder, DvbCFileHelper>(out, baseDir + "/dvb-c");
	readScanDirectory<DvbSTransponder, DvbSFileHelper>(out, baseDir + "/dvb-s");
	readScanDirectory<DvbTTransponder, DvbTFileHelper>(out, baseDir + "/dvb-t");
	readScanDirectory<AtscTransponder, AtscFileHelper>(out, baseDir + "/atsc");

	out << "# sha1sum " << flush;
	out << QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex() << '\n' << flush;

	QFile file(argv[2]);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCritical() << "Error: can't open file" << file.fileName();
	}

	file.write(data);

	return 0;
}
