/*
 * playlisttab.cpp
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

#include "playlisttab.h"

#include <QBoxLayout>
#include <QDomDocument>
#include <QKeyEvent>
#include <QListView>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <KAction>
#include <KActionCollection>
#include <KDebug>
#include <KFileDialog>
#include <kfilewidget.h>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include <KStandardDirs>
#include "../mediawidget.h"
#include "playlistmodel.h"

Playlist *Playlist::readPLSFile(const QString &path)
{
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly)) {
		return NULL;
	}

	QTextStream stream(&file);
	stream.setCodec("UTF-8");

	if (stream.readLine() != "[playlist]") {
		return NULL;
	}

	Playlist *playlist = new Playlist(path.mid(path.lastIndexOf('/') + 1));
	KUrl baseUrl = KUrl::fromLocalFile(file.fileName());
	playlist->setUrl(baseUrl);

	while (!stream.atEnd()) {
		QString line = stream.readLine();

		if (line.startsWith(QLatin1String("File"))) {
			KUrl url(line.mid(line.indexOf('=') + 1));

			if (url.isRelative()) {
				url = baseUrl.resolved(url);
			}

			if (url.isValid()) {
				playlist->tracks.append(PlaylistTrack(url));
			}
		}

		// FIXME what about the other tags?
	}

	return playlist;
}

void Playlist::writePLSFile(QFile *file) const
{
	QTextStream stream(file);
	stream.setCodec("UTF-8");

	stream << "[playlist]\n"
		  "NumberOfEntries=" << tracks.size() << '\n';

	for (int i = 0; i < tracks.size(); ++i) {
		stream << "File" << (i + 1) << '=' << tracks.at(i).getUrl().pathOrUrl() << '\n';
	}

	stream << "Version=2\n";
}

Playlist *Playlist::readM3UFile(const QString &path)
{
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly)) {
		return NULL;
	}

	QTextStream stream(&file);
	stream.setCodec("UTF-8");

	Playlist *playlist = new Playlist(path.mid(path.lastIndexOf('/') + 1));
	KUrl baseUrl = KUrl::fromLocalFile(file.fileName());
	playlist->setUrl(baseUrl);

	while (!stream.atEnd()) {
		QString line = stream.readLine();

		if (!line.startsWith('#')) {
			KUrl url(line); // FIXME relative paths

			if (url.isRelative()) {
				url = baseUrl.resolved(url);
			}

			if (url.isValid()) {
				playlist->tracks.append(PlaylistTrack(url));
			}
		}
	}

	return playlist;
}

void Playlist::writeM3UFile(QFile *file) const
{
	QTextStream stream(file);
	stream.setCodec("UTF-8");

	foreach (const PlaylistTrack &track, tracks) {
		stream << track.getUrl().pathOrUrl() << '\n';
	}
}

Playlist *Playlist::readXSPFFile(const QString &path)
{
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly)) {
		return NULL;
	}

	QDomDocument document;

	if (!document.setContent(&file)) {
		return NULL;
	}

	QDomElement root = document.documentElement();

	if (root.nodeName() != "playlist") {
		return NULL;
	}

	Playlist *playlist = new Playlist(path.mid(path.lastIndexOf('/') + 1));
	KUrl baseUrl = KUrl::fromLocalFile(path);
	playlist->setUrl(baseUrl);

	for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (!node.isElement()) {
			continue;
		}

		QString nodeName = node.nodeName();

		if (nodeName == "title") {
			playlist->name = node.toElement().text();
			continue;
		}

		if (nodeName == "trackList") {
			for (QDomNode childNode = node.firstChild(); !childNode.isNull();
			     childNode = childNode.nextSibling()) {
				if (!childNode.isElement()) {
					continue;
				}

				for (QDomNode trackNode = childNode.firstChild();
				     !trackNode.isNull(); trackNode = trackNode.nextSibling()) {
					if (!trackNode.isElement()) {
						continue;
					}

					if (trackNode.nodeName() == "location") {
						KUrl url(trackNode.toElement().text());

						if (url.isRelative()) {
							url = baseUrl.resolved(url);
						}

						if (url.isValid()) {
							playlist->tracks.append(PlaylistTrack(url));
							break;
						}
					}
				}
			}
		}
	}

	return playlist;
}

void Playlist::writeXSPFFile(QFile *file) const
{
	QTextStream stream(file);
	stream.setCodec("UTF-8");

	stream << "<?xml version='1.0' encoding='UTF-8'?>\n"
		  "<playlist>\n"
		  "  <title>" << name << "</title>\n"
		  "  <trackList>\n";

	foreach (const PlaylistTrack &track, tracks) {
		stream << "    <track>\n"
			  "      <location>" << track.getUrl().pathOrUrl() << "</location>\n"
			  "    </track>\n";
	}

	stream << "  </trackList>\n"
		  "</playlist>\n";
}

Playlist *Playlist::readKaffeineFile(const QString &path)
{
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly)) {
		return NULL;
	}

	QDomDocument document;

	if (!document.setContent(&file)) {
		return NULL;
	}

	QDomElement root = document.documentElement();

	if (root.nodeName() != "playlist") {
		return NULL;
	}

	Playlist *playlist = new Playlist(path.mid(path.lastIndexOf('/') + 1));
	KUrl baseUrl = KUrl::fromLocalFile(path);
	playlist->setUrl(baseUrl);

	for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (!node.isElement()) {
			continue;
		}

		QString urlString = node.attributes().namedItem("url").nodeValue();

		if (urlString.isEmpty()) {
			continue;
		}

		KUrl url(urlString);

		if (url.isRelative()) {
			url = baseUrl.resolved(url);
		}

		if (url.isValid()) {
			playlist->tracks.append(PlaylistTrack(url));
			break;
		}
	}

	return playlist;
}

PlaylistBrowserModel::PlaylistBrowserModel(PlaylistModel *playlistModel_,
	Playlist *temporaryPlaylist, QObject *parent) : QAbstractListModel(parent),
	playlistModel(playlistModel_), currentPlaylist(-1)
{
	connect(playlistModel, SIGNAL(currentPlaylistChanged(Playlist*)),
		this, SLOT(setCurrentPlaylist(Playlist*)));

	playlists.append(temporaryPlaylist);

	QFile file(KStandardDirs::locateLocal("appdata", "playlists"));

	if (!file.open(QIODevice::ReadOnly)) {
		kDebug() << "can't open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	unsigned int version;
	stream >> version;

	if (version != 0xc39637a1) {
		kWarning() << "wrong version" << file.fileName();
		return;
	}

	while (!stream.atEnd()) {
		QString name;
		stream >> name;
		QString url;
		stream >> url;
		int count;
		stream >> count;

		Playlist playlist(name);
		playlist.setUrl(url);

		for (int i = 0; (i < count) && !stream.atEnd(); ++i) {
			QString trackUrl;
			stream >> trackUrl;
			playlist.tracks.append(PlaylistTrack(trackUrl));
		}
		
		if (stream.status() != QDataStream::Ok) {
			kWarning() << "corrupt data" << file.fileName();
			break;
		}

		playlists.append(new Playlist(playlist));
	}
}

PlaylistBrowserModel::~PlaylistBrowserModel()
{
	QFile file(KStandardDirs::locateLocal("appdata", "playlists"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		kWarning() << "can't open" << file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	int version = 0xc39637a1;
	stream << version;

	for (int i = 1; i < playlists.size(); ++i) {
		const Playlist *playlist = playlists.at(i);
		stream << playlist->getName();
		stream << playlist->getUrl().url();
		stream << playlist->tracks.size();

		foreach (const PlaylistTrack &track, playlist->tracks) {
			stream << track.getUrl().url();
		}
	}

	qDeleteAll(playlists);
}

int PlaylistBrowserModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return playlists.size();
}

QVariant PlaylistBrowserModel::data(const QModelIndex &index, int role) const
{
	switch (role) {
	case Qt::DecorationRole:
		if (index.row() == currentPlaylist) {
			return KIcon("arrow-right");
		}

		break;

	case Qt::DisplayRole:
		return playlists.at(index.row())->getName();
	}

	return QVariant();
}

bool PlaylistBrowserModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (parent.isValid()) {
		return false;
	}

	if (row == 0) {
		++row;

		if ((--count) == 0) {
			return false;
		}
	}

	int end = row + count;

	if ((currentPlaylist >= row) && (currentPlaylist < end)) {
		if (end < playlists.size()) {
			playlistModel->setCurrentPlaylist(playlists.at(end));
		} else {
			playlistModel->setCurrentPlaylist(playlists.at(row - 1));
			setCurrentPlaylist(NULL);
		}
	}

	QList<Playlist *>::iterator beginIt = playlists.begin() + row;
	QList<Playlist *>::iterator endIt = beginIt + count;

	beginRemoveRows(QModelIndex(), row, row + count - 1);
	qDeleteAll(beginIt, endIt);
	playlists.erase(beginIt, endIt);

	if (currentPlaylist >= (row + count)) {
		currentPlaylist -= count;
	}

	endRemoveRows();

	return true;
}

Qt::ItemFlags PlaylistBrowserModel::flags(const QModelIndex &index) const
{
	return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool PlaylistBrowserModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (role == Qt::EditRole) {
		QString name = value.toString();

		if (name.isEmpty()) {
			return false;
		}

		playlists.at(index.row())->setName(name);
		emit dataChanged(index, index);
		return true;
	}

	return false;
}

void PlaylistBrowserModel::append(Playlist *playlist)
{
	beginInsertRows(QModelIndex(), playlists.size(), playlists.size());
	playlists.append(playlist);
	endInsertRows();
}

Playlist *PlaylistBrowserModel::getPlaylist(int row) const
{
	return playlists.at(row);
}

void PlaylistBrowserModel::setCurrentPlaylist(Playlist *playlist)
{
	int newPlaylist = playlists.indexOf(playlist);

	if (currentPlaylist == newPlaylist) {
		return;
	}

	int oldPlaylist = currentPlaylist;
	currentPlaylist = newPlaylist;

	if (oldPlaylist != -1) {
		QModelIndex modelIndex = index(oldPlaylist, 0);
		emit dataChanged(modelIndex, modelIndex);
	}

	if (newPlaylist != -1) {
		QModelIndex modelIndex = index(newPlaylist, 0);
		emit dataChanged(modelIndex, modelIndex);
	}
}

class PlaylistBrowserView : public QListView
{
public:
	explicit PlaylistBrowserView(QWidget *parent) : QListView(parent) { }
	~PlaylistBrowserView() { }

protected:
	void keyPressEvent(QKeyEvent *event);
};

void PlaylistBrowserView::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		QModelIndexList selectedRows = selectionModel()->selectedRows();
		qSort(selectedRows);

		for (int i = selectedRows.size() - 1; i >= 0; --i) {
			// FIXME compress
			model()->removeRows(selectedRows.at(i).row(), 1);
		}

		return;
	}

	QListView::keyPressEvent(event);
}

PlaylistView::PlaylistView(QWidget *parent) : QTreeView(parent)
{
}

PlaylistView::~PlaylistView()
{
}

void PlaylistView::removeSelectedRows()
{
	QModelIndexList selectedRows = selectionModel()->selectedRows();
	qSort(selectedRows);

	for (int i = selectedRows.size() - 1; i >= 0; --i) {
		// FIXME compress
		model()->removeRows(selectedRows.at(i).row(), 1);
	}
}

void PlaylistView::contextMenuEvent(QContextMenuEvent *event)
{
	if (!currentIndex().isValid()) {
		  return;
	}

	QMenu::exec(actions(), event->globalPos());
}

void PlaylistView::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		removeSelectedRows();
		return;
	}

	QTreeView::keyPressEvent(event);
}

PlaylistTab::PlaylistTab(KMenu *menu, KActionCollection *collection, MediaWidget *mediaWidget_) :
	mediaWidget(mediaWidget_)
{
	Playlist *temporaryPlaylist = new Playlist(i18n("Temporary Playlist"));
	playlistModel = new PlaylistModel(mediaWidget, temporaryPlaylist, this);

	KAction *repeatAction = new KAction(KIcon("media-playlist-repeat"),
					    i18nc("playlist menu", "Repeat"), this);
	repeatAction->setCheckable(true);
	connect(repeatAction, SIGNAL(triggered(bool)), playlistModel, SLOT(repeatPlaylist(bool)));
	menu->addAction(collection->addAction("playlist_repeat", repeatAction));

	KAction *shuffleAction = new KAction(KIcon("media-playlist-shuffle"),
					     i18nc("playlist menu", "Shuffle"), this);
	connect(shuffleAction, SIGNAL(triggered(bool)), playlistModel, SLOT(shufflePlaylist()));
	menu->addAction(collection->addAction("playlist_shuffle", shuffleAction));

	KAction *removeTrackAction = new KAction(KIcon("edit-delete"),
					       i18nc("remove an item from a list", "Remove"), this);
	collection->addAction("playlist_remove_track", removeTrackAction);

	KAction *clearAction = new KAction(KIcon("edit-clear-list"),
					   i18nc("remove all items from a list", "Clear"), this);
	connect(clearAction, SIGNAL(triggered(bool)), playlistModel, SLOT(clearPlaylist()));
	menu->addAction(collection->addAction("playlist_clear", clearAction));

	menu->addSeparator();

	KAction *newAction = new KAction(KIcon("list-add"),
					 i18nc("add a new item to a list", "New"), this);
	connect(newAction, SIGNAL(triggered(bool)), this, SLOT(newPlaylist()));
	menu->addAction(collection->addAction("playlist_new", newAction));

	KAction *renameAction = new KAction(KIcon("edit-rename"),
					    i18nc("rename an entry in a list", "Rename"), this);
	connect(renameAction, SIGNAL(triggered(bool)), this, SLOT(renamePlaylist()));
	menu->addAction(collection->addAction("playlist_rename", renameAction));

	KAction *removePlaylistAction = new KAction(KIcon("edit-delete"),
					       i18nc("remove an item from a list", "Remove"), this);
	connect(removePlaylistAction, SIGNAL(triggered(bool)), this, SLOT(removePlaylist()));
	menu->addAction(collection->addAction("playlist_remove", removePlaylistAction));

	KAction *savePlaylistAction = KStandardAction::save(this, SLOT(savePlaylist()), this);
	menu->addAction(collection->addAction("playlist_save", savePlaylistAction));

	KAction *saveAsPlaylistAction = KStandardAction::saveAs(this, SLOT(saveAsPlaylist()), this);
	menu->addAction(collection->addAction("playlist_save_as", saveAsPlaylistAction));

	QBoxLayout *widgetLayout = new QHBoxLayout(this);
	widgetLayout->setMargin(0);

	QSplitter *horizontalSplitter = new QSplitter(this);
	widgetLayout->addWidget(horizontalSplitter);

	QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, horizontalSplitter);

	QWidget *widget = new QWidget(verticalSplitter);
	QBoxLayout *sideLayout = new QVBoxLayout(widget);
	sideLayout->setMargin(0);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QToolButton *toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(newAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(renameAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(removePlaylistAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(savePlaylistAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(saveAsPlaylistAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	boxLayout->addStretch();
	sideLayout->addLayout(boxLayout);

	playlistBrowserModel = new PlaylistBrowserModel(playlistModel, temporaryPlaylist, this);

	playlistBrowserView = new PlaylistBrowserView(widget);
	playlistBrowserView->addAction(newAction);
	playlistBrowserView->addAction(renameAction);
	playlistBrowserView->addAction(removePlaylistAction);
	playlistBrowserView->addAction(savePlaylistAction);
	playlistBrowserView->addAction(saveAsPlaylistAction);
	playlistBrowserView->setContextMenuPolicy(Qt::ActionsContextMenu);
	playlistBrowserView->setModel(playlistBrowserModel);
	connect(playlistBrowserView, SIGNAL(activated(QModelIndex)),
		this, SLOT(playlistActivated(QModelIndex)));
	sideLayout->addWidget(playlistBrowserView);

	// KFileWidget creates a local event loop which can cause bad
	// side effects (because Kaffeine isn't fully constructed yet)
	fileWidgetSplitter = verticalSplitter;
	QTimer::singleShot(0, this, SLOT(createFileWidget()));

	verticalSplitter = new QSplitter(Qt::Vertical, horizontalSplitter);

	widget = new QWidget(verticalSplitter);
	sideLayout = new QVBoxLayout(widget);
	sideLayout->setMargin(0);

	boxLayout = new QHBoxLayout();

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(repeatAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(shuffleAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(removeTrackAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	toolButton = new QToolButton(widget);
	toolButton->setDefaultAction(clearAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	boxLayout->addStretch();
	sideLayout->addLayout(boxLayout);

	playlistView = new PlaylistView(widget);
	playlistView->setAlternatingRowColors(true);
	playlistView->setDragDropMode(QAbstractItemView::DragDrop);
	playlistView->setRootIsDecorated(false);
	playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	playlistView->setModel(playlistModel);
	playlistView->sortByColumn(0, Qt::AscendingOrder);
	playlistView->setSortingEnabled(true);
	playlistView->addAction(removeTrackAction);
	connect(removeTrackAction, SIGNAL(triggered(bool)),
		playlistView, SLOT(removeSelectedRows()));
	connect(playlistView, SIGNAL(activated(QModelIndex)),
		playlistModel, SLOT(playTrack(QModelIndex)));
	sideLayout->addWidget(playlistView);

	QWidget *mediaContainer = new QWidget(verticalSplitter);
	mediaLayout = new QHBoxLayout(mediaContainer);
	mediaLayout->setMargin(0);

	verticalSplitter->setStretchFactor(1, 1);
	horizontalSplitter->setStretchFactor(1, 1);
}

PlaylistTab::~PlaylistTab()
{
}

void PlaylistTab::playUrls(const QList<KUrl> &urls)
{
	int playlistIndex = playlistBrowserModel->rowCount(QModelIndex());
	bool playlists = false;

	foreach (const KUrl &url, urls) {
		QString localFile = url.toLocalFile();

		if (localFile.endsWith(QLatin1String(".pls"), Qt::CaseInsensitive)) {
			Playlist *playlist = Playlist::readPLSFile(localFile);

			if (playlist != NULL) {
				playlistBrowserModel->append(playlist);
				playlists = true;
			}
		} else if (localFile.endsWith(QLatin1String(".m3u"), Qt::CaseInsensitive)) {
			Playlist *playlist = Playlist::readM3UFile(localFile);

			if (playlist != NULL) {
				playlistBrowserModel->append(playlist);
				playlists = true;
			}
		} else if (localFile.endsWith(QLatin1String(".xspf"), Qt::CaseInsensitive)) {
			Playlist *playlist = Playlist::readXSPFFile(localFile);

			if (playlist != NULL) {
				playlistBrowserModel->append(playlist);
				playlists = true;
			}
		} else if (localFile.endsWith(QLatin1String(".kaffeine"), Qt::CaseInsensitive)) {
			Playlist *playlist = Playlist::readKaffeineFile(localFile);

			if (playlist != NULL) {
				playlistBrowserModel->append(playlist);
				playlists = true;
			}
		}
	}

	if (!playlists) {
		playlistModel->appendUrls(urls, false);
	} else {
		QModelIndex index = playlistBrowserModel->index(playlistIndex, 0);
		playlistBrowserView->setCurrentIndex(index);
		playlistActivated(index);

		if (playlistModel->rowCount(QModelIndex()) >= 1) {
			index = playlistModel->index(0, 0);
			playlistView->setCurrentIndex(index);
			playlistModel->playTrack(index);
		}
	}
}

void PlaylistTab::createFileWidget()
{
	KFileWidget *fileWidget = new KFileWidget(KUrl(), fileWidgetSplitter);
	fileWidget->setFilter(MediaWidget::extensionFilter());
	fileWidget->setMode(KFile::Files | KFile::ExistingOnly);
	fileWidgetSplitter->setStretchFactor(1, 1);
}

void PlaylistTab::newPlaylist()
{
	Playlist *playlist = new Playlist("Unnamed Playlist"); // FIXME
	playlistBrowserModel->append(playlist);
}

void PlaylistTab::renamePlaylist()
{
	QModelIndex index = playlistBrowserView->currentIndex();

	if (!index.isValid() || (index.row() == 0)) {
		return;
	}

	playlistBrowserView->edit(index);
}

void PlaylistTab::removePlaylist()
{
	QModelIndexList selectedRows = playlistBrowserView->selectionModel()->selectedRows();
	qSort(selectedRows);

	for (int i = selectedRows.size() - 1; i >= 0; --i) {
		// FIXME compress
		playlistBrowserModel->removeRows(selectedRows.at(i).row(), 1);
	}
}

void PlaylistTab::savePlaylist()
{
	savePlaylist(false);
}

void PlaylistTab::saveAsPlaylist()
{
	savePlaylist(true);
}

void PlaylistTab::playlistActivated(const QModelIndex &index)
{
	playlistModel->setPlaylist(playlistBrowserModel->getPlaylist(index.row()));
}

void PlaylistTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
}

void PlaylistTab::savePlaylist(bool askName)
{
	QModelIndex index = playlistBrowserView->currentIndex();

	if (!index.isValid()) {
		return;
	}

	Playlist *playlist = playlistBrowserModel->getPlaylist(index.row());
	KUrl url = playlist->getUrl();

	if (askName || !url.isValid()) {
		url = KFileDialog::getSaveUrl(KUrl(), i18n("*.m3u|M3U Playlist\n*.pls|PLS Playlist\n*.xspf|XSPF Playlist"), this);

		if (!url.isValid()) {
			return;
		}

		playlist->setUrl(url);
	}

	QFile file(url.toLocalFile());

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		KMessageBox::sorry(this, i18n("Cannot open %1.", file.fileName()));
		return;
	}

	QString localFile = file.fileName();

	if (localFile.endsWith(QLatin1String(".pls"), Qt::CaseInsensitive)) {
		playlist->writePLSFile(&file);
	} else if (localFile.endsWith(QLatin1String(".m3u"), Qt::CaseInsensitive)) {
		playlist->writeM3UFile(&file);
	} else if (localFile.endsWith(QLatin1String(".xspf"), Qt::CaseInsensitive)) {
		playlist->writeXSPFFile(&file);
	} else {
		KMessageBox::sorry(this, i18n("Unknown playlist format."));
		playlist->setUrl(KUrl());
	}
}
