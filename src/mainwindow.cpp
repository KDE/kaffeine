/*
 * mainwindow.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
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

#include "log.h"

#include <KAboutData>
#include <KActionCollection>
#include <KConfigGroup>
#include <KHelpMenu>
#include <kio/deletejob.h>
#include <KRecentFilesAction>
#include <KSharedConfig>
#include <KShortcutsDialog>
#include <QApplication>
#include <QCommandLineOption>
#include <QDBusConnection>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QHoverEvent>
#include <QInputDialog>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStackedLayout>
#include <QTabBar>
#include <QToolBar>

#include "configuration.h"
#include "configurationdialog.h"
#include "dbusobjects.h"
#include "dvb/dvbtab.h"
#include "mainwindow.h"
#include "playlist/playlisttab.h"

// log categories. Should match log.h

Q_LOGGING_CATEGORY(logCam, "kaffeine.cam")
Q_LOGGING_CATEGORY(logDev, "kaffeine.dev")
Q_LOGGING_CATEGORY(logDvb, "kaffeine.dvb")
Q_LOGGING_CATEGORY(logDvbSi, "kaffeine.dvbsi")
Q_LOGGING_CATEGORY(logEpg, "kaffeine.epg")

Q_LOGGING_CATEGORY(logConfig, "kaffeine.config")
Q_LOGGING_CATEGORY(logMediaWidget, "kaffeine.mediawidget")
Q_LOGGING_CATEGORY(logPlaylist, "kaffeine.playlist")
Q_LOGGING_CATEGORY(logSql, "kaffeine.sql")
Q_LOGGING_CATEGORY(logVlc, "kaffeine.vlc")

#define FILTER_RULE "kaffeine.*.debug=true"

#define CATEGORIES "cam, dev, dvb, dvbsi, epg, config, mediawidget, playlist, sql, vlc"

class StackedLayout : public QStackedLayout
{
public:
	explicit StackedLayout(QWidget *parent) : QStackedLayout(parent) { }
	~StackedLayout() { }

	QSize minimumSize() const
	{
		QWidget *widget = currentWidget();

		if (widget != NULL) {
			return widget->minimumSizeHint();
		}

		return QSize();
	}
};

class StartTab : public TabBase
{
public:
	explicit StartTab(MainWindow *mainWindow);
	~StartTab() { }

private:
	void activate() { }

	QAbstractButton *addShortcut(const QString &name, const QIcon &icon, QWidget *parent);
};

StartTab::StartTab(MainWindow *mainWindow)
{
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);

	QGridLayout *gridLayout = new QGridLayout(this);
	gridLayout->setAlignment(Qt::AlignCenter);
	gridLayout->setMargin(10);
	gridLayout->setSpacing(15);

	QAbstractButton *button =
		addShortcut(i18n("&1 Play File"), QIcon::fromTheme(QLatin1String("video-x-generic"), QIcon(":video-x-generic")), this);
	button->setShortcut(Qt::Key_1);
	button->setWhatsThis(i18n("Open dialog to play a file"));
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(open()));
	gridLayout->addWidget(button, 0, 0);

	button = addShortcut(i18n("&2 Play Audio CD"), QIcon::fromTheme(QLatin1String("media-optical-audio"), QIcon(":media-optical-audio")), this);
	button->setShortcut(Qt::Key_2);
	button->setWhatsThis(i18n("Start playing an audio CD. It assumes that the CD is already there at the CD drive"));
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(openAudioCd()));
	gridLayout->addWidget(button, 0, 1);

	button = addShortcut(i18n("&3 Play Video CD"), QIcon::fromTheme(QLatin1String("media-optical"), QIcon(":media-optical-video")), this);
	button->setShortcut(Qt::Key_3);
	button->setWhatsThis(i18n("Start playing a Video CD. It assumes that the CD is already there at the CD drive"));
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(openVideoCd()));
	gridLayout->addWidget(button, 0, 2);

	button = addShortcut(i18n("&4 Play DVD"), QIcon::fromTheme(QLatin1String("media-optical"), QIcon(":media-optical")), this);
	button->setShortcut(Qt::Key_4);
	button->setWhatsThis(i18n("Start playing a DVD. It assumes that the DVD is already there at the DVD drive"));
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(openDvd()));
	gridLayout->addWidget(button, 1, 0);

#if HAVE_DVB == 1
	button = addShortcut(i18n("&5 Digital TV"), QIcon::fromTheme(QLatin1String("video-television"), QIcon(":video-television")), this);
	button->setShortcut(Qt::Key_5);
	button->setWhatsThis("Open the Digital TV live view window. If the TV channels are already scanned, it will start playing the last channel");
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(playDvb()));
	gridLayout->addWidget(button, 1, 1);
#endif /* HAVE_DVB == 1 */
}

