/*
 * kaffeine.cpp
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

#include "kaffeine.h"

#include <QHoverEvent>
#include <QLabel>
#include <QPushButton>
#include <QStackedLayout>
#include <QTimer>
#include <KActionCollection>
#include <KCmdLineOptions>
#include <KFileDialog>
#include <KInputDialog>
#include <KMenu>
#include <KMenuBar>
#include <KRecentFilesAction>
#include <KShortcutsDialog>
#include <KToolBar>
#include "dvb/dvbtab.h"
#include "playlist/playlisttab.h"
#include "mediawidget.h"

class StartTab : public TabBase
{
public:
	explicit StartTab(Kaffeine *kaffeine);
	~StartTab() { }

private:
	void activate() { }

	QPushButton *addShortcut(const QString &name, const KIcon &icon, QWidget *parent);
};

StartTab::StartTab(Kaffeine *kaffeine)
{
	QVBoxLayout *boxLayout = new QVBoxLayout(this);
	boxLayout->setMargin(0);
	boxLayout->setSpacing(0);

	QLabel *label = new QLabel(i18n("<center><font size=\"+4\">"
		"<b>[Kaffeine Player]</b><br>caffeine for your desktop!</font></center>"));
	label->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
	QPalette palette = label->palette();
	palette.setColor(label->backgroundRole(), QColor(127, 127, 255));
	label->setPalette(palette);
	label->setAutoFillBackground(true);
	boxLayout->addWidget(label);

	QWidget *widget = new QWidget(this);
	palette = widget->palette();
	palette.setColor(widget->backgroundRole(), QColor(255, 255, 255));
	widget->setPalette(palette);
	widget->setAutoFillBackground(true);
	boxLayout->addWidget(widget);

	QGridLayout *gridLayout = new QGridLayout(widget);
	gridLayout->setAlignment(Qt::AlignCenter);
	gridLayout->setMargin(15);
	gridLayout->setSpacing(15);

	QPushButton *button = addShortcut(i18n("Play File"), KIcon("video-x-generic"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(open()));
	gridLayout->addWidget(button, 0, 0);

	button = addShortcut(i18n("Play Audio CD"), KIcon("media-optical-audio"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openAudioCd()));
	gridLayout->addWidget(button, 0, 1);

	button = addShortcut(i18n("Play Video CD"), KIcon("media-optical"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openVideoCd()));
	gridLayout->addWidget(button, 0, 2);

	button = addShortcut(i18n("Play DVD"), KIcon("media-optical"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openDvd()));
	gridLayout->addWidget(button, 1, 0);

	button = addShortcut(i18n("Digital TV"), KIcon("video-television"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(activateDvbTab()));
	gridLayout->addWidget(button, 1, 1);
}

QPushButton *StartTab::addShortcut(const QString &name, const KIcon &icon, QWidget *parent)
{
	QPushButton *button = new QPushButton(parent);
	button->setText(name);
	button->setIcon(icon);
	button->setIconSize(QSize(48, 48));
	button->setFocusPolicy(Qt::NoFocus); // FIXME deal with shortcut <-> visual appearance
	return button;
}

class PlayerTab : public TabBase
{
public:
	explicit PlayerTab(MediaWidget *mediaWidget_);
	~PlayerTab() { }

	void activate()
	{
		layout()->addWidget(mediaWidget);
	}

private:
	MediaWidget *mediaWidget;
};

PlayerTab::PlayerTab(MediaWidget *mediaWidget_) : mediaWidget(mediaWidget_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
}

Kaffeine::Kaffeine()
{
	// menu structure

	KMenuBar *menuBar = KMainWindow::menuBar();
	collection = new KActionCollection(this);

	KMenu *menu = new KMenu(i18n("&File"));
	menuBar->addMenu(menu);

	KAction *action = KStandardAction::open(this, SLOT(open()), collection);
	menu->addAction(collection->addAction("file_open", action));

	action = new KAction(KIcon("uri-mms"), i18n("Open URL"), collection);
	action->setShortcut(Qt::CTRL | Qt::Key_U);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openUrl()));
	menu->addAction(collection->addAction("file_open_url", action));

	actionOpenRecent = KStandardAction::openRecent(this, SLOT(openRecent(KUrl)), collection);
	actionOpenRecent->loadEntries(KConfigGroup(KGlobal::config(), "Recent Files"));
	menu->addAction(collection->addAction("file_open_recent", actionOpenRecent));

	menu->addSeparator();

	action = new KAction(KIcon("media-optical-audio"), i18n("Play Audio CD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openAudioCd()));
	menu->addAction(collection->addAction("file_play_audiocd", action));

	action = new KAction(KIcon("media-optical"), i18n("Play Video CD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openVideoCd()));
	menu->addAction(collection->addAction("file_play_videocd", action));

	action = new KAction(KIcon("media-optical"), i18n("Play DVD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openDvd()));
	menu->addAction(collection->addAction("file_play_dvd", action));

	menu->addSeparator();

	action = KStandardAction::quit(this, SLOT(close()), collection);
	menu->addAction(collection->addAction("file_quit", action));

	menu = new KMenu(i18n("&View"));
	menuBar->addMenu(menu);

	action = new KAction(KIcon("start-here-kde"), i18n("Switch to Start Tab"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(activateStartTab()));
	menu->addAction(collection->addAction("view_start_tab", action));

	action = new KAction(KIcon("kaffeine"), i18n("Switch to Player Tab"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(activatePlayerTab()));
	menu->addAction(collection->addAction("view_player_tab", action));

	action = new KAction(KIcon("view-media-playlist"), i18n("Switch to Playlist Tab"),
		collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(activatePlaylistTab()));
	menu->addAction(collection->addAction("view_playlist_tab", action));

	action = new KAction(KIcon("video-television"), i18n("Switch to DVB Tab"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(activateDvbTab()));
	menu->addAction(collection->addAction("view_dvb_tab", action));

	menu->addSeparator();

	action = new KAction(KIcon("view-fullscreen"), i18n("Full Screen Mode"), collection);
	action->setShortcut(Qt::Key_F);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(toggleFullscreen()));
	menu->addAction(collection->addAction("view_fullscreen", action));

	KMenu *dvbMenu = new KMenu(i18n("&DVB"));
	menuBar->addMenu(dvbMenu);

	menu = new KMenu(i18n("&Settings"));
	menuBar->addMenu(menu);

	action = KStandardAction::keyBindings(this, SLOT(configureKeys()), collection);
	menu->addAction(collection->addAction("settings_keys", action));

	menuBar->addMenu(helpMenu());

	// control bar

	controlBar = new KToolBar("control_bar", this, Qt::BottomToolBarArea);
	controlBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	cursorHideTimer = new QTimer(this);
	cursorHideTimer->setInterval(1500);
	cursorHideTimer->setSingleShot(true);
	connect(cursorHideTimer, SIGNAL(timeout()), this, SLOT(hideCursor()));

	// main area

	QWidget *widget = new QWidget(this);
	stackedLayout = new QStackedLayout(widget);
	setCentralWidget(widget);

	mediaWidget = new MediaWidget(controlBar, collection, widget);
	connect(mediaWidget, SIGNAL(toggleFullscreen()), this, SLOT(toggleFullscreen()));

	// tabs

	startTab = new StartTab(this);
	stackedLayout->addWidget(startTab);

	playerTab = new PlayerTab(mediaWidget);
	stackedLayout->addWidget(playerTab);

	playlistTab = new PlaylistTab(mediaWidget);
	stackedLayout->addWidget(playlistTab);

	dvbTab = new DvbTab(dvbMenu, collection, mediaWidget);
	stackedLayout->addWidget(dvbTab);

	activateTab(startTab);

	// actions also have to work if the menu bar is hidden (fullscreen) - FIXME better solution?
	foreach (QAction *action, collection->actions()) {
		addAction(action);
	}

	// restore custom key bindings
	collection->readSettings();

	// let KMainWindow save / restore its settings
	setAutoSaveSettings();

	// make sure that the bars are visible (fullscreen -> quit -> restore -> hidden)
	menuBar->show();
	controlBar->show();

	// workaround setAutoSaveSettings() which doesn't accept "IconOnly" as initial state
	controlBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	show();
}

Kaffeine::~Kaffeine()
{
	actionOpenRecent->saveEntries(KConfigGroup(KGlobal::config(), "Recent Files"));
}

KCmdLineOptions Kaffeine::cmdLineOptions()
{
	KCmdLineOptions options;
	options.add("f");
	options.add("fullscreen", ki18n("Start in fullscreen mode"));
	options.add("dvb <channel>", ki18n("Tune to the selected channel"));
	options.add("+[file]", ki18n("File or url to play"));
	return options;
}

void Kaffeine::parseArgs()
{
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->isSet("fullscreen") && !isFullScreen()) {
		toggleFullscreen();
	}

	if (args->count() > 0) {
		KUrl url = args->url(0);
		actionOpenRecent->addUrl(url);
		activateTab(playerTab);
		mediaWidget->play(url);
		args->clear();
		// FIXME fix case where more than one url is passed
		return;
	}

	QString dvb = args->getOption("dvb");

	if (!dvb.isEmpty()) {
		activateDvbTab();
		dvbTab->playChannel(dvb);
	}
	args->clear();
}

void Kaffeine::open()
{
	// FIXME do we want to be able to open several files at once or not?
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Open file"));
	if (url.isValid()) {
		actionOpenRecent->addUrl(url);
		activateTab(playerTab);
		mediaWidget->play(url);
	}
}

void Kaffeine::openUrl()
{
	KUrl url(KInputDialog::getText(i18n("Open URL"), i18n("Enter a URL:")));
	if (url.isValid()) {
		actionOpenRecent->addUrl(url);
		activateTab(playerTab);
		mediaWidget->play(url);
	}
}

void Kaffeine::openRecent(const KUrl &url)
{
	// we need to copy "url" because addUrl() invalidates it
	KUrl copy(url);
	actionOpenRecent->addUrl(copy); // moves the url to the top of the list
	activateTab(playerTab);
	mediaWidget->play(copy);
}

void Kaffeine::openAudioCd()
{
	activateTab(playerTab); // FIXME
	mediaWidget->playAudioCd();
}

void Kaffeine::openVideoCd()
{
	activateTab(playerTab);
	mediaWidget->playVideoCd();
}

void Kaffeine::openDvd()
{
	activateTab(playerTab);
	mediaWidget->playDvd();
}

void Kaffeine::toggleFullscreen()
{
	setWindowState(windowState() ^ Qt::WindowFullScreen);

	if (isFullScreen()) {
		menuBar()->hide();
		controlBar->hide();
		cursorHideTimer->start();

		stackedLayout->setCurrentWidget(playerTab);
		playerTab->activate();
	} else {
		cursorHideTimer->stop();
		unsetCursor();
		menuBar()->show();
		controlBar->show();

		stackedLayout->setCurrentWidget(currentTab);
		currentTab->activate();
	}
}

void Kaffeine::configureKeys()
{
	KShortcutsDialog::configure(collection);
}

void Kaffeine::activateStartTab()
{
	activateTab(startTab);
}

void Kaffeine::activatePlayerTab()
{
	activateTab(playerTab);
}

void Kaffeine::activatePlaylistTab()
{
	activateTab(playlistTab);
}

void Kaffeine::activateDvbTab()
{
	activateTab(dvbTab);
}

void Kaffeine::hideCursor()
{
	setCursor(Qt::BlankCursor);
}

void Kaffeine::activateTab(TabBase *tab)
{
	currentTab = tab;

	if (isFullScreen()) {
		return;
	}

	stackedLayout->setCurrentWidget(currentTab);
	currentTab->activate();
}

bool Kaffeine::event(QEvent *event)
{
	// FIXME we depend on QEvent::HoverMove (instead of QEvent::MouseMove)
	// but the latter depends on mouse tracking being enabled on this widget
	// and all its children (especially the phonon video widget) ...

	if ((event->type() == QEvent::HoverMove) && isFullScreen()) {
		cursorHideTimer->stop();
		unsetCursor();

		int y = reinterpret_cast<QHoverEvent *> (event)->pos().y();

		switch (toolBarArea(controlBar)) {
		case Qt::TopToolBarArea:
			controlBar->setVisible((y < 60) && (y >= 0));
			break;
		case Qt::BottomToolBarArea:
			controlBar->setVisible((y >= (height() - 60)) && (y < height()));
			break;
		default:
			break;
		}

		if (controlBar->isHidden()) {
			cursorHideTimer->start();
		}
	}

	return KMainWindow::event(event);
}
