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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "kaffeine.h"

#include <QDBusConnection>
#include <QHoverEvent>
#include <QLabel>
#include <QSpinBox>
#include <QStackedLayout>
#include <KActionCollection>
#include <KCmdLineOptions>
#include <KFileDialog>
#include <KInputDialog>
#include <KMenu>
#include <KMenuBar>
#include <KRecentFilesAction>
#include <KShortcutsDialog>
#include <KSystemTrayIcon>
#include <KTabBar>
#include <KToolBar>
#include "dvb/dvbtab.h"
#include "playlist/playlisttab.h"
#include "dbusobjects.h"
#include "mediawidget.h"

class StartTab : public TabBase
{
public:
	explicit StartTab(Kaffeine *kaffeine);
	~StartTab() { }

private:
	void activate() { }

	QAbstractButton *addShortcut(const QString &name, const KIcon &icon, QWidget *parent);
};

StartTab::StartTab(Kaffeine *kaffeine)
{
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);

	QGridLayout *gridLayout = new QGridLayout(this);
	gridLayout->setAlignment(Qt::AlignCenter);
	gridLayout->setMargin(10);
	gridLayout->setSpacing(15);

	QAbstractButton *button = addShortcut(i18n("&1 Play File"), KIcon("video-x-generic"), this);
	button->setShortcut(Qt::Key_1);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(open()));
	gridLayout->addWidget(button, 0, 0);

	button = addShortcut(i18n("&2 Play Audio CD"), KIcon("media-optical-audio"), this);
	button->setShortcut(Qt::Key_2);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openAudioCd()));
	gridLayout->addWidget(button, 0, 1);

	button = addShortcut(i18n("&3 Play Video CD"), KIcon("media-optical"), this);
	button->setShortcut(Qt::Key_3);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openVideoCd()));
	gridLayout->addWidget(button, 0, 2);

	button = addShortcut(i18n("&4 Play DVD"), KIcon("media-optical"), this);
	button->setShortcut(Qt::Key_4);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(openDvd()));
	gridLayout->addWidget(button, 1, 0);

	button = addShortcut(i18n("&5 Digital TV"), KIcon("video-television"), this);
	button->setShortcut(Qt::Key_5);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(playDvb()));
	gridLayout->addWidget(button, 1, 1);
}

QAbstractButton *StartTab::addShortcut(const QString &name, const KIcon &icon, QWidget *parent)
{
	// QPushButton has visual problems with big icons
	QToolButton *button = new QToolButton(parent);
	button->setText(name);
	button->setIcon(icon);
	button->setFocusPolicy(Qt::NoFocus);
	button->setIconSize(QSize(48, 48));
	button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
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

class ConfigurationDialog : public KDialog
{
public:
	ConfigurationDialog(MediaWidget *mediaWidget_, QWidget *parent);
	~ConfigurationDialog();

	void accept();

private:
	MediaWidget *mediaWidget;
	QSpinBox *shortSkipBox;
	QSpinBox *longSkipBox;
};

ConfigurationDialog::ConfigurationDialog(MediaWidget *mediaWidget_, QWidget *parent) :
	KDialog(parent), mediaWidget(mediaWidget_)
{
	setCaption(i18nc("dialog", "Configure Kaffeine"));

	QWidget *widget = new QWidget(this);
	QGridLayout *gridLayout = new QGridLayout(widget);

	gridLayout->addWidget(new QLabel(i18nc("'Configure Kaffeine' dialog",
		"Short skip duration:"), widget), 0, 0);

	shortSkipBox = new QSpinBox(widget);
	shortSkipBox->setRange(1, 600);
	shortSkipBox->setValue(mediaWidget->getShortSkipDuration());
	gridLayout->addWidget(shortSkipBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18nc("'Configure Kaffeine' dialog",
		"Long skip duration:"), widget), 1, 0);

	longSkipBox = new QSpinBox(widget);
	longSkipBox->setRange(1, 600);
	longSkipBox->setValue(mediaWidget->getLongSkipDuration());
	gridLayout->addWidget(longSkipBox, 1, 1);

	setMainWidget(widget);
}

ConfigurationDialog::~ConfigurationDialog()
{
}