QAbstractButton *StartTab::addShortcut(const QString &name, const QIcon &icon, QWidget *parent)
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
		mediaWidget->setFocus();
	}

private:
	MediaWidget *mediaWidget;
};

PlayerTab::PlayerTab(MediaWidget *mediaWidget_) : mediaWidget(mediaWidget_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
}

void MainWindow::run()
{
	// Allow the user to enable or disable debugging
	// We handle this before the other parameters, as it may affect some
	// early debug messages
	//
	// --debug enables all debugging categories. It is possible to enable
	// each category individually by calling Kaffeine with:
	//	QT_LOGGING_RULES="epg.debug=true" kaffeine

	if (parser->isSet("debug")) {
		QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);
		QLoggingCategory::setFilterRules(QStringLiteral(FILTER_RULE));
	} else {
		QLoggingCategory::setFilterRules(QStringLiteral("kaffeine.*.debug=false"));
	}

	readSettings();

	setAttribute(Qt::WA_DeleteOnClose, true);

	// menu structure

	QMenuBar *menuBar = QMainWindow::menuBar();
	collection = new KActionCollection(this);

	QMenu *menu = new QMenu(i18n("&File"), this);
	menuBar->addMenu(menu);

	QAction *action = KStandardAction::open(this, SLOT(open()), collection);
	menu->addAction(collection->addAction(QLatin1String("file_open"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("text-html"), QIcon(":text-html")),
		i18nc("@action:inmenu", "Open URL..."), collection);
	action->setShortcut(Qt::CTRL | Qt::Key_U);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openUrl()));
	menu->addAction(collection->addAction(QLatin1String("file_open_url"), action));

	actionOpenRecent = KStandardAction::openRecent(this, SLOT(openUrl(QUrl)), collection);
	actionOpenRecent->loadEntries(KSharedConfig::openConfig()->group("Recent Files"));
	menu->addAction(collection->addAction(QLatin1String("file_open_recent"), actionOpenRecent));

	menu->addSeparator();

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical-audio"), QIcon(":media-optical-audio")), i18n("Play Audio CD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openAudioCd()));
	menu->addAction(collection->addAction(QLatin1String("file_play_audiocd"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical"), QIcon(":media-optical-video")), i18n("Play Video CD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openVideoCd()));
	menu->addAction(collection->addAction(QLatin1String("file_play_videocd"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical"), QIcon(":media-optical")), i18n("Play DVD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openDvd()));
	menu->addAction(collection->addAction(QLatin1String("file_play_dvd"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical"), QIcon(":media-optical")), i18nc("@action:inmenu", "Play DVD Folder"),
		collection);
	connect(action, SIGNAL(triggered()), this, SLOT(playDvdFolder()));
	menu->addAction(collection->addAction(QLatin1String("file_play_dvd_folder"), action));

	menu->addSeparator();

	action = KStandardAction::quit(this, SLOT(close()), collection);
	menu->addAction(collection->addAction(QLatin1String("file_quit"), action));

	QMenu *playerMenu = new QMenu(i18n("&Playback"), this);
	menuBar->addMenu(playerMenu);

	QMenu *playlistMenu = new QMenu(i18nc("menu bar", "Play&list"), this);
	menuBar->addMenu(playlistMenu);

#if HAVE_DVB == 1
	QMenu *dvbMenu = new QMenu(i18n("&Television"), this);
	menuBar->addMenu(dvbMenu);
#endif /* HAVE_DVB == 1 */

	menu = new QMenu(i18n("&Settings"), this);
	menuBar->addMenu(menu);

	action = KStandardAction::keyBindings(this, SLOT(configureKeys()), collection);
	menu->addAction(collection->addAction(QLatin1String("settings_keys"), action));

	action = KStandardAction::preferences(this, SLOT(configureKaffeine()), collection);
	menu->addAction(collection->addAction(QLatin1String("settings_kaffeine"), action));

	menuBar->addSeparator();
	KHelpMenu *helpMenu = new KHelpMenu(this, *aboutData);
	menuBar->addMenu(helpMenu->menu());

	// navigation bar - keep in sync with TabIndex enum!

	navigationBar = new QToolBar(QLatin1String("navigation_bar"));
	this->addToolBar(Qt::LeftToolBarArea, navigationBar);
	connect(navigationBar, SIGNAL(orientationChanged(Qt::Orientation)),
		this, SLOT(navigationBarOrientationChanged(Qt::Orientation)));

	tabBar = new QTabBar(navigationBar);
	tabBar->addTab(QIcon::fromTheme(QLatin1String("start-here-kde"), QIcon(":start-here-kde")), i18n("Start"));
	tabBar->addTab(QIcon::fromTheme(QLatin1String("kaffeine"), QIcon(":kaffeine")), i18n("Playback"));
	tabBar->addTab(QIcon::fromTheme(QLatin1String("view-media-playlist"), QIcon(":view-media-playlist")), i18n("Playlist"));
#if HAVE_DVB == 1
	tabBar->addTab(QIcon::fromTheme(QLatin1String("video-television"), QIcon(":video-television")), i18n("Television"));
#endif /* HAVE_DVB == 1 */
	tabBar->setShape(QTabBar::RoundedWest);
	tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	connect(tabBar, SIGNAL(currentChanged(int)), this, SLOT(activateTab(int)));
	navigationBar->addWidget(tabBar);

	// control bar

	controlBar = new QToolBar(QLatin1String("control_bar"), this);
	this->addToolBar(Qt::BottomToolBarArea, controlBar);

	controlBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	autoHideControlBar = false;
	cursorHideTimer = new QTimer(this);
	cursorHideTimer->setInterval(1500);
	cursorHideTimer->setSingleShot(true);
	connect(cursorHideTimer, SIGNAL(timeout()), this, SLOT(hideCursor()));

	// main area

	QWidget *widget = new QWidget(this);
	stackedLayout = new StackedLayout(widget);
	setCentralWidget(widget);

	mediaWidget = new MediaWidget(playerMenu, controlBar, collection, widget);
	connect(mediaWidget, SIGNAL(displayModeChanged()), this, SLOT(displayModeChanged()));
	connect(mediaWidget, SIGNAL(changeCaption(QString)), this, SLOT(setWindowTitle(QString)));

	// tabs - keep in sync with TabIndex enum!

	TabBase *startTab = new StartTab(this);
	tabs.append(startTab);
	stackedLayout->addWidget(startTab);

	playerTab = new PlayerTab(mediaWidget);
	playerTab->activate();
	tabs.append(playerTab);
	stackedLayout->addWidget(playerTab);

	playlistTab = new PlaylistTab(playlistMenu, collection, mediaWidget);
	tabs.append(playlistTab);
	stackedLayout->addWidget(playlistTab);

#if HAVE_DVB == 1
	dvbTab = new DvbTab(dvbMenu, collection, mediaWidget);
	connect(this, SIGNAL(mayCloseApplication(bool*,QWidget*)),
		dvbTab, SLOT(mayCloseApplication(bool*,QWidget*)));
	tabs.append(dvbTab);
	stackedLayout->addWidget(dvbTab);
#endif /* HAVE_DVB == 1 */

	currentTabIndex = StartTabId;

	// actions also have to work if the menu bar is hidden (fullscreen)
	collection->addAssociatedWidget(this);

	// restore custom key bindings
	collection->readSettings();

	// Tray menu
	menu = new QMenu(i18n("Kaffeine"), this);

	action = new QAction(i18n("Play &File"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(open()));
	menu->addAction(action);

	action = new QAction(i18n("Play &Audio CD"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openAudioCd()));
	menu->addAction(action);

	action = new QAction(i18n("Play &Video CD"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openVideoCd()));
	menu->addAction(action);

	action = new QAction(i18n("Play &DVD"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openDvd()));
	menu->addAction(action);

#if HAVE_DVB == 1
	action = new QAction(i18n("&Watch Digital TV"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(playDvb()));
	menu->addAction(action);
#endif
	action = new QAction(i18n("&Quit"), this);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(close()));
	menu->addAction(action);

	// Tray Icon and its menu
	QMenu *trayMenu = new QMenu(this);
	trayIcon = new QSystemTrayIcon(this);
	trayIcon->setContextMenu(trayMenu);
	trayIcon->setIcon(QIcon::fromTheme(QLatin1String("kaffeine"), QIcon(":kaffeine")));
	trayIcon->setToolTip(i18n("Kaffeine"));
	trayIcon->setContextMenu(menu);
	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayShowHide(QSystemTrayIcon::ActivationReason)) );

	// make sure that the bars are visible (fullscreen -> quit -> restore -> hidden)
	menuBar->show();
	navigationBar->show();
	controlBar->show();
	trayIcon->show();

	// workaround setAutoSaveSettings() which doesn't accept "IconOnly" as initial state
	controlBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	// initialize random number generator
	qsrand(QTime(0, 0, 0).msecsTo(QTime::currentTime()));

	// initialize dbus objects
	QDBusConnection::sessionBus().registerObject(QLatin1String("/"), new MprisRootObject(this),
		QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerObject(QLatin1String("/Player"),
		new MprisPlayerObject(this, mediaWidget, playlistTab, this),
		QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerObject(QLatin1String("/TrackList"),
		new MprisTrackListObject(playlistTab, this), QDBusConnection::ExportAllContents);
#if HAVE_DVB == 1
	QDBusConnection::sessionBus().registerObject(QLatin1String("/Television"),
		new DBusTelevisionObject(dvbTab, this), QDBusConnection::ExportAllContents);
#endif /* HAVE_DVB == 1 */
	QDBusConnection::sessionBus().registerService(QLatin1String("org.mpris.kaffeine"));

	parseArgs();
	show();
}

MainWindow::~MainWindow()
{
	actionOpenRecent->saveEntries(KSharedConfig::openConfig()->group("Recent Files"));

	if (!temporaryUrls.isEmpty()) {
		KIO::del(temporaryUrls);
	}

	int value = 0;

	switch (mediaWidget->getDisplayMode()) {
	case MediaWidget::NormalMode: value = 0; break;
	case MediaWidget::MinimalMode: value = 1; break;
	case MediaWidget::FullScreenMode: value = 2; break;
	case MediaWidget::FullScreenReturnToMinimalMode: value = 2; break;
	}

	KSharedConfig::openConfig()->group("MainWindow").writeEntry("DisplayMode", value);
}

void MainWindow::close()
{
	bool ok = true;

#if HAVE_DVB == 1
	dvbTab->mayCloseApplication(&ok, mediaWidget);
#endif /* HAVE_DVB == 1 */

	if (ok) {
		writeSettings();
		mediaWidget->stop();
		QMainWindow::close();
		Configuration::detach();
		QCoreApplication::exit(0);
	}
}

void MainWindow::readSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }
}

void MainWindow::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
}

MainWindow::MainWindow(KAboutData *aboutData, QCommandLineParser *parser)
{
	this->aboutData = aboutData;
	this->parser = parser;

	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("d") << QLatin1String("debug"), i18n("Enable all debug messages. Please notice that Kaffeine also allows enabling debug messages per category, by using the environment var:\nQT_LOGGING_RULES=kaffeine.category.debug=true\nwhere 'category' can be:\n" CATEGORIES)));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("tempfile"), i18n("The files/URLs opened by the application will be deleted after use")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("f") << QLatin1String("fullscreen"), i18n("Start in full screen mode")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("m") << QLatin1String("minimal"), i18n("Start in minimal mode")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("t") << QLatin1String("alwaysontop"), i18n("Start with always on top")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("audiocd"), i18n("Play Audio CD")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("videocd"), i18n("Play Video CD")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("dvd"), i18n("Play DVD")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("aspectratio"), "Force starting with an specific aspect ratio", QLatin1String("aspect ratio")));

