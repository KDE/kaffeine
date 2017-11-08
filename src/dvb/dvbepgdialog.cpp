/*
 * dvbepgdialog.cpp
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "../log.h"

#include <KConfigGroup>
#include <QAction>
#include <QBoxLayout>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "dvbepgdialog.h"
#include "dvbepgdialog_p.h"
#include "dvbmanager.h"

/*
 * ISO 639-2 language codes as defined on 2017-11-08 at:
 *	https://www.loc.gov/standards/iso639-2/ISO-639-2_utf-8.txt
 */
static const QHash<QString, QString> iso639_2_codes({
	{ "AAR", I18N_NOOP("Afar")},
	{ "ABK", I18N_NOOP("Abkhazian")},
	{ "ACE", I18N_NOOP("Achinese")},
	{ "ACH", I18N_NOOP("Acoli")},
	{ "ADA", I18N_NOOP("Adangme")},
	{ "ADY", I18N_NOOP("Adyghe")},
	{ "AFA", I18N_NOOP("Afro-Asiatic languages")},
	{ "AFH", I18N_NOOP("Afrihili")},
	{ "AFR", I18N_NOOP("Afrikaans")},
	{ "AIN", I18N_NOOP("Ainu")},
	{ "AKA", I18N_NOOP("Akan")},
	{ "AKK", I18N_NOOP("Akkadian")},
	{ "ALB", I18N_NOOP("Albanian")},
	{ "ALE", I18N_NOOP("Aleut")},
	{ "ALG", I18N_NOOP("Algonquian languages")},
	{ "ALT", I18N_NOOP("Southern Altai")},
	{ "AMH", I18N_NOOP("Amharic")},
	{ "ANG", I18N_NOOP("English")},
	{ "ANP", I18N_NOOP("Angika")},
	{ "APA", I18N_NOOP("Apache languages")},
	{ "ARA", I18N_NOOP("Arabic")},
	{ "ARC", I18N_NOOP("Official Aramaic (700-300 BCE)")},
	{ "ARG", I18N_NOOP("Aragonese")},
	{ "ARM", I18N_NOOP("Armenian")},
	{ "ARN", I18N_NOOP("Mapudungun")},
	{ "ARP", I18N_NOOP("Arapaho")},
	{ "ART", I18N_NOOP("Artificial languages")},
	{ "ARW", I18N_NOOP("Arawak")},
	{ "ASM", I18N_NOOP("Assamese")},
	{ "AST", I18N_NOOP("Asturian")},
	{ "ATH", I18N_NOOP("Athapascan languages")},
	{ "AUS", I18N_NOOP("Australian languages")},
	{ "AVA", I18N_NOOP("Avaric")},
	{ "AVE", I18N_NOOP("Avestan")},
	{ "AWA", I18N_NOOP("Awadhi")},
	{ "AYM", I18N_NOOP("Aymara")},
	{ "AZE", I18N_NOOP("Azerbaijani")},
	{ "BAD", I18N_NOOP("Banda languages")},
	{ "BAI", I18N_NOOP("Bamileke languages")},
	{ "BAK", I18N_NOOP("Bashkir")},
	{ "BAL", I18N_NOOP("Baluchi")},
	{ "BAM", I18N_NOOP("Bambara")},
	{ "BAN", I18N_NOOP("Balinese")},
	{ "BAQ", I18N_NOOP("Basque")},
	{ "BAS", I18N_NOOP("Basa")},
	{ "BAT", I18N_NOOP("Baltic languages")},
	{ "BEJ", I18N_NOOP("Beja")},
	{ "BEL", I18N_NOOP("Belarusian")},
	{ "BEM", I18N_NOOP("Bemba")},
	{ "BEN", I18N_NOOP("Bengali")},
	{ "BER", I18N_NOOP("Berber languages")},
	{ "BHO", I18N_NOOP("Bhojpuri")},
	{ "BIH", I18N_NOOP("Bihari languages")},
	{ "BIK", I18N_NOOP("Bikol")},
	{ "BIN", I18N_NOOP("Bini")},
	{ "BIS", I18N_NOOP("Bislama")},
	{ "BLA", I18N_NOOP("Siksika")},
	{ "BNT", I18N_NOOP("Bantu (Other)")},
	{ "BOS", I18N_NOOP("Bosnian")},
	{ "BRA", I18N_NOOP("Braj")},
	{ "BRE", I18N_NOOP("Breton")},
	{ "BTK", I18N_NOOP("Batak languages")},
	{ "BUA", I18N_NOOP("Buriat")},
	{ "BUG", I18N_NOOP("Buginese")},
	{ "BUL", I18N_NOOP("Bulgarian")},
	{ "BUR", I18N_NOOP("Burmese")},
	{ "BYN", I18N_NOOP("Blin")},
	{ "CAD", I18N_NOOP("Caddo")},
	{ "CAI", I18N_NOOP("Central American Indian languages")},
	{ "CAR", I18N_NOOP("Galibi Carib")},
	{ "CAT", I18N_NOOP("Catalan")},
	{ "CAU", I18N_NOOP("Caucasian languages")},
	{ "CEB", I18N_NOOP("Cebuano")},
	{ "CEL", I18N_NOOP("Celtic languages")},
	{ "CHA", I18N_NOOP("Chamorro")},
	{ "CHB", I18N_NOOP("Chibcha")},
	{ "CHE", I18N_NOOP("Chechen")},
	{ "CHG", I18N_NOOP("Chagatai")},
	{ "CHI", I18N_NOOP("Chinese")},
	{ "CHK", I18N_NOOP("Chuukese")},
	{ "CHM", I18N_NOOP("Mari")},
	{ "CHN", I18N_NOOP("Chinook jargon")},
	{ "CHO", I18N_NOOP("Choctaw")},
	{ "CHP", I18N_NOOP("Chipewyan")},
	{ "CHR", I18N_NOOP("Cherokee")},
	{ "CHU", I18N_NOOP("Church Slavic")},
	{ "CHV", I18N_NOOP("Chuvash")},
	{ "CHY", I18N_NOOP("Cheyenne")},
	{ "CMC", I18N_NOOP("Chamic languages")},
	{ "COP", I18N_NOOP("Coptic")},
	{ "COR", I18N_NOOP("Cornish")},
	{ "COS", I18N_NOOP("Corsican")},
	{ "CPE", I18N_NOOP("Creoles and pidgins")},
	{ "CPF", I18N_NOOP("Creoles and pidgins")},
	{ "CPP", I18N_NOOP("Creoles and pidgins")},
	{ "CRE", I18N_NOOP("Cree")},
	{ "CRH", I18N_NOOP("Crimean Tatar")},
	{ "CRP", I18N_NOOP("Creoles and pidgins ")},
	{ "CSB", I18N_NOOP("Kashubian")},
	{ "CUS", I18N_NOOP("Cushitic languages")},
	{ "CZE", I18N_NOOP("Czech")},
	{ "DAK", I18N_NOOP("Dakota")},
	{ "DAN", I18N_NOOP("Danish")},
	{ "DAR", I18N_NOOP("Dargwa")},
	{ "DAY", I18N_NOOP("Land Dayak languages")},
	{ "DEL", I18N_NOOP("Delaware")},
	{ "DEN", I18N_NOOP("Slave (Athapascan)")},
	{ "DGR", I18N_NOOP("Dogrib")},
	{ "DIN", I18N_NOOP("Dinka")},
	{ "DIV", I18N_NOOP("Divehi")},
	{ "DOI", I18N_NOOP("Dogri")},
	{ "DRA", I18N_NOOP("Dravidian languages")},
	{ "DSB", I18N_NOOP("Lower Sorbian")},
	{ "DUA", I18N_NOOP("Duala")},
	{ "DUM", I18N_NOOP("Dutch")},
	{ "DUT", I18N_NOOP("Dutch")},
	{ "DYU", I18N_NOOP("Dyula")},
	{ "DZO", I18N_NOOP("Dzongkha")},
	{ "EFI", I18N_NOOP("Efik")},
	{ "EGY", I18N_NOOP("Egyptian (Ancient)")},
	{ "EKA", I18N_NOOP("Ekajuk")},
	{ "ELX", I18N_NOOP("Elamite")},
	{ "ENG", I18N_NOOP("English")},
	{ "ENM", I18N_NOOP("English")},
	{ "EPO", I18N_NOOP("Esperanto")},
	{ "EST", I18N_NOOP("Estonian")},
	{ "EWE", I18N_NOOP("Ewe")},
	{ "EWO", I18N_NOOP("Ewondo")},
	{ "FAN", I18N_NOOP("Fang")},
	{ "FAO", I18N_NOOP("Faroese")},
	{ "FAT", I18N_NOOP("Fanti")},
	{ "FIJ", I18N_NOOP("Fijian")},
	{ "FIL", I18N_NOOP("Filipino")},
	{ "FIN", I18N_NOOP("Finnish")},
	{ "FIU", I18N_NOOP("Finno-Ugrian languages")},
	{ "FON", I18N_NOOP("Fon")},
	{ "FRE", I18N_NOOP("French")},
	{ "FRM", I18N_NOOP("French")},
	{ "FRO", I18N_NOOP("French")},
	{ "FRR", I18N_NOOP("Northern Frisian")},
	{ "FRS", I18N_NOOP("Eastern Frisian")},
	{ "FRY", I18N_NOOP("Western Frisian")},
	{ "FUL", I18N_NOOP("Fulah")},
	{ "FUR", I18N_NOOP("Friulian")},
	{ "GAA", I18N_NOOP("Ga")},
	{ "GAY", I18N_NOOP("Gayo")},
	{ "GBA", I18N_NOOP("Gbaya")},
	{ "GEM", I18N_NOOP("Germanic languages")},
	{ "GEO", I18N_NOOP("Georgian")},
	{ "GER", I18N_NOOP("German")},
	{ "GEZ", I18N_NOOP("Geez")},
	{ "GIL", I18N_NOOP("Gilbertese")},
	{ "GLA", I18N_NOOP("Gaelic")},
	{ "GLE", I18N_NOOP("Irish")},
	{ "GLG", I18N_NOOP("Galician")},
	{ "GLV", I18N_NOOP("Manx")},
	{ "GMH", I18N_NOOP("German")},
	{ "GOH", I18N_NOOP("German")},
	{ "GON", I18N_NOOP("Gondi")},
	{ "GOR", I18N_NOOP("Gorontalo")},
	{ "GOT", I18N_NOOP("Gothic")},
	{ "GRB", I18N_NOOP("Grebo")},
	{ "GRC", I18N_NOOP("Greek")},
	{ "GRE", I18N_NOOP("Greek")},
	{ "GRN", I18N_NOOP("Guarani")},
	{ "GSW", I18N_NOOP("Swiss German")},
	{ "GUJ", I18N_NOOP("Gujarati")},
	{ "GWI", I18N_NOOP("Gwich'in")},
	{ "HAI", I18N_NOOP("Haida")},
	{ "HAT", I18N_NOOP("Haitian")},
	{ "HAU", I18N_NOOP("Hausa")},
	{ "HAW", I18N_NOOP("Hawaiian")},
	{ "HEB", I18N_NOOP("Hebrew")},
	{ "HER", I18N_NOOP("Herero")},
	{ "HIL", I18N_NOOP("Hiligaynon")},
	{ "HIM", I18N_NOOP("Himachali languages")},
	{ "HIN", I18N_NOOP("Hindi")},
	{ "HIT", I18N_NOOP("Hittite")},
	{ "HMN", I18N_NOOP("Hmong")},
	{ "HMO", I18N_NOOP("Hiri Motu")},
	{ "HRV", I18N_NOOP("Croatian")},
	{ "HSB", I18N_NOOP("Upper Sorbian")},
	{ "HUN", I18N_NOOP("Hungarian")},
	{ "HUP", I18N_NOOP("Hupa")},
	{ "IBA", I18N_NOOP("Iban")},
	{ "IBO", I18N_NOOP("Igbo")},
	{ "ICE", I18N_NOOP("Icelandic")},
	{ "IDO", I18N_NOOP("Ido")},
	{ "III", I18N_NOOP("Sichuan Yi")},
	{ "IJO", I18N_NOOP("Ijo languages")},
	{ "IKU", I18N_NOOP("Inuktitut")},
	{ "ILE", I18N_NOOP("Interlingue")},
	{ "ILO", I18N_NOOP("Iloko")},
	{ "INA", I18N_NOOP("Interlingua")},
	{ "INC", I18N_NOOP("Indic languages")},
	{ "IND", I18N_NOOP("Indonesian")},
	{ "INE", I18N_NOOP("Indo-European languages")},
	{ "INH", I18N_NOOP("Ingush")},
	{ "IPK", I18N_NOOP("Inupiaq")},
	{ "IRA", I18N_NOOP("Iranian languages")},
	{ "IRO", I18N_NOOP("Iroquoian languages")},
	{ "ITA", I18N_NOOP("Italian")},
	{ "JAV", I18N_NOOP("Javanese")},
	{ "JBO", I18N_NOOP("Lojban")},
	{ "JPN", I18N_NOOP("Japanese")},
	{ "JPR", I18N_NOOP("Judeo-Persian")},
	{ "JRB", I18N_NOOP("Judeo-Arabic")},
	{ "KAA", I18N_NOOP("Kara-Kalpak")},
	{ "KAB", I18N_NOOP("Kabyle")},
	{ "KAC", I18N_NOOP("Kachin")},
	{ "KAL", I18N_NOOP("Kalaallisut")},
	{ "KAM", I18N_NOOP("Kamba")},
	{ "KAN", I18N_NOOP("Kannada")},
	{ "KAR", I18N_NOOP("Karen languages")},
	{ "KAS", I18N_NOOP("Kashmiri")},
	{ "KAU", I18N_NOOP("Kanuri")},
	{ "KAW", I18N_NOOP("Kawi")},
	{ "KAZ", I18N_NOOP("Kazakh")},
	{ "KBD", I18N_NOOP("Kabardian")},
	{ "KHA", I18N_NOOP("Khasi")},
	{ "KHI", I18N_NOOP("Khoisan languages")},
	{ "KHM", I18N_NOOP("Central Khmer")},
	{ "KHO", I18N_NOOP("Khotanese")},
	{ "KIK", I18N_NOOP("Kikuyu")},
	{ "KIN", I18N_NOOP("Kinyarwanda")},
	{ "KIR", I18N_NOOP("Kirghiz")},
	{ "KMB", I18N_NOOP("Kimbundu")},
	{ "KOK", I18N_NOOP("Konkani")},
	{ "KOM", I18N_NOOP("Komi")},
	{ "KON", I18N_NOOP("Kongo")},
	{ "KOR", I18N_NOOP("Korean")},
	{ "KOS", I18N_NOOP("Kosraean")},
	{ "KPE", I18N_NOOP("Kpelle")},
	{ "KRC", I18N_NOOP("Karachay-Balkar")},
	{ "KRL", I18N_NOOP("Karelian")},
	{ "KRO", I18N_NOOP("Kru languages")},
	{ "KRU", I18N_NOOP("Kurukh")},
	{ "KUA", I18N_NOOP("Kuanyama")},
	{ "KUM", I18N_NOOP("Kumyk")},
	{ "KUR", I18N_NOOP("Kurdish")},
	{ "KUT", I18N_NOOP("Kutenai")},
	{ "LAD", I18N_NOOP("Ladino")},
	{ "LAH", I18N_NOOP("Lahnda")},
	{ "LAM", I18N_NOOP("Lamba")},
	{ "LAO", I18N_NOOP("Lao")},
	{ "LAT", I18N_NOOP("Latin")},
	{ "LAV", I18N_NOOP("Latvian")},
	{ "LEZ", I18N_NOOP("Lezghian")},
	{ "LIM", I18N_NOOP("Limburgan")},
	{ "LIN", I18N_NOOP("Lingala")},
	{ "LIT", I18N_NOOP("Lithuanian")},
	{ "LOL", I18N_NOOP("Mongo")},
	{ "LOZ", I18N_NOOP("Lozi")},
	{ "LTZ", I18N_NOOP("Luxembourgish")},
	{ "LUA", I18N_NOOP("Luba-Lulua")},
	{ "LUB", I18N_NOOP("Luba-Katanga")},
	{ "LUG", I18N_NOOP("Ganda")},
	{ "LUI", I18N_NOOP("Luiseno")},
	{ "LUN", I18N_NOOP("Lunda")},
	{ "LUO", I18N_NOOP("Luo (Kenya and Tanzania)")},
	{ "LUS", I18N_NOOP("Lushai")},
	{ "MAC", I18N_NOOP("Macedonian")},
	{ "MAD", I18N_NOOP("Madurese")},
	{ "MAG", I18N_NOOP("Magahi")},
	{ "MAH", I18N_NOOP("Marshallese")},
	{ "MAI", I18N_NOOP("Maithili")},
	{ "MAK", I18N_NOOP("Makasar")},
	{ "MAL", I18N_NOOP("Malayalam")},
	{ "MAN", I18N_NOOP("Mandingo")},
	{ "MAO", I18N_NOOP("Maori")},
	{ "MAP", I18N_NOOP("Austronesian languages")},
	{ "MAR", I18N_NOOP("Marathi")},
	{ "MAS", I18N_NOOP("Masai")},
	{ "MAY", I18N_NOOP("Malay")},
	{ "MDF", I18N_NOOP("Moksha")},
	{ "MDR", I18N_NOOP("Mandar")},
	{ "MEN", I18N_NOOP("Mende")},
	{ "MGA", I18N_NOOP("Irish")},
	{ "MIC", I18N_NOOP("Mi'kmaq")},
	{ "MIN", I18N_NOOP("Minangkabau")},
	{ "MIS", I18N_NOOP("Uncoded languages")},
	{ "MKH", I18N_NOOP("Mon-Khmer languages")},
	{ "MLG", I18N_NOOP("Malagasy")},
	{ "MLT", I18N_NOOP("Maltese")},
	{ "MNC", I18N_NOOP("Manchu")},
	{ "MNI", I18N_NOOP("Manipuri")},
	{ "MNO", I18N_NOOP("Manobo languages")},
	{ "MOH", I18N_NOOP("Mohawk")},
	{ "MON", I18N_NOOP("Mongolian")},
	{ "MOS", I18N_NOOP("Mossi")},
	{ "MUL", I18N_NOOP("Multiple languages")},
	{ "MUN", I18N_NOOP("Munda languages")},
	{ "MUS", I18N_NOOP("Creek")},
	{ "MWL", I18N_NOOP("Mirandese")},
	{ "MWR", I18N_NOOP("Marwari")},
	{ "MYN", I18N_NOOP("Mayan languages")},
	{ "MYV", I18N_NOOP("Erzya")},
	{ "NAH", I18N_NOOP("Nahuatl languages")},
	{ "NAI", I18N_NOOP("North American Indian languages")},
	{ "NAP", I18N_NOOP("Neapolitan")},
	{ "NAU", I18N_NOOP("Nauru")},
	{ "NAV", I18N_NOOP("Navajo")},
	{ "NBL", I18N_NOOP("Ndebele")},
	{ "NDE", I18N_NOOP("Ndebele")},
	{ "NDO", I18N_NOOP("Ndonga")},
	{ "NDS", I18N_NOOP("Low German")},
	{ "NEP", I18N_NOOP("Nepali")},
	{ "NEW", I18N_NOOP("Nepal Bhasa")},
	{ "NIA", I18N_NOOP("Nias")},
	{ "NIC", I18N_NOOP("Niger-Kordofanian languages")},
	{ "NIU", I18N_NOOP("Niuean")},
	{ "NNO", I18N_NOOP("Norwegian Nynorsk")},
	{ "NOB", I18N_NOOP("Bokmål")},
	{ "NOG", I18N_NOOP("Nogai")},
	{ "NON", I18N_NOOP("Norse")},
	{ "NOR", I18N_NOOP("Norwegian")},
	{ "NQO", I18N_NOOP("N'Ko")},
	{ "NSO", I18N_NOOP("Pedi")},
	{ "NUB", I18N_NOOP("Nubian languages")},
	{ "NWC", I18N_NOOP("Classical Newari")},
	{ "NYA", I18N_NOOP("Chichewa")},
	{ "NYM", I18N_NOOP("Nyamwezi")},
	{ "NYN", I18N_NOOP("Nyankole")},
	{ "NYO", I18N_NOOP("Nyoro")},
	{ "NZI", I18N_NOOP("Nzima")},
	{ "OCI", I18N_NOOP("Occitan (post 1500)")},
	{ "OJI", I18N_NOOP("Ojibwa")},
	{ "ORI", I18N_NOOP("Oriya")},
	{ "ORM", I18N_NOOP("Oromo")},
	{ "OSA", I18N_NOOP("Osage")},
	{ "OSS", I18N_NOOP("Ossetian")},
	{ "OTA", I18N_NOOP("Turkish")},
	{ "OTO", I18N_NOOP("Otomian languages")},
	{ "PAA", I18N_NOOP("Papuan languages")},
	{ "PAG", I18N_NOOP("Pangasinan")},
	{ "PAL", I18N_NOOP("Pahlavi")},
	{ "PAM", I18N_NOOP("Pampanga")},
	{ "PAN", I18N_NOOP("Panjabi")},
	{ "PAP", I18N_NOOP("Papiamento")},
	{ "PAU", I18N_NOOP("Palauan")},
	{ "PEO", I18N_NOOP("Persian")},
	{ "PER", I18N_NOOP("Persian")},
	{ "PHI", I18N_NOOP("Philippine languages")},
	{ "PHN", I18N_NOOP("Phoenician")},
	{ "PLI", I18N_NOOP("Pali")},
	{ "POL", I18N_NOOP("Polish")},
	{ "PON", I18N_NOOP("Pohnpeian")},
	{ "POR", I18N_NOOP("Portuguese")},
	{ "PRA", I18N_NOOP("Prakrit languages")},
	{ "PRO", I18N_NOOP("Provençal")},
	{ "PUS", I18N_NOOP("Pushto")},
	{ "QAA-QTZ", I18N_NOOP("Reserved for local use")},
	{ "QUE", I18N_NOOP("Quechua")},
	{ "RAJ", I18N_NOOP("Rajasthani")},
	{ "RAP", I18N_NOOP("Rapanui")},
	{ "RAR", I18N_NOOP("Rarotongan")},
	{ "ROA", I18N_NOOP("Romance languages")},
	{ "ROH", I18N_NOOP("Romansh")},
	{ "ROM", I18N_NOOP("Romany")},
	{ "RUM", I18N_NOOP("Romanian")},
	{ "RUN", I18N_NOOP("Rundi")},
	{ "RUP", I18N_NOOP("Aromanian")},
	{ "RUS", I18N_NOOP("Russian")},
	{ "SAD", I18N_NOOP("Sandawe")},
	{ "SAG", I18N_NOOP("Sango")},
	{ "SAH", I18N_NOOP("Yakut")},
	{ "SAI", I18N_NOOP("South American Indian (Other)")},
	{ "SAL", I18N_NOOP("Salishan languages")},
	{ "SAM", I18N_NOOP("Samaritan Aramaic")},
	{ "SAN", I18N_NOOP("Sanskrit")},
	{ "SAS", I18N_NOOP("Sasak")},
	{ "SAT", I18N_NOOP("Santali")},
	{ "SCN", I18N_NOOP("Sicilian")},
	{ "SCO", I18N_NOOP("Scots")},
	{ "SEL", I18N_NOOP("Selkup")},
	{ "SEM", I18N_NOOP("Semitic languages")},
	{ "SGA", I18N_NOOP("Irish")},
	{ "SGN", I18N_NOOP("Sign Languages")},
	{ "SHN", I18N_NOOP("Shan")},
	{ "SID", I18N_NOOP("Sidamo")},
	{ "SIN", I18N_NOOP("Sinhala")},
	{ "SIO", I18N_NOOP("Siouan languages")},
	{ "SIT", I18N_NOOP("Sino-Tibetan languages")},
	{ "SLA", I18N_NOOP("Slavic languages")},
	{ "SLO", I18N_NOOP("Slovak")},
	{ "SLV", I18N_NOOP("Slovenian")},
	{ "SMA", I18N_NOOP("Southern Sami")},
	{ "SME", I18N_NOOP("Northern Sami")},
	{ "SMI", I18N_NOOP("Sami languages")},
	{ "SMJ", I18N_NOOP("Lule Sami")},
	{ "SMN", I18N_NOOP("Inari Sami")},
	{ "SMO", I18N_NOOP("Samoan")},
	{ "SMS", I18N_NOOP("Skolt Sami")},
	{ "SNA", I18N_NOOP("Shona")},
	{ "SND", I18N_NOOP("Sindhi")},
	{ "SNK", I18N_NOOP("Soninke")},
	{ "SOG", I18N_NOOP("Sogdian")},
	{ "SOM", I18N_NOOP("Somali")},
	{ "SON", I18N_NOOP("Songhai languages")},
	{ "SOT", I18N_NOOP("Sotho")},
	{ "SPA", I18N_NOOP("Spanish")},
	{ "SRD", I18N_NOOP("Sardinian")},
	{ "SRN", I18N_NOOP("Sranan Tongo")},
	{ "SRP", I18N_NOOP("Serbian")},
	{ "SRR", I18N_NOOP("Serer")},
	{ "SSA", I18N_NOOP("Nilo-Saharan languages")},
	{ "SSW", I18N_NOOP("Swati")},
	{ "SUK", I18N_NOOP("Sukuma")},
	{ "SUN", I18N_NOOP("Sundanese")},
	{ "SUS", I18N_NOOP("Susu")},
	{ "SUX", I18N_NOOP("Sumerian")},
	{ "SWA", I18N_NOOP("Swahili")},
	{ "SWE", I18N_NOOP("Swedish")},
	{ "SYC", I18N_NOOP("Classical Syriac")},
	{ "SYR", I18N_NOOP("Syriac")},
	{ "TAH", I18N_NOOP("Tahitian")},
	{ "TAI", I18N_NOOP("Tai languages")},
	{ "TAM", I18N_NOOP("Tamil")},
	{ "TAT", I18N_NOOP("Tatar")},
	{ "TEL", I18N_NOOP("Telugu")},
	{ "TEM", I18N_NOOP("Timne")},
	{ "TER", I18N_NOOP("Tereno")},
	{ "TET", I18N_NOOP("Tetum")},
	{ "TGK", I18N_NOOP("Tajik")},
	{ "TGL", I18N_NOOP("Tagalog")},
	{ "THA", I18N_NOOP("Thai")},
	{ "TIB", I18N_NOOP("Tibetan")},
	{ "TIG", I18N_NOOP("Tigre")},
	{ "TIR", I18N_NOOP("Tigrinya")},
	{ "TIV", I18N_NOOP("Tiv")},
	{ "TKL", I18N_NOOP("Tokelau")},
	{ "TLH", I18N_NOOP("Klingon")},
	{ "TLI", I18N_NOOP("Tlingit")},
	{ "TMH", I18N_NOOP("Tamashek")},
	{ "TOG", I18N_NOOP("Tonga (Nyasa)")},
	{ "TON", I18N_NOOP("Tonga (Tonga Islands)")},
	{ "TPI", I18N_NOOP("Tok Pisin")},
	{ "TSI", I18N_NOOP("Tsimshian")},
	{ "TSN", I18N_NOOP("Tswana")},
	{ "TSO", I18N_NOOP("Tsonga")},
	{ "TUK", I18N_NOOP("Turkmen")},
	{ "TUM", I18N_NOOP("Tumbuka")},
	{ "TUP", I18N_NOOP("Tupi languages")},
	{ "TUR", I18N_NOOP("Turkish")},
	{ "TUT", I18N_NOOP("Altaic languages")},
	{ "TVL", I18N_NOOP("Tuvalu")},
	{ "TWI", I18N_NOOP("Twi")},
	{ "TYV", I18N_NOOP("Tuvinian")},
	{ "UDM", I18N_NOOP("Udmurt")},
	{ "UGA", I18N_NOOP("Ugaritic")},
	{ "UIG", I18N_NOOP("Uighur")},
	{ "UKR", I18N_NOOP("Ukrainian")},
	{ "UMB", I18N_NOOP("Umbundu")},
	{ "UND", I18N_NOOP("Undetermined")},
	{ "URD", I18N_NOOP("Urdu")},
	{ "UZB", I18N_NOOP("Uzbek")},
	{ "VAI", I18N_NOOP("Vai")},
	{ "VEN", I18N_NOOP("Venda")},
	{ "VIE", I18N_NOOP("Vietnamese")},
	{ "VOL", I18N_NOOP("Volapük")},
	{ "VOT", I18N_NOOP("Votic")},
	{ "WAK", I18N_NOOP("Wakashan languages")},
	{ "WAL", I18N_NOOP("Walamo")},
	{ "WAR", I18N_NOOP("Waray")},
	{ "WAS", I18N_NOOP("Washo")},
	{ "WEL", I18N_NOOP("Welsh")},
	{ "WEN", I18N_NOOP("Sorbian languages")},
	{ "WLN", I18N_NOOP("Walloon")},
	{ "WOL", I18N_NOOP("Wolof")},
	{ "XAL", I18N_NOOP("Kalmyk")},
	{ "XHO", I18N_NOOP("Xhosa")},
	{ "YAO", I18N_NOOP("Yao")},
	{ "YAP", I18N_NOOP("Yapese")},
	{ "YID", I18N_NOOP("Yiddish")},
	{ "YOR", I18N_NOOP("Yoruba")},
	{ "YPK", I18N_NOOP("Yupik languages")},
	{ "ZAP", I18N_NOOP("Zapotec")},
	{ "ZBL", I18N_NOOP("Blissymbols")},
	{ "ZEN", I18N_NOOP("Zenaga")},
	{ "ZGH", I18N_NOOP("Standard Moroccan Tamazight")},
	{ "ZHA", I18N_NOOP("Zhuang")},
	{ "ZND", I18N_NOOP("Zande languages")},
	{ "ZUL", I18N_NOOP("Zulu")},
	{ "ZUN", I18N_NOOP("Zuni")},
	{ "ZXX", I18N_NOOP("No linguistic content")},
	{ "ZZA", I18N_NOOP("Zaza")},
});

