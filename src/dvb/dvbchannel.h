/*
 * dvbchannel.h
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <QAbstractTableModel>
#include <QSharedData>

class DvbTransponder
{
public:
	enum TransmissionType
	{
		DvbC,
		DvbS,
		DvbT,
		Atsc
	};

	enum FecRate
	{
		Fec1_2,
		Fec2_3,
		Fec3_4,
		Fec4_5,
		Fec5_6,
		Fec6_7,
		Fec7_8,
		Fec8_9,
		FecAuto
	};

	virtual ~DvbTransponder() { }

	const TransmissionType transmissionType;

protected:
	DvbTransponder(TransmissionType transmissionType_) : transmissionType(transmissionType_) { }
};

class DvbCTransponder : public DvbTransponder
{
public:
	enum ModulationType
	{
		Qam16,
		Qam32,
		Qam64,
		Qam128,
		Qam256,
		QamAuto
	};

	DvbCTransponder(int frequency_, ModulationType modulationType_, int symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbC), frequency(frequency_),
		modulationType(modulationType_), symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbCTransponder() { }

	/*
	 * frequency (Hz)
	 */

	int frequency;

	/*
	 * modulation type
	 */

	ModulationType modulationType;

	/*
	 * symbol rate (symbols per second)
	 */

	int symbolRate;

	/*
	 * FEC rate
	 */

	FecRate fecRate;
};

class DvbSTransponder : public DvbTransponder
{
public:
	enum Polarization
	{
		Horizontal,
		Vertical
	};

	DvbSTransponder(Polarization polarization_, int frequency_, int symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbS), polarization(polarization_),
		frequency(frequency_), symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbSTransponder() { }

	/*
	 * polarization
	 */

	Polarization polarization;

	/*
	 * frequency (kHz)
	 */

	int frequency;

	/*
	 * symbol rate (symbols per second)
	 */

	int symbolRate;

	/*
	 * FEC rate
	 */

	FecRate fecRate;
};

class DvbTTransponder : public DvbTransponder
{
public:
	enum ModulationType
	{
		Qpsk,
		Qam16,
		Qam64,
		Auto
	};

	enum Bandwidth
	{
		Bandwidth6Mhz,
		Bandwidth7Mhz,
		Bandwidth8Mhz,
		BandwidthAuto
	};

	enum TransmissionMode
	{
		TransmissionMode2k,
		TransmissionMode8k,
		TransmissionModeAuto
	};

	enum GuardInterval
	{
		GuardInterval1_4,
		GuardInterval1_8,
		GuardInterval1_16,
		GuardInterval1_32,
		GuardIntervalAuto
	};

	enum Hierarchy
	{
		HierarchyNone,
		Hierarchy1,
		Hierarchy2,
		Hierarchy4,
		HierarchyAuto
	};

	DvbTTransponder(int frequency_, ModulationType modulationType_, Bandwidth bandwidth_,
		FecRate fecRateHigh_, FecRate fecRateLow_, TransmissionMode transmissionMode_,
		GuardInterval guardInterval_, Hierarchy hierarchy_) : DvbTransponder(DvbT),
		frequency(frequency_), modulationType(modulationType_), bandwidth(bandwidth_),
		fecRateHigh(fecRateHigh_), fecRateLow(fecRateLow_),
		transmissionMode(transmissionMode_), guardInterval(guardInterval_),
		hierarchy(hierarchy_) { }
	~DvbTTransponder() { }

	/*
	 * frequency (Hz)
	 */

	int frequency;

	/*
	 * modulation type
	 */

	ModulationType modulationType;

	/*
	 * bandwidth
	 */

	Bandwidth bandwidth;

	/*
	 * FEC rate (high priority stream)
	 */

	FecRate fecRateHigh;

	/*
	 * FEC rate (low priority stream)
	 */

	FecRate fecRateLow;

	/*
	 * transmission mode
	 */

	TransmissionMode transmissionMode;

	/*
	 * guard interval
	 */

	GuardInterval guardInterval;

	/*
	 * hierarchy
	 */

	Hierarchy hierarchy;
};

class AtscTransponder : public DvbTransponder
{
public:
	enum ModulationType
	{
		Qam64,
		Qam256,
		QamAuto,
		Vsb8,
		Vsb16
	};

	AtscTransponder(int frequency_, ModulationType modulationType_) : DvbTransponder(Atsc),
		frequency(frequency_), modulationType(modulationType_) { }
	~AtscTransponder() { }

	/*
	 * frequency (Hz)
	 */

	int frequency;

	/*
	 * modulation type
	 */

	ModulationType modulationType;
};

class DvbChannel : public QSharedData
{
public:
	DvbChannel(const QString &name_, int number_, const QString &source_,
		DvbTransponder *transponder_, int serviceId_, int videoPid_) : name(name_),
		number(number_), source(source_), serviceId(serviceId_), videoPid(videoPid_),
		transponder(transponder_) { }

	~DvbChannel()
	{
		delete transponder;
	}

	/*
	 * name
	 */

	QString name;

	/*
	 * number
	 */

	int number;

	/*
	 * source
	 */

	QString source;

	/*
	 * transponder (owned by DvbChannel)
	 */

	const DvbTransponder *getTransponder() const
	{
		return transponder;
	}

	void setTransponder(DvbTransponder *value)
	{
		delete transponder;
		transponder = value;
	}

	/*
	 * service id
	 */

	int serviceId;

	/*
	 * video pid
	 */

	int videoPid;

private:
	Q_DISABLE_COPY(DvbChannel)

	DvbTransponder *transponder;
};

/*
 * explicitly shared means here that all "copies" will access and modify the same data
 */

typedef QExplicitlySharedDataPointer<DvbChannel> DvbSharedChannel;
typedef QList<DvbSharedChannel> DvbChannelList;

class DvbChannelModel : public QAbstractTableModel
{
public:
	explicit DvbChannelModel(QObject *parent);
	~DvbChannelModel();

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	DvbChannelList getList() const
	{
		return list;
	}

	void setList(const DvbChannelList &list_);

private:
	DvbChannelList list;
};

#endif /* DVBCHANNEL_H */