#if HAVE_DVB == 1
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("dumpdvb"), i18nc("command line option", "Dump dvb data (debug option)")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("channel"), i18nc("command line option", "Play TV channel"), QLatin1String("name / number")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("tv"), i18nc("command line option", "(deprecated option)"), QLatin1String("channel")));
	parser->addOption(QCommandLineOption(QStringList() << QLatin1String("lastchannel"), i18nc("command line option", "Play last tuned TV channel")));
#endif
	parser->addPositionalArgument(QLatin1String("[file]"), i18n("Files or URLs to play"));
}

void MainWindow::parseArgs(const QString workingDirectory)
{
	/* Parse first arguments that aren't mutually exclusive */

	if (parser->isSet("alwaysontop")) {
		Qt::WindowFlags flags = this->windowFlags();
		flags |= Qt::WindowStaysOnTopHint;
		this->setWindowFlags(flags);
	}

	if (parser->isSet("fullscreen")) {
		mediaWidget->setDisplayMode(MediaWidget::FullScreenMode);
	} else if (parser->isSet("minimal")) {
		mediaWidget->setDisplayMode(MediaWidget::MinimalMode);
	} else {
		// set display mode
		switch (Configuration::instance()->getStartupDisplayMode()) {
		case Configuration::StartupNormalMode:
			// nothing to do
			break;
		case Configuration::StartupMinimalMode:
			mediaWidget->setDisplayMode(MediaWidget::MinimalMode);
			break;
		case Configuration::StartupFullScreenMode:
			mediaWidget->setDisplayMode(MediaWidget::FullScreenMode);
			break;
		case Configuration::StartupRememberLastSetting: {
			int value = KSharedConfig::openConfig()->group("MainWindow").readEntry("DisplayMode", 0);

			switch (value) {
			case 0:
				// nothing to do
				break;
			case 1:
				mediaWidget->setDisplayMode(MediaWidget::MinimalMode);
				break;
			case 2:
				mediaWidget->setDisplayMode(MediaWidget::FullScreenMode);
				break;
			}

			break;
		} /* case */
		} /* switch */
	}

	if (parser->isSet("aspectratio")) {
		MediaWidget::AspectRatio aspectRatio = MediaWidget::AspectRatioAuto;
		QString aspect;

		aspect = parser->value("aspectratio");

		if (aspect == "1:1")
			aspectRatio = MediaWidget::AspectRatio1_1;
		else if (aspect == "4:3")
			aspectRatio = MediaWidget::AspectRatio4_3;
		else if (aspect == "5:4")
			aspectRatio = MediaWidget::AspectRatio5_4;
		else if (aspect == "16:9")
			aspectRatio = MediaWidget::AspectRatio16_9;
		else if (aspect == "16:10")
			aspectRatio = MediaWidget::AspectRatio16_10;
		else if (aspect == "2.21:1")
			aspectRatio = MediaWidget::AspectRatio221_100;
		else if (aspect == "2.35:1")
			aspectRatio = MediaWidget::AspectRatio235_100;
		else if (aspect == "2.39:1")
			aspectRatio = MediaWidget::AspectRatio239_100;

		mediaWidget->setAspectRatio(aspectRatio);
	}


#if HAVE_DVB == 1
	if (parser->isSet("dumpdvb")) {
		dvbTab->enableDvbDump();
	}
#endif

	/*
	 * Now, parse arguments that are mutually exclusive
	 */

#if HAVE_DVB == 1
	QString channel;

	channel = parser->value("channel");

	if (!channel.isEmpty()) {
		activateTab(DvbTabId);
		dvbTab->playChannel(channel);

		return;
	}

	channel = parser->value("tv");
	if (!channel.isEmpty()) {
		activateTab(DvbTabId);
		dvbTab->playChannel(channel);

		return;
	}

	if (parser->isSet("lastchannel")) {
		activateTab(DvbTabId);
		dvbTab->playLastChannel();

		return;
	}
#endif /* HAVE_DVB == 1 */

	if (parser->isSet("audiocd")) {
		if (parser->positionalArguments().count() > 0) {
			openAudioCd(parser->positionalArguments().first());
		} else {
			openAudioCd();
		}
		return;
	}

	if (parser->isSet("videocd")) {
		if (parser->positionalArguments().count() > 0) {
			openVideoCd(parser->positionalArguments().first());
		} else {
			openVideoCd();
		}
		return;
	}

	if (parser->isSet("dvd")) {
		if (parser->positionalArguments().count() > 0) {
			openDvd(parser->positionalArguments().first());
		} else {
			openDvd();
		}
		return;
	}

	if (parser->positionalArguments().count() > 0) {
		QList<QUrl> urls;

		for (int i = 0; i < parser->positionalArguments().count(); ++i) {
			QUrl url = QUrl::fromUserInput(parser->positionalArguments().at(i), workingDirectory);

			if (url.isValid()) {
				urls.append(url);
			}
		}

		if (parser->isSet("tempfile")) {
			temporaryUrls.append(urls);
		}

		if (urls.size() >= 2) {
			activateTab(PlaylistTabId);
			playlistTab->appendToVisiblePlaylist(urls, true);
		} else if (!urls.isEmpty()) {
			openUrl(urls.at(0));
		}
		return;
	}
}

