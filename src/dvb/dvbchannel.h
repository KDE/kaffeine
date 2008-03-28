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

#include <QSharedData>
#include <QTreeView>

class QSortFilterProxyModel;
class DvbChannelReader;
class DvbChannelWriter;

class DvbTransponder : public QSharedData
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
		Fec1_2 = 0,
		Fec2_3 = 1,
		Fec3_4 = 2,
		Fec4_5 = 3,
		Fec5_6 = 4,
		Fec6_7 = 5,
		Fec7_8 = 6,
		Fec8_9 = 7,
		FecAuto = 8,
		FecRateMax = FecAuto
	};

	virtual ~DvbTransponder() { }

	virtual void writeTransponder(DvbChannelWriter &writer) const = 0;

	const TransmissionType transmissionType;

protected:
	DvbTransponder(TransmissionType transmissionType_) : transmissionType(transmissionType_) { }

private:
	Q_DISABLE_COPY(DvbTransponder);
};

class DvbCTransponder : public DvbTransponder
{
public:
	enum ModulationType
	{
		Qam16 = 0,
		Qam32 = 1,
		Qam64 = 2,
		Qam128 = 3,
		Qam256 = 4,
		Auto = 5,
		ModulationTypeMax = Auto
	};

	DvbCTransponder(int frequency_, ModulationType modulationType_, int symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbC), frequency(frequency_),
		modulationType(modulationType_), symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbCTransponder() { }

	static DvbCTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

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
		Horizontal = 0,
		Vertical = 1,
		CircularLeft = 2,
		CircularRight = 3,
		PolarizationMax = CircularRight
	};

	DvbSTransponder(Polarization polarization_, int frequency_, int symbolRate_,
		FecRate fecRate_) : DvbTransponder(DvbS), polarization(polarization_),
		frequency(frequency_), symbolRate(symbolRate_), fecRate(fecRate_) { }
	~DvbSTransponder() { }

	static DvbSTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

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
		Qpsk = 0,
		Qam16 = 1,
		Qam64 = 2,
		Auto = 3,
		ModulationTypeMax = Auto
	};

	enum Bandwidth
	{
		Bandwidth6Mhz = 0,
		Bandwidth7Mhz = 1,
		Bandwidth8Mhz = 2,
		BandwidthAuto = 3,
		BandwidthMax = BandwidthAuto
	};

	enum TransmissionMode
	{
		TransmissionMode2k = 0,
		TransmissionMode8k = 1,
		TransmissionModeAuto = 2,
		TransmissionModeMax = TransmissionModeAuto
	};

	enum GuardInterval
	{
		GuardInterval1_4 = 0,
		GuardInterval1_8 = 1,
		GuardInterval1_16 = 2,
		GuardInterval1_32 = 3,
		GuardIntervalAuto = 4,
		GuardIntervalMax = GuardIntervalAuto
	};

	enum Hierarchy
	{
		HierarchyNone = 0,
		Hierarchy1 = 1,
		Hierarchy2 = 2,
		Hierarchy4 = 3,
		HierarchyAuto = 4,
		HierarchyMax = HierarchyAuto
	};

	DvbTTransponder(int frequency_, ModulationType modulationType_, Bandwidth bandwidth_,
		FecRate fecRateHigh_, FecRate fecRateLow_, TransmissionMode transmissionMode_,
		GuardInterval guardInterval_, Hierarchy hierarchy_) : DvbTransponder(DvbT),
		frequency(frequency_), modulationType(modulationType_), bandwidth(bandwidth_),
		fecRateHigh(fecRateHigh_), fecRateLow(fecRateLow_),
		transmissionMode(transmissionMode_), guardInterval(guardInterval_),
		hierarchy(hierarchy_) { }
	~DvbTTransponder() { }

	static DvbTTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

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
		Qam64 = 0,
		Qam256 = 1,
		Vsb8 = 2,
		Vsb16 = 3,
		Auto = 4,
		ModulationTypeMax = Auto
	};

	AtscTransponder(int frequency_, ModulationType modulationType_) : DvbTransponder(Atsc),
		frequency(frequency_), modulationType(modulationType_) { }
	~AtscTransponder() { }

	static AtscTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

	/*
	 * frequency (Hz)
	 */

	int frequency;

	/*
	 * modulation type
	 */

	ModulationType modulationType;
};

typedef QSharedDataPointer<DvbTransponder> DvbSharedTransponder;

class DvbChannel
{
public:
	DvbChannel() : name(), number(-1), source(), networkId(-1), tsId(-1), serviceId(-1),
		videoPid(-1), videoType(-1), transponder() { }
	~DvbChannel() { }

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
	 * network id
	 */

	int networkId;

	/*
	 * transport stream id
	 */

	int tsId;

	/*
	 * service id
	 */

	int serviceId;

	/*
	 * video pid
	 */

	int videoPid;

	/*
	 * video type
	 */

	int videoType;

	/*
	 * transponder (owned by DvbChannel)
	 */

	const DvbTransponder *getTransponder() const
	{
		return transponder;
	}

	void setTransponder(DvbTransponder *value)
	{
		transponder = DvbSharedTransponder(value);
	}

private:
	DvbSharedTransponder transponder;
};

class DvbChannelModel : public QAbstractTableModel
{
public:
	DvbChannelModel(QObject *parent);
	~DvbChannelModel();

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	const DvbChannel *getChannel(const QModelIndex &index) const;

	QList<DvbChannel> getList() const
	{
		return list;
	}

	void setList(const QList<DvbChannel> &list_);

	static QList<DvbChannel> readList();
	static void writeList(QList<DvbChannel> list);

private:
	QList<DvbChannel> list;
};

class DvbChannelView : public QTreeView
{
	Q_OBJECT
public:
	explicit DvbChannelView(QWidget *parent);
	~DvbChannelView();

	void setModel(QAbstractItemModel *model);

	/*
	 * should be only used in the scan dialog
	 */

	void enableDeleteAction();

public slots:
	void setFilterRegExp(const QString& string);

protected:
	void contextMenuEvent(QContextMenuEvent *event);

private slots:
	void actionEdit();
	void actionChangeIcon();
	void actionDelete();

private:
	QSortFilterProxyModel *proxyModel;
	QMenu *menu;
	QPersistentModelIndex menuIndex;
};

#endif /* DVBCHANNEL_H */
