/*
 * proxytreeview.h
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef PROXYTREEVIEW_H
#define PROXYTREEVIEW_H

#include <QTreeView>

class QSortFilterProxyModel;
class KMenu;

class ProxyTreeView : public QTreeView
{
public:
	explicit ProxyTreeView(QWidget *parent);
	~ProxyTreeView() { }

	int mapToSource(const QModelIndex &index) const;
	int selectedRow() const;
	QList<int> selectedRows() const;

	void setContextMenu(KMenu *menu_);
	void setModel(QAbstractItemModel *model);

protected:
	QAbstractItemModel *sourceModel() const;
	void contextMenuEvent(QContextMenuEvent *event);

private:
	QSortFilterProxyModel *proxyModel;
	KMenu *menu;
};

#endif /* PROXYTREEVIEW_H */