void MainWindow::displayModeChanged()
{
	MediaWidget::DisplayMode displayMode = mediaWidget->getDisplayMode();

	switch (displayMode) {
	case MediaWidget::FullScreenMode:
	case MediaWidget::FullScreenReturnToMinimalMode:
		setWindowState(windowState() | Qt::WindowFullScreen);
		break;
	case MediaWidget::MinimalMode:
	case MediaWidget::NormalMode:
		setWindowState(windowState() & (~Qt::WindowFullScreen));
		break;
	}

	switch (displayMode) {
	case MediaWidget::FullScreenMode:
	case MediaWidget::FullScreenReturnToMinimalMode:
	case MediaWidget::MinimalMode:
		menuBar()->hide();
		navigationBar->hide();
		controlBar->hide();
		autoHideControlBar = true;
		cursorHideTimer->start();
		break;
	case MediaWidget::NormalMode:
		menuBar()->show();
		navigationBar->show();
		controlBar->show();
		autoHideControlBar = false;
		cursorHideTimer->stop();
		unsetCursor();
		break;
	}
	tabs.at(currentTabIndex)->toggleDisplayMode(displayMode);
}

void MainWindow::trayShowHide(QSystemTrayIcon::ActivationReason reason)
{
	if (reason != QSystemTrayIcon::DoubleClick)
		return;

	if (isVisible())
		hide();
	else {
		show();
		raise();
		setFocus();
	}
}

