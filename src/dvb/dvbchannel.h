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
#include <QSortFilterProxyModel>
#include <QTreeView>

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

// you can't modify it anyway; explicit sharing just makes implementation easier
typedef QExplicitlySharedDataPointer<DvbTransponder> DvbSharedTransponder;

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

	DvbSharedTransponder getTransponder() const
	{
		return transponder;
	}

	void setTransponder(DvbSharedTransponder transponder_)
	{
		transponder = transponder_;
	}

	/*
	 * model functions
	 */

	static int columnCount();
	static QVariant headerData(int column);
	QVariant modelData(int column) const;

	/*
	 * static functions for reading / writing channel list
	 */

	static QList<DvbChannel> readList();
	static void writeList(const QList<DvbChannel> &list);

private:
	DvbSharedTransponder transponder;
};

template<class T> class DvbGenericChannelModel : protected QAbstractTableModel
{
public:
	explicit DvbGenericChannelModel(QObject *parent) : QAbstractTableModel(parent)
	{
		proxyModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
		proxyModel.setSortLocaleAware(true);
		proxyModel.setSourceModel(this);
	}

	~DvbGenericChannelModel() { }

	QList<T> getList() const
	{
		return list;
	}

	void setList(const QList<T> &list_)
	{
		list = list_;
		reset();
	}

	void appendList(const QList<T> &values)
	{
		beginInsertRows(QModelIndex(), list.size(), list.size() + values.size() - 1);
		list += values;
		endInsertRows();
	}

	QSortFilterProxyModel *getProxyModel()
	{
		return &proxyModel;
	}

	const T *getChannel(const QModelIndex &index) const
	{
		QModelIndex sourceIndex = proxyModel.mapToSource(index);

		if (!sourceIndex.isValid() || sourceIndex.row() >= list.size()) {
			return NULL;
		}

		return &list.at(sourceIndex.row());
	}

	void updateChannel(int position, const T &channel)
	{
		list.replace(position, channel);
		emit dataChanged(index(position, 0), index(position, T::columnCount() - 1));
	}

private:
	int columnCount(const QModelIndex &parent) const
	{
		if (parent.isValid()) {
			return 0;
		}

		return T::columnCount();
	}

	int rowCount(const QModelIndex &parent) const
	{
		if (parent.isValid()) {
			return 0;
		}

		return list.size();
	}

	QVariant data(const QModelIndex &index, int role) const
	{
		if (!index.isValid() || role != Qt::DisplayRole || index.row() >= list.size()) {
			return QVariant();
		}

		return list.at(index.row()).modelData(index.column());
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const
	{
		if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
			return QVariant();
		}

		return T::headerData(section);
	}

	QList<T> list;
	QSortFilterProxyModel proxyModel;
};

class DvbChannelModel : public DvbGenericChannelModel<DvbChannel>
{
public:
	explicit DvbChannelModel(QObject *parent) : DvbGenericChannelModel<DvbChannel>(parent) { }
	~DvbChannelModel() { }
};

class DvbChannelViewBase : public QTreeView
{
public:
	explicit DvbChannelViewBase(QWidget *parent);
	~DvbChannelViewBase();
};

// this class adds a context menu
class DvbChannelView : public DvbChannelViewBase
{
	Q_OBJECT
public:
	explicit DvbChannelView(QWidget *parent);
	~DvbChannelView();

	/*
	 * should be only used in the scan dialog
	 */

	void enableDeleteAction();

protected:
	void contextMenuEvent(QContextMenuEvent *event);

private slots:
	void actionEdit();
	void actionChangeIcon();
	void actionDelete();

private:
	QMenu *menu;
	QPersistentModelIndex menuIndex;
};

#endif /* DVBCHANNEL_H */