void ConfigurationDialog::accept()
{
	mediaWidget->setShortSkipDuration(shortSkipBox->value());
	mediaWidget->setLongSkipDuration(longSkipBox->value());

	KDialog::accept();
}

Kaffeine::Kaffeine()
{
	// menu structure

	KMenuBar *menuBar = KMainWindow::menuBar();
	collection = new KActionCollection(this);

	KMenu *menu = new KMenu(i18n("&File"), this);
	menuBar->addMenu(menu);

	KAction *action = KStandardAction::open(this, SLOT(open()), collection);
	menu->addAction(collection->addAction("file_open", action));

	action = new KAction(KIcon("uri-mms"), i18n("Open URL"), collection);
	action->setShortcut(Qt::CTRL | Qt::Key_U);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openUrl()));
	menu->addAction(collection->addAction("file_open_url", action));

	actionOpenRecent = KStandardAction::openRecent(this, SLOT(openUrl(KUrl)), collection);
	actionOpenRecent->loadEntries(KGlobal::config()->group("Recent Files"));
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

	KMenu *playerMenu = new KMenu(i18n("&Playback"), this);
	menuBar->addMenu(playerMenu);

	fullScreenAction = new KAction(KIcon("view-fullscreen"), i18n("Full Screen Mode"), this);
	fullScreenAction->setShortcut(Qt::Key_F);
	connect(fullScreenAction, SIGNAL(triggered(bool)), this, SLOT(toggleFullScreen()));

	KMenu *playlistMenu = new KMenu(i18nc("menu bar", "Play&list"), this);
	menuBar->addMenu(playlistMenu);

	KMenu *dvbMenu = new KMenu(i18n("&Television"), this);
	menuBar->addMenu(dvbMenu);

	menu = new KMenu(i18n("&Settings"), this);
	menuBar->addMenu(menu);

	action = KStandardAction::keyBindings(this, SLOT(configureKeys()), collection);
	menu->addAction(collection->addAction("settings_keys", action));

	action = new KAction(KIcon("configure"), i18nc("dialog", "Configure Kaffeine"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(configureKaffeine()));
	menu->addAction(collection->addAction("settings_kaffeine", action));

	menuBar->addMenu(helpMenu());

	// navigation bar - keep in sync with TabIndex enum!

	navigationBar = new KToolBar("navigation_bar", this, Qt::LeftToolBarArea);
	connect(navigationBar, SIGNAL(orientationChanged(Qt::Orientation)),
		this, SLOT(navigationBarOrientationChanged(Qt::Orientation)));

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
	connect(mediaWidget, SIGNAL(resizeToVideo(int)), this, SLOT(resizeToVideo(int)));

	// tabs - keep in sync with TabIndex enum!

	TabBase *startTab = new StartTab(this);
	tabs.append(startTab);
	stackedLayout->addWidget(startTab);

	playerTab = new PlayerTab(mediaWidget);
	tabs.append(playerTab);
	stackedLayout->addWidget(playerTab);

	playlistTab = new PlaylistTab(playlistMenu, collection, mediaWidget);
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

	KSystemTrayIcon *trayIcon = new KSystemTrayIcon(KIcon("kaffeine"), this);
	trayIcon->show();

	// initialize random number generator
	qsrand(QTime().msecsTo(QTime::currentTime()));

	// initialize dbus objects
	QDBusConnection::sessionBus().registerObject("/", new MprisRootObject(this),
		QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerObject("/Player",
		new MprisPlayerObject(this, mediaWidget, playlistTab, this),
		QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerObject("/TrackList",
		new MprisTrackListObject(playlistTab, this), QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerObject("/Television",
		new DBusTelevisionObject(dvbTab, this), QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerService("org.mpris.kaffeine");

	show();
}

Kaffeine::~Kaffeine()
{
	actionOpenRecent->saveEntries(KGlobal::config()->group("Recent Files"));
}

KCmdLineOptions Kaffeine::cmdLineOptions()
{
	KCmdLineOptions options;
	options.add("f");
	options.add("fullscreen", ki18n("Start in full screen mode"));
	options.add("audiocd", ki18n("Play Audio CD"));
	options.add("videocd", ki18n("Play Video CD"));
	options.add("dvd", ki18n("Play DVD"));
	options.add("tv <channel>", ki18nc("command line option", "Deprecated"));
	options.add("channel <name / number>", ki18nc("command line option", "Play TV channel"));
	options.add("lastchannel", ki18nc("command line option", "Play last tuned TV channel"));
	options.add("dumpdvb", ki18nc("command line option", "Dump dvb data (debug option)"));
	options.add("+[file]", ki18n("Files or URLs to play"));
	return options;
}

void Kaffeine::parseArgs()
{
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->isSet("fullscreen") && !isFullScreen()) {
		toggleFullScreen();
	}

	if (args->isSet("dumpdvb")) {
		dvbTab->enableDvbDump();
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

	QString channel = args->getOption("channel");

	if (!channel.isEmpty()) {
		activateTab(DvbTabId);
		dvbTab->playChannel(channel);

		args->clear();
		return;
	}

	channel = args->getOption("tv");

	if (!channel.isEmpty()) {
		activateTab(DvbTabId);
		dvbTab->playChannel(channel);

		args->clear();
		return;
	}

	if (args->isSet("lastchannel")) {
		activateTab(DvbTabId);
		dvbTab->playLastChannel();

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
			playlistTab->appendUrls(urls, true);
		} else if (!urls.isEmpty()) {
			openUrl(urls.at(0));
		}
	}

	args->clear();
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

void Kaffeine::open()
{
	QList<KUrl> urls = KFileDialog::getOpenUrls(KUrl(), MediaWidget::extensionFilter(), this);

	if (urls.size() >= 2) {
		activateTab(PlaylistTabId);
		playlistTab->appendUrls(urls, true);
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

	playlistTab->appendUrls(QList<KUrl>() << copy, true);
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

void Kaffeine::playDvb()
{
	activateTab(DvbTabId);
	dvbTab->playLastChannel();
}

void Kaffeine::resizeToVideo(int factor)
{
	if (!isFullScreen()) {
		if (isMaximized()) {
			setWindowState(windowState() & ~Qt::WindowMaximized);
		}

		resize(size() - centralWidget()->size() + factor * mediaWidget->sizeHint());
	}
}

void Kaffeine::configureKeys()
{
	KShortcutsDialog::configure(collection);
}

void Kaffeine::configureKaffeine()
{
	ConfigurationDialog dialog(mediaWidget, this);
	dialog.exec();
}

void Kaffeine::activateDvbTab()
{
	activateTab(DvbTabId);
}

void Kaffeine::navigationBarOrientationChanged(Qt::Orientation orientation)
{
	if (orientation == Qt::Horizontal) {
		tabBar->setShape(KTabBar::RoundedNorth);
	} else {
		tabBar->setShape(KTabBar::RoundedWest);
	}
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

bool Kaffeine::event(QEvent *event)
{
	bool retVal = KMainWindow::event(event); // this has to be done before calling setVisible()

	// FIXME we depend on QEvent::HoverMove (instead of QEvent::MouseMove)
	// but the latter depends on mouse tracking being enabled on this widget
	// and all its children (especially the phonon video widget) ...

	if ((event->type() == QEvent::HoverMove) && isFullScreen()) {
		int y = reinterpret_cast<QHoverEvent *> (event)->pos().y();

		if ((y < 0) || (y >= height())) {
			// QHoverEvent sometimes reports quite strange coordinates - ignore them
			return retVal;
		}

		cursorHideTimer->stop();
		unsetCursor();

		switch (toolBarArea(controlBar)) {
		case Qt::TopToolBarArea:
			controlBar->setVisible(y < 60);
			break;
		case Qt::BottomToolBarArea:
			controlBar->setVisible(y >= (height() - 60));
			break;
		default:
			break;
		}

		if (controlBar->isHidden()) {
			cursorHideTimer->start();
		}
	}

	return retVal;
}

void Kaffeine::keyPressEvent(QKeyEvent *event)
{
	if ((event->key() == Qt::Key_Escape) && isFullScreen()) {
		toggleFullScreen();
		return;
	}

	KMainWindow::keyPressEvent(event);
}

void Kaffeine::leaveEvent(QEvent *event)
{
	if (isFullScreen()) {
		controlBar->setVisible(false);
	}

	KMainWindow::leaveEvent(event);
}
