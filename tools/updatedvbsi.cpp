/*
 * updatedvbsi.cpp
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

#include <QFile>
#include <QDomDocument>
#include <QtDebug>

class Element
{
public:
	enum Type
	{
		Bool,
		Int,
		List,
		String,
		EntryLength
	};

	Element() { }
	~Element() { }

	Type type;
	QString name;
	int bits;
	int bitIndex;
	QString offsetString;
	QString lengthFunc;
	QString listType;
};

QTextStream &operator<<(QTextStream &stream, const Element &element)
{
	if (element.type == Element::Bool) {
		int bitsLeft = 8 - (element.bitIndex % 8);
		int andValue = (1 << (bitsLeft - 1));

		stream << "(at(" << (element.bitIndex / 8) << element.offsetString << ") & 0x" << QString::number(andValue, 16) << ") != 0";

		return stream;
	}

	if ((element.type == Element::List) || (element.type == Element::String)) {
		stream << (element.bitIndex / 8) << element.offsetString;

		return stream;
	}

	int bitIndex = element.bitIndex;
	int bits = element.bits;

	while (true) {
		int bitsLeft = 8 - (bitIndex % 8);
		int andValue = (1 << (bitsLeft)) - 1;
		int leftShift = bits - bitsLeft;

		if (leftShift != 0) {
			stream << "(";
		}

		if (andValue != 255) {
			stream << "(";
		}

		stream << "at(" << (bitIndex / 8) << element.offsetString << ")";

		if (andValue != 255) {
			stream << " & 0x" << QString::number(andValue, 16) << ")";
		}

		if (leftShift != 0) {
			if (leftShift > 0) {
				stream << " << " << leftShift << ")";
			} else {
				stream << " >> " << (-leftShift) << ")";
			}
		}

		if (bits <= bitsLeft) {
			break;
		}

		stream << " | ";
		bitIndex += bitsLeft;
		bits -= bitsLeft;
	}

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

	explicit SiXmlParser(QTextStream &stream_) : stream(stream_) { }
	~SiXmlParser() { }

	bool parse(QDomDocument &document);

private:
	bool parseEntry(QDomNode node, Type type);

	QTextStream &stream;
};

bool SiXmlParser::parse(QDomDocument &document)
{
	QDomElement root = document.documentElement();

	if (root.nodeName() != "dvbsi") {
		qCritical() << "Error: expected \"dvbsi\"";
		return false;
	}

	QDomNode child = root.firstChild();

	if (child.nodeName() == "descriptors") {
		QDomNode entry = child.firstChild();

		while (!entry.isNull()) {
			if (!parseEntry(entry, Descriptor)) {
				return false;
			}

			entry = entry.nextSibling();
		}

		child = child.nextSibling();
	}

	if (child.nodeName() == "entries") {
		QDomNode entry = child.firstChild();

		while (!entry.isNull()) {
			if (!parseEntry(entry, Entry)) {
				return false;
			}

			entry = entry.nextSibling();
		}

		child = child.nextSibling();
	}

	if (child.nodeName() == "sections") {
		QDomNode entry = child.firstChild();

		while (!entry.isNull()) {
			if (!parseEntry(entry, Section)) {
				return false;
			}

			entry = entry.nextSibling();
		}

		child = child.nextSibling();
	}

	if (!child.isNull()) {
		qWarning() << "Warning: unknown elements";
	}

	return true;
}

bool SiXmlParser::parseEntry(QDomNode node, Type type)
{
	QDomNode child = node.firstChild();
	QList<Element> elements;
	int bitIndex;
	int minBits;
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

	default:
		Q_ASSERT(false);
		return false;
	}

	while (!child.isNull()) {
		Element element;
		element.name = child.nodeName();
		element.bitIndex = bitIndex;
		element.offsetString = offsetString;

		QDomNamedNodeMap attributes = child.attributes();
		QString type = attributes.namedItem("type").nodeValue();

		if ((element.name == "unused") || (type == "bool") || (type == "int") || (type == "entryLength")) {
			QString bitString = attributes.namedItem("bits").nodeValue();

			element.bits = bitString.toInt();

			if (bitString.isEmpty() || (element.bits <= 0)) {
				qCritical() << "Error: invalid \"bits\"";
				return false;
			}

			if (type == "entryLength") {
				element.type = Element::EntryLength;
			} else if (type == "bool") {
				if (element.bits != 1) {
					qCritical() << "Error: element.bits isn't 1";
					return false;
				}

				element.type = Element::Bool;
			} else {
				element.type = Element::Int;
			}
		} else if ((type == "list") || (type == "string")) {
			element.lengthFunc = attributes.namedItem("lengthFunc").nodeValue();
			element.listType = attributes.namedItem("listType").nodeValue();

			if (type == "string") {
				element.listType = "QString";
			} else if (element.listType.isEmpty()) {
				qCritical() << "Error: invalid list definition";
				return false;
			}

			if (element.lengthFunc.isEmpty()) {
				if (!child.nextSibling().isNull()) {
					qCritical() << "Error: variable list isn't at the end";
					return false;
				}
			}

			if ((element.bitIndex % 8) != 0) {
				qCritical() << "Error: element.bitIndex isn't a multiple of 8";
				return false;
			}

			if (type == "string") {
				element.type = Element::String;
			} else {
				element.type = Element::List;
			}

			element.bits = 0;

			offsetString += " + " + element.name + "Length";
		} else {
			qCritical() << "Error: invalid \"type\"";
			return false;
		}

		bitIndex += element.bits;
		minBits += element.bits;

		if (element.name != "unused") {
			elements.append(element);
		}

		child = child.nextSibling();
	}

	if ((minBits % 8) != 0) {
		qCritical() << "Error: minBits isn't a multiple of 8";
		return false;
	}

	QString entryName = node.nodeName();
	bool seenLength = false;

	switch (type) {
	case Descriptor:
		stream << "\n";
		stream << "class " << entryName << " : public DvbDescriptor\n";
		stream << "{\n";
		stream << "public:\n";
		stream << "\texplicit " << entryName << "(const DvbDescriptor &descriptor) : DvbDescriptor(descriptor)\n";
		stream << "\t{\n";
		stream << "\t\tif (length < " << (minBits / 8) << ") {\n";
		stream << "\t\t\tlength = 0;\n";
		stream << "\t\t\treturn;\n";
		stream << "\t\t}\n";

		seenLength = true;
		break;

	case Entry:
		stream << "\n";
		stream << "class " << entryName << " : public DvbSectionData\n";
		stream << "{\n";
		stream << "public:\n";
		stream << "\texplicit " << entryName << "(const DvbSectionData &data_) : DvbSectionData(data_)\n";
		stream << "\t{\n";
		stream << "\t\tif (size < " << (minBits / 8) << ") {\n";
		stream << "\t\t\tlength = 0;\n";
		stream << "\t\t\treturn;\n";
		stream << "\t\t}\n";
		break;

	case Section:
		stream << "\n";
		stream << "class " << entryName << " : public DvbStandardSection\n";
		stream << "{\n";
		stream << "public:\n";
		stream << "\texplicit " << entryName << "(const DvbSection &section) : DvbStandardSection(section)\n";
		stream << "\t{\n";
		stream << "\t\tif (length < " << (minBits / 8) << ") {\n";
		stream << "\t\t\tlength = 0;\n";
		stream << "\t\t\treturn;\n";
		stream << "\t\t}\n";

		seenLength = true;
		break;
	}

	bool privateVars = false;

	for (int i = 0; i < elements.size(); ++i) {
		const Element &element = elements.at(i);

		if (element.type == Element::EntryLength) {
			if (seenLength) {
				qCritical() << "Error: more than one entry length defined";
				return false;
			}

			if (((element.bitIndex + element.bits) % 8) != 0) {
				qCritical() << "Error: (element.bitIndex + element.bits) isn't a multiple of 8";
				return false;
			}

			stream << "\n";
			stream << "\t\tlength = (" << element << ") + " << ((element.bitIndex + element.bits) / 8) << ";\n";
			stream << "\n";
			stream << "\t\tif (size < length) {\n";
			stream << "\t\t\tlength = 0;\n";
			stream << "\t\t\treturn;\n";
			stream << "\t\t}\n";

			elements.removeAt(i);
			--i;

			seenLength = true;
		} else if (((element.type == Element::List) || (element.type == Element::String)) && !element.lengthFunc.isEmpty()) {
			if ((i <= 0) || (elements.at(i - 1).name != element.lengthFunc)) {
				qCritical() << "Error: lengthFunc isn't a valid back reference";
				return false;
			}

			stream << "\n";
			stream << "\t\t" << element.name << "Length = " << elements.at(i - 1) << ";\n";
			stream << "\n";
			stream << "\t\tif (length < (" << (minBits / 8) << element.offsetString << " + " << element.name << "Length)) {\n";
			stream << "\t\t\tlength = 0;\n";
			stream << "\t\t\treturn;\n";
			stream << "\t\t}\n";

			elements.removeAt(i - 1);
			--i;

			privateVars = true;
		}
	}

	if (!seenLength) {
		stream << "\n";
		stream << "\t\tlength = " << (minBits / 8) << ";\n";
	}

	stream << "\t}\n";
	stream << "\n";
	stream << "\t~" << entryName << "() { }\n";

	if (type == Entry) {
		stream << "\n";
		stream << "\tvoid advance()\n";
		stream << "\t{\n";
		stream << "\t\t*this = " << entryName << "(next());\n";
		stream << "\t}\n";
	}

	if (type == Section) {
		QString extension = node.attributes().namedItem("extension").nodeValue();

		if (!extension.isEmpty()) {
			stream << "\n";
			stream << "\tint " << extension << "() const\n";
			stream << "\t{\n";
			stream << "\t\treturn (at(3) << 8) | at(4);\n";
			stream << "\t}\n";
		}
	}

	foreach (const Element &element, elements) {
		switch (element.type) {
		case Element::Bool:
			stream << "\n";
			stream << "\tbool " << element.name << "() const\n";
			stream << "\t{\n";
			stream << "\t\treturn (" << element << ");\n";
			stream << "\t}\n";
			break;

		case Element::Int:
			stream << "\n";
			stream << "\tint " << element.name << "() const\n";
			stream << "\t{\n";
			stream << "\t\treturn " << element << ";\n";
			stream << "\t}\n";
			break;

		case Element::List:
		case Element::String:
			stream << "\n";
			stream << "\t" << element.listType << " " << element.name << "() const\n";
			stream << "\t{\n";

			if (element.type == Element::List) {
				stream << "\t\treturn " << element.listType << "(subArray(" << element << ", ";
			} else {
				stream << "\t\treturn DvbSiText::convertText(subArray(" << element << ", ";
			}

			if (element.lengthFunc.isEmpty()) {
				if (element.offsetString.isEmpty()) {
					stream << "length - " << (minBits / 8) << "));\n";
				} else {
					stream << "length - (" << (minBits / 8) << element.offsetString << ")));\n";
				}
			} else {
				stream << element.name << "Length));\n";
			}

			stream << "\t}\n";
			break;

		default:
			Q_ASSERT(false);
		}
	}

	if (privateVars) {
		stream << "\n";
		stream << "private:\n";

		foreach (const Element &element, elements) {
			if (((element.type == Element::List) || (element.type == Element::String)) && !element.lengthFunc.isEmpty()) {
				stream << "\tint " << element.name << "Length;\n";
			}
		}
	}

	stream << "};\n";

	return true;
}

int main()
{
	QFile xmlFile("dvbsi.xml");

	if (!xmlFile.open(QIODevice::ReadOnly)) {
		qCritical() << "Error: can't open file" << xmlFile.fileName();
		return 1;
	}

	QDomDocument document;

	if (!document.setContent(&xmlFile)) {
		qCritical() << "Error: can't read file" << xmlFile.fileName();
		return 1;
	}

	if (!QFile::rename("../src/dvb/dvbsi.h", "../src/dvb/dvbsi.h.orig")) {
		qCritical() << "Error: can't rename \"../src/dvb/dvbsi.h\" to \"../src/dvb/dvbsi.h.orig\"";
		return 1;
	}

	QFile inFile("../src/dvb/dvbsi.h.orig");

	if (!inFile.open(QIODevice::ReadOnly)) {
		qCritical() << "Error: can't open file" << inFile.fileName();
		return 1;
	}

	QFile outFile("../src/dvb/dvbsi.h");

	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qCritical() << "Error: can't open file" << outFile.fileName();
		return 1;
	}

	QTextStream inStream(&inFile);
	inStream.setCodec("UTF-8");

	QTextStream outStream(&outFile);
	outStream.setCodec("UTF-8");

	while (true) {
		if (inStream.atEnd()) {
			qCritical() << "Error: can't find the autogeneration boundary";
			return 1;
		}

		QString line = inStream.readLine();
		outStream << line << "\n";

		if (line == "// everything below this line is automatically generated") {
			break;
		}
	}

	if (!SiXmlParser(outStream).parse(document)) {
		return 1;
	}

	outStream << "\n#endif /* DVBSI_H */\n";

	if (!inFile.remove()) {
		qCritical() << "Error: can't remove file" << inFile.fileName();
		return 1;
	}

	xmlFile.close();

	if (xmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		xmlFile.write(document.toByteArray(2));
	}

	return 0;
}
