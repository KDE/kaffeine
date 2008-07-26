/*
 * dvbchannelview.cpp
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

#include "dvbchannelview.h"

#include <QContextMenuEvent>
#include <KAction>
#include <KLocalizedString>
#include <KMenu>
#include "dvbchannel.h"

int DvbChannelModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 2;
}

QVariant DvbChannelModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole || index.row() >= list.size()) {
		return QVariant();
	}

	switch (index.column()) {
	case 0:
		return list.at(index.row())->name;
	case 1:
		return list.at(index.row())->number;
	}

	return QVariant();
}

QVariant DvbChannelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18n("Name");
	case 1:
		return i18n("Number");
	}

	return QVariant();
}

DvbChannelViewBase::DvbChannelViewBase(QWidget *parent) : QTreeView(parent)
{
	setIndentation(0);
	setSortingEnabled(true);
	sortByColumn(0, Qt::AscendingOrder);
}

DvbChannelViewBase::~DvbChannelViewBase()
{
}

DvbChannelView::DvbChannelView(QWidget *parent) : DvbChannelViewBase(parent)
{
	menu = new KMenu(this);

	KAction *action = new KAction(i18n("Edit channel"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(actionEdit()));
	menu->addAction(action);

	action = new KAction(i18n("Change icon"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(actionChangeIcon()));
	menu->addAction(action);
}

DvbChannelView::~DvbChannelView()
{
}

void DvbChannelView::enableDeleteAction()
{
	KAction *action = new KAction(i18n("Delete channel"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(actionDelete()));
	menu->addAction(action);
}

void DvbChannelView::contextMenuEvent(QContextMenuEvent *event)
{
	QModelIndex index = indexAt(event->pos());

	if (!index.isValid()) {
		return;
	}

	menuIndex = index;
	menu->popup(event->globalPos());
}

void DvbChannelView::actionEdit()
{
	if (!menuIndex.isValid()) {
		return;
	}

	// FIXME
}

void DvbChannelView::actionChangeIcon()
{
	if (!menuIndex.isValid()) {
		return;
	}

	// FIXME
}

void DvbChannelView::actionDelete()
{
	if (!menuIndex.isValid()) {
		return;
	}

	// FIXME
}
