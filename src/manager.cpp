/*
 * manager.cpp
 *
 * Copyright (C) 2007 Christoph Pfister <christophpfister@gmail.com>
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

#include <QButtonGroup>
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStackedLayout>
#include <QToolBar>

#include <KAction>
#include <KActionCollection>
#include <KLocalizedString>
#include <KRecentFilesAction>

#include "dvb/dvbtab.h"
#include "kaffeine.h"
#include "mediawidget.h"
#include "manager.h"
#include "manager.moc"

class StartTab : public TabBase
{
public:
	StartTab(Manager *manager_);
	~StartTab() { }

private:
	void activate() { }

	QAbstractButton *addShortcut(const QString &name, const KIcon &icon,
		QWidget *parent);
};

StartTab::StartTab(Manager *manager_) : TabBase(manager_)
{
	Kaffeine *kaffeine = manager->getKaffeine();

	QVBoxLayout *boxLayout = new QVBoxLayout(this);
	boxLayout->setMargin(0);
	boxLayout->setSpacing(0);

	QLabel *label = new QLabel(i18n("<font size=\"+4\"><b>[Kaffeine Player]</b><br>caffeine for your desktop!</font>"));
	label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
	label->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum));
	QPalette pal = label->palette();
	pal.setColor(label->backgroundRole(), QColor(127, 127, 255));
	label->setPalette(pal);
	label->setAutoFillBackground(true);
	boxLayout->addWidget(label);

	QWidget *widget = new QWidget(this);
	pal = widget->palette();
	pal.setColor(widget->backgroundRole(), QColor(255, 255, 255));
	widget->setPalette(pal);
	widget->setAutoFillBackground(true);
	boxLayout->addWidget(widget);

	boxLayout = new QVBoxLayout(widget);
	boxLayout->setMargin(0);
	boxLayout->setSpacing(0);

	widget = new QWidget(widget);
	boxLayout->addWidget(widget, 0, Qt::AlignCenter);

	QGridLayout *gridLayout = new QGridLayout(widget);
	gridLayout->setMargin(15);
	gridLayout->setSpacing(15);

	QAbstractButton *button = addShortcut(i18n("Play File"), KIcon("video"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(actionOpen()));
	gridLayout->addWidget(button, 0, 0);

	button = addShortcut(i18n("Play Audio CD"), KIcon("media-optical-audio"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(actionOpenAudioCd()));
	gridLayout->addWidget(button, 0, 1);

	button = addShortcut(i18n("Play Video CD"), KIcon("media-optical"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(actionOpenVideoCd()));
	gridLayout->addWidget(button, 0, 2);

	button = addShortcut(i18n("Play DVD"), KIcon("media-optical"), widget);
	connect(button, SIGNAL(clicked()), kaffeine, SLOT(actionOpenDvd()));
	gridLayout->addWidget(button, 1, 0);

	button = addShortcut(i18n("Digital TV"), KIcon("video-television"), widget);
	gridLayout->addWidget(button, 1, 1);
}

QAbstractButton *StartTab::addShortcut(const QString &name, const KIcon &icon,
	QWidget *parent)
{
	QPushButton *button = new QPushButton(parent);
	button->setText(name);
	button->setIcon(icon);
	button->setIconSize(QSize(48, 48));
	button->setFocusPolicy(Qt::NoFocus);
	button->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
	return button;
}

class PlayerTab : public TabBase
{
public:
	PlayerTab(Manager *manager_);
	~PlayerTab() { }

private:
	void activate()
	{
		layout()->addWidget(manager->getMediaWidget());
	}
};

PlayerTab::PlayerTab(Manager *manager_) : TabBase(manager_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
}

Manager::Manager(Kaffeine *kaffeine_) : QWidget(kaffeine_), currentState(~stateAlways),
	kaffeine(kaffeine_), ignoreActivate(false)
{
	mediaWidget = new MediaWidget(this);

	stackedLayout = new QStackedLayout(this);
	buttonGroup = new QButtonGroup(this);

	startTab = createTab(i18n("Start"), new StartTab(this));
	playerTab = createTab(i18n("Player"), new PlayerTab(this));
	dvbTab = createTab(i18n("Digital TV"), new DvbTab(this));

	KActionCollection *collection = kaffeine->actionCollection();

	KAction *action = KStandardAction::open(kaffeine, SLOT(actionOpen()), collection);
	addAction(collection, "file_open", stateAlways, action);

	action = new KAction(KIcon("uri-mms"), i18n("Open URL"), collection);
	QObject::connect(action, SIGNAL(triggered(bool)), kaffeine, SLOT(actionOpenUrl()));
	action->setShortcut(Qt::Key_U | Qt::CTRL);
	addAction(collection, "file_open_url", stateAlways, action);

	actionOpenRecent = KStandardAction::openRecent(kaffeine, SLOT(actionOpenRecent(KUrl)), collection);
	addAction(collection, "file_open_recent", stateAlways, actionOpenRecent);
	actionOpenRecent->loadEntries(KConfigGroup(KGlobal::config(), "Recent Files"));

	action = new KAction(KIcon("media-optical-audio"), i18n("Play Audio CD"), collection);
	QObject::connect(action, SIGNAL(triggered(bool)), kaffeine, SLOT(actionOpenAudioCd()));
	addAction(collection, "file_play_audiocd", stateAlways, action);

	action = new KAction(KIcon("media-optical"), i18n("Play Video CD"), collection);
	QObject::connect(action, SIGNAL(triggered(bool)), kaffeine, SLOT(actionOpenVideoCd()));
	addAction(collection, "file_play_videocd", stateAlways, action);

	action = new KAction(KIcon("media-optical"), i18n("Play DVD"), collection);
	QObject::connect(action, SIGNAL(triggered(bool)), kaffeine, SLOT(actionOpenDvd()));
	addAction(collection, "file_play_dvd", stateAlways, action);

	action = KStandardAction::quit(kaffeine, SLOT(close()), collection);
	addAction(collection, "file_quit", stateAlways, action);

	action = new KAction(KIcon("view-fullscreen"), i18n("Full Screen Mode"), collection);
	// FIXME need to translate shortcuts?
	action->setShortcut(KShortcut(i18nc("View|Full Screen Mode", "F")));
	QObject::connect(action, SIGNAL(triggered(bool)), kaffeine, SLOT(actionFullscreen()));
	addAction(collection, "view_fullscreen", stateAlways, action);
	// FIXME right way to achieve correct fullscreen behaviour?
	mediaWidget->addAction(action);

	action = new KAction(KIcon("media-skip-backward"), i18n("Previous"), collection);
	addAction(collection, "controls_previous", statePrevNext | statePlaying, action);

	actionPlayPause = new KAction(collection);
	QObject::connect(actionPlayPause, SIGNAL(triggered(bool)), kaffeine, SLOT(actionPlayPause(bool)));
	actionPlayPause->setShortcut(Qt::Key_Space);
	textPlay = i18n("Play");
	textPause = i18n("Pause");
	iconPlay = KIcon("media-playback-start");
	iconPause = KIcon("media-playback-pause");
	addAction(collection, "controls_play_pause", stateAlways, actionPlayPause);

	action = new KAction(KIcon("media-playback-stop"), i18n("Stop"), collection);
	QObject::connect(action, SIGNAL(triggered(bool)), mediaWidget, SLOT(stop()));
	addAction(collection, "controls_stop", statePlaying, action);

	action = new KAction(KIcon("media-skip-forward"), i18n("Next"), collection);
	addAction(collection, "controls_next", statePrevNext, action);

	action = new KAction(collection);
	action->setDefaultWidget(mediaWidget->newVolumeSlider());
	addAction(collection, "controls_volume", stateAlways, action);

	action = new KAction(collection);
	widgetPositionSlider = mediaWidget->newPositionSlider();
	action->setDefaultWidget(widgetPositionSlider);
	addAction(collection, "position_slider", stateAlways, action);

	action = new KAction(KIcon("configure"), i18n("Configure DVB"), collection);
	connect(action, SIGNAL(triggered(bool)), dvbTab, SLOT(configureDvb()));
	addAction(collection, "settings_dvb", stateAlways, action);

	action = new KAction(collection);
	action->setDefaultWidget(startTab->button);
	addAction(collection, "tabs_start", stateAlways, action);

	action = new KAction(collection);
	action->setDefaultWidget(playerTab->button);
	addAction(collection, "tabs_player", stateAlways, action);

	action = new KAction(collection);
	action->setDefaultWidget(dvbTab->button);
	addAction(collection, "tabs_dvb", stateAlways, action);

	activate(startTab);
	setState(stateAlways);
}

Manager::~Manager()
{
	actionOpenRecent->saveEntries(KConfigGroup(KGlobal::config(), "Recent Files"));
}

void Manager::addRecentUrl(const KUrl &url)
{
	actionOpenRecent->addUrl(url);
}

void Manager::activate(tabs tab)
{
	TabBase *tabBase = NULL;

	switch(tab) {
	case tabStart:
		tabBase = startTab;
		break;
	case tabPlayer:
		tabBase = playerTab;
		break;
	case tabPlaylist:
		// FIXME
		tabBase = playerTab;
		break;
	case tabAudioCd:
		// FIXME
		tabBase = playerTab;
		break;
	case tabDvb:
		tabBase = dvbTab;
		break;
	}

	activate(tabBase);
}

void Manager::activate(TabBase *tab)
{
	if (ignoreActivate) {
		return;
	}

	ignoreActivate = true;
	tab->button->click();
	ignoreActivate = false;

	tab->activate();
	stackedLayout->setCurrentWidget(tab);
}

void Manager::addAction(KActionCollection *collection, const QString &name,
	Manager::stateFlags flags, KAction *action)
{
	collection->addAction(name, action);
	if (flags != stateAlways)
		actionList.append(qMakePair(flags, action));
}

void Manager::setState(stateFlags newState)
{
	if (currentState == newState)
		return;

	if (((currentState ^ newState) & statePlaying) == statePlaying) {
		if ((newState & statePlaying) == statePlaying) {
			actionPlayPause->setText(textPause);
			actionPlayPause->setIcon(iconPause);
			actionPlayPause->setCheckable(true);
			widgetPositionSlider->setEnabled(true);
		} else {
			actionPlayPause->setText(textPlay);
			actionPlayPause->setIcon(iconPlay);
			actionPlayPause->setCheckable(false);
			widgetPositionSlider->setEnabled(false);
		}
	}

	if ((currentState ^ newState) & stateFullscreen) {
		bool fullscreen = ((newState & stateFullscreen) == stateFullscreen);
		mediaWidget->setFullscreen(fullscreen);
		if (!fullscreen) {
			activate(dynamic_cast<TabBase *>(stackedLayout->currentWidget()));
		}
	}

	QPair<stateFlags, KAction *> action;
	foreach (action, actionList)
		action.second->setEnabled((action.first & newState) != stateAlways);

	currentState = newState;
}

TabBase *Manager::createTab(const QString &name, TabBase *tab)
{
	TabButton *tabButton = new TabButton(name);
	connect(tabButton, SIGNAL(clicked(bool)), tab, SLOT(clicked()));
	buttonGroup->addButton(tabButton);
	stackedLayout->addWidget(tab);

	tab->button = tabButton;
	return tab;
}

TabButton::TabButton(const QString &name)
{
	setCheckable(true);
	setFocusPolicy(Qt::NoFocus);

	QSize size = fontMetrics().size(Qt::TextShowMnemonic, name);

	horizontal = new QPixmap(size);
	horizontal->fill(QColor(0, 0, 0, 0));

	{
		QPainter painter(horizontal);
		painter.setBrush(palette().text());
		painter.drawText(0, 0, size.width(), size.height(), Qt::TextShowMnemonic, name);
	}

	vertical = new QPixmap(size.height(), size.width());
	vertical->fill(QColor(0, 0, 0, 0));

	{
		QPainter painter(vertical);
		painter.rotate(270);
		painter.setBrush(palette().text());
		painter.drawText(-size.width(), 0, size.width(), size.height(), Qt::TextShowMnemonic, name);
	}

	orientationChanged(Qt::Horizontal);
}

TabButton::~TabButton()
{
	delete horizontal;
	delete vertical;
}

void TabButton::orientationChanged(Qt::Orientation orientation)
{
	if (orientation == Qt::Vertical) {
		setIcon(*vertical);
		setIconSize(vertical->size());
	} else {
		setIcon(*horizontal);
		setIconSize(horizontal->size());
	}
}

void TabButton::changeEvent(QEvent *event)
{
	switch(event->type()) {
	case QEvent::ParentChange: {
		QToolBar *toolBar = dynamic_cast<QToolBar *> (parent());
		if (toolBar)
			connect(toolBar, SIGNAL(orientationChanged(Qt::Orientation)), this, SLOT(orientationChanged(Qt::Orientation)));
		break;
		}
	case QEvent::ParentAboutToChange: {
		QToolBar *toolBar = dynamic_cast<QToolBar *> (parent());
		if (toolBar)
			disconnect(toolBar, SIGNAL(orientationChanged(Qt::Orientation)), this, SLOT(orientationChanged(Qt::Orientation)));
		break;
		}
	default:
		break;
	}
}
