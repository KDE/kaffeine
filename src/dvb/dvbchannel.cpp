/*
 * dvbchannel.cpp
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

#include <KLocalizedString>

#include "dvbchannel.h"

DvbChannelModel::DvbChannelModel(QObject *parent) : QAbstractTableModel(parent)
{
}

DvbChannelModel::~DvbChannelModel()
{
}

int DvbChannelModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return list.size();
}

int DvbChannelModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 2;
}

QVariant DvbChannelModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole || index.column() >= 2
	    || index.row() >= list.size()) {
		return QVariant();
	}

	if (index.column() == 0) {
		return list.at(index.row())->name;
	} else {
		return list.at(index.row())->number;
	}
}

QVariant DvbChannelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}

	if (section == 0) {
		return i18n("Name");
	} else {
		return i18n("Number");
	}
}

void DvbChannelModel::setList(const DvbChannelList &list_)
{
	reset();
	list = list_;
}