void MainWindow::open()
{
	if (isMinimized())
		showNormal();

	QList<QUrl> urls = QFileDialog::getOpenFileUrls(this, i18nc("@title:window", "Open files"), QUrl(), MediaWidget::extensionFilter());

//	trayIcon->showMessage("Open", "Opening file(s)");
	if (urls.size() >= 2) {
		activateTab(PlaylistTabId);
		playlistTab->appendToVisiblePlaylist(urls, true);
	} else if (!urls.isEmpty()) {
		openUrl(urls.at(0));
	}
}

void MainWindow::openUrl()
{
	openUrl(QInputDialog::getText(this, i18nc("@title:window", "Open URL"), i18n("Enter a URL:")));
}

void MainWindow::openUrl(const QUrl &url)
{
	if (!url.isValid()) {
		return;
	}

	// we need to copy "url" because addUrl() may invalidate it
	QUrl copy(url);
	actionOpenRecent->addUrl(copy); // moves the url to the top of the list

	if (currentTabIndex != PlaylistTabId) {
		activateTab(PlayerTabId);
	}

	playlistTab->appendToVisiblePlaylist(QList<QUrl>() << copy, true);
}

void MainWindow::openAudioCd(const QString &device)
{
	if (isMinimized())
		showNormal();

	activateTab(PlayerTabId);
	mediaWidget->playAudioCd(device);
}

