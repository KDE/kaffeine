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
#include "mediawidget.h"

class QStackedLayout;
class KCmdLineOptions;
class KRecentFilesAction;
class KTabBar;
class DvbTab;
class PlayerTab;
class PlaylistTab;
class TabBase;

class MainWindow : public KMainWindow
{
	Q_OBJECT
public:
	MainWindow();
	~MainWindow();

	static KCmdLineOptions cmdLineOptions();
	void parseArgs();

signals:
	void mayCloseApplication(bool *ok, QWidget *parent);

private slots:
	void displayModeChanged();
	void open();
	void openUrl();
	void openUrl(const KUrl &url);
	void openAudioCd(const QString &device = QString());
	void openVideoCd(const QString &device = QString());
	void openDvd(const QString &device = QString());
	void playDvdFolder();
	void playDvb();
	void resizeToVideo(MediaWidget::ResizeFactor resizeFactor);
	void configureKeys();
	void configureKaffeine();
	void navigationBarOrientationChanged(Qt::Orientation orientation);
	void activateTab(int tabIndex);
	void hideCursor();

private:
	enum TabIndex {
		StartTabId = 0,
		PlayerTabId = 1,
		PlaylistTabId = 2,
		DvbTabId = 3
	};

	void closeEvent(QCloseEvent *event);
	bool event(QEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void leaveEvent(QEvent *event);

	KActionCollection *collection;
	KRecentFilesAction *actionOpenRecent;
	KToolBar *navigationBar;
	KTabBar *tabBar;
	KToolBar *controlBar;
	bool autoHideControlBar;
	QTimer *cursorHideTimer;
	QList<KUrl> temporaryUrls;

	MediaWidget *mediaWidget;
	QStackedLayout *stackedLayout;
	QList<TabBase *> tabs;
	int currentTabIndex;

	PlayerTab *playerTab;
	PlaylistTab *playlistTab;
	DvbTab *dvbTab;
};

#endif /* MAINWINDOW_H */