DvbEpgDialog::DvbEpgDialog(DvbManager *manager_, QWidget *parent) : QDialog(parent),
	manager(manager_)
{
	setWindowTitle(i18nc("@title:window", "Program Guide"));

	QWidget *mainWidget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout;
	setLayout(mainLayout);

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	mainLayout->addWidget(mainWidget);

	QWidget *widget = new QWidget(this);

	epgChannelTableModel = new DvbEpgChannelTableModel(this);
	epgChannelTableModel->setManager(manager);
	channelView = new QTreeView(widget);
	channelView->setMaximumWidth(30 * fontMetrics().averageCharWidth());
	channelView->setModel(epgChannelTableModel);
	channelView->setRootIsDecorated(false);
	connect(channelView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(channelActivated(QModelIndex)));
	mainLayout->addWidget(channelView);

	QBoxLayout *rightLayout = new QVBoxLayout();
	QBoxLayout *boxLayout = new QHBoxLayout();
	QBoxLayout *langLayout = new QHBoxLayout();

	QLabel *label = new QLabel(i18n("EPG language:"), mainWidget);
	langLayout->addWidget(label);

	languageBox = new QComboBox(mainWidget);
	languageBox->addItem("");
	QHashIterator<QString, bool> i(manager_->languageCodes);
	int j = 1;
	while (i.hasNext()) {
		i.next();
		QString lang = i.key();
		if (lang != FIRST_LANG) {
			languageBox->addItem(lang);
			if (manager_->currentEpgLanguage == lang) {
				languageBox->setCurrentIndex(j);
				currentLanguage = lang;
			}
			j++;
		}
	}
	langLayout->addWidget(languageBox);
	connect(languageBox, SIGNAL(currentTextChanged(QString)),
		this, SLOT(languageChanged(QString)));
	connect(manager_->getEpgModel(), SIGNAL(languageAdded(const QString)),
		this, SLOT(languageAdded(const QString)));

	languageLabel = new QLabel(mainWidget);
	langLayout->addWidget(languageLabel);
	languageLabel->setBuddy(languageBox);
	if (iso639_2_codes.contains(currentLanguage))
		languageLabel->setText(iso639_2_codes[currentLanguage]);
	else
		languageLabel->setText(i18n("Any language"));

	rightLayout->addLayout(langLayout);

	QAction *scheduleAction = new QAction(QIcon::fromTheme(QLatin1String("media-record"), QIcon(":media-record")),
		i18nc("@action:inmenu tv show", "Record Show"), this);
	connect(scheduleAction, SIGNAL(triggered()), this, SLOT(scheduleProgram()));

	QPushButton *pushButton =
		new QPushButton(scheduleAction->icon(), scheduleAction->text(), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(scheduleProgram()));
	boxLayout->addWidget(pushButton);

	boxLayout->addWidget(new QLabel(i18nc("@label:textbox", "Search:"), widget));

	epgTableModel = new DvbEpgTableModel(this);
	epgTableModel->setEpgModel(manager->getEpgModel());
	epgTableModel->setLanguage(currentLanguage);
	connect(epgTableModel, SIGNAL(layoutChanged()), this, SLOT(checkEntry()));
	QLineEdit *lineEdit = new QLineEdit(widget);
	lineEdit->setClearButtonEnabled(true);
	connect(lineEdit, SIGNAL(textChanged(QString)),
		epgTableModel, SLOT(setContentFilter(QString)));
	boxLayout->addWidget(lineEdit);
	rightLayout->addLayout(boxLayout);

	epgView = new QTreeView(widget);
	epgView->addAction(scheduleAction);
	epgView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	epgView->setContextMenuPolicy(Qt::ActionsContextMenu);
	epgView->setMinimumWidth(75 * fontMetrics().averageCharWidth());
	epgView->setModel(epgTableModel);
	epgView->setRootIsDecorated(false);
	epgView->setUniformRowHeights(true);
	connect(epgView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		this, SLOT(entryActivated(QModelIndex)));
	rightLayout->addWidget(epgView);

	contentLabel = new QLabel(widget);
	contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	contentLabel->setMargin(5);
	contentLabel->setWordWrap(true);

	QScrollArea *scrollArea = new QScrollArea(widget);
	scrollArea->setBackgroundRole(QPalette::Light);
	scrollArea->setMinimumHeight(12 * fontMetrics().height());
	scrollArea->setWidget(contentLabel);
	scrollArea->setWidgetResizable(true);
	rightLayout->addWidget(scrollArea);
	mainLayout->addLayout(rightLayout);
	mainLayout->addWidget(widget);

	rightLayout->addWidget(buttonBox);
}

