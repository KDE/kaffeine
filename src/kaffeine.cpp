/*
 * kaffeine.cpp
 *
 * Copyright (C) 2007-2009 Christoph Pfister <christophpfister@gmail.com>
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

#include <QDBusInterface>
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
#include <KTabBar>
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

	QLabel *label = new QLabel(i18n("<center><font size=\"+4\"><b>[Kaffeine Player]</b><br>"
		"caffeine for your desktop!</font></center>"));
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
	gridLayout->setMargin(10);
	gridLayout->setSpacing(15);

	QPushButton *button = addShortcut(i18n("&1 Play File"), KIcon("video-x-generic"), widget);
	button->setShortcut(Qt::Key_1);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(open()));
	gridLayout->addWidget(button, 0, 0);

	button = addShortcut(i18n("&2 Play Audio CD"), KIcon("media-optical-audio"), widget);
	button->setShortcut(Qt::Key_2);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openAudioCd()));
	gridLayout->addWidget(button, 0, 1);

	button = addShortcut(i18n("&3 Play Video CD"), KIcon("media-optical"), widget);
	button->setShortcut(Qt::Key_3);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openVideoCd()));
	gridLayout->addWidget(button, 0, 2);

	button = addShortcut(i18n("&4 Play DVD"), KIcon("media-optical"), widget);
	button->setShortcut(Qt::Key_4);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openDvd()));
	gridLayout->addWidget(button, 1, 0);

	button = addShortcut(i18n("&5 Digital TV"), KIcon("video-television"), widget);
	button->setShortcut(Qt::Key_5);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(activateDvbTab()));
	gridLayout->addWidget(button, 1, 1);
}

QPushButton *StartTab::addShortcut(const QString &name, const KIcon &icon, QWidget *parent)
{
	QPushButton *button = new QPushButton(parent);
	button->setText(name);
	button->setIcon(icon);
	button->setIconSize(QSize(48, 48));
	button->setFocusPolicy(Qt::NoFocus);
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

	actionOpenRecent = KStandardAction::openRecent(this, SLOT(openUrl(KUrl)), collection);
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

	KMenu *playerMenu = new KMenu(i18n("&Playback"));
	menuBar->addMenu(playerMenu);

	fullScreenAction = new KAction(KIcon("view-fullscreen"), i18n("Full Screen Mode"), this);
	fullScreenAction->setShortcut(Qt::Key_F);
	connect(fullScreenAction, SIGNAL(triggered(bool)), this, SLOT(toggleFullScreen()));

	KMenu *dvbMenu = new KMenu(i18n("&Television"));
	menuBar->addMenu(dvbMenu);

	menu = new KMenu(i18n("&Settings"));
	menuBar->addMenu(menu);

	action = KStandardAction::keyBindings(this, SLOT(configureKeys()), collection);
	menu->addAction(collection->addAction("settings_keys", action));

	menuBar->addMenu(helpMenu());

	// navigation bar - keep in sync with TabIndex enum!

	navigationBar = new KToolBar("navigation_bar", this, Qt::LeftToolBarArea);

	tabBar = new KTabBar(navigationBar);
	tabBar->addTab(KIcon("start-here-kde"), i18n("Start"));
	tabBar->addTab(KIcon("kaffeine"), i18n("Playback"));
	tabBar->addTab(KIcon("view-media-playlist"), i18n("Playlist"));
	tabBar->addTab(KIcon("video-television"), i18n("Television"));
	tabBar->setShape(KTabBar::RoundedWest);
	connect(tabBar, SIGNAL(currentChanged(int)), this, SLOT(activateTab(int)));
	navigationBar->addWidget(tabBar);

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

	mediaWidget = new MediaWidget(playerMenu, fullScreenAction, controlBar, collection, widget);
	connect(mediaWidget, SIGNAL(changeCaption(QString)), this, SLOT(setCaption(QString)));
	connect(mediaWidget, SIGNAL(toggleFullScreen()), this, SLOT(toggleFullScreen()));

	QTimer *timer = new QTimer(this);
	timer->start(50000);
	connect(timer, SIGNAL(timeout()), this, SLOT(checkScreenSaver()));

	// tabs - keep in sync with TabIndex enum!

	TabBase *startTab = new StartTab(this);
	tabs.append(startTab);
	stackedLayout->addWidget(startTab);

	playerTab = new PlayerTab(mediaWidget);
	tabs.append(playerTab);
	stackedLayout->addWidget(playerTab);

	playlistTab = new PlaylistTab(mediaWidget);
	tabs.append(playlistTab);
	stackedLayout->addWidget(playlistTab);

	dvbTab = new DvbTab(dvbMenu, collection, mediaWidget);
	tabs.append(dvbTab);
	stackedLayout->addWidget(dvbTab);

	currentTabIndex = StartTabId;

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
	navigationBar->show();
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
	options.add("fullscreen", ki18n("Start in full screen mode"));
	options.add("audiocd", ki18n("Play Audio CD"));
	options.add("videocd", ki18n("Play Video CD"));
	options.add("dvd", ki18n("Play DVD"));
	options.add("tv <channel>", ki18n("Play TV channel"));
	options.add("+[file]", ki18n("Files or URLs to play"));
	return options;
}

void Kaffeine::parseArgs()
{
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->isSet("fullscreen") && !isFullScreen()) {
		toggleFullScreen();
	}

	if (args->isSet("audiocd")) {
		// FIXME device is ignored
		openAudioCd();

		args->clear();
		return;
	}

	if (args->isSet("videocd")) {
		// FIXME device is ignored
		openVideoCd();

		args->clear();
		return;
	}

	if (args->isSet("dvd")) {
		// FIXME device is ignored
		openDvd();

		args->clear();
		return;
	}

	QString dvb = args->getOption("tv");

	if (!dvb.isEmpty()) {
		activateTab(DvbTabId);
		dvbTab->playChannel(dvb);

		args->clear();
		return;
	}

	if (args->count() > 0) {
		QList<KUrl> urls;

		for (int i = 0; i < args->count(); ++i) {
			KUrl url = args->url(i);

			if (url.isValid()) {
				urls.append(url);
			}
		}

		if (urls.size() >= 2) {
			activateTab(PlaylistTabId);
			playlistTab->playUrls(urls);
		} else if (!urls.isEmpty()) {
			openUrl(urls.at(0));
		}
	}

	args->clear();
}

void Kaffeine::open()
{
	QList<KUrl> urls = KFileDialog::getOpenUrls(KUrl(), MediaWidget::extensionFilter(), this);

	if (urls.size() >= 2) {
		activateTab(PlaylistTabId);
		playlistTab->playUrls(urls);
	} else if (!urls.isEmpty()) {
		openUrl(urls.at(0));
	}
}

void Kaffeine::openUrl()
{
	openUrl(KInputDialog::getText(i18n("Open URL"), i18n("Enter a URL:")));
}

void Kaffeine::openUrl(const KUrl &url)
{
	if (!url.isValid()) {
		return;
	}

	// we need to copy "url" because addUrl() may invalidate it
	KUrl copy(url);
	actionOpenRecent->addUrl(copy); // moves the url to the top of the list

	if (currentTabIndex != PlaylistTabId) {
		activateTab(PlayerTabId);
	}

	playlistTab->playUrls(QList<KUrl>() << copy);
}

void Kaffeine::openAudioCd()
{
	activateTab(PlayerTabId); // FIXME
	mediaWidget->playAudioCd();
}

void Kaffeine::openVideoCd()
{
	activateTab(PlayerTabId);
	mediaWidget->playVideoCd();
}

void Kaffeine::openDvd()
{
	activateTab(PlayerTabId);
	mediaWidget->playDvd();
}

void Kaffeine::toggleFullScreen()
{
	setWindowState(windowState() ^ Qt::WindowFullScreen);

	if (isFullScreen()) {
		menuBar()->hide();
		navigationBar->hide();
		controlBar->hide();
		fullScreenAction->setText(i18n("Exit Full Screen Mode"));
		fullScreenAction->setIcon(KIcon("view-restore"));
		cursorHideTimer->start();

		stackedLayout->setCurrentIndex(PlayerTabId);
		playerTab->activate();
	} else {
		cursorHideTimer->stop();
		unsetCursor();
		fullScreenAction->setText(i18n("Full Screen Mode"));
		fullScreenAction->setIcon(KIcon("view-fullscreen"));
		menuBar()->show();
		navigationBar->show();
		controlBar->show();

		stackedLayout->setCurrentIndex(currentTabIndex);
		tabs.at(currentTabIndex)->activate();
	}
}

void Kaffeine::configureKeys()
{
	KShortcutsDialog::configure(collection);
}

void Kaffeine::activateDvbTab()
{
	activateTab(DvbTabId);
}

void Kaffeine::activateTab(int tabIndex)
{
	currentTabIndex = tabIndex;
	tabBar->setCurrentIndex(tabIndex);

	if (isFullScreen()) {
		return;
	}

	stackedLayout->setCurrentIndex(currentTabIndex);
	tabs.at(currentTabIndex)->activate();
}

void Kaffeine::hideCursor()
{
	setCursor(Qt::BlankCursor);
}

void Kaffeine::checkScreenSaver()
{
	if (isFullScreen() || mediaWidget->shouldInhibitScreenSaver()) {
		QDBusInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
			"org.freedesktop.ScreenSaver").call(QDBus::NoBlock, "SimulateUserActivity");
	}
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
