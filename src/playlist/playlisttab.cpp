/*
 * playlisttab.cpp
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

#include "playlisttab.h"

#include <QBoxLayout>
#include <QKeyEvent>
#include <QListView>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <KAction>
#include <KActionCollection>
#include <KFileDialog>
#include <kfilewidget.h>
#include <KLocalizedString>
#include <KMenu>
#include <KStandardDirs>
#include "../log.h"
#include "playlistmodel.h"

PlaylistBrowserModel::PlaylistBrowserModel(PlaylistModel *playlistModel_,
	Playlist *temporaryPlaylist, QObject *parent) : QAbstractListModel(parent),
	playlistModel(playlistModel_), currentPlaylist(-1)
{
	playlists.append(temporaryPlaylist);

	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("playlistsK4")));

	if (!file.open(QIODevice::ReadOnly)) {
		file.setFileName(KStandardDirs::locateLocal("appdata", QLatin1String("playlists")));

		if (!file.open(QIODevice::ReadOnly)) {
			Log("PlaylistBrowserModel::PlaylistBrowserModel: cannot open file") <<
				file.fileName();
			return;
		}
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	unsigned int version;
	stream >> version;
	bool hasMetadata = true;
	bool hasSubtitles = true;

	if (version == 0xc39637a1) {
		// compatibility code
		hasMetadata = false;
		hasSubtitles = false;
	} else if (version == 0x2e00f3ea) {
		// compatibility code
		hasSubtitles = false;
	} else if (version != 0x361c4a3c) {
		Log("PlaylistBrowserModel::PlaylistBrowserModel: cannot read file") <<
			file.fileName();
		return;
	}

	while (!stream.atEnd()) {
		Playlist *playlist = new Playlist();
		stream >> playlist->title;
		QString urlString;
		stream >> urlString;
		playlist->url = urlString;
		int count;
		stream >> count;

		for (int i = 0; (i < count) && !stream.atEnd(); ++i) {
			PlaylistTrack track;
			stream >> urlString;
			track.url = urlString;

			if (hasMetadata) {
				stream >> track.title;
				stream >> track.artist;
				stream >> track.album;
				stream >> track.trackNumber;
				stream >> track.length;
			} else {
				track.title = track.url.fileName();
			}

			if (hasSubtitles) {
				QStringList subtitleStrings;
				stream >> subtitleStrings;

				foreach (const QString &subtitleString, subtitleStrings) {
					track.subtitles.append(subtitleString);
				}

				stream >> track.currentSubtitle;
			}

			playlist->tracks.append(track);
		}

		if (stream.status() != QDataStream::Ok) {
			Log("PlaylistBrowserModel::PlaylistBrowserModel: cannot read file") <<
				file.fileName();
			delete playlist;
			break;
		}

		playlists.append(playlist);
	}
}

PlaylistBrowserModel::~PlaylistBrowserModel()
{
	QFile file(KStandardDirs::locateLocal("appdata", QLatin1String("playlistsK4")));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		Log("PlaylistBrowserModel::~PlaylistBrowserModel: cannot open file") <<
			file.fileName();
		return;
	}

	QDataStream stream(&file);
	stream.setVersion(QDataStream::Qt_4_4);

	int version = 0x361c4a3c;
	stream << version;

	for (int i = 1; i < playlists.size(); ++i) {
		const Playlist *playlist = playlists.at(i);
		stream << playlist->title;
		stream << playlist->url.url();
		stream << playlist->tracks.size();

		foreach (const PlaylistTrack &track, playlist->tracks) {
			stream << track.url.url();
			stream << track.title;
			stream << track.artist;
			stream << track.album;
			stream << track.trackNumber;
			stream << track.length;

			QStringList subtitleStrings;

			for (int j = 0; j < track.subtitles.size(); ++j) {
				const KUrl &url = track.subtitles.at(j);
				subtitleStrings.append(url.url());
			}

			stream << subtitleStrings;
			stream << track.currentSubtitle;
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
	if (role == Qt::DecorationRole) {
		if (index.row() == currentPlaylist) {
			return KIcon(QLatin1String("arrow-right"));
		}
	} else if (role == Qt::DisplayRole) {
		return playlists.at(index.row())->title;
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

	beginRemoveRows(QModelIndex(), row, row + count - 1);
	Playlist *visiblePlaylist = playlistModel->getVisiblePlaylist();

	for (int i = row; i < (row + count); ++i) {
		if (playlists.at(i) == visiblePlaylist) {
			if ((row + count) < playlists.size()) {
				playlistModel->setVisiblePlaylist(playlists.at(row + count));
			} else {
				playlistModel->setVisiblePlaylist(playlists.at(row - 1));
			}
		}
	}

	QList<Playlist *>::Iterator begin = playlists.begin() + row;
	QList<Playlist *>::Iterator end = begin + count;
	qDeleteAll(begin, end);
	playlists.erase(begin, end);

	if (currentPlaylist >= row) {
		if (currentPlaylist >= (row + count)) {
			currentPlaylist -= count;
		} else {
			currentPlaylist = -1;
			emit playTrack(NULL, -1);
		}
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
		QString title = value.toString();

		if (title.isEmpty()) {
			return false;
		}

		playlists.at(index.row())->title = title;
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
	int oldPlaylist = currentPlaylist;
	currentPlaylist = playlists.indexOf(playlist);

	if (oldPlaylist == currentPlaylist) {
		return;
	}

	if (oldPlaylist != -1) {
		playlistModel->setCurrentTrack(playlists.at(oldPlaylist), -1);
		QModelIndex modelIndex = index(oldPlaylist, 0);
		emit dataChanged(modelIndex, modelIndex);
	}

	if (currentPlaylist != -1) {
		QModelIndex modelIndex = index(currentPlaylist, 0);
		emit dataChanged(modelIndex, modelIndex);
	}
}

Playlist *PlaylistBrowserModel::getCurrentPlaylist() const
{
	if (currentPlaylist >= 0) {
		return playlists.at(currentPlaylist);
	} else {
		return playlistModel->getVisiblePlaylist();
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
	Playlist *temporaryPlaylist = new Playlist();
	temporaryPlaylist->title = i18nc("playlist browser", "Temporary Playlist");

	playlistModel = new PlaylistModel(temporaryPlaylist, this);
	connect(playlistModel, SIGNAL(playTrack(Playlist*,int)),
		this, SLOT(playTrack(Playlist*,int)));
	connect(playlistModel, SIGNAL(appendPlaylist(Playlist*,bool)),
		this, SLOT(appendPlaylist(Playlist*,bool)));

	playlistBrowserModel = new PlaylistBrowserModel(playlistModel, temporaryPlaylist, this);
	playlistModel->setVisiblePlaylist(playlistBrowserModel->getPlaylist(0));
	connect(playlistBrowserModel, SIGNAL(playTrack(Playlist*,int)),
		this, SLOT(playTrack(Playlist*,int)));

	connect(mediaWidget, SIGNAL(playlistPrevious()), this, SLOT(playPreviousTrack()));
	connect(mediaWidget, SIGNAL(playlistPlay()), this, SLOT(playCurrentTrack()));
	connect(mediaWidget, SIGNAL(playlistNext()), this, SLOT(playNextTrack()));
	connect(mediaWidget, SIGNAL(playlistUrlsDropped(QList<KUrl>)),
		this, SLOT(appendUrls(QList<KUrl>)));
	connect(mediaWidget, SIGNAL(playlistTrackLengthChanged(int)),
		this, SLOT(updateTrackLength(int)));
	connect(mediaWidget,
		SIGNAL(playlistTrackMetadataChanged(QMap<MediaWidget::MetadataType,QString>)),
		this, SLOT(updateTrackMetadata(QMap<MediaWidget::MetadataType,QString>)));

	repeatAction = new KAction(KIcon(QLatin1String("media-playlist-repeat")),
		i18nc("playlist menu", "Repeat"), this);
	repeatAction->setCheckable(true);
	menu->addAction(collection->addAction(QLatin1String("playlist_repeat"), repeatAction));

	randomAction = new KAction(KIcon(QLatin1String("media-playlist-shuffle")),
		i18nc("playlist menu", "Random"), this);
	randomAction->setCheckable(true);
	menu->addAction(collection->addAction(QLatin1String("playlist_random"), randomAction));

	KAction *addSubtitleAction =
		new KAction(KIcon(QLatin1String("application-x-subrip")), i18n("Add Subtitle"), this);
	collection->addAction(QLatin1String("playlist_add_subtitle"), addSubtitleAction);

	KAction *removeTrackAction =
		new KAction(KIcon(QLatin1String("edit-delete")), i18nc("@action", "Remove"), this);
	collection->addAction(QLatin1String("playlist_remove_track"), removeTrackAction);

	KAction *clearAction = new KAction(KIcon(QLatin1String("edit-clear-list")),
		i18nc("remove all items from a list", "Clear"), this);
	connect(clearAction, SIGNAL(triggered(bool)), playlistModel, SLOT(clearVisiblePlaylist()));
	menu->addAction(collection->addAction(QLatin1String("playlist_clear"), clearAction));

	menu->addSeparator();

	KAction *newAction = new KAction(KIcon(QLatin1String("list-add")), i18nc("@action", "New"), this);
	connect(newAction, SIGNAL(triggered(bool)), this, SLOT(newPlaylist()));
	menu->addAction(collection->addAction(QLatin1String("playlist_new"), newAction));

	KAction *renameAction = new KAction(KIcon(QLatin1String("edit-rename")),
		i18nc("rename an entry in a list", "Rename"), this);
	connect(renameAction, SIGNAL(triggered(bool)), this, SLOT(renamePlaylist()));
	menu->addAction(collection->addAction(QLatin1String("playlist_rename"), renameAction));

	KAction *removePlaylistAction =
		new KAction(KIcon(QLatin1String("edit-delete")), i18nc("@action", "Remove"), this);
	connect(removePlaylistAction, SIGNAL(triggered(bool)), this, SLOT(removePlaylist()));
	menu->addAction(collection->addAction(QLatin1String("playlist_remove"), removePlaylistAction));

	KAction *savePlaylistAction = KStandardAction::save(this, SLOT(savePlaylist()), this);
	menu->addAction(collection->addAction(QLatin1String("playlist_save"), savePlaylistAction));

	KAction *savePlaylistAsAction =
		KStandardAction::saveAs(this, SLOT(savePlaylistAs()), this);
	menu->addAction(collection->addAction(QLatin1String("playlist_save_as"), savePlaylistAsAction));

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
	toolButton->setDefaultAction(savePlaylistAsAction);
	toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	boxLayout->addWidget(toolButton);

	boxLayout->addStretch();
	sideLayout->addLayout(boxLayout);

	playlistBrowserView = new PlaylistBrowserView(widget);
	playlistBrowserView->addAction(newAction);
	playlistBrowserView->addAction(renameAction);
	playlistBrowserView->addAction(removePlaylistAction);
	playlistBrowserView->addAction(savePlaylistAction);
	playlistBrowserView->addAction(savePlaylistAsAction);
	playlistBrowserView->setContextMenuPolicy(Qt::ActionsContextMenu);
	playlistBrowserView->setModel(playlistBrowserModel);
	connect(playlistBrowserView, SIGNAL(activated(QModelIndex)),
		this, SLOT(playlistActivated(QModelIndex)));
	sideLayout->addWidget(playlistBrowserView);

	// KFileWidget creates a local event loop which can cause bad side
	// effects (because the main window isn't fully constructed yet)
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
	toolButton->setDefaultAction(randomAction);
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
	playlistView->sortByColumn(-1, Qt::AscendingOrder);
	playlistView->setSortingEnabled(true);
	playlistView->addAction(addSubtitleAction);
	connect(addSubtitleAction, SIGNAL(triggered(bool)), this, SLOT(addSubtitle()));
	playlistView->addAction(removeTrackAction);
	connect(removeTrackAction, SIGNAL(triggered(bool)),
		playlistView, SLOT(removeSelectedRows()));
	connect(playlistView, SIGNAL(activated(QModelIndex)),
		this, SLOT(playTrack(QModelIndex)));
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

void PlaylistTab::appendToCurrentPlaylist(const QList<KUrl> &urls, bool playImmediately)
{
	playlistModel->appendUrls(playlistBrowserModel->getCurrentPlaylist(), urls,
		playImmediately);
}

void PlaylistTab::appendToVisiblePlaylist(const QList<KUrl> &urls, bool playImmediately)
{
	playlistModel->appendUrls(playlistModel->getVisiblePlaylist(), urls, playImmediately);
}

void PlaylistTab::removeTrack(int row)
{
	Playlist *currentPlaylist = playlistBrowserModel->getCurrentPlaylist();

	if ((row >= 0) && (row < currentPlaylist->tracks.size())) {
		playlistModel->removeRows(currentPlaylist, row, 1);
	}
}

void PlaylistTab::setRandom(bool random)
{
	randomAction->setChecked(random);
}

void PlaylistTab::setRepeat(bool repeat)
{
	repeatAction->setChecked(repeat);
}

int PlaylistTab::getCurrentTrack() const
{
	const Playlist *currentPlaylist = playlistBrowserModel->getCurrentPlaylist();

	if (currentPlaylist->currentTrack >= 0) {
		return currentPlaylist->currentTrack;
	} else {
		return 0;
	}
}

int PlaylistTab::getTrackCount() const
{
	return playlistBrowserModel->getCurrentPlaylist()->tracks.size();
}

bool PlaylistTab::getRandom() const
{
	return randomAction->isChecked();
}

bool PlaylistTab::getRepeat() const
{
	return repeatAction->isChecked();
}

void PlaylistTab::createFileWidget()
{
	KFileWidget *fileWidget = new KFileWidget(KUrl(), fileWidgetSplitter);
	fileWidget->setFilter(MediaWidget::extensionFilter());
	fileWidget->setMode(KFile::Files | KFile::ExistingOnly);
	fileWidgetSplitter->setStretchFactor(1, 1);

	// KFileWidget creates a KUrlComboBox without layout (!), which steals the focus:
	//     kDebug() << QApplication::focusWidget();
	//     kDebug() << QApplication::focusWidget()->layout();
	// Let's reclaim the focus (and give it back to the main window).
	// FIXME report issue
	window()->setFocus();
}

void PlaylistTab::newPlaylist()
{
	Playlist *playlist = new Playlist();
	playlist->title = i18nc("playlist browser", "Unnamed Playlist");
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

void PlaylistTab::savePlaylistAs()
{
	savePlaylist(true);
}

void PlaylistTab::addSubtitle()
{
	QModelIndexList selectedRows = playlistView->selectionModel()->selectedRows();

	if (selectedRows.size() != 1) {
		return;
	}

	int row = selectedRows.at(0).row();
	Playlist *playlist = playlistModel->getVisiblePlaylist();
	QList<KUrl> urls = KFileDialog::getOpenUrls(KUrl(), subtitleExtensionFilter(), this);

	if ((row < playlist->tracks.size()) && !urls.isEmpty()) {
		PlaylistTrack &track = playlist->tracks[row];
		track.subtitles += urls;

		if (track.subtitles.size() == urls.size()) {
			track.currentSubtitle = 0;
		}

		if ((playlist == playlistBrowserModel->getCurrentPlaylist()) &&
		    (playlist->currentTrack == row)) {
			// FIXME !
		}
	}
}

void PlaylistTab::playlistActivated(const QModelIndex &index)
{
	playlistModel->setVisiblePlaylist(playlistBrowserModel->getPlaylist(index.row()));
}

void PlaylistTab::playPreviousTrack()
{
	Playlist *currentPlaylist = playlistBrowserModel->getCurrentPlaylist();
	playTrack(currentPlaylist, currentPlaylist->currentTrack - 1);
}

void PlaylistTab::playCurrentTrack()
{
	Playlist *currentPlaylist = playlistBrowserModel->getCurrentPlaylist();

	if (currentPlaylist->currentTrack >= 0) {
		playTrack(currentPlaylist, currentPlaylist->currentTrack);
	} else {
		playNextTrack();
	}
}

void PlaylistTab::playNextTrack()
{
	Playlist *currentPlaylist = playlistBrowserModel->getCurrentPlaylist();

	if (randomAction->isChecked()) {
		int size = currentPlaylist->tracks.size();

		if (size < 2) {
			playTrack(currentPlaylist, 0);
		} else if (currentPlaylist->currentTrack != -1) {
			int track = (qrand() % (size - 1));

			if (track >= currentPlaylist->currentTrack) {
				++track;
			}

			playTrack(currentPlaylist, track);
		} else {
			playTrack(currentPlaylist, qrand() % size);
		}
	} else if (((currentPlaylist->currentTrack + 1) == currentPlaylist->tracks.size()) &&
		   repeatAction->isChecked()) {
		playTrack(currentPlaylist, 0);
	} else {
		playTrack(currentPlaylist, currentPlaylist->currentTrack + 1);
	}
}

void PlaylistTab::activate()
{
	mediaLayout->addWidget(mediaWidget);
}

void PlaylistTab::playTrack(const QModelIndex &index)
{
	playTrack(playlistModel->getVisiblePlaylist(), index.row());
}

void PlaylistTab::playTrack(Playlist *playlist, int track)
{
	if ((track < 0) || (playlist == NULL) || (track >= playlist->tracks.size())) {
		track = -1;
	}

	if (track != -1) {
		PlaylistTrack &playlistTrack = playlist->tracks[track];
		KUrl subtitleUrl;

		if ((playlistTrack.currentSubtitle >= 0) &&
		    (playlistTrack.currentSubtitle < playlistTrack.subtitles.size())) {
			subtitleUrl = playlistTrack.subtitles.at(playlistTrack.currentSubtitle);
		} else if (playlistTrack.subtitles.isEmpty()) {
			// check whether there's a possible subtitle file candidate
			QString localFile = playlistTrack.url.toLocalFile();
			localFile.truncate(localFile.lastIndexOf(QLatin1Char('.')));

			if (!localFile.isEmpty()) {
				if (QFile::exists(localFile + QLatin1String(".asc"))) {
					subtitleUrl = KUrl::fromLocalFile(localFile + QLatin1String(".asc"));
				} else if (QFile::exists(localFile + QLatin1String(".smi"))) {
					subtitleUrl = KUrl::fromLocalFile(localFile + QLatin1String(".smi"));
				} else if (QFile::exists(localFile + QLatin1String(".srt"))) {
					subtitleUrl = KUrl::fromLocalFile(localFile + QLatin1String(".srt"));
				} else if (QFile::exists(localFile + QLatin1String(".ssa"))) {
					subtitleUrl = KUrl::fromLocalFile(localFile + QLatin1String(".ssa"));
				} else if (QFile::exists(localFile + QLatin1String(".sub"))) {
					subtitleUrl = KUrl::fromLocalFile(localFile + QLatin1String(".sub"));
				} else if (QFile::exists(localFile + QLatin1String(".txt"))) {
					subtitleUrl = KUrl::fromLocalFile(localFile + QLatin1String(".txt"));
				}
			}

			if (subtitleUrl.isValid()) {
				playlistTrack.subtitles += subtitleUrl;
				playlistTrack.currentSubtitle = 0;
			}
		}

		mediaWidget->play(playlistTrack.url, subtitleUrl);
		// FIXME !
		playlistBrowserModel->setCurrentPlaylist(playlist);
	} else {
		mediaWidget->stop();
	}

	if (playlist != NULL) {
		playlistModel->setCurrentTrack(playlist, track);
	}
}

void PlaylistTab::appendUrls(const QList<KUrl> &urls)
{
	playlistModel->appendUrls(playlistModel->getVisiblePlaylist(), urls, true);
}

void PlaylistTab::appendPlaylist(Playlist *playlist, bool playImmediately)
{
	playlistBrowserModel->append(playlist);

	if (playImmediately) {
		playlistBrowserModel->setCurrentPlaylist(playlist);
		playlistModel->setVisiblePlaylist(playlist);
		playNextTrack();
	}
}

void PlaylistTab::updateTrackLength(int length)
{
	playlistModel->updateTrackLength(playlistBrowserModel->getCurrentPlaylist(), length);
}

void PlaylistTab::updateTrackMetadata(const QMap<MediaWidget::MetadataType, QString> &metadata)
{
	playlistModel->updateTrackMetadata(playlistBrowserModel->getCurrentPlaylist(), metadata);
}

QString PlaylistTab::subtitleExtensionFilter()
{
	return QString(QLatin1String("*.asc *.smi *.srt *.ssa *.sub *.txt|")) +
		i18nc("file filter", "Subtitle Files");
}

void PlaylistTab::savePlaylist(bool askName)
{
	QModelIndex index = playlistBrowserView->currentIndex();

	if (!index.isValid()) {
		return;
	}

	Playlist *playlist = playlistBrowserModel->getPlaylist(index.row());
	KUrl url = playlist->url;

	if (askName || !url.isValid() ||
	    url.fileName().endsWith(QLatin1String(".kaffeine"), Qt::CaseInsensitive)) {
		url = KFileDialog::getSaveUrl(KUrl(), i18nc("file filter",
			"*.xspf|XSPF Playlist\n*.m3u|M3U Playlist\n*.pls|PLS Playlist"), this);

		if (!url.isValid()) {
			return;
		}

		playlist->url = url;
	}

	QString fileName = url.fileName();
	Playlist::Format format = Playlist::Invalid;

	if (fileName.endsWith(QLatin1String(".m3u"), Qt::CaseInsensitive)) {
		format = Playlist::M3U;
	} else if (fileName.endsWith(QLatin1String(".pls"), Qt::CaseInsensitive)) {
		format = Playlist::PLS;
	} else if (fileName.endsWith(QLatin1String(".xspf"), Qt::CaseInsensitive)) {
		format = Playlist::XSPF;
	}

	if (format != Playlist::Invalid) {
		playlist->save(format);
	}
}