DvbEpgDialog::~DvbEpgDialog()
{
}

void DvbEpgDialog::setCurrentChannel(const DvbSharedChannel &channel)
{
	channelView->setCurrentIndex(epgChannelTableModel->find(channel));
}

void DvbEpgDialog::channelActivated(const QModelIndex &index)
{
	if (!index.isValid()) {
		epgTableModel->setChannelFilter(DvbSharedChannel());
		return;
	}

	epgTableModel->setChannelFilter(epgChannelTableModel->value(index));
	epgView->setCurrentIndex(epgTableModel->index(0, 0));
}

void DvbEpgDialog::languageChanged(const QString lang)
{
	// Handle the any language case
	if (languageBox->currentIndex() <= 0)
		currentLanguage = "";
	else
		currentLanguage = lang;
	if (iso639_2_codes.contains(currentLanguage))
		languageLabel->setText(iso639_2_codes[currentLanguage]);
	else
		languageLabel->setText(i18n("Any language"));

	epgTableModel->setLanguage(currentLanguage);
	epgView->setCurrentIndex(epgTableModel->index(0, 0));
	entryActivated(epgTableModel->index(0, 0));
	manager->currentEpgLanguage = currentLanguage;
}

void DvbEpgDialog::languageAdded(const QString lang)
{
	languageBox->addItem(lang);
}

