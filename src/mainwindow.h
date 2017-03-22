/*
 * mainwindow.h
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KMainWindow>
#include <QCommandLineParser>
#include <QSystemTrayIcon>
#include "mediawidget.h"

class KAboutData;
class QStackedLayout;
class KCmdLineOptions;
class KRecentFilesAction;
class QTabBar;
class DvbTab;
class PlayerTab;
class PlaylistTab;
class TabBase;

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow(KAboutData *aboutData, QCommandLineParser *parser);
	~MainWindow();

	void run();

signals:
	void mayCloseApplication(bool *ok, QWidget *parent);

private slots:
	void displayModeChanged();
	void open();
	void close();
	void openUrl();
	void openUrl(const QUrl &url);
	void openAudioCd(const QString &device = QString());
	void openVideoCd(const QString &device = QString());
	void openDvd(const QString &device = QString());
	void playDvdFolder();
	void playDvb();
	void configureKeys();
	void configureKaffeine();
	void navigationBarOrientationChanged(Qt::Orientation orientation);
	void activateTab(int tabIndex);
	void hideCursor();
	void trayShowHide(QSystemTrayIcon::ActivationReason reason);

private:
	enum TabIndex {
		StartTabId = 0,
		PlayerTabId = 1,
		PlaylistTabId = 2,
		DvbTabId = 3
	};

	void readSettings();
	void writeSettings();
	void closeEvent(QCloseEvent *event);
	bool event(QEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void leaveEvent(QEvent *event);

	KAboutData *aboutData;
	QCommandLineParser *parser;
	void parseArgs();

	QSystemTrayIcon *trayIcon;

	KActionCollection *collection;
	KRecentFilesAction *actionOpenRecent;
	QToolBar *navigationBar;
	QTabBar *tabBar;
	QToolBar *controlBar;
	bool autoHideControlBar;
	QTimer *cursorHideTimer;
	QList<QUrl> temporaryUrls;

	MediaWidget *mediaWidget;
	QStackedLayout *stackedLayout;
	QList<TabBase *> tabs;
	int currentTabIndex;

	PlayerTab *playerTab;
	PlaylistTab *playlistTab;
	DvbTab *dvbTab;
};

#endif /* MAINWINDOW_H */
