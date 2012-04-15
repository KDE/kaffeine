/*
 * playlistmodel.cpp
 *
 * Copyright (C) 2009-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "playlistmodel.h"

#include <QDir>
#include <QDomDocument>
#include <QMimeData>
#include <QSet>
#include <QTextStream>
#include <QXmlStreamWriter>
#include <KGlobal>
#include <KLocale>
#include "../log.h"

bool Playlist::load(const KUrl &url_, Format format)
{
	url = url_;
	title = url.fileName();
	QString localFile = url.toLocalFile();

	if (localFile.isEmpty()) {
		// FIXME
		Log("Playlist::load: opening remote playlists not supported yet");
		return false;
	}

	QFile file(localFile);

	if (!file.open(QIODevice::ReadOnly)) {
		Log("Playlist::load: cannot open file") << file.fileName();
		return false;
	}

	switch (format) {
	case Invalid:
		return false;
	case Kaffeine:
		return loadKaffeinePlaylist(&file);
	case M3U:
		return loadM3UPlaylist(&file);
	case PLS:
		return loadPLSPlaylist(&file);
	case XSPF:
		return loadXSPFPlaylist(&file);
	}

	return false;
}

bool Playlist::save(Format format) const
{
	QString localFile = url.toLocalFile();

	if (localFile.isEmpty()) {
		// FIXME
		Log("Playlist::save: opening remote playlists not supported yet");
		return false;
	}

	QFile file(localFile);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		Log("Playlist::save: cannot open file") << file.fileName();
		return false;
	}

	switch (format) {
	case Invalid:
		return false;
	case Kaffeine:
		return false; // read-only
	case M3U:
		saveM3UPlaylist(&file);
		return true;
	case PLS:
		savePLSPlaylist(&file);
		return true;
	case XSPF:
		saveXSPFPlaylist(&file);
		return true;
	}

	return false;
}

void Playlist::appendTrack(PlaylistTrack &track)
{
	if (track.url.isValid()) {
		if (track.title.isEmpty()) {
			track.title = track.url.fileName();
		}

		tracks.append(track);
	}
}

KUrl Playlist::fromFileOrUrl(const QString &fileOrUrl) const
{
	if (!QFileInfo(fileOrUrl).isRelative()) {
		return KUrl::fromLocalFile(fileOrUrl);
	}

	KUrl trackUrl(fileOrUrl);

	if (trackUrl.isRelative()) {
		trackUrl = url.resolved(KUrl::fromLocalFile(fileOrUrl));

		if (trackUrl.encodedPath() == url.encodedPath()) {
			return KUrl();
		}
	}

	return trackUrl;
}

KUrl Playlist::fromRelativeUrl(const QString &trackUrlString) const
{
	KUrl trackUrl(trackUrlString);

	if (trackUrl.isRelative()) {
		trackUrl = url.resolved(trackUrl);
	}

	return trackUrl;
}

QString Playlist::toFileOrUrl(const KUrl &trackUrl) const
{
	QString localFile = trackUrl.toLocalFile();

	if (!localFile.isEmpty()) {
		QString playlistPath = url.path();
		int index = playlistPath.lastIndexOf(QLatin1Char('/'));
		playlistPath.truncate(index + 1);

		if (localFile.startsWith(playlistPath)) {
			localFile.remove(0, index + 1);
		}

		return QDir::toNativeSeparators(localFile);
	} else {
		return trackUrl.url();
	}
}

QString Playlist::toRelativeUrl(const KUrl &trackUrl) const
{
	if ((trackUrl.scheme() == url.scheme()) && (trackUrl.authority() == url.authority())) {
		QByteArray playlistPath = url.encodedPath();
		int index = playlistPath.lastIndexOf('/');
		playlistPath.truncate(index + 1);
		QByteArray trackPath = trackUrl.encodedPath();

		if (trackPath.startsWith(playlistPath)) {
			trackPath.remove(0, index + 1);
			KUrl relativeUrl;
			relativeUrl.setEncodedPath(trackPath);
			relativeUrl.setEncodedQuery(trackUrl.encodedQuery());
			relativeUrl.setEncodedFragment(trackUrl.encodedFragment());
			return relativeUrl.url();
		}
	}

	return trackUrl.url();
}

bool Playlist::loadKaffeinePlaylist(QIODevice *device)
{
	QDomDocument document;

	if (!document.setContent(device)) {
		return false;
	}

	QDomElement root = document.documentElement();

	if (root.nodeName() != QLatin1String("playlist")) {
		return false;
	}

	for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (!node.isElement() || (node.nodeName() != QLatin1String("entry"))) {
			continue;
		}

		PlaylistTrack track;
		track.url = fromFileOrUrl(node.attributes().namedItem(QLatin1String("url")).nodeValue());
		track.title = node.attributes().namedItem(QLatin1String("title")).nodeValue();
		track.artist = node.attributes().namedItem(QLatin1String("artist")).nodeValue();
		track.album = node.attributes().namedItem(QLatin1String("album")).nodeValue();

		bool ok;
		int trackNumber = node.attributes().namedItem(QLatin1String("track")).nodeValue().toInt(&ok);

		if (ok && (trackNumber >= 0)) {
			track.trackNumber = trackNumber;
		}

		QString length = node.attributes().namedItem(QLatin1String("length")).nodeValue();

		if (length.size() == 7) {
			length.prepend(QLatin1Char('0'));
		}

		track.length = QTime::fromString(length, Qt::ISODate);

		if (QTime().msecsTo(track.length) == 0) {
			track.length = QTime();
		}

		appendTrack(track);
	}

	return true;
}

bool Playlist::loadM3UPlaylist(QIODevice *device)
{
	QTextStream stream(device);
	stream.setCodec("UTF-8");
	PlaylistTrack track;

	while (!stream.atEnd()) {
		QString line = stream.readLine();

		if (line.startsWith(QLatin1Char('#'))) {
			if (line.startsWith(QLatin1String("#EXTINF:"))) {
				int index = line.indexOf(QLatin1Char(','), 8);
				bool ok;
				int length = line.mid(8, index - 8).toInt(&ok);

				if (ok && (length >= 0)) {
					track.length = QTime().addSecs(length);
				}

				track.title = line.mid(index + 1);
			}
		} else {
			track.url = fromFileOrUrl(line);
			appendTrack(track);
			track = PlaylistTrack();
		}
	}

	return true;
}

void Playlist::saveM3UPlaylist(QIODevice *device) const
{
	QTextStream stream(device);
	stream.setCodec("UTF-8");
	stream << "#EXTM3U\n";

	foreach (const PlaylistTrack &track, tracks) {
		int length = -1;

		if (track.length.isValid()) {
			length = QTime().secsTo(track.length);
		}

		stream << "#EXTINF:" << length << QLatin1Char(',') << track.title << QLatin1Char('\n');
		stream << toFileOrUrl(track.url) << QLatin1Char('\n');
	}
}

bool Playlist::loadPLSPlaylist(QIODevice *device)
{
	QTextStream stream(device);
	stream.setCodec("UTF-8");

	if (stream.readLine().compare(QLatin1String("[playlist]"), Qt::CaseInsensitive) != 0) {
		return false;
	}

	PlaylistTrack track;
	int lastNumber = -1;

	while (!stream.atEnd()) {
		QString line = stream.readLine();
		int start;

		if (line.startsWith(QLatin1String("File"), Qt::CaseInsensitive)) {
			start = 4;
		} else if (line.startsWith(QLatin1String("Title"), Qt::CaseInsensitive)) {
			start = 5;
		} else if (line.startsWith(QLatin1String("Length"), Qt::CaseInsensitive)) {
			start = 6;
		} else {
			continue;
		}

		int index = line.indexOf(QLatin1Char('='), start);
		QString content = line.mid(index + 1);
		bool ok;
		int number = line.mid(start, index - start).toInt(&ok);

		if (!ok) {
			continue;
		}

		if (lastNumber != number) {
			if (lastNumber >= 0) {
				appendTrack(track);
				track = PlaylistTrack();
			}

			lastNumber = number;
		}

		switch (start) {
		case 4:
			track.url = fromFileOrUrl(content);
			break;
		case 5:
			track.title = content;
			break;
		case 6: {
			int length = content.toInt(&ok);

			if (ok && (length >= 0)) {
				track.length = QTime().addSecs(content.toInt());
			}

			break;
		    }
		}
	}

	if (lastNumber >= 0) {
		appendTrack(track);
	}

	return true;
}

void Playlist::savePLSPlaylist(QIODevice *device) const
{
	QTextStream stream(device);
	stream.setCodec("UTF-8");
	stream << "[Playlist]\n"
		"NumberOfEntries=" << tracks.size() << '\n';
	int index = 1;

	foreach (const PlaylistTrack &track, tracks) {
		int length = -1;

		if (track.length.isValid()) {
			length = QTime().secsTo(track.length);
		}

		stream << "File" << index << '=' << toFileOrUrl(track.url) << '\n';
		stream << "Title" << index << '=' << track.title << '\n';
		stream << "Length" << index << '=' << length << '\n';
		++index;
	}

	stream << "Version=2\n";
}

bool Playlist::loadXSPFPlaylist(QIODevice *device)
{
	QDomDocument document;

	if (!document.setContent(device)) {
		return false;
	}

	QDomElement root = document.documentElement();

	if (root.nodeName() != QLatin1String("playlist")) {
		return false;
	}

	for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (!node.isElement()) {
			continue;
		}

		QString nodeName = node.nodeName();

		if (nodeName == QLatin1String("title")) {
			title = node.toElement().text();
		} else if (nodeName == QLatin1String("trackList")) {
			for (QDomNode childNode = node.firstChild(); !childNode.isNull();
			     childNode = childNode.nextSibling()) {
				if (!childNode.isElement()) {
					continue;
				}

				PlaylistTrack track;

				for (QDomNode trackNode = childNode.firstChild();
				     !trackNode.isNull(); trackNode = trackNode.nextSibling()) {
					if (!trackNode.isElement()) {
						continue;
					}

					if (trackNode.nodeName() == QLatin1String("location")) {
						if (!track.url.isValid()) {
							track.url = fromRelativeUrl(
								trackNode.toElement().text());
						}
					} else if (trackNode.nodeName() == QLatin1String("title")) {
						track.title = trackNode.toElement().text();
					} else if (trackNode.nodeName() == QLatin1String("creator")) {
						track.artist = trackNode.toElement().text();
					} else if (trackNode.nodeName() == QLatin1String("album")) {
						track.album = trackNode.toElement().text();
					} else if (trackNode.nodeName() == QLatin1String("trackNum")) {
						bool ok;
						int trackNumber =
							trackNode.toElement().text().toInt(&ok);

						if (ok && (trackNumber >= 0)) {
							track.trackNumber = trackNumber;
						}
					} else if (trackNode.nodeName() == QLatin1String("duration")) {
						bool ok;
						int length =
							trackNode.toElement().text().toInt(&ok);

						if (ok && (length >= 0)) {
							track.length = QTime().addMSecs(length);
						}
					}
				}

				appendTrack(track);
			}
		}
	}

	return true;
}

void Playlist::saveXSPFPlaylist(QIODevice *device) const
{
	QXmlStreamWriter stream(device);
	stream.setAutoFormatting(true);
	stream.setAutoFormattingIndent(2);
	stream.writeStartDocument();
	stream.writeStartElement(QLatin1String("playlist"));
	stream.writeAttribute(QLatin1String("version"), QLatin1String("1"));
	stream.writeAttribute(QLatin1String("xmlns"), QLatin1String("http://xspf.org/ns/0/"));
	stream.writeTextElement(QLatin1String("title"), title);
	stream.writeStartElement(QLatin1String("trackList"));

	foreach (const PlaylistTrack &track, tracks) {
		stream.writeStartElement(QLatin1String("track"));
		stream.writeTextElement(QLatin1String("location"), toRelativeUrl(track.url));

		if (!track.title.isEmpty()) {
			stream.writeTextElement(QLatin1String("title"), track.title);
		}

		if (!track.artist.isEmpty()) {
			stream.writeTextElement(QLatin1String("creator"), track.artist);
		}

		if (!track.album.isEmpty()) {
			stream.writeTextElement(QLatin1String("album"), track.album);
		}

		if (track.trackNumber >= 0) {
			stream.writeTextElement(QLatin1String("trackNum"), QString::number(track.trackNumber));
		}

		if (track.length.isValid()) {
			stream.writeTextElement(QLatin1String("duration"),
				QString::number(QTime().msecsTo(track.length)));
		}

		stream.writeEndElement();
	}

	stream.writeEndElement();
	stream.writeEndElement();
	stream.writeEndDocument();
}

PlaylistModel::PlaylistModel(Playlist *visiblePlaylist_, QObject *parent) :
	QAbstractTableModel(parent), visiblePlaylist(visiblePlaylist_)
{
	setSupportedDragActions(Qt::MoveAction);
}

PlaylistModel::~PlaylistModel()
{
}

void PlaylistModel::setVisiblePlaylist(Playlist *visiblePlaylist_)
{
	if (visiblePlaylist != visiblePlaylist_) {
		visiblePlaylist = visiblePlaylist_;
		reset();
	}
}

Playlist *PlaylistModel::getVisiblePlaylist() const
{
	return visiblePlaylist;
}

void PlaylistModel::appendUrls(Playlist *playlist, const QList<KUrl> &urls, bool playImmediately)
{
	insertUrls(playlist, playlist->tracks.size(), urls, playImmediately);
}

void PlaylistModel::removeRows(Playlist *playlist, int row, int count)
{
	if (playlist == visiblePlaylist) {
		beginRemoveRows(QModelIndex(), row, row + count - 1);
	}

	QList<PlaylistTrack>::Iterator begin = playlist->tracks.begin() + row;
	playlist->tracks.erase(begin, begin + count);

	if (playlist->currentTrack >= row) {
		if (playlist->currentTrack >= (row + count)) {
			playlist->currentTrack -= count;
		} else {
			playlist->currentTrack = -1;
			emit playTrack(playlist, -1);
		}
	}

	if (playlist == visiblePlaylist) {
		endRemoveRows();
	}
}

void PlaylistModel::setCurrentTrack(Playlist *playlist, int track)
{
	int oldTrack = playlist->currentTrack;
	playlist->currentTrack = track;

	if (playlist == visiblePlaylist) {
		if (oldTrack >= 0) {
			QModelIndex modelIndex = index(oldTrack, 0);
			emit dataChanged(modelIndex, modelIndex);
		}

		if (track >= 0) {
			QModelIndex modelIndex = index(track, 0);
			emit dataChanged(modelIndex, modelIndex);
		}
	}
}

void PlaylistModel::updateTrackLength(Playlist *playlist, int length)
{
	if (playlist->currentTrack >= 0) {
		if (QTime().msecsTo(playlist->tracks.at(playlist->currentTrack).length) < length) {
			playlist->tracks[playlist->currentTrack].length = QTime().addMSecs(length);

			if (playlist == visiblePlaylist) {
				QModelIndex modelIndex = index(playlist->currentTrack, 4);
				emit dataChanged(modelIndex, modelIndex);
			}
		}
	}
}

void PlaylistModel::updateTrackMetadata(Playlist *playlist,
	const QMap<MediaWidget::MetadataType, QString> &metadata)
{
	if (playlist->currentTrack >= 0) {
		PlaylistTrack &currentTrack = playlist->tracks[playlist->currentTrack];

		if ((currentTrack.title != metadata.value(MediaWidget::Title)) &&
		    !metadata.value(MediaWidget::Title).isEmpty()) {
			currentTrack.title = metadata.value(MediaWidget::Title);

			if (playlist == visiblePlaylist) {
				QModelIndex modelIndex = index(playlist->currentTrack, 0);
				emit dataChanged(modelIndex, modelIndex);
			}
		}

		if ((currentTrack.artist != metadata.value(MediaWidget::Artist)) &&
		    !metadata.value(MediaWidget::Artist).isEmpty()) {
			currentTrack.artist = metadata.value(MediaWidget::Artist);

			if (playlist == visiblePlaylist) {
				QModelIndex modelIndex = index(playlist->currentTrack, 1);
				emit dataChanged(modelIndex, modelIndex);
			}
		}

		if ((currentTrack.album != metadata.value(MediaWidget::Album)) &&
		    !metadata.value(MediaWidget::Album).isEmpty()) {
			currentTrack.album = metadata.value(MediaWidget::Album);

			if (playlist == visiblePlaylist) {
				QModelIndex modelIndex = index(playlist->currentTrack, 2);
				emit dataChanged(modelIndex, modelIndex);
			}
		}

		bool ok;
		int trackNumber = metadata.value(MediaWidget::TrackNumber).toInt(&ok);

		if (!ok) {
			trackNumber = -1;
		}

		if ((currentTrack.trackNumber != trackNumber) && (trackNumber >= 0)) {
			currentTrack.trackNumber = trackNumber;

			if (playlist == visiblePlaylist) {
				QModelIndex modelIndex = index(playlist->currentTrack, 3);
				emit dataChanged(modelIndex, modelIndex);
			}
		}
	}
}

void PlaylistModel::clearVisiblePlaylist()
{
	visiblePlaylist->tracks.clear();

	if (visiblePlaylist->currentTrack >= 0) {
		visiblePlaylist->currentTrack = -1;
		emit playTrack(visiblePlaylist, -1);
	}

	reset();
}

void PlaylistModel::insertUrls(Playlist *playlist, int row, const QList<KUrl> &urls,
	bool playImmediately)
{
	QList<KUrl> processedUrls;

	foreach (const KUrl &url, urls) {
		QString fileName = url.fileName();
		Playlist::Format format = Playlist::Invalid;

		if (fileName.endsWith(QLatin1String(".kaffeine"), Qt::CaseInsensitive)) {
			format = Playlist::Kaffeine;
		} else if (fileName.endsWith(QLatin1String(".m3u"), Qt::CaseInsensitive) ||
			   fileName.endsWith(QLatin1String(".m3u8"), Qt::CaseInsensitive)) {
			format = Playlist::M3U;
		} else if (fileName.endsWith(QLatin1String(".pls"), Qt::CaseInsensitive)) {
			format = Playlist::PLS;
		} else if (fileName.endsWith(QLatin1String(".xspf"), Qt::CaseInsensitive)) {
			format = Playlist::XSPF;
		}

		if (format != Playlist::Invalid) {
			Playlist *nestdPlaylist = new Playlist();

			if (nestdPlaylist->load(url, format)) {
				emit appendPlaylist(nestdPlaylist, playImmediately);
				playImmediately = false;
			} else {
				delete nestdPlaylist;
			}

			continue;
		}

		QString localFile = url.toLocalFile();

		if (!localFile.isEmpty() && QFileInfo(localFile).isDir()) {
			QDir dir(localFile);
			QString extensionFilter = MediaWidget::extensionFilter();
			extensionFilter.truncate(extensionFilter.indexOf(QLatin1Char('|')));
			QStringList entries = dir.entryList(extensionFilter.split(QLatin1Char(' ')),
				QDir::Files, QDir::Name | QDir::LocaleAware);

			for (int i = 0; i < entries.size(); ++i) {
				const QString &entry = entries.at(i);
				processedUrls.append(KUrl::fromLocalFile(dir.filePath(entry)));
			}
		} else {
			processedUrls.append(url);
		}
	}

	if (!processedUrls.isEmpty()) {
		if (playlist == visiblePlaylist) {
			beginInsertRows(QModelIndex(), row, row + processedUrls.size() - 1);
		}

		for (int i = 0; i < processedUrls.size(); ++i) {
			PlaylistTrack track;
			track.url = processedUrls.at(i);
			track.title = track.url.fileName();
			playlist->tracks.insert(row + i, track);
		}

		if (playlist->currentTrack >= row) {
			playlist->currentTrack += processedUrls.size();
		}

		if (playlist == visiblePlaylist) {
			endInsertRows();
		}

		if (playImmediately) {
			emit playTrack(playlist, row);
		}
	}
}

int PlaylistModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 5;
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return visiblePlaylist->tracks.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
	if (role == Qt::DecorationRole) {
		if ((index.row() == visiblePlaylist->currentTrack) && (index.column() == 0)) {
			return KIcon(QLatin1String("arrow-right"));
		}
	} else if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0:
			return visiblePlaylist->at(index.row()).title;
		case 1:
			return visiblePlaylist->at(index.row()).artist;
		case 2:
			return visiblePlaylist->at(index.row()).album;
		case 3: {
			int trackNumber = visiblePlaylist->at(index.row()).trackNumber;

			if (trackNumber >= 0) {
				return trackNumber;
			} else {
				return QVariant();
			}
		    }
		case 4: {
			QTime length = visiblePlaylist->at(index.row()).length;

			if (length.isValid()) {
				return KGlobal::locale()->formatTime(length, true, true);
			} else {
				return QVariant();
			}
		    }
		}
	}

	return QVariant();
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole)) {
		switch (section) {
		case 0:
			return i18nc("playlist track", "Title");
		case 1:
			return i18nc("playlist track", "Artist");
		case 2:
			return i18nc("playlist track", "Album");
		case 3:
			return i18nc("playlist track", "Track Number");
		case 4:
			return i18nc("playlist track", "Length");
		}
	}

	return QVariant();
}

bool PlaylistModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (parent.isValid()) {
		return false;
	}

	removeRows(visiblePlaylist, row, count);
	return true;
}

class PlaylistTrackLessThan
{
public:
	PlaylistTrackLessThan(const Playlist *playlist_, int column_, Qt::SortOrder order_) :
		playlist(playlist_), column(column_), order(order_) { }
	~PlaylistTrackLessThan() { }

	bool operator()(int x, int y)
	{
		int compare = 0;

		switch (column) {
		case 0:
			compare = playlist->at(x).title.localeAwareCompare(playlist->at(y).title);
			break;
		case 1:
			compare =
				playlist->at(x).artist.localeAwareCompare(playlist->at(y).artist);
			break;
		case 2:
			compare = playlist->at(x).album.localeAwareCompare(playlist->at(y).album);
			break;
		case 3:
			compare = (playlist->at(x).trackNumber - playlist->at(y).trackNumber);
			break;
		case 4:
			compare = playlist->at(y).length.msecsTo(playlist->at(x).length);
			break;
		}

		if (compare != 0) {
			if (order == Qt::AscendingOrder) {
				return (compare < 0);
			} else {
				return (compare > 0);
			}
		}

		return (x < y);
	}

private:
	const Playlist *playlist;
	int column;
	Qt::SortOrder order;
};

void PlaylistModel::sort(int column, Qt::SortOrder order)
{
	if (visiblePlaylist->tracks.size() <= 1) {
		return;
	}

	QVector<int> mapping(visiblePlaylist->tracks.size());

	for (int i = 0; i < mapping.size(); ++i) {
		mapping[i] = i;
	}

	qSort(mapping.begin(), mapping.end(),
		PlaylistTrackLessThan(visiblePlaylist, column, order));

	QVector<int> reverseMapping(mapping.size());

	for (int i = 0; i < mapping.size(); ++i) {
		reverseMapping[mapping.at(i)] = i;
	}

	emit layoutAboutToBeChanged();

	for (int i = 0; i < mapping.size();) {
		int target = mapping.at(i);

		if (i != target) {
			qSwap(mapping[i], mapping[target]);
			visiblePlaylist->tracks.swap(i, target);
		} else {
			++i;
		}
	}

	if (visiblePlaylist->currentTrack >= 0) {
		visiblePlaylist->currentTrack =
			reverseMapping.value(visiblePlaylist->currentTrack);
	}

	QModelIndexList oldIndexes = persistentIndexList();
	QModelIndexList newIndexes;

	foreach (const QModelIndex &oldIndex, oldIndexes) {
		newIndexes.append(index(reverseMapping.value(oldIndex.row()), oldIndex.column()));
	}

	changePersistentIndexList(oldIndexes, newIndexes);
	emit layoutChanged();
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const
{
	if (index.isValid()) {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDragEnabled);
	} else {
		return (QAbstractTableModel::flags(index) | Qt::ItemIsDropEnabled);
	}
}

QStringList PlaylistModel::mimeTypes() const
{
	return (QStringList() << QLatin1String("text/uri-list") << QLatin1String("x-org.kde.kaffeine-playlist"));
}

Qt::DropActions PlaylistModel::supportedDropActions() const
{
	return (Qt::CopyAction | Qt::MoveAction);
}

Q_DECLARE_METATYPE(QList<PlaylistTrack>)

QMimeData *PlaylistModel::mimeData(const QModelIndexList &indexes) const
{
	QList<PlaylistTrack> tracks;
	QSet<int> rows;

	foreach (const QModelIndex &index, indexes) {
		int row = index.row();

		if (!rows.contains(row)) {
			tracks.append(visiblePlaylist->at(row));
			rows.insert(row);
		}
	}

	QMimeData *mimeData = new QMimeData();
	mimeData->setData(QLatin1String("x-org.kde.kaffeine-playlist"), QByteArray());
	mimeData->setProperty("tracks", QVariant::fromValue(tracks));
	return mimeData;
}

bool PlaylistModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
	const QModelIndex &parent)
{
	Q_UNUSED(action)
	Q_UNUSED(column)
	Q_UNUSED(parent)

	if (row < 0) {
		row = visiblePlaylist->tracks.size();
	}

	QList<PlaylistTrack> tracks = data->property("tracks").value<QList<PlaylistTrack> >();

	if (!tracks.isEmpty()) {
		beginInsertRows(QModelIndex(), row, row + tracks.size() - 1);

		for (int i = 0; i < tracks.size(); ++i) {
			visiblePlaylist->tracks.insert(row + i, tracks.at(i));
		}

		if (visiblePlaylist->currentTrack >= row) {
			visiblePlaylist->currentTrack += tracks.size();
		}

		endInsertRows();
		return true;
	}

	if (data->hasUrls()) {
		insertUrls(visiblePlaylist, row, KUrl::List::fromMimeData(data), false);
		return true;
	}

	return false;
}
