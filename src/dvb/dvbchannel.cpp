/*
 * dvbchannel.cpp
 *
 * Copyright (C) 2007-2010 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbchannel.h"
#include "dvbchannel_p.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QFile>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <KAction>
#include <KComboBox>
#include <KDebug>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardDirs>
#include "dvbsi.h"

void DvbChannelBase::readChannel(QDataStream &stream)
{
	int type;
	stream >> type;

	stream >> name;
	stream >> number;

	stream >> source;

	switch (type) {
	case DvbTransponderBase::DvbC:
		transponder = DvbTransponder(DvbTransponderBase::DvbC);
		transponder.as<DvbCTransponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::DvbS:
		transponder = DvbTransponder(DvbTransponderBase::DvbS);
		transponder.as<DvbSTransponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::DvbS2:
		transponder = DvbTransponder(DvbTransponderBase::DvbS2);
		transponder.as<DvbS2Transponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::DvbT:
		transponder = DvbTransponder(DvbTransponderBase::DvbT);
		transponder.as<DvbTTransponder>()->readTransponder(stream);
		break;
	case DvbTransponderBase::Atsc:
		transponder = DvbTransponder(DvbTransponderBase::Atsc);
		transponder.as<AtscTransponder>()->readTransponder(stream);
		break;
	default:
		stream.setStatus(QDataStream::ReadCorruptData);
		return;
	}

	stream >> networkId;
	stream >> transportStreamId;
	int serviceId;
	stream >> serviceId;
	stream >> pmtPid;

	stream >> pmtSectionData;
	int videoPid;
	stream >> videoPid;
	stream >> audioPid;

	int flags;
	stream >> flags;
	hasVideo = (videoPid >= 0);
	isScrambled = (flags & 0x1) != 0;
}

static QString enumToString(DvbTransponderBase::FecRate fecRate)
{
	switch (fecRate) {
	case DvbTransponderBase::FecNone: return "NONE";
	case DvbTransponderBase::Fec1_2: return "1/2";
	case DvbTransponderBase::Fec1_3: return "1/3";
	case DvbTransponderBase::Fec1_4: return "1/4";
	case DvbTransponderBase::Fec2_3: return "2/3";
	case DvbTransponderBase::Fec2_5: return "2/5";
	case DvbTransponderBase::Fec3_4: return "3/4";
	case DvbTransponderBase::Fec3_5: return "3/5";
	case DvbTransponderBase::Fec4_5: return "4/5";
	case DvbTransponderBase::Fec5_6: return "5/6";
	case DvbTransponderBase::Fec6_7: return "6/7";
	case DvbTransponderBase::Fec7_8: return "7/8";
	case DvbTransponderBase::Fec8_9: return "8/9";
	case DvbTransponderBase::Fec9_10: return "9/10";
	case DvbTransponderBase::FecAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbCTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbCTransponder::Qam16: return "16-QAM";
	case DvbCTransponder::Qam32: return "32-QAM";
	case DvbCTransponder::Qam64: return "64-QAM";
	case DvbCTransponder::Qam128: return "128-QAM";
	case DvbCTransponder::Qam256: return "256-QAM";
	case DvbCTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbSTransponder::Polarization polarization)
{
	switch (polarization) {
	case DvbSTransponder::Horizontal: return i18n("Horizontal");
	case DvbSTransponder::Vertical: return i18n("Vertical");
	case DvbSTransponder::CircularLeft: return i18n("Circular left");
	case DvbSTransponder::CircularRight: return i18n("Circular right");
	}

	return QString();
}

static QString enumToString(DvbS2Transponder::Modulation modulation)
{
	switch (modulation) {
	case DvbS2Transponder::Qpsk: return "QPSK";
	case DvbS2Transponder::Psk8: return "8-PSK";
	case DvbS2Transponder::Apsk16: return "16-APSK";
	case DvbS2Transponder::Apsk32: return "32-APSK";
	case DvbS2Transponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbS2Transponder::RollOff rollOff)
{
	switch (rollOff) {
	case DvbS2Transponder::RollOff20: return "0.20";
	case DvbS2Transponder::RollOff25: return "0.25";
	case DvbS2Transponder::RollOff35: return "0.35";
	case DvbS2Transponder::RollOffAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Bandwidth bandwidth)
{
	switch (bandwidth) {
	case DvbTTransponder::Bandwidth6MHz: return "6MHz";
	case DvbTTransponder::Bandwidth7MHz: return "7MHz";
	case DvbTTransponder::Bandwidth8MHz: return "8MHz";
	case DvbTTransponder::BandwidthAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Modulation modulation)
{
	switch (modulation) {
	case DvbTTransponder::Qpsk: return "QPSK";
	case DvbTTransponder::Qam16: return "16-QAM";
	case DvbTTransponder::Qam64: return "64-QAM";
	case DvbTTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::TransmissionMode transmissionMode)
{
	switch (transmissionMode) {
	case DvbTTransponder::TransmissionMode2k: return "2k";
	case DvbTTransponder::TransmissionMode4k: return "4k";
	case DvbTTransponder::TransmissionMode8k: return "8k";
	case DvbTTransponder::TransmissionModeAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::GuardInterval guardInterval)
{
	switch (guardInterval) {
	case DvbTTransponder::GuardInterval1_4: return "1/4";
	case DvbTTransponder::GuardInterval1_8: return "1/8";
	case DvbTTransponder::GuardInterval1_16: return "1/16";
	case DvbTTransponder::GuardInterval1_32: return "1/32";
	case DvbTTransponder::GuardIntervalAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(DvbTTransponder::Hierarchy hierarchy)
{
	switch (hierarchy) {
	case DvbTTransponder::HierarchyNone: return "NONE";
	case DvbTTransponder::Hierarchy1: return "1";
	case DvbTTransponder::Hierarchy2: return "2";
	case DvbTTransponder::Hierarchy4: return "4";
	case DvbTTransponder::HierarchyAuto: return "AUTO";
	}

	return QString();
}

static QString enumToString(AtscTransponder::Modulation modulation)
{
	switch (modulation) {
	case AtscTransponder::Qam64: return "64-QAM";
	case AtscTransponder::Qam256: return "256-QAM";
	case AtscTransponder::Vsb8: return "8-VSB";
	case AtscTransponder::Vsb16: return "16-VSB";
	case AtscTransponder::ModulationAuto: return "AUTO";
	}

	return QString();
}

DvbChannelModel::DvbChannelModel(QObject *parent) : QAbstractTableModel(parent)
{
	qRegisterMetaType<QList<QPersistentModelIndex> >("QList<QPersistentModelIndex>");
	connect(this, SIGNAL(queueInternalMove(QList<QPersistentModelIndex>,int)),
		this, SLOT(internalMove(QList<QPersistentModelIndex>,int)), Qt::QueuedConnection);
}

DvbChannelModel::~DvbChannelModel()
{
}

QList<QSharedDataPointer<DvbChannel> > DvbChannelModel::getChannels() const
{
	return channels;
}

QModelIndex DvbChannelModel::findChannelByName(const QString &name) const
{
	for (int row = 0; row < channels.size(); ++row) {
		if (channels.at(row)->name == name) {
			return index(row, 0);
		}
	}

	return QModelIndex();
}

QModelIndex DvbChannelModel::findChannelByNumber(int number) const
{
	for (int row = 0; row < channels.size(); ++row) {
		if (channels.at(row)->number == number) {
			return index(row, 0);
		}
	}

	return QModelIndex();
}

void DvbChannelModel::cloneFrom(const DvbChannelModel *other)
{
	if (this == other) {
		kWarning() << "self assignment";
		return;
	}

	QList<QSharedDataPointer<DvbChannel> > newChannels = other->channels;
	qSort(newChannels);

	for (int row = 0; row < channels.size(); ++row) {
		int nextValidRow = row;

		while (nextValidRow < channels.size()) {
			int position = (qBinaryFind(newChannels, channels.at(nextValidRow)) -
				newChannels.constBegin());

			if (position < newChannels.size()) {
				newChannels.removeAt(position);
				break;
			}

			++nextValidRow;
		}

		if (nextValidRow > row) {
			beginRemoveRows(QModelIndex(), row, nextValidRow - 1);

			for (int i = row; i < nextValidRow; ++i) {
				emit channelAboutToBeRemoved(channels.at(i));
			}

			channels.erase(channels.begin() + row, channels.begin() + nextValidRow);
			endRemoveRows();
		}
	}

	if (!newChannels.isEmpty()) {
		int row = channels.size();
		beginInsertRows(QModelIndex(), row, row + newChannels.size() - 1);
		channels.append(newChannels);

		for (int i = row; i < channels.size(); ++i) {
			emit channelAdded(channels.at(i));
		}

		endInsertRows();
	}

	names = other->names;
	numbers = other->numbers;
}

QAbstractProxyModel *DvbChannelModel::createProxyModel(QObject *parent)
{
	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(parent);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);
	proxyModel->setSourceModel(this);
	return proxyModel;
}

int DvbChannelModel::columnCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return 2;
	}

	return 0;
}

int DvbChannelModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return channels.size();
	}

	return 0;
}

QVariant DvbChannelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		switch (section) {
		case 0:
			return i18n("Name");
		case 1:
			return i18n("Number");
		}
	}

	return QVariant();
}

QVariant DvbChannelModel::data(const QModelIndex &index, int role) const
{
	const DvbChannel *channel = channels.at(index.row());

	switch (role) {
	case Qt::DecorationRole:
		if (index.column() == 0) {
			if (channel->hasVideo) {
				if (!channel->isScrambled) {
					return KIcon("video-television");
				} else {
					return KIcon("video-television-encrypted");
				}
			} else {
				if (!channel->isScrambled) {
					return KIcon("text-speak");
				} else {
					return KIcon("audio-radio-encrypted");
				}
			}
		}

		break;
	case Qt::DisplayRole:
		switch (index.column()) {
		case 0:
			return channel->name;
		case 1:
			return channel->number;
		}

		break;
	case DvbChannelRole:
		return QVariant::fromValue(channel);
	}

	return QVariant();
}

bool DvbChannelModel::removeRows(int row, int count, const QModelIndex &parent)
{
	Q_UNUSED(parent)
	beginRemoveRows(QModelIndex(), row, row + count - 1);

	for (int currentRow = row; currentRow < (row + count); ++currentRow) {
		names.remove(channels.at(currentRow)->name);
		numbers.remove(channels.at(currentRow)->number);
		emit channelAboutToBeRemoved(channels.at(currentRow));
	}

	channels.erase(channels.begin() + row, channels.begin() + row + count);
	endRemoveRows();
	return true;
}

bool DvbChannelModel::setData(const QModelIndex &modelIndex, const QVariant &value, int role)
{
	Q_UNUSED(role)
	const DvbChannel *constChannel = value.value<const DvbChannel *>();

	if (constChannel == NULL) {
		return false;
	}

	DvbChannel *channel = new DvbChannel(*constChannel);
	int row;

	if (modelIndex.isValid()) {
		// explicit update
		row = modelIndex.row();
	} else {
		channel->number = 1;
		row = -1;

		for (int i = 0; i < channels.size(); ++i) {
			const DvbChannel *currentChannel = channels.at(i);

			if ((channel->source == currentChannel->source) &&
			    (channel->networkId == currentChannel->networkId) &&
			    (channel->transportStreamId == currentChannel->transportStreamId) &&
			    (channel->getServiceId() == currentChannel->getServiceId())) {
				// implicit update
				QString currentChannelBaseName = currentChannel->name;
				int position = currentChannelBaseName.lastIndexOf('-');

				if (position > 0) {
					QString suffix = currentChannelBaseName.mid(position + 1);

					if (suffix == QString::number(suffix.toInt())) {
						currentChannelBaseName.truncate(position);
					}
				}

				if (channel->name == currentChannelBaseName) {
					channel->name = currentChannel->name;
				} else {
					channel->name = findNextFreeName(channel->name);
				}

				channel->number = currentChannel->number;
				channel->audioPid = currentChannel->audioPid;
				row = i;
				break;
			}
		}

		DvbPmtParser pmtParser(DvbPmtSection(channel->pmtSectionData));
		bool containsAudioPid = false;

		for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
			if (channel->audioPid == pmtParser.audioPids.at(i).first) {
				containsAudioPid = true;
				break;
			}
		}

		if (!containsAudioPid) {
			if (!pmtParser.audioPids.isEmpty()) {
				channel->audioPid = pmtParser.audioPids.at(0).first;
			} else {
				channel->audioPid = -1;
			}
		}
	}

	if (row >= 0) {
		QString oldName = channels.at(row)->name;
		QString newName = channel->name;

		if (oldName != newName) {
			names.remove(oldName);

			if (!names.contains(newName)) {
				names.insert(newName);
			} else {
				for (int i = 0; i < channels.size(); ++i) {
					if (channels.at(i)->name == newName) {
						const QSharedDataPointer<DvbChannel> oldChannel =
							channels.at(i);
						channels[i]->name =
							findNextFreeName(channels.at(i)->name);
						names.insert(channels.at(i)->name);
						emit channelChanged(oldChannel, channels.at(i));
						QModelIndex modelIndex = index(i, 0);
						emit dataChanged(modelIndex, modelIndex);
						break;
					}
				}
			}
		}

		int oldNumber = channels.at(row)->number;
		int newNumber = channel->number;

		if (oldNumber != newNumber) {
			numbers.remove(oldNumber);

			if (!numbers.contains(newNumber)) {
				numbers.insert(newNumber);
			} else {
				for (int i = 0; i < channels.size(); ++i) {
					if (channels.at(i)->number == newNumber) {
						const QSharedDataPointer<DvbChannel> oldChannel =
							channels.at(i);
						channels[i]->number =
							findNextFreeNumber(channels.at(i)->number);
						numbers.insert(channels.at(i)->number);
						emit channelChanged(oldChannel, channels.at(i));
						QModelIndex modelIndex = index(i, 1);
						emit dataChanged(modelIndex, modelIndex);
						break;
					}
				}
			}
		}

		const QSharedDataPointer<DvbChannel> oldChannel = channels.at(row);
		channels.replace(row, QSharedDataPointer<DvbChannel>(channel));
		emit channelChanged(oldChannel, channels.at(row));
		emit dataChanged(index(row, 0), index(row, 1));
	} else {
		row = channels.size();
		beginInsertRows(QModelIndex(), row, row);
		channel->name = findNextFreeName(channel->name);
		channel->number = findNextFreeNumber(channel->number);
		channels.append(QSharedDataPointer<DvbChannel>(channel));
		names.insert(channel->name);
		numbers.insert(channel->number);
		emit channelAdded(channel);
		endInsertRows();
	}

	return true;
}

Qt::ItemFlags DvbChannelModel::flags(const QModelIndex &index) const
{
	if (index.isValid()) {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDragEnabled);
	} else {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDropEnabled);
	}
}

QMimeData *DvbChannelModel::mimeData(const QModelIndexList &indexes) const
{
	QList<QPersistentModelIndex> persistentIndexes;
	QSet<int> selectedRows;

	foreach (const QModelIndex &index, indexes) {
		int row = index.row();

		if (!selectedRows.contains(row)) {
			selectedRows.insert(row);
			persistentIndexes.append(index);
		}
	}

	QMimeData *mimeData = new QMimeData();
	mimeData->setData("application/x-org.kde.kaffeine-channelindexes", QByteArray());
	mimeData->setProperty("DvbChannelIndexes", QVariant::fromValue(persistentIndexes));
	return mimeData;
}

QStringList DvbChannelModel::mimeTypes() const
{
	return QStringList("application/x-org.kde.kaffeine-channelindexes");
}

Qt::DropActions DvbChannelModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

bool DvbChannelModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
	int column, const QModelIndex &parent)
{
	Q_UNUSED(column)
	Q_UNUSED(parent)

	if (action != Qt::MoveAction) {
		return false;
	}

	QList<QPersistentModelIndex> indexes =
		data->property("DvbChannelIndexes").value<QList<QPersistentModelIndex> >();

	if (indexes.isEmpty()) {
		return false;
	}

	int newNumber = 1;

	if (row < 0) {
		foreach (int number, numbers) {
			if (newNumber <= number) {
				newNumber = (number + 1);
			}
		}
	} else {
		newNumber = channels.at(row)->number;

		while ((newNumber > 1) && !numbers.contains(newNumber - 1)) {
			--newNumber;
		}
	}

	emit queueInternalMove(indexes, newNumber);
	return false;
}

QString DvbChannelModel::findNextFreeName(const QString &name) const
{
	if (!names.contains(name)) {
		return name;
	}

	QString baseName = name;
	int position = baseName.lastIndexOf('-');

	if (position > 0) {
		QString suffix = baseName.mid(position + 1);

		if (suffix == QString::number(suffix.toInt())) {
			baseName.truncate(position);
		}
	}

	QString newName = baseName;
	int suffix = 1;

	while (names.contains(newName)) {
		newName = baseName + '-' + QString::number(suffix);
		++suffix;
	}

	return newName;
}

int DvbChannelModel::findNextFreeNumber(int number) const
{
	while (numbers.contains(number)) {
		++number;
	}

	return number;
}

void DvbChannelModel::internalMove(const QList<QPersistentModelIndex> &indexes, int newNumber)
{
	bool ok = true;
	emit checkInternalMove(&ok);

	if (!ok) {
		return;
	}

	QMap<int, int> mapping; // number --> row

	foreach (const QPersistentModelIndex &index, indexes) {
		if (index.isValid()) {
			int row = index.row();
			numbers.remove(channels.at(row)->number);
			channels[row]->number = -1;
			mapping.insert(newNumber, row);
			++newNumber;
		}
	}

	while (!mapping.isEmpty()) {
		int number = mapping.constBegin().key();
		int row = mapping.constBegin().value();

		if (!numbers.contains(number)) {
			numbers.insert(number);
		} else {
			for (int i = 0; i < channels.size(); ++i) {
				if (channels.at(i)->number == number) {
					mapping.insert(newNumber, i);
					++newNumber;
				}
			}
		}

		const QSharedDataPointer<DvbChannel> oldChannel = channels.at(row);
		channels[row]->number = number;
		emit channelChanged(oldChannel, channels.at(row));
		QModelIndex modelIndex = index(row, 1);
		emit dataChanged(modelIndex, modelIndex);
		mapping.erase(mapping.begin());
	}
}

DvbSqlChannelModel::DvbSqlChannelModel(QObject *parent) : DvbChannelModel(parent)
{
	sqlInterface = new DvbChannelSqlInterface(this, &channels, this);

	for (int row = 0; row < channels.size(); ++row) {
		const DvbChannel *constChannel = channels.at(row);
		QString newName = findNextFreeName(constChannel->name);
		int newNumber = findNextFreeNumber(constChannel->number);
		names.insert(newName);
		numbers.insert(newNumber);

		if ((constChannel->name != newName) || (constChannel->number != newNumber)) {
			DvbChannel *channel = channels[row];
			channel->name = newName;
			channel->number = newNumber;
			emit dataChanged(index(row, 0), index(row, 1));
		}
	}

	// compatibility code

	QFile file(KStandardDirs::locateLocal("appdata", "channels.dtv"));

	if (!file.exists()) {
		return;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		kWarning() << "cannot open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	while (!stream.atEnd()) {
		DvbChannel channel;
		channel.readChannel(stream);

		if (stream.status() != QDataStream::Ok) {
			kWarning() << "invalid channels in file" << file.fileName();
			break;
		}

		// if the index is invalid, the channel is added
		const DvbChannel *constChannel = &channel;
		setData(QModelIndex(), QVariant::fromValue(constChannel));
	}

	if (!file.remove()) {
		kWarning() << "cannot remove" << file.fileName();
	}
}

DvbSqlChannelModel::~DvbSqlChannelModel()
{
	sqlInterface->flush();
}

DvbChannelView::DvbChannelView(QWidget *parent) : QTreeView(parent)
{
}

DvbChannelView::~DvbChannelView()
{
}

KAction *DvbChannelView::addEditAction()
{
	KAction *action = new KAction(KIcon("configure"), i18n("Edit"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(editChannel()));
	addAction(action);
	return action;
}

KAction *DvbChannelView::addRemoveAction()
{
	KAction *action = new KAction(KIcon("edit-delete"),
		i18nc("remove an item from a list", "Remove"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(removeChannel()));
	addAction(action);
	return action;
}

void DvbChannelView::checkInternalMove(bool *ok)
{
	if ((*ok) && ((header()->sortIndicatorSection() != 1) ||
	    (header()->sortIndicatorOrder() != Qt::AscendingOrder))) {
		if (KMessageBox::warningContinueCancel(this, i18nc("message box",
			"The channels will be sorted by number to allow drag and drop.\n"
			"Do you want to continue?")) == KMessageBox::Continue) {
			sortByColumn(1, Qt::AscendingOrder);
		} else {
			*ok = false;
		}
	}
}

void DvbChannelView::editChannel()
{
	QModelIndex index = currentIndex();

	if (index.isValid()) {
		KDialog *dialog = new DvbChannelEditor(model(), index, this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setModal(true);
		dialog->show();
	}
}

void DvbChannelView::removeChannel()
{
	QList<int> selectedRows;

	foreach (const QItemSelectionRange &range, selectionModel()->selection()) {
		if (range.left() == 0) {
			for (int row = range.top(); row <= range.bottom(); ++row) {
				selectedRows.append(row);
			}
		}
	}

	qSort(selectedRows);
	QAbstractItemModel *channelProxyModel = model();
	int offset = 0;

	for (int i = 0; i < selectedRows.size(); ++i) {
		int begin = selectedRows.at(i);
		int end = (begin + 1);

		while (((i + 1) < selectedRows.size()) && (selectedRows.at(i + 1) == end)) {
			++end;
			++i;
		}

		channelProxyModel->removeRows(begin - offset, end - begin);
		offset += (end - begin);
	}
}

void DvbChannelView::removeAllChannels()
{
	QAbstractItemModel *channelModel = model();
	int count = channelModel->rowCount();

	if (count > 0) {
		channelModel->removeRows(0, count);
	}
}

DvbChannelSqlInterface::DvbChannelSqlInterface(QAbstractItemModel *model,
	QList<QSharedDataPointer<DvbChannel> > *channels_, QObject *parent) :
	SqlTableModelInterface(parent), channels(channels_)
{
	init(model, "Channels",
		QStringList() << "Name" << "Number" << "Source" << "Transponder" << "NetworkId" <<
		"TransportStreamId" << "PmtPid" << "PmtSection" << "AudioPid" << "Flags");
}

DvbChannelSqlInterface::~DvbChannelSqlInterface()
{
}

int DvbChannelSqlInterface::insertFromSqlQuery(const QSqlQuery &query, int index)
{
	DvbChannel *channel = new DvbChannel();
	channel->name = query.value(index++).toString();
	channel->number = query.value(index++).toInt();
	channel->source = query.value(index++).toString();
	channel->transponder = DvbTransponder::fromString(query.value(index++).toString());
	channel->networkId = query.value(index++).toInt();
	channel->transportStreamId = query.value(index++).toInt();
	channel->pmtPid = query.value(index++).toInt();
	channel->pmtSectionData = query.value(index++).toByteArray();
	channel->audioPid = query.value(index++).toInt();
	int flags = query.value(index++).toInt();
	channel->hasVideo = ((flags & 0x01) != 0);
	channel->isScrambled = ((flags & 0x02) != 0);

	if (channel->name.isEmpty() || (channel->number < 1) || channel->source.isEmpty() ||
	    !channel->transponder.isValid() ||
	    (channel->networkId < -1) || (channel->networkId > 0xffff) ||
	    (channel->transportStreamId < 0) || (channel->transportStreamId > 0xffff) ||
	    (channel->pmtPid < 0) || (channel->pmtPid > 0x1fff) ||
	    channel->pmtSectionData.isEmpty() ||
	    (channel->audioPid < -1) || (channel->audioPid > 0x1fff)) {
		delete channel;
		return -1;
	}

	int row = channels->size();
	channels->append(QSharedDataPointer<DvbChannel>(channel));
	return row;
}

void DvbChannelSqlInterface::bindToSqlQuery(QSqlQuery &query, int index, int row) const
{
	const DvbChannel *channel = channels->at(row);
	query.bindValue(index++, channel->name);
	query.bindValue(index++, channel->number);
	query.bindValue(index++, channel->source);
	query.bindValue(index++, channel->transponder.toString());
	query.bindValue(index++, channel->networkId);
	query.bindValue(index++, channel->transportStreamId);
	query.bindValue(index++, channel->pmtPid);
	query.bindValue(index++, channel->pmtSectionData);
	query.bindValue(index++, channel->audioPid);
	query.bindValue(index++, (channel->hasVideo ? 0x01 : 0) |
				 (channel->isScrambled ? 0x02 : 0));
}

DvbChannelEditor::DvbChannelEditor(QAbstractItemModel *model_, const QModelIndex &modelIndex_,
	QWidget *parent) : KDialog(parent), model(model_), persistentIndex(modelIndex_)
{
	setCaption(i18n("Channel Settings"));
	channel = model->data(persistentIndex,
		DvbChannelModel::DvbChannelRole).value<const DvbChannel *>();

	QWidget *widget = new QWidget(this);
	QBoxLayout *mainLayout = new QVBoxLayout(widget);
	QGridLayout *gridLayout = new QGridLayout();

	nameEdit = new KLineEdit(widget);
	nameEdit->setText(channel->name);
	gridLayout->addWidget(nameEdit, 0, 1);

	QLabel *label = new QLabel(i18n("Name:"), widget);
	label->setBuddy(nameEdit);
	gridLayout->addWidget(label, 0, 0);

	numberBox = new QSpinBox(widget);
	numberBox->setRange(1, 99999);
	numberBox->setValue(channel->number);
	gridLayout->addWidget(numberBox, 0, 3);

	label = new QLabel(i18n("Number:"), widget);
	label->setBuddy(numberBox);
	gridLayout->addWidget(label, 0, 2);
	mainLayout->addLayout(gridLayout);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QGroupBox *groupBox = new QGroupBox(widget);
	gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18n("Source:")), 0, 0);
	gridLayout->addWidget(new QLabel(channel->source), 0, 1);

	switch (channel->transponder.getTransmissionType()) {
	case DvbTransponderBase::Invalid:
		break;
	case DvbTransponderBase::DvbC: {
		const DvbCTransponder *tp = channel->transponder.as<DvbCTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Symbol rate (kS/s):")), 2, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->symbolRate / 1000.0)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 3, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRate)), 4, 1);
		break;
	    }
	case DvbTransponderBase::DvbS:
	case DvbTransponderBase::DvbS2: {
		const DvbS2Transponder *tp2 = channel->transponder.as<DvbS2Transponder>();
		const DvbSTransponder *tp = channel->transponder.as<DvbSTransponder>();

		if (tp == NULL) {
			tp = tp2;
		}

		gridLayout->addWidget(new QLabel(i18n("Polarization:")), 1, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->polarization)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 2, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->frequency / 1000.0)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Symbol rate (kS/s):")), 3, 0);
		gridLayout->addWidget(new QLabel(QString::number(tp->symbolRate / 1000.0)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRate)), 4, 1);

		if (tp2 != NULL) {
			gridLayout->addWidget(new QLabel(i18n("Modulation:")), 5, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp2->modulation)), 5, 1);
			gridLayout->addWidget(new QLabel(i18n("Roll-off:")), 6, 0);
			gridLayout->addWidget(new QLabel(enumToString(tp2->rollOff)), 6, 1);
		}

		break;
	    }
	case DvbTransponderBase::DvbT: {
		const DvbTTransponder *tp = channel->transponder.as<DvbTTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Bandwidth:")), 2, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->bandwidth)), 2, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 3, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 3, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate:")), 4, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRateHigh)), 4, 1);
		gridLayout->addWidget(new QLabel(i18n("FEC rate LP:")), 5, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->fecRateLow)), 5, 1);
		gridLayout->addWidget(new QLabel(i18n("Transmission mode:")), 6, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->transmissionMode)), 6, 1);
		gridLayout->addWidget(new QLabel(i18n("Guard interval:")), 7, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->guardInterval)), 7, 1);
		gridLayout->addWidget(new QLabel(i18n("Hierarchy:")), 8, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->hierarchy)), 8, 1);
		break;
	    }
	case DvbTransponderBase::Atsc: {
		const AtscTransponder *tp = channel->transponder.as<AtscTransponder>();
		gridLayout->addWidget(new QLabel(i18n("Frequency (MHz):")), 1, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(tp->frequency / 1000000.0)), 1, 1);
		gridLayout->addWidget(new QLabel(i18n("Modulation:")), 2, 0);
		gridLayout->addWidget(new QLabel(enumToString(tp->modulation)), 2, 1);
		break;
	    }
	}

	gridLayout->addWidget(new QLabel(), 10, 0, 1, 2);

	gridLayout->addWidget(new QLabel(i18n("PMT PID:")), 11, 0);
	gridLayout->addWidget(new QLabel(QString::number(channel->pmtPid)), 11, 1);

	DvbPmtParser pmtParser(DvbPmtSection(channel->pmtSectionData));
	int row = 12;

	if (pmtParser.videoPid >= 0) {
		gridLayout->addWidget(new QLabel(i18n("Video PID:")), row, 0);
		gridLayout->addWidget(new QLabel(QString::number(pmtParser.videoPid)), row++, 1);
	}

	if (!pmtParser.subtitlePids.isEmpty()) {
		gridLayout->addWidget(new QLabel(i18n("Subtitle PID:")), row, 0);
	}

	for (int i = 0; i < pmtParser.subtitlePids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.subtitlePids.at(i);
		gridLayout->addWidget(new QLabel(QString("%1 (%2)").arg(it.first).arg(it.second)),
			row++, 1);
	}

	if (pmtParser.teletextPid != -1) {
		gridLayout->addWidget(new QLabel(i18n("Teletext PID:")), row, 0);
		gridLayout->addWidget(
			new QLabel(QString::number(pmtParser.teletextPid)), row++, 1);
	}

	gridLayout->addItem(new QSpacerItem(0, 0), row, 0, 1, 2);
	gridLayout->setRowStretch(row, 1);
	boxLayout->addWidget(groupBox);

	groupBox = new QGroupBox(widget);
	gridLayout = new QGridLayout(groupBox);
	gridLayout->addWidget(new QLabel(i18n("Network ID:")), 0, 0);

	networkIdBox = new QSpinBox(groupBox);
	networkIdBox->setRange(-1, (1 << 16) - 1);
	networkIdBox->setValue(channel->networkId);
	gridLayout->addWidget(networkIdBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("Transport stream ID:")), 1, 0);

	transportStreamIdBox = new QSpinBox(groupBox);
	transportStreamIdBox->setRange(0, (1 << 16) - 1);
	transportStreamIdBox->setValue(channel->transportStreamId);
	gridLayout->addWidget(transportStreamIdBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Service ID:")), 2, 0);

	serviceIdBox = new QSpinBox(groupBox);
	serviceIdBox->setRange(0, (1 << 16) - 1);
	serviceIdBox->setValue(channel->getServiceId());
	gridLayout->addWidget(serviceIdBox, 2, 1);

	gridLayout->addWidget(new QLabel(i18n("Audio channel:")), 3, 0);

	audioChannelBox = new KComboBox(groupBox);

	for (int i = 0; i < pmtParser.audioPids.size(); ++i) {
		const QPair<int, QString> &it = pmtParser.audioPids.at(i);
		QString text = QString::number(it.first);

		if (!it.second.isEmpty()) {
			text = text + " (" + it.second + ')';
		}

		audioChannelBox->addItem(text);
		audioPids.append(it.first);
	}

	audioChannelBox->setCurrentIndex(audioPids.indexOf(channel->audioPid));

	if (audioPids.size() <= 1) {
		audioChannelBox->setEnabled(false);
	}

	gridLayout->addWidget(audioChannelBox, 3, 1);

	gridLayout->addWidget(new QLabel(i18n("Scrambled:")), 4, 0);

	scrambledBox = new QCheckBox(groupBox);
	scrambledBox->setChecked(channel->isScrambled);
	gridLayout->addWidget(scrambledBox, 4, 1);

	gridLayout->addItem(new QSpacerItem(0, 0), 5, 0, 1, 2);
	gridLayout->setRowStretch(5, 1);
	boxLayout->addWidget(groupBox);
	mainLayout->addLayout(boxLayout);

	setMainWidget(widget);
}

DvbChannelEditor::~DvbChannelEditor()
{
}

void DvbChannelEditor::accept()
{
	if (persistentIndex.isValid()) {
		DvbChannel updatedChannel = *channel;
		updatedChannel.name = nameEdit->text();
		updatedChannel.number = numberBox->value();
		updatedChannel.networkId = networkIdBox->value();
		updatedChannel.transportStreamId = transportStreamIdBox->value();
		updatedChannel.setServiceId(serviceIdBox->value());

		if (audioChannelBox->currentIndex() != -1) {
			updatedChannel.audioPid = audioPids.at(audioChannelBox->currentIndex());
		}

		updatedChannel.isScrambled = scrambledBox->isChecked();

		const DvbChannel *constUpdatedChannel = &updatedChannel;
		model->setData(persistentIndex, QVariant::fromValue(constUpdatedChannel));
	}

	KDialog::accept();
}
