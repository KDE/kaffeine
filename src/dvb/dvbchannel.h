/*
 * dvbchannel.h
 *
 * Copyright (C) 2007-2008 Christoph Pfister <christophpfister@gmail.com>
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
class DvbCTransponder;
class DvbSTransponder;
class DvbTTransponder;
class AtscTransponder;

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

	virtual const DvbCTransponder *getDvbCTransponder() const
	{
		return NULL;
	}

	virtual const DvbSTransponder *getDvbSTransponder() const
	{
		return NULL;
	}

	virtual const DvbTTransponder *getDvbTTransponder() const
	{
		return NULL;
	}

	virtual const AtscTransponder *getAtscTransponder() const
	{
		return NULL;
	}

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

	const DvbCTransponder *getDvbCTransponder() const
	{
		return this;
	}

	static DvbCTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

	int frequency; // Hz
	ModulationType modulationType;
	int symbolRate; // symbols per second
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

	const DvbSTransponder *getDvbSTransponder() const
	{
		return this;
	}

	static DvbSTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

	Polarization polarization;
	int frequency; // kHz
	int symbolRate; // symbols per second
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

	const DvbTTransponder *getDvbTTransponder() const
	{
		return this;
	}

	static DvbTTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

	int frequency; // Hz
	ModulationType modulationType;
	Bandwidth bandwidth;
	FecRate fecRateHigh; // high priority stream
	FecRate fecRateLow; // low priority stream
	TransmissionMode transmissionMode;
	GuardInterval guardInterval;
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

	const AtscTransponder *getAtscTransponder() const
	{
		return this;
	}

	static AtscTransponder *readTransponder(DvbChannelReader &reader);
	void writeTransponder(DvbChannelWriter &writer) const;

	int frequency; // Hz
	ModulationType modulationType;
};

typedef QSharedDataPointer<DvbTransponder> DvbSharedTransponder;

class DvbChannel
{
public:
	DvbChannel() : name(), number(-1), source(), networkId(-1), transportStreamId(-1),
		serviceId(-1), pmtPid(-1), videoPid(-1), audioPid(-1), scrambled(false),
		transponder() { }
	~DvbChannel() { }

	QString name;
	int number;

	QString source;
	int networkId; // may be -1 (not present)
	int transportStreamId; // may be -1 (not present)
	int serviceId;

	int pmtPid;
	int videoPid; // may be -1 (not present)
	int audioPid; // may be -1 (not present)

	bool scrambled;

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

	/*
	 * static functions for reading / writing channel list
	 */

	static QList<DvbChannel> readList();
	static void writeList(const QList<DvbChannel> &list);

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
