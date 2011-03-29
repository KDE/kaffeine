/*
 * tablemodel.h
 *
 * Copyright (C) 2011 Christoph Pfister <christophpfister@gmail.com>
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

#ifndef TABLEMODEL_H
#define TABLEMODEL_H

#include <QAbstractTableModel>

template<class T> class TableModel : public QAbstractTableModel
{
	typedef typename T::ItemType ItemType;
	typedef typename T::LessThanType LessThanType;
	typedef typename LessThanType::SortOrder SortOrder;
public:
	explicit TableModel(QObject *parent) : QAbstractTableModel(parent), updatingRow(-1) { }
	~TableModel() { }

	QModelIndex find(const ItemType &item) const
	{
		if (item.isValid()) {
			int row = binaryFind(item);

			if (row < items.size()) {
				return index(row, 0);
			}
		}

		return QModelIndex();
	}

	const ItemType &value(int row) const
	{
		if ((row >= 0) && (row < items.size())) {
			return items.at(row);
		}

		return sharedNull;
	}

	const ItemType &value(const QModelIndex &index) const
	{
		if ((index.row() >= 0) && (index.row() < items.size())) {
			return items.at(index.row());
		}

		return sharedNull;
	}

	int columnCount(const QModelIndex &parent) const
	{
		if (!parent.isValid()) {
			return helper.columnCount();
		}

		return 0;
	}

	int rowCount(const QModelIndex &parent) const
	{
		if (!parent.isValid()) {
			return items.size();
		}

		return 0;
	}

protected:
	template<class U> void reset(const U &container)
	{
		beginLayoutChange();
		items.clear();

		for (typename U::ConstIterator it = container.constBegin();
		     it != container.constEnd(); ++it) {
			const ItemType &item = *it;

			if (helper.filterAcceptsItem(item)) {
				items.append(item);
			}
		}

		qSort(items.begin(), items.end(), lessThan);
		endLayoutChange();
	}

	template<class U> void resetFromKeys(const U &container)
	{
		beginLayoutChange();
		items.clear();

		for (typename U::ConstIterator it = container.constBegin();
		     it != container.constEnd(); ++it) {
			const ItemType &item = it.key();

			if (helper.filterAcceptsItem(item)) {
				items.append(item);
			}
		}

		qSort(items.begin(), items.end(), lessThan);
		endLayoutChange();
	}

	void insert(const ItemType &item)
	{
		if (item.isValid() && helper.filterAcceptsItem(item)) {
			int row = upperBound(item);
			beginInsertRows(QModelIndex(), row, row);
			items.insert(row, item);
			endInsertRows();
		}
	}

	void aboutToUpdate(const ItemType &item)
	{
		updatingRow = -1;

		if (item.isValid()) {
			updatingRow = binaryFind(item);
		}
	}

	void update(const ItemType &item)
	{
		int row = updatingRow;
		updatingRow = -1;

		if ((row >= 0) && (row < items.size())) {
			if (item.isValid() && helper.filterAcceptsItem(item)) {
				items.replace(row, item);
				int targetRow = row;

				while (((targetRow - 1) >= 0) &&
				       lessThan(item, items.at(targetRow - 1))) {
					--targetRow;
				}

				while (((targetRow + 1) < items.size()) &&
				       lessThan(items.at(targetRow + 1), item)) {
					++targetRow;
				}

				if (row == targetRow) {
					emit dataChanged(index(row, 0),
						index(row, helper.columnCount() - 1));
				} else {
					beginLayoutChange();
					items.move(row, targetRow);
					endLayoutChange();
				}
			} else {
				beginRemoveRows(QModelIndex(), row, row);
				items.removeAt(row);
				endRemoveRows();
			}
		} else {
			insert(item);
		}
	}

	void remove(const ItemType &item)
	{
		if (item.isValid()) {
			int row = binaryFind(item);
			beginRemoveRows(QModelIndex(), row, row);
			items.removeAt(row);
			endRemoveRows();
		}
	}

	void internalSort(SortOrder sortOrder)
	{
		if (lessThan.getSortOrder() != sortOrder) {
			beginLayoutChange();
			lessThan.setSortOrder(sortOrder);
			qSort(items.begin(), items.end(), lessThan);
			endLayoutChange();
		}
	}

private:
	int binaryFind(const ItemType &item) const
	{
		return (qBinaryFind(items.constBegin(), items.constEnd(), item, lessThan) -
			items.constBegin());
	}

	int upperBound(const ItemType &item) const
	{
		return (qUpperBound(items.constBegin(), items.constEnd(), item, lessThan) -
			items.constBegin());
	}

	void beginLayoutChange()
	{
		emit layoutAboutToBeChanged();
		oldPersistentIndexes = persistentIndexList();
		persistentItems.clear();

		foreach (const QModelIndex &index, oldPersistentIndexes) {
			if ((index.row() >= 0) && (index.row() < items.size())) {
				persistentItems.append(items.at(index.row()));
			} else {
				persistentItems.append(ItemType());
			}
		}
	}

	void endLayoutChange()
	{
		QModelIndexList newPersistentIndexes;

		for (int i = 0; i < oldPersistentIndexes.size(); ++i) {
			const QModelIndex &oldIndex = oldPersistentIndexes.at(i);
			const ItemType &item = persistentItems.at(i);

			if (item.isValid()) {
				int row = binaryFind(item);
				newPersistentIndexes.append(index(row, oldIndex.column()));
			} else {
				newPersistentIndexes.append(QModelIndex());
			}
		}

		changePersistentIndexList(oldPersistentIndexes, newPersistentIndexes);
		oldPersistentIndexes.clear();
		persistentItems.clear();
		emit layoutChanged();
	}

private:
	QList<ItemType> items;
	LessThanType lessThan;
	ItemType sharedNull;
	QModelIndexList oldPersistentIndexes;
	QList<ItemType> persistentItems;
	int updatingRow;

protected:
	T helper;
};

#endif /* TABLEMODEL_H */
