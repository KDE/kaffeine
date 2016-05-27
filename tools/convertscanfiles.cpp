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

#include <QDebug>
#if QT_VERSION < 0x050500
# define qInfo qDebug
#endif

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QRegularExpression>

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

class parseDvbv5
{
public:
	bool parseInputLine(QString line);
	void resetParser();
	QString outputLine();
	parseDvbv5(QString name);
	int getPos();
	bool hasTransponder;
	DvbTransponderBase::TransmissionType type;

private:
	QString name;

	int lineno;

	QString delsys = "";
	QString frq = "";
	QString modulation = "";
	QString symbolRate = "";
	QString fec = "";
	QString polar = "";
	QString inversion = "";
	QString rollOff = "";
	QString plscode = "";
	QString plsmode = "";
	QString bandwith = "";
	QString fec_hi = "";
	QString fec_lo = "";
	QString t_mode = "";
	QString g_interval = "";
	QString hierarchy = "";

	// ISDB-T specific fields

	QString isdbtLayerEnabled = "";
	QString isdbtPartialReception = "";
	QString isdbtSb = "";
	QString isdbtSbSubchId = "";
	QString isdbtSbSegIdx = "";
	QString isdbtSbSegCount = "";
	QString isdbtLayerAFec = "";
	QString isdbtLayerAModulation = "";
	QString isdbtLayerASegCount = "";
	QString isdbtLayerAInterleaving = "";
	QString isdbtLayerBFec = "";
	QString isdbtLayerBModulation = "";
	QString isdbtLayerBSegCount = "";
	QString isdbtLayerBInterleaving = "";
	QString isdbtLayerCFec = "";
	QString isdbtLayerCModulation = "";
	QString isdbtLayerCSegCount = "";
	QString isdbtLayerCInterleaving = "";
	int isdbtLayers = 0;
	int streamid = 0;
};

int parseDvbv5::getPos()
{
	return lineno;
};

parseDvbv5::parseDvbv5(QString name)
{
	type = DvbTransponderBase::Invalid;
	hasTransponder = false;

	this->name = name;

	lineno = 0;
};

void parseDvbv5::resetParser()
{
	delsys = "";
	frq = "";
	modulation = "";
	symbolRate = "";
	fec = "";
	polar = "";
	inversion = "";
	rollOff = "";
	plscode = "";
	plsmode = "";
	bandwith = "";
	fec_hi = "";
	fec_lo = "";
	t_mode = "";
	g_interval = "";
	hierarchy = "";

	isdbtLayerEnabled = "";
	isdbtPartialReception = "";
	isdbtSb = "";
	isdbtSbSubchId = "";
	isdbtSbSegIdx = "";
	isdbtSbSegCount = "";
	isdbtLayerAFec = "";
	isdbtLayerAModulation = "";
	isdbtLayerASegCount = "";
	isdbtLayerAInterleaving = "";
	isdbtLayerBFec = "";
	isdbtLayerBModulation = "";
	isdbtLayerBSegCount = "";
	isdbtLayerBInterleaving = "";
	isdbtLayerCFec = "";
	isdbtLayerCModulation = "";
	isdbtLayerCSegCount = "";
	isdbtLayerCInterleaving = "";
	isdbtLayers = 0;
	streamid = 0;
}

