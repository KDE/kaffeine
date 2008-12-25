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

private:
	void activate()
	{
		layout()->addWidget(mediaWidget);
	}

	MediaWidget *mediaWidget;
};

PlayerTab::PlayerTab(MediaWidget *mediaWidget_) : mediaWidget(mediaWidget_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
}

KCmdLineOptions Kaffeine::cmdLineOptions()
{
	// FIXME add options
	return KCmdLineOptions();
}

Kaffeine::Kaffeine()
{
	// unlike qt, kde sets this flag
	setAttribute(Qt::WA_DeleteOnClose, false);

	collection = new KActionCollection(this);

	QWidget *widget = new QWidget(this);
	stackedLayout = new QStackedLayout(widget);
	setCentralWidget(widget);

	KToolBar *toolBar = new KToolBar("control_bar", this, Qt::BottomToolBarArea);
	toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	mediaWidget = new MediaWidget(widget, toolBar, collection);
	connect(mediaWidget, SIGNAL(toggleFullscreen()), this, SLOT(toggleFullscreen()));

	// the index of a tab in "tabs" has to correspond with the value from "TabIds"!

	TabBase *startTab = new StartTab(this);
	stackedLayout->addWidget(startTab);
	tabs.append(startTab);

	TabBase *playerTab = new PlayerTab(mediaWidget);
	stackedLayout->addWidget(playerTab);
	tabs.append(playerTab);

	TabBase *dvbTab = new DvbTab(mediaWidget);
	stackedLayout->addWidget(dvbTab);
	tabs.append(dvbTab);

	KMenuBar *menuBar = KMainWindow::menuBar();

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

	action = new KAction(KIcon("video-television"), i18n("Switch to DVB Tab"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(activateDvbTab()));
	menu->addAction(collection->addAction("view_dvb_tab", action));

	menu->addSeparator();

	action = new KAction(KIcon("view-fullscreen"), i18n("Full Screen Mode"), collection);
	action->setShortcut(Qt::Key_F);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(toggleFullscreen()));
	menu->addAction(collection->addAction("view_fullscreen", action));

	menu = new KMenu(i18n("&DVB"));
	menuBar->addMenu(menu);

	action = new KAction(KIcon("configure"), i18n("Configure Channels"), collection);
	connect(action, SIGNAL(triggered(bool)), dvbTab, SLOT(configureChannels()));
	menu->addAction(collection->addAction("dvb_channels", action));

	menu = new KMenu(i18n("&Settings"));
	menuBar->addMenu(menu);

	action = KStandardAction::keyBindings(this, SLOT(configureKeys()), collection);
	menu->addAction(collection->addAction("settings_keys", action));

	menu->addSeparator();

	action = new KAction(KIcon("configure"), i18n("Configure DVB"), collection);
	connect(action, SIGNAL(triggered(bool)), dvbTab, SLOT(configureDvb()));
	menu->addAction(collection->addAction("settings_dvb", action));

	menuBar->addMenu(helpMenu());

	// actions also have to work if the menu bar is hidden (fullscreen) - FIXME better solution?
	foreach (QAction *action, collection->actions()) {
		addAction(action);
	}

	// restore custom key bindings
	collection->readSettings();

	// let KMainWindow save / restore its settings
	setAutoSaveSettings();

	// make sure that the menu bar is visible (fullscreen -> quit -> restore -> hidden)
	menuBar->show();

	// workaround setAutoSaveSettings() which doesn't accept "IconOnly" as initial state
	toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	// workaround broken size restoring
	if (KConfig().group("Workarounds").readEntry("Maximized", false)) {
		QTimer::singleShot(0, this, SLOT(showMaximized()));
	}

	activateTab(TabStart);

	show();
}

Kaffeine::~Kaffeine()
{
	// workaround broken size restoring
	KConfig().group("Workarounds").writeEntry("Maximized", isMaximized());

	actionOpenRecent->saveEntries(KConfigGroup(KGlobal::config(), "Recent Files"));
}

void Kaffeine::parseArgs()
{
	// FIXME implement
}

void Kaffeine::open()
{
	// FIXME do we want to be able to open several files at once or not?
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Open file"));
	if (url.isValid()) {
		actionOpenRecent->addUrl(url);
		activateTab(TabPlayer);
		mediaWidget->play(url);
	}
}

void Kaffeine::openUrl()
{
	KUrl url(KInputDialog::getText(i18n("Open URL"), i18n("Enter a URL:")));
	if (url.isValid()) {
		actionOpenRecent->addUrl(url);
		activateTab(TabPlayer);
		mediaWidget->play(url);
	}
}

void Kaffeine::openRecent(const KUrl &url)
{
	// we need to copy "url" because addUrl() invalidates it
	KUrl copy(url);
	actionOpenRecent->addUrl(copy); // moves the url to the top of the list
	activateTab(TabPlayer);
	mediaWidget->play(copy);
}

void Kaffeine::openAudioCd()
{
	activateTab(TabPlayer); // FIXME
	mediaWidget->playAudioCd();
}

void Kaffeine::openVideoCd()
{
	activateTab(TabPlayer);
	mediaWidget->playVideoCd();
}

void Kaffeine::openDvd()
{
	activateTab(TabPlayer);
	mediaWidget->playDvd();
}

void Kaffeine::toggleFullscreen()
{
	setWindowState(windowState() ^ Qt::WindowFullScreen);

	if (isFullScreen()) {
		menuBar()->hide();

		TabBase *playerTab = tabs.at(TabPlayer);
		stackedLayout->setCurrentWidget(playerTab);
		playerTab->activate();
	} else {
		menuBar()->show();

		stackedLayout->setCurrentWidget(currentTab);
		currentTab->activate();
	}

	// FIXME hide cursor / toolbar
}

void Kaffeine::configureKeys()
{
	KShortcutsDialog::configure(collection);
}

void Kaffeine::activateStartTab()
{
	activateTab(TabStart);
}

void Kaffeine::activatePlayerTab()
{
	activateTab(TabPlayer);
}

void Kaffeine::activateDvbTab()
{
	activateTab(TabDvb);
}

void Kaffeine::activateTab(TabIds tabId)
{
	currentTab = tabs.at(tabId);

	if (isFullScreen()) {
		return;
	}

	stackedLayout->setCurrentWidget(currentTab);
	currentTab->activate();
}