void DvbEpgDialog::entryActivated(const QModelIndex &index)
{
	const DvbSharedEpgEntry &entry = epgTableModel->value(index.row());

	if (!entry.isValid()) {
		contentLabel->setText(QString());
		return;
	}

	QString text = "<font color=#008000 size=\"+1\">" + entry->title(currentLanguage) + "</font>";

	if (!entry->subheading().isEmpty()) {
		text += "<br/><font color=#808000>" + entry->subheading(currentLanguage) + "</font>";
	}

	QDateTime begin = entry->begin.toLocalTime();
	QTime end = entry->begin.addSecs(QTime(0, 0, 0).secsTo(entry->duration)).toLocalTime().time();
	text += "<br/><br/><font color=#800080>" + QLocale().toString(begin, QLocale::LongFormat) + " - " + QLocale().toString(end) + "</font>";

	if (!entry->details(currentLanguage).isEmpty() && entry->details(currentLanguage) !=  entry->title(currentLanguage)) {
		text += "<br/><br/>" + entry->details(currentLanguage);
	}

	if (!entry->content.isEmpty()) {
		text += "<br/><br/><font color=#000080>" + entry->content + "</font>";
	}
	if (!entry->parental(currentLanguage).isEmpty()) {
		text += "<br/><br/><font color=#800000>" + entry->parental(currentLanguage) + "</font>";
	}

	contentLabel->setText(text);
}

