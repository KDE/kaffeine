/*
 * proxytreeview.cpp
 *
 * Copyright (C) 2009 Christoph Pfister <christophpfister@gmail.com>
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

#include "proxytreeview.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QSortFilterProxyModel>

ProxyTreeView::ProxyTreeView(QWidget *parent) : QTreeView(parent)
{
	proxyModel = new QSortFilterProxyModel(this);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);
	QTreeView::setModel(proxyModel);
}

int ProxyTreeView::mapToSource(const QModelIndex &index) const
{
	QModelIndex sourceIndex = proxyModel->mapToSource(index);

	if (!sourceIndex.isValid()) {
		return -1;
	}

	return sourceIndex.row();
}

int ProxyTreeView::selectedRow() const
{
	// it's enough to look at a single element if you can only select a single row
	QModelIndex index = proxyModel->mapToSource(currentIndex());

	if (!index.isValid()) {
		return -1;
	}

	return index.row();
}

QList<int> ProxyTreeView::selectedRows() const
{
	QSet<int> selectedProxyRows;
	QList<int> result;

	foreach (const QModelIndex &proxyIndex, selectionModel()->selection().indexes()) {
		if (selectedProxyRows.contains(proxyIndex.row())) {
			continue;
		}

		selectedProxyRows.insert(proxyIndex.row());

		QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);

		if (!sourceIndex.isValid()) {
			continue;
		}

		result.append(sourceIndex.row());
	}

	return result;
}

void ProxyTreeView::setModel(QAbstractItemModel *model)
{
	proxyModel->setSourceModel(model);
}

void ProxyTreeView::contextMenuEvent(QContextMenuEvent *event)
{
	if (!currentIndex().isValid()) {
		return;
	}

	QList<QAction *> contextActions = actions();

	if (contextActions.isEmpty()) {
		return;
	}

	QMenu::exec(contextActions, event->globalPos());
}
