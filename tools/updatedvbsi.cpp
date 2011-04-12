/*
 * updatedvbsi.cpp
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
#include <QDomDocument>
#include <QFile>
#include <QRegExp>

class Element
{
public:
	enum Type
	{
		Bool,
		Int,
		List
	};

	Element() { }
	~Element() { }

	QString toString() const;

	Type type;
	QString name;
	int bits;
	int bitIndex;
	QString offsetString;
	QString lengthFunc;
	QString listType;
};

QString Element::toString() const
{
	if (type == Bool) {
		int bitsLeft = 8 - (bitIndex % 8);
		int andValue = (1 << (bitsLeft - 1));
		return (QString("(at(") + QString::number(bitIndex / 8) + offsetString + ") & 0x" + QString::number(andValue, 16) + ") != 0");
	}

	if (type == List) {
		return (QString::number(bitIndex / 8) + offsetString);
	}

	QString result;
	int currentBitIndex = bitIndex;
	int currentBits = bits;

	while (true) {
		int bitsLeft = 8 - (currentBitIndex % 8);
		int andValue = (1 << (bitsLeft)) - 1;
		int leftShift = currentBits - bitsLeft;

		if (leftShift != 0) {
			result += '(';
		}

		if (andValue != 255) {
			result += '(';
		}

		result += "at(" + QString::number(currentBitIndex / 8) + offsetString + ')';

		if (andValue != 255) {
			result += " & 0x" + QString::number(andValue, 16) + ')';
		}

		if (leftShift != 0) {
			if (leftShift > 0) {
				result += " << " + QString::number(leftShift) + ')';
			} else {
				result += " >> " + QString::number(-leftShift) + ')';
			}
		}

		if (currentBits <= bitsLeft) {
			break;
		}

		result += " | ";
		currentBitIndex += bitsLeft;
		currentBits -= bitsLeft;
	}

	return result;
}

QTextStream &operator<<(QTextStream &stream, const Element &element)
{
	stream << element.toString();
	return stream;
}

class SiXmlParser
{
public:
	enum Type
	{
		Descriptor,
		Entry,
		Section
	};

	static void parseEntry(QDomNode node, Type type, QTextStream &headerStream, QTextStream &cppStream);
};

void SiXmlParser::parseEntry(QDomNode node, Type type, QTextStream &headerStream, QTextStream &cppStream)
{
	QList<Element> elements;
	int bitIndex = 0;
	int minBits = 0;
	QString offsetString;

	switch (type) {
	case Descriptor:
		bitIndex = 2 * 8;
		minBits = 2 * 8;
		break;

	case Entry:
		bitIndex = 0;
		minBits = 0;
		break;

	case Section:
		bitIndex = 8 * 8;
		minBits = 12 * 8;
		break;
	}

	for (QDomNode child = node.firstChild(); !child.isNull(); child = child.nextSibling()) {
		QDomNamedNodeMap attributes = child.attributes();

		Element element;
		element.name = child.nodeName();
		element.bits = attributes.namedItem("bits").nodeValue().toInt();
		element.bitIndex = bitIndex;
		element.offsetString = offsetString;

		QString type = attributes.namedItem("type").nodeValue();

		if ((element.name == "unused") || (type == "int")) {
			if (element.bits <= 0) {
				qCritical() << "invalid number of bits";
				return;
			}

			element.type = Element::Int;
		} else if (type == "bool") {
			element.type = Element::Bool;

			if (element.bits != 1) {
				qCritical() << "invalid number of bits";
				return;
			}
		} else if (type == "list") {
			element.type = Element::List;
			element.bits = 0;
			element.lengthFunc = attributes.namedItem("lengthFunc").nodeValue();
			element.listType = attributes.namedItem("listType").nodeValue();

			if ((element.bitIndex % 8) != 0) {
				qCritical() << "list doesn't start at a byte boundary";
				return;
			}

			offsetString += " + " + element.name + "Length";
		} else {
			qCritical() << "unknown type:" << type;
			return;
		}

		bitIndex += element.bits;
		minBits += element.bits;

		if (element.name != "unused") {
			elements.append(element);
		}
	}

	if ((minBits % 8) != 0) {
		qCritical() << "minBits isn't a multiple of 8";
		return;
	}

	QString entryName = node.nodeName();
	QString initFunctionName = QString(entryName).replace(QRegExp("^Dvb|^Atsc"), "init");
	bool ignoreFirstNewLine = false;

	switch (type) {
	case Descriptor:
		cppStream << "\n";
		cppStream << entryName << "::" << entryName << "(const DvbDescriptor &descriptor) : DvbDescriptor(descriptor)\n";
		cppStream << "{\n";
		cppStream << "\tif (getLength() < " << (minBits / 8) << ") {\n";
		cppStream << "\t\tkDebug() << \"invalid descriptor\";\n";
		cppStream << "\t\tinitSectionData();\n";
		cppStream << "\t\treturn;\n";
		cppStream << "\t}\n";
		break;

	case Entry: {
		cppStream << "\n";
		cppStream << "void " << entryName << "::" << initFunctionName << "(const char *data, int size)\n";
		cppStream << "{\n";
		cppStream << "\tif (size < " << (minBits / 8) << ") {\n";
		cppStream << "\t\tif (size != 0) {\n";
		cppStream << "\t\t\tkDebug() << \"invalid entry\";\n";
		cppStream << "\t\t}\n";
		cppStream << "\n";
		cppStream << "\t\tinitSectionData();\n";
		cppStream << "\t\treturn;\n";
		cppStream << "\t}\n";
		cppStream << "\n";

		bool fixedLength = true;

		for (int i = 0; i < elements.size(); ++i) {
			const Element &element = elements.at(i);

			if (element.name != "entryLength") {
				continue;
			}

			if (((element.bitIndex + element.bits) % 8) != 0) {
				qCritical() << "entry length doesn't start at a byte boundary";
				return;
			}

			QString entryLengthCalculation = element.toString();

			while (true) {
				int oldSize = entryLengthCalculation.size();
				entryLengthCalculation.replace(QRegExp("at\\(([0-9]*)\\)"), "static_cast<unsigned char>(data[\\1])");

				if (entryLengthCalculation.size() == oldSize) {
					break;
				}
			}

			cppStream << "\tint entryLength = ((" << entryLengthCalculation << ") + " << ((element.bitIndex + element.bits) / 8) << ");\n";
			cppStream << "\n";
			cppStream << "\tif (entryLength > size) {\n";
			cppStream << "\t\tkDebug() << \"adjusting length\";\n";
			cppStream << "\t\tentryLength = size;\n";
			cppStream << "\t}\n";
			cppStream << "\n";
			cppStream << "\tinitSectionData(data, entryLength, size);\n";
			ignoreFirstNewLine = true;

			elements.removeAt(i);
			fixedLength = false;
			break;
		}

		if (fixedLength) {
			cppStream << "\tinitSectionData(data, " << (minBits / 8) << ", size);\n";
			ignoreFirstNewLine = true;
		}

		break;
	    }

	case Section:
		cppStream << "\n";
		cppStream << "void " << entryName << "::" << initFunctionName << "(const char *data, int size)\n";
		cppStream << "{\n";
		cppStream << "\tif (size < " << (minBits / 8) << ") {\n";
		cppStream << "\t\tinitSectionData();\n";
		cppStream << "\t\treturn;\n";
		cppStream << "\t}\n";
		cppStream << "\n";
		cppStream << "\tinitStandardSection(data, size);\n";
		ignoreFirstNewLine = true;
		break;
	}

	QList<QString> privateVars;

	for (int i = 0; i < elements.size(); ++i) {
		const Element &element = elements.at(i);

		if ((element.type != Element::List) || element.lengthFunc.isEmpty()) {
			continue;
		}

		if ((i <= 0) || (elements.at(i - 1).name != element.lengthFunc)) {
			qCritical() << "lengthFunc isn't a valid back reference";
			return;
		}

		if (ignoreFirstNewLine) {
			ignoreFirstNewLine = false;
		} else {
			cppStream << "\n";
		}

		cppStream << "\t" << element.name << "Length = " << elements.at(i - 1) << ";\n";
		cppStream << "\n";

		if (element.offsetString.isEmpty()) {
			cppStream << "\tif (" << element.name << "Length > (getLength() - " << (minBits / 8) << ")) {\n";
			cppStream << "\t\tkDebug() << \"adjusting length\";\n";
			cppStream << "\t\t" << element.name << "Length = (getLength() - " << (minBits / 8) << ");\n";
		} else {
			cppStream << "\tif (" << element.name << "Length > (getLength() - (" << (minBits / 8) << element.offsetString << "))) {\n";
			cppStream << "\t\tkDebug() << \"adjusting length\";\n";
			cppStream << "\t\t" << element.name << "Length = (getLength() - (" << (minBits / 8) << element.offsetString << "));\n";
		}

		cppStream << "\t}\n";

		elements.removeAt(i - 1);
		--i;

		privateVars.append(element.name + "Length");
	}

	cppStream << "}\n";

	switch (type) {
	case Descriptor:
		headerStream << "\n";
		headerStream << "class " << entryName << " : public DvbDescriptor\n";
		headerStream << "{\n";
		headerStream << "public:\n";
		headerStream << "\texplicit " << entryName << "(const DvbDescriptor &descriptor);\n";
		break;

	case Entry:
		headerStream << "\n";
		headerStream << "class " << entryName << " : public DvbSectionData\n";
		headerStream << "{\n";
		headerStream << "public:\n";
		headerStream << "\t" << entryName << "(const char *data, int size)\n";
		headerStream << "\t{\n";
		headerStream << "\t\t" << initFunctionName << "(data, size);\n";
		headerStream << "\t}\n";
		headerStream << "\n";
		break;

	case Section:
		headerStream << "\n";
		headerStream << "class " << entryName << " : public DvbStandardSection\n";
		headerStream << "{\n";
		headerStream << "public:\n";
		headerStream << "\t" << entryName << "(const char *data, int size)\n";
		headerStream << "\t{\n";
		headerStream << "\t\t" << initFunctionName << "(data, size);\n";
		headerStream << "\t}\n";
		headerStream << "\n";
		headerStream << "\texplicit " << entryName << "(const QByteArray &byteArray)\n";
		headerStream << "\t{\n";
		headerStream << "\t\t" << initFunctionName << "(byteArray.constData(), byteArray.size());\n";
		headerStream << "\t}\n";
		headerStream << "\n";
		break;
	}

	headerStream << "\t~" << entryName << "() { }\n";

	if (type == Entry) {
		headerStream << "\n";
		headerStream << "\tvoid advance()\n";
		headerStream << "\t{\n";
		headerStream << "\t\t" << initFunctionName << "(getData() + getLength(), getSize() - getLength());\n";
		headerStream << "\t}\n";
	}

	if (type == Section) {
		QString extension = node.attributes().namedItem("extension").nodeValue();

		if (!extension.isEmpty()) {
			headerStream << "\n";
			headerStream << "\tint " << extension << "() const\n";
			headerStream << "\t{\n";
			headerStream << "\t\treturn (at(3) << 8) | at(4);\n";
			headerStream << "\t}\n";
		}
	}

	foreach (const Element &element, elements) {
		switch (element.type) {
		case Element::Bool:
			headerStream << "\n";
			headerStream << "\tbool " << element.name << "() const\n";
			headerStream << "\t{\n";
			headerStream << "\t\treturn (" << element << ");\n";
			headerStream << "\t}\n";
			break;

		case Element::Int:
			headerStream << "\n";
			headerStream << "\tint " << element.name << "() const\n";
			headerStream << "\t{\n";
			headerStream << "\t\treturn " << element << ";\n";
			headerStream << "\t}\n";
			break;

		case Element::List:
			if (element.listType == "unused") {
				break;
			}

			if (element.listType == "DvbString") {
				headerStream << "\n";
				headerStream << "\tQString " << element.name << "() const\n";
				headerStream << "\t{\n";
				headerStream << "\t\treturn DvbSiText::convertText(getData() + " << element << ", ";
			} else if (element.listType == "AtscString") {
				headerStream << "\n";
				headerStream << "\tQString " << element.name << "() const\n";
				headerStream << "\t{\n";
				headerStream << "\t\treturn AtscPsipText::convertText(getData() + " << element << ", ";
			} else {
				headerStream << "\n";
				headerStream << "\t" << element.listType << " " << element.name << "() const\n";
				headerStream << "\t{\n";
				headerStream << "\t\treturn " << element.listType << "(getData() + " << element << ", ";
			}

			if (element.lengthFunc.isEmpty()) {
				if (element.offsetString.isEmpty()) {
					headerStream << "getLength() - " << (minBits / 8) << ");\n";
				} else {
					headerStream << "getLength() - (" << (minBits / 8) << element.offsetString << "));\n";
				}
			} else {
				headerStream << element.name << "Length);\n";
			}

			headerStream << "\t}\n";
			break;
		}
	}

	headerStream << "\n";
	headerStream << "private:\n";

	if ((type == Descriptor) || (type == Section)) {
		headerStream << "\tQ_DISABLE_COPY(" << entryName << ")\n";
	}

	if ((type == Entry) || (type == Section)) {
		headerStream << "\tvoid " << initFunctionName << "(const char *data, int size);\n";
	}

	if (!privateVars.isEmpty()) {
		headerStream << '\n';

		foreach (const QString &privateVar, privateVars) {
			headerStream << "\tint " << privateVar << ";\n";
		}
	}

	headerStream << "};\n";
}

class FileHelper
{
public:
	explicit FileHelper(const QString &name);

	bool isValid() const
	{
		return valid;
	}

	QTextStream &getStream()
	{
		return outStream;
	}

	void removeOrig()
	{
		if (!QFile::remove(origName)) {
			qCritical() << "can't remove" << origName;
		}
	}

private:
	bool valid;
	QString origName;
	QFile outFile;
	QTextStream outStream;
};

FileHelper::FileHelper(const QString &name) : valid(false)
{
	origName = name + ".orig";

	if (!QFile::rename(name, origName)) {
		qCritical() << "can't rename" << name << "to" << origName;
		return;
	}

	QFile inFile(origName);

	if (!inFile.open(QIODevice::ReadOnly)) {
		qCritical() << "can't open file" << inFile.fileName();
		return;
	}

	outFile.setFileName(name);

	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCritical() << "can't open file" << outFile.fileName();
	}

	QTextStream inStream(&inFile);
	inStream.setCodec("UTF-8");

	outStream.setDevice(&outFile);
	outStream.setCodec("UTF-8");

	while (true) {
		if (inStream.atEnd()) {
			qCritical() << "can't find autogeneration boundary in" << inFile.fileName();
			return;
		}

		QString line = inStream.readLine();
		outStream << line << "\n";

		if (line == "// everything below this line is automatically generated") {
			break;
		}
	}

	valid = true;
}

int main()
{
	QFile xmlFile("dvbsi.xml");

	if (!xmlFile.open(QIODevice::ReadOnly)) {
		qCritical() << "can't open file" << xmlFile.fileName();
		return 1;
	}

	QDomDocument document;

	if (!document.setContent(&xmlFile)) {
		qCritical() << "can't read file" << xmlFile.fileName();
		return 1;
	}

	QDomElement root = document.documentElement();

	if (root.nodeName() != "dvbsi") {
		qCritical() << "invalid root name";
		return 1;
	}

	FileHelper headerFile("../src/dvb/dvbsi.h");

	if (!headerFile.isValid()) {
		return 1;
	}

	FileHelper cppFile("../src/dvb/dvbsi.cpp");

	if (!cppFile.isValid()) {
		return 1;
	}

	QTextStream &headerStream = headerFile.getStream();
	QTextStream &cppStream = cppFile.getStream();

	for (QDomNode child = root.firstChild(); !child.isNull(); child = child.nextSibling()) {
		if (child.nodeName() == "descriptors") {
			for (QDomNode grandChild = child.firstChild(); !grandChild.isNull();
			     grandChild = grandChild.nextSibling()) {
				SiXmlParser::parseEntry(grandChild, SiXmlParser::Descriptor, headerStream, cppStream);
			}
		} else if (child.nodeName() == "entries") {
			for (QDomNode grandChild = child.firstChild(); !grandChild.isNull();
			     grandChild = grandChild.nextSibling()) {
				SiXmlParser::parseEntry(grandChild, SiXmlParser::Entry, headerStream, cppStream);
			}
		} else if (child.nodeName() == "sections") {
			for (QDomNode grandChild = child.firstChild(); !grandChild.isNull();
			     grandChild = grandChild.nextSibling()) {
				SiXmlParser::parseEntry(grandChild, SiXmlParser::Section, headerStream, cppStream);
			}
		} else {
			qCritical() << "unknown entry:" << child.nodeName();
		}
	}

	headerStream << "\n";
	headerStream << "#endif /* DVBSI_H */\n";

	headerFile.removeOrig();
	cppFile.removeOrig();

	xmlFile.close();

	if (xmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		xmlFile.write(document.toByteArray(2));
	}

	return 0;
}