void DvbEpgDialog::checkEntry()
{
	if (!epgView->currentIndex().isValid()) {
		// FIXME workaround --> file bug
		contentLabel->setText(QString());
	}
}

void DvbEpgDialog::scheduleProgram()
{
	const DvbSharedEpgEntry &entry = epgTableModel->value(epgView->currentIndex());

	if (entry.isValid()) {
		manager->getEpgModel()->scheduleProgram(entry, manager->getBeginMargin(),
			manager->getEndMargin());
	}
}

bool DvbEpgEntryLessThan::operator()(const DvbSharedEpgEntry &x, const DvbSharedEpgEntry &y) const
{
	if (x->channel != y->channel) {
		return (x->channel->name.localeAwareCompare(y->channel->name) < 0);
	}

	if (x->begin != y->begin) {
		return (x->begin < y->begin);
	}

	if (x->duration != y->duration) {
		return (x->duration < y->duration);
	}

	if (x->title(FIRST_LANG) != y->title(FIRST_LANG)) {
		return (x->title(FIRST_LANG) < y->title(FIRST_LANG));
	}

	if (x->subheading(FIRST_LANG) != y->subheading(FIRST_LANG)) {
		return (x->subheading(FIRST_LANG) < y->subheading(FIRST_LANG));
	}

	if (x->details(FIRST_LANG) < y->details(FIRST_LANG)) {
		return (x->details(FIRST_LANG) < y->details(FIRST_LANG));
	}

	return (x < y);
}

