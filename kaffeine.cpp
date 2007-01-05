#include <KToggleAction>
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

	player = new MediaWidget();
	connect( player, SIGNAL(newState(MediaState)), this, SLOT(newMediaState(MediaState)) );
	initActions();
	createGUI();
	setCentralWidget( player );
	
	// FIXME
	addToolBar(Qt::BottomToolBarArea, toolBar("main_controls_toolbar"));
	addToolBar(Qt::BottomToolBarArea, toolBar("position_slider_toolbar"));
	
	stateChanged( "stopped" );

	show();
}

Kaffeine::~Kaffeine()
{
}

void Kaffeine::updateArgs()
{
	// FIXME
}

void Kaffeine::initActions()
{
	// FIXME
	KStandardAction::open(this, SLOT(actionOpen()), actionCollection(), "file_open_x");
	KStandardAction::quit(this, SLOT(actionQuit()), actionCollection(), "file_quit_x");

	actionControlPrevious = new KAction(KIcon("player_start"), QString(), actionCollection(), "controls_previous");
	actionControlPlay = new KAction(KIcon("player_play"), QString(), actionCollection(), "controls_play");
	connect(actionControlPlay, SIGNAL(triggered(bool)), player, SLOT(play()));
	actionControlPause = new KToggleAction(KIcon("player_pause"), i18n("Pause"), actionCollection(), "controls_pause");
	connect(actionControlPause, SIGNAL(triggered(bool)), player, SLOT(togglePause(bool)));
	actionControlStop = new KAction(KIcon("player_stop"), QString(), actionCollection(), "controls_stop");
	connect(actionControlStop, SIGNAL(triggered(bool)), player, SLOT(stop()));
	actionControlNext = new KAction(KIcon("player_end"), QString(), actionCollection(), "controls_next");

	// FIXME
	actionControlVolume = new KAction(KIcon("player_eject"), QString(), actionCollection(), "controls_volume");
	actionControlMute = new KAction(KIcon("player_eject"), QString(), actionCollection(), "controls_mute");
}

void Kaffeine::actionOpen()
{
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Open file"));
	if (url.isValid())
		player->play( url );
}

void Kaffeine::newMediaState( MediaState status )
{
	switch (status) {
		case MediaPlaying:
			stateChanged( "playing" );
			break;
		case MediaPaused:
			stateChanged( "paused" );
			break;
		case MediaStopped:
			stateChanged( "stopped" );
			break;
		default:
			break;
	}
}

void Kaffeine::actionQuit()
{
	close();
}