void MainWindow::openVideoCd(const QString &device)
{
	if (isMinimized())
		showNormal();

	activateTab(PlayerTabId);
	mediaWidget->playVideoCd(device);
}

void MainWindow::openDvd(const QString &device)
{
	if (isMinimized())
		showNormal();

	activateTab(PlayerTabId);
	mediaWidget->playDvd(device);
}

void MainWindow::playDvdFolder()
{
	QString folder = QFileDialog::getExistingDirectory(this, QString());

	if (!folder.isEmpty()) {
		openDvd(folder);
	}
}

void MainWindow::playDvb()
{
	if (isMinimized())
		showNormal();

	activateTab(DvbTabId);
	dvbTab->playLastChannel();
}

void MainWindow::configureKeys()
{
	KShortcutsDialog::configure(collection);
}

void MainWindow::configureKaffeine()
{
	QDialog *dialog = new ConfigurationDialog(this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void MainWindow::navigationBarOrientationChanged(Qt::Orientation orientation)
{
	if (orientation == Qt::Horizontal) {
		tabBar->setShape(QTabBar::RoundedNorth);
	} else {
		tabBar->setShape(QTabBar::RoundedWest);
	}
}

void MainWindow::activateTab(int tabIndex)
{
	currentTabIndex = tabIndex;
	tabBar->setCurrentIndex(tabIndex);

	/*
	 * Ensure that menus and timers will be properly shown,
	 * according with the selected display mode.
	 */
	stackedLayout->setCurrentIndex(currentTabIndex);
	tabs.at(currentTabIndex)->activate();

	emit displayModeChanged();
}

void MainWindow::hideCursor()
{
	setCursor(Qt::BlankCursor);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	bool ok = true;
	emit mayCloseApplication(&ok, this);

	if (!ok) {
		event->ignore();
	} else {
		MainWindow::close();
	}
}

bool MainWindow::event(QEvent *event)
{
	bool retVal = QMainWindow::event(event); // this has to be done before calling setVisible()

	// FIXME we depend on QEvent::HoverMove (instead of QEvent::MouseMove)
	// but the latter depends on mouse tracking being enabled on this widget
	// and all its children (especially the video widget) ...

	switch (event->type()) {
	case QEvent::HoverMove: {
		int x = reinterpret_cast<QHoverEvent *> (event)->pos().x();
		int y = reinterpret_cast<QHoverEvent *> (event)->pos().y();

		if ((y < 0) || (y >= height()) ||
		    (x < 0) || (x >= width())) {
			// QHoverEvent sometimes reports quite strange coordinates - ignore them
			return retVal;
		}

		if (autoHideControlBar) {
			cursorHideTimer->stop();
			unsetCursor();

			switch (toolBarArea(controlBar)) {
			case Qt::TopToolBarArea:
				controlBar->setVisible(y < 60);
				break;
			case Qt::BottomToolBarArea:
				controlBar->setVisible(y >= (height() - 60));
				menuBar()->setVisible(y < 60);
				break;
			default:
				break;
			}

			if (controlBar->isHidden() || menuBar()->isHidden()) {
				cursorHideTimer->start();
			}
		}

		tabs.at(currentTabIndex)->mouse_move(x, y);
		break;
	}
	default:
		break;
	}

	return retVal;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		mediaWidget->setDisplayMode(MediaWidget::NormalMode);
	}

	QMainWindow::keyPressEvent(event);
}

void MainWindow::leaveEvent(QEvent *event)
{
	if (autoHideControlBar) {
		menuBar()->setVisible(false);
		controlBar->setVisible(false);
		tabs.at(currentTabIndex)->mouse_move(-1, -1);
	}

	QMainWindow::leaveEvent(event);
}