DvbEpgChannelTableModel::DvbEpgChannelTableModel(QObject *parent) :
	TableModel<DvbEpgChannelTableModelHelper>(parent)
{
}

DvbEpgChannelTableModel::~DvbEpgChannelTableModel()
{
}

void DvbEpgChannelTableModel::setManager(DvbManager *manager)
{
	DvbEpgModel *epgModel = manager->getEpgModel();
	connect(epgModel, SIGNAL(epgChannelAdded(DvbSharedChannel)),
		this, SLOT(epgChannelAdded(DvbSharedChannel)));
	connect(epgModel, SIGNAL(epgChannelRemoved(DvbSharedChannel)),
		this, SLOT(epgChannelRemoved(DvbSharedChannel)));
	// theoretically we should monitor the channel model for updated channels,
	// but it's very unlikely that this has practical relevance

	QHeaderView *headerView = manager->getChannelView()->header();
	DvbChannelLessThan::SortOrder sortOrder;

	if (headerView->sortIndicatorOrder() == Qt::AscendingOrder) {
		if (headerView->sortIndicatorSection() == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameAscending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberAscending;
		}
	} else {
		if (headerView->sortIndicatorSection() == 0) {
			sortOrder = DvbChannelLessThan::ChannelNameDescending;
		} else {
			sortOrder = DvbChannelLessThan::ChannelNumberDescending;
		}
	}

	internalSort(sortOrder);
	resetFromKeys(epgModel->getEpgChannels());
}

