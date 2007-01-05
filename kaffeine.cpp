#include <KAction>
#include <KFileDialog>
#include <KLocalizedString>
#include <KStandardAction>
#include <KToolBar>

#include "kaffeine.h"
#include "kaffeine.moc"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	// FIXME
	KCmdLineLastOption
};

Kaffeine::Kaffeine()
{
	// FIXME
	setAttribute(Qt::WA_DeleteOnClose, false);

	// FIXME
	KStandardAction::open(this, SLOT(actionOpen()), actionCollection(), "file_open_x");
	KStandardAction::quit(this, SLOT(actionQuit()), actionCollection(), "file_quit_x");

	actionControlPrevious = new KAction(KIcon("player_start"), QString(), actionCollection(), "controls_previous");
	actionControlPlay = new KAction(KIcon("player_play"), QString(), actionCollection(), "controls_play");
	actionControlStop = new KAction(KIcon("player_stop"), QString(), actionCollection(), "controls_stop");
	actionControlNext = new KAction(KIcon("player_end"), QString(), actionCollection(), "controls_next");

	// FIXME
	actionControlVolume = new KAction(KIcon("player_eject"), QString(), actionCollection(), "controls_volume");
	actionControlMute = new KAction(KIcon("player_eject"), QString(), actionCollection(), "controls_mute");

	createGUI();

	// FIXME
	addToolBar(Qt::BottomToolBarArea, toolBar("main_controls_toolbar"));
	addToolBar(Qt::BottomToolBarArea, toolBar("position_slider_toolbar"));

	show();
}

Kaffeine::~Kaffeine()
{
}

void Kaffeine::updateArgs()
{
	// FIXME
}

void Kaffeine::actionOpen()
{
	// FIXME
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Open file(s)"));
	if (url.isValid())
		mediaWidget.play(url.url());
}

void Kaffeine::actionQuit()
{
	close();
}
