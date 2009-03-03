/*
 * kaffeine.h
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

#ifndef KAFFEINE_H
#define KAFFEINE_H

#include <KMainWindow>

class QStackedLayout;
class KActionCollection;
class KCmdLineOptions;
class KRecentFilesAction;
class KUrl;
class DvbTab;
class MediaWidget;
class PlayerTab;
class StartTab;
class TabBase;

class Kaffeine : public KMainWindow
{
	Q_OBJECT
public:
	Kaffeine();
	~Kaffeine();

	static KCmdLineOptions cmdLineOptions();
	void parseArgs();

private slots:
	void open();
	void openUrl();
	void openRecent(const KUrl &url);
	void openAudioCd();
	void openVideoCd();
	void openDvd();
	void toggleFullscreen();
	void configureKeys();

	void activateStartTab();
	void activatePlayerTab();
	void activateDvbTab();

	void hideCursor();

private:
	void activateTab(TabBase *tab);
	bool event(QEvent *event);

	KActionCollection *collection;
	KRecentFilesAction *actionOpenRecent;
	KToolBar *controlBar;
	QTimer *cursorHideTimer;

	MediaWidget *mediaWidget;
	QStackedLayout *stackedLayout;
	TabBase *currentTab;

	StartTab *startTab;
	PlayerTab *playerTab;
	DvbTab *dvbTab;
};

class TabBase : public QWidget
{
public:
	TabBase() { }
	~TabBase() { }

	virtual void activate() = 0;
};

#endif /* KAFFEINE_H */