bool parseDvbv5::parseInputLine(QString line)
{
	lineno++;

	QRegularExpression rejex = QRegularExpression("^\\s*\\[(.*)]");
	if (line.contains(rejex)) {
		bool oldHasTransponder = hasTransponder;
		hasTransponder = true;
		return oldHasTransponder;
	}

	int pos = line.indexOf('#');

	if (pos != -1) {
		while ((pos > 0) && (line[pos - 1] == ' ')) {
			--pos;
		}

		line.truncate(pos);
	}

	if (line.isEmpty()) {
		return false;
	}

	if (line.contains("DELIVERY_SYSTEM")) {
		delsys = line.split(" = ")[1];
		if (!delsys.compare("ATSC", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::Atsc;
		} else if (!delsys.compare("DVBC/ANNEX_A", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::DvbC;
		} else if (!delsys.compare("DVBC/ANNEX_B", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::Atsc;
		} else if (!delsys.compare("DVBS", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::DvbS;
		} else if (!delsys.compare("DVBS2", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::DvbS2;
		} else if (!delsys.compare("DVBT", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::DvbT;
		} else if (!delsys.compare("DVBT2", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::DvbT2;
		} else if (!delsys.compare("ISDBT", Qt::CaseInsensitive)) {
			type = DvbTransponderBase::IsdbT;
		} else {
			type = DvbTransponderBase::Invalid;
		}
		return false;
	}
	if (line.contains("FREQUENCY = ")) {
		frq = line.split(" = ")[1];
		return false;
	}
	if (line.contains("INNER_FEC")) {
		fec = line.split(" = ")[1];
		return false;
	}
	if (line.contains("SYMBOL_RATE")) {
		symbolRate = line.split(" = ")[1];
		return false;
	}
	if (line.contains("MODULATION")) {
		modulation = line.split(" = ")[1];
		return false;
	}
	if (line.contains("POLARIZATION")) {
		polar = line.split(" = ")[1];
		return false;
	}
	if (line.contains("INVERSION")) {
		inversion = line.split(" = ")[1];
		return false;
	}
	if (line.contains("ROLLOFF")) {
		rollOff = line.split(" = ")[1];
		return false;
	}
	if (line.contains("STREAM_ID")) {
		streamid = line.split(" = ")[1].toInt();
		return false;
	}
	if (line.contains("PLS_CODE")) {
		plscode = line.split(" = ")[1];
		return false;
	}
	if (line.contains("PLS_MODE")) {
		plsmode = line.split(" = ")[1];
		return false;
	}
	if (line.contains("BANDWIDTH_HZ")) {
		bandwith = line.split(" = ")[1];
		return false;
	}
	if (line.contains("TRANSMISSION_MODE")) {
		t_mode = line.split(" = ")[1];
		return false;
	}
	if (line.contains("CODE_RATE_HP")) {
		fec_hi = line.split(" = ")[1];
		return false;
	}
	if (line.contains("CODE_RATE_LP")) {
		fec_lo = line.split(" = ")[1];
		return false;
	}
	if (line.contains("HIERARCHY")) {
		hierarchy = line.split(" = ")[1];
		return false;
	}
	if (line.contains("GUARD_INTERVAL")) {
		g_interval = line.split(" = ")[1];
		return false;
	}
	if (line.contains("ISDBT_LAYER_ENABLED")) {
		isdbtLayerEnabled = line.split(" = ")[1];
		return false;
	}
	if (line.contains("ISDBT_PARTIAL_RECEPTION")) {
		isdbtPartialReception = line.split(" = ")[1];
		return false;
	}
	if (line.contains("ISDBT_SOUND_BROADCASTING")) {
		isdbtSb = line.split(" = ")[1];
		return false;
	}
	if (line.contains("ISDBT_SB_SUBCHANNEL_ID")) {
		isdbtSbSubchId = line.split(" = ")[1];
		return false;
	}
	if (line.contains("ISDBT_SB_SEGMENT_IDX")) {
		isdbtSbSegIdx = line.split(" = ")[1];
		return false;
	}
	if (line.contains("ISDBT_SB_SEGMENT_COUNT")) {
		isdbtSbSegCount = line.split(" = ")[1];
		return false;
	}
	// Layer A
	if (line.contains("ISDBT_LAYERA_FEC")) {
		isdbtLayerAFec = line.split(" = ")[1];
		isdbtLayers |= 1;
		return false;
	}
	if (line.contains("ISDBT_LAYERA_MODULATION")) {
		isdbtLayerAModulation = line.split(" = ")[1];
		isdbtLayers |= 1;
		return false;
	}
	if (line.contains("ISDBT_LAYERA_SEGMENT_COUNT")) {
		isdbtLayerASegCount = line.split(" = ")[1];
		isdbtLayers |= 1;
		return false;
	}
	if (line.contains("ISDBT_LAYERA_TIME_INTERLEAVING")) {
		isdbtLayerAInterleaving = line.split(" = ")[1];
		isdbtLayers |= 1;
		return false;
	}
	// Layer B
	if (line.contains("ISDBT_LAYERB_FEC")) {
		isdbtLayerBFec = line.split(" = ")[1];
		isdbtLayers |= 2;
		return false;
	}
	if (line.contains("ISDBT_LAYERB_MODULATION")) {
		isdbtLayerBModulation = line.split(" = ")[1];
		isdbtLayers |= 2;
		return false;
	}
	if (line.contains("ISDBT_LAYERB_SEGMENT_COUNT")) {
		isdbtLayerBSegCount = line.split(" = ")[1];
		isdbtLayers |= 2;
		return false;
	}
	if (line.contains("ISDBT_LAYERB_TIME_INTERLEAVING")) {
		isdbtLayerBInterleaving = line.split(" = ")[1];
		isdbtLayers |= 2;
		return false;
	}
	// Layer C
	if (line.contains("ISDBT_LAYERC_FEC")) {
		isdbtLayerCFec = line.split(" = ")[1];
		isdbtLayers |= 4;
		return false;
	}
	if (line.contains("ISDBT_LAYERC_MODULATION")) {
		isdbtLayerCModulation = line.split(" = ")[1];
		isdbtLayers |= 4;
		return false;
	}
	if (line.contains("ISDBT_LAYERC_SEGMENT_COUNT")) {
		isdbtLayerCSegCount = line.split(" = ")[1];
		isdbtLayers |= 4;
		return false;
	}
	if (line.contains("ISDBT_LAYERC_TIME_INTERLEAVING")) {
		isdbtLayerCInterleaving = line.split(" = ")[1];
		isdbtLayers |= 4;
		return false;
	}
	qWarning() << "Can't parse line" << lineno << ":" << line << " for " << name;
	return false;
};


QString parseDvbv5::outputLine()
{
	QString line = "";

	if (frq.isEmpty()) {
		qWarning() << "frequency is empty  in pos "
				<< lineno << " file" << name;
		return line;
	}

	switch (type) {
	case DvbTransponderBase::Invalid:
		if (!hasTransponder)
			return "";
		qWarning() << "Invalid transponder type in pos "
				<< lineno << " file" << name;
		return line;
	case DvbTransponderBase::DvbC: {
		if (symbolRate.isEmpty()) {
			qWarning() << "No symbol rate in pos "
					<< lineno << " file" << name;
			return line;
		}
		if (modulation.isEmpty()) {
			qWarning() << "No symbol rate in pos "
					<< lineno << " file" << name;
			return line;
		}
		line = "C " + frq + " " + symbolRate + " " + fec + " " + modulation.replace("/", "");
		return line;
	}
	case DvbTransponderBase::DvbS: {
		if (rollOff.isEmpty() && (modulation.isEmpty() || !modulation.compare("QPSK"))) {
			line = "S " + frq + " " + polar[0] + " " + symbolRate + " " + fec;
			return line;
		}
		// Fall though, because it is not DvbS
		type = DvbTransponderBase::DvbS2;
	}
	case DvbTransponderBase::DvbS2: {
		if (rollOff.isEmpty())
			rollOff = "25";

		if (modulation.isEmpty()) {
			modulation = "AUTO";
		} if (modulation.contains("/")) {
			QString temp1 = modulation.split("/")[0];
			QString temp2 = modulation.split("/")[1];
			modulation = temp2 + temp1;
		}

		line = "S2 " + frq + " " + polar[0] + " " + symbolRate + " " + fec + " " + rollOff + " " + modulation;

		return line;
	}
	case DvbTransponderBase::DvbT: {
		line = "T " + frq;
		if (!bandwith.isEmpty()) {
			int number = bandwith.toInt();
			number = number / 1000000;
			line += " " + QString::number(number) + "MHz";
		}
		if (!fec_hi.isEmpty()) {
			line += " " + fec_hi;
		} else {
			line += " AUTO";
		}
		if (!fec_lo.isEmpty()) {
			line += " " + fec_lo;
		} else {
			line += " AUTO";
		}
		if (!modulation.isEmpty()) {
			line += " " + modulation.replace("/", "").replace("QAMAUTO", "AUTO");
		} else {
			line += " AUTO";
		}
		if (!t_mode.isEmpty()) {
			line += " " + t_mode.replace("K", "k");
		} else {
			line += " AUTO";
		}
		if (!g_interval.isEmpty()) {
			line += " " + g_interval;
		} else {
			line += " AUTO";
		}
		if (!hierarchy.isEmpty()) {
			line += " " + hierarchy;
		} else {
			line += " AUTO";
		}
		return line;
	}
	case DvbTransponderBase::DvbT2: {
		line = "T2 " + frq;
		if (!bandwith.isEmpty()) {
			int number = bandwith.toInt();
			number = number / 1000000;
			line += " " + QString::number(number) + "MHz";
		}
		if (!fec_hi.isEmpty()) {
			line += " " + fec_hi;
		} else {
			line += " AUTO";
		}
		if (!fec_lo.isEmpty()) {
			line += " " + fec_lo;
		} else {
			line += " AUTO";
		}
		if (!modulation.isEmpty()) {
			line += " " + modulation.replace("/", "").replace("QAMAUTO", "AUTO");
		} else {
			line += " AUTO";
		}
		if (!t_mode.isEmpty()) {
			line += " " + t_mode.replace("K", "k");
		} else {
			line += " AUTO";
		}
		if (!g_interval.isEmpty()) {
			line += " " + g_interval;
		} else {
			line += " AUTO";
		}
		if (!hierarchy.isEmpty()) {
			line += " " + hierarchy;
		} else {
			line += " AUTO";
		}
		line += " " + QString::number(streamid);
		return line;
	}
	case DvbTransponderBase::Atsc: {
		line = "A " + frq;
		if (!modulation.isEmpty()) {
			QString temp1 = modulation.split("/")[0];
			QString temp2 = modulation.split("/")[1];
			if (!(temp1 == "QAM")) {
				line += " " + temp2 + temp1;
			} else {
				line += " " + temp1 + temp2;
			}
		} else {
			line += " AUTO";
		}
		return line;
	}
	case DvbTransponderBase::IsdbT: {
		line = "I " + frq;
		if (!bandwith.isEmpty()) {
			int number = bandwith.toInt();
			number = number / 1000000;
			line += " " + QString::number(number) + "MHz";
		} else {
			line += " 6MHz";
		}
		if (!t_mode.isEmpty()) {
			line += " " + t_mode.replace("K", "k");
		} else {
			line += " AUTO";
		}
		if (!g_interval.isEmpty()) {
			line += " " + g_interval;
		} else {
			line += " AUTO";
		}
		if (!isdbtPartialReception.isEmpty()) {
			line += " " + isdbtPartialReception;
		} else {
			line += " AUTO";
		}
		if (!isdbtSb.isEmpty()) {
			line += " " + isdbtSb;
		} else {
			line += " AUTO";
		}
		if (!isdbtSbSubchId.isEmpty()) {
			line += " " + isdbtSbSubchId;
		} else {
			line += " AUTO";
		}
		if (!isdbtSbSegCount.isEmpty()) {
			line += " " + isdbtSbSegCount;
		} else {
			line += " AUTO";
		}
		if (!isdbtSbSegIdx.isEmpty()) {
			line += " " + isdbtSbSegIdx;
		} else {
			line += " AUTO";
		}

		line += " " + QString::number(isdbtLayers);

		// Layer A
		if (!isdbtLayerAModulation.isEmpty()) {
			line += " " + isdbtLayerAModulation.replace("/", "").replace("QAMAUTO", "AUTO");
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerAFec.isEmpty()) {
			line += " " + isdbtLayerAFec;
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerASegCount.isEmpty()) {
			line += " " + isdbtLayerASegCount;
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerAInterleaving.isEmpty()) {
			line += " " + isdbtLayerAInterleaving;
		} else {
			line += " AUTO";
		}
		// Layer B
		if (!isdbtLayerBModulation.isEmpty()) {
			line += " " + isdbtLayerBModulation.replace("/", "").replace("QAMAUTO", "AUTO");
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerBFec.isEmpty()) {
			line += " " + isdbtLayerBFec;
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerBSegCount.isEmpty()) {
			line += " " + isdbtLayerBSegCount;
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerBInterleaving.isEmpty()) {
			line += " " + isdbtLayerBInterleaving;
		} else {
			line += " AUTO";
		}
		// Layer C
		if (!isdbtLayerCModulation.isEmpty()) {
			line += " " + isdbtLayerCModulation.replace("/", "").replace("QAMAUTO", "AUTO");
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerCFec.isEmpty()) {
			line += " " + isdbtLayerCFec;
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerCSegCount.isEmpty()) {
			line += " " + isdbtLayerCSegCount;
		} else {
			line += " AUTO";
		}
		if (!isdbtLayerCInterleaving.isEmpty()) {
			line += " " + isdbtLayerCInterleaving;
		} else {
			line += " AUTO";
		}
		return line;
	}
	}

	return line;
}

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
		DvbSTransponder transponder;

		if (!transponder.fromString(line)) {
			break;
		}
		if (transponder.fecRate == DvbSTransponder::FecNone) {
			qWarning() << "Warning: fec rate == NONE in file" << fileName;
		}
		// fecRate == AUTO is ok

		return transponder.toString();
	    }
	case DvbTransponderBase::DvbS2: {
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
	case DvbTransponderBase::DvbT2: {
		DvbT2Transponder transponder;

		if (!transponder.fromString(line)) {
			break;
		}
		return transponder.toString();
	    }
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
	case DvbTransponderBase::IsdbT: {
		IsdbTTransponder transponder;

		if (!transponder.fromString(line)) {
			break;
		}

		return transponder.toString();
	    }
	}

	return QString();
}

static void readScanDirectory(QTextStream &out, const QString &path)
{
	QDir dir;

	dir.setPath(path);
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
		QString name = dir.dirName() + '/' + fileName;

		parseDvbv5 parser(name);

		while (!stream.atEnd()) {
			QString line = stream.readLine();

			if (!parser.parseInputLine(line) && !stream.atEnd())
				continue;

			if (!parser.hasTransponder)
				continue;

			QString parsedLine = parser.outputLine();
			parser.resetParser();

			QString string = parseLine(parser.type, parsedLine, fileName);

			if (string.isEmpty()) {
				qCritical() << "Error: can't parse Transponder pos " << parser.getPos() << "line" << parsedLine << "in file" << name;
				continue;
			}

			// reduce multiple spaces to one space

			for (int i = 1; i < parsedLine.length(); ++i) {
				if (parsedLine.at(i - 1) != ' ') {
					continue;
				}

				if (parsedLine.at(i) == ' ') {
					parsedLine.remove(i, 1);
					--i;
				}
			}
			if (parsedLine.at(parsedLine.length() - 1) == ' ') {
					parsedLine.remove(parsedLine.length() - 1, 1);
			}

			if (parsedLine != string) {
				qWarning() << "Warning: suboptimal representation:";
				qWarning() << parsedLine << "<-->";
				qWarning() << string << "in file" << name;
			}

			transponders.append(string);
		}

		if (transponders.isEmpty()) {
			qWarning() << "Warning: no transponder found in file" << name;
			continue;
		}

		if (parser.type == DvbTransponderBase::DvbS ||
		    parser.type == DvbTransponderBase::DvbS2) {
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
				qWarning() << "Warning: invalid orbital position for file" << name;
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

	out << "# this file is automatically generated from https://linuxtv.org/downloads/dtv-scan-tables\n";
	out << "[date]\n";
	out << QDate::currentDate().toString(Qt::ISODate) << '\n';

	QString path(argv[1]);

	readScanDirectory(out, path + "/dvb-c");
	readScanDirectory(out, path + "/dvb-s");
	readScanDirectory(out, path + "/dvb-t");
	readScanDirectory(out, path + "/atsc");
	readScanDirectory(out, path + "/isdb-t");

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