QVariant DvbEpgChannelTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedChannel &channel = value(index);

	if (channel.isValid() && (role == Qt::DisplayRole) && (index.column() == 0)) {
		return channel->name;
	}

	return QVariant();
}

QVariant DvbEpgChannelTableModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if ((section == 0) && (orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		return i18nc("@title:column tv show", "Channel");
	}

	return QVariant();
}

void DvbEpgChannelTableModel::epgChannelAdded(const DvbSharedChannel &channel)
{
	insert(channel);
}

void DvbEpgChannelTableModel::epgChannelRemoved(const DvbSharedChannel &channel)
{
	remove(channel);
}

bool DvbEpgTableModelHelper::filterAcceptsItem(const DvbSharedEpgEntry &entry) const
{
	switch (filterType) {
	case ChannelFilter:
		return (entry->channel == channelFilter);
	case ContentFilter:
		return ((contentFilter.indexIn(entry->title(FIRST_LANG)) >= 0) ||
			(contentFilter.indexIn(entry->subheading(FIRST_LANG)) >= 0) ||
			(contentFilter.indexIn(entry->details(FIRST_LANG)) >= 0));
	}

	return false;
}

DvbEpgTableModel::DvbEpgTableModel(QObject *parent) : TableModel<DvbEpgTableModelHelper>(parent),
	epgModel(NULL), contentFilterEventPending(false)
{
	helper.contentFilter.setCaseSensitivity(Qt::CaseInsensitive);
}

DvbEpgTableModel::~DvbEpgTableModel()
{
}

void DvbEpgTableModel::setEpgModel(DvbEpgModel *epgModel_)
{
	if (epgModel != NULL) {
		qCWarning(logEpg, "EPG model already set");
		return;
	}

	epgModel = epgModel_;
	connect(epgModel, SIGNAL(entryAdded(DvbSharedEpgEntry)),
		this, SLOT(entryAdded(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryAboutToBeUpdated(DvbSharedEpgEntry)),
		this, SLOT(entryAboutToBeUpdated(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryUpdated(DvbSharedEpgEntry)),
		this, SLOT(entryUpdated(DvbSharedEpgEntry)));
	connect(epgModel, SIGNAL(entryRemoved(DvbSharedEpgEntry)),
		this, SLOT(entryRemoved(DvbSharedEpgEntry)));
}

void DvbEpgTableModel::setChannelFilter(const DvbSharedChannel &channel)
{
	helper.channelFilter = channel;
	helper.contentFilter.setPattern(QString());
	helper.filterType = DvbEpgTableModelHelper::ChannelFilter;
	reset(epgModel->getEntries());
}

void DvbEpgTableModel::setLanguage(QString lang)
{
	currentLanguage = lang;
	reset(epgModel->getEntries());
}

QVariant DvbEpgTableModel::data(const QModelIndex &index, int role) const
{
	const DvbSharedEpgEntry &entry = value(index);

	if (entry.isValid()) {
		switch (role) {
		case Qt::DecorationRole:
			if ((index.column() == 2) && entry->recording.isValid()) {
				return QIcon::fromTheme(QLatin1String("media-record"), QIcon(":media-record"));
			}

			break;
		case Qt::DisplayRole:
			switch (index.column()) {
			case 0:
				return QLocale().toString((entry->begin.toLocalTime()), QLocale::NarrowFormat);
			case 1:
				return entry->duration.toString("HH:mm");
			case 2:
				return entry->title(currentLanguage);
			case 3:
				return entry->channel->name;
			}

			break;
		}
	}

	return QVariant();
}

QVariant DvbEpgTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		switch (section) {
		case 0:
			return i18nc("@title:column tv show", "Start");
		case 1:
			return i18nc("@title:column tv show", "Duration");
		case 2:
			return i18nc("@title:column tv show", "Title");
		case 3:
			return i18nc("@title:column tv show", "Channel");
		}
	}

	return QVariant();
}

void DvbEpgTableModel::setContentFilter(const QString &pattern)
{
	helper.channelFilter = DvbSharedChannel();
	helper.contentFilter.setPattern(pattern);

	if (!pattern.isEmpty()) {
		helper.filterType = DvbEpgTableModelHelper::ContentFilter;

		if (!contentFilterEventPending) {
			contentFilterEventPending = true;
			QCoreApplication::postEvent(this, new QEvent(QEvent::User),
				Qt::LowEventPriority);
		}
	} else {
		// use channel filter so that content won't be unnecessarily filtered
		helper.filterType = DvbEpgTableModelHelper::ChannelFilter;
		reset(QMap<DvbEpgEntryId, DvbSharedEpgEntry>());
	}
}

void DvbEpgTableModel::entryAdded(const DvbSharedEpgEntry &entry)
{
	insert(entry);
}

void DvbEpgTableModel::entryAboutToBeUpdated(const DvbSharedEpgEntry &entry)
{
	aboutToUpdate(entry);
}

void DvbEpgTableModel::entryUpdated(const DvbSharedEpgEntry &entry)
{
	update(entry);
}

void DvbEpgTableModel::entryRemoved(const DvbSharedEpgEntry &entry)
{
	remove(entry);
}

void DvbEpgTableModel::customEvent(QEvent *event)
{
	Q_UNUSED(event)
	contentFilterEventPending = false;

	if (helper.filterType == DvbEpgTableModelHelper::ContentFilter) {
		reset(epgModel->getEntries());
	}
}
