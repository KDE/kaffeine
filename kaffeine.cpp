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

	actionControlPrevious = new KAction(KIcon("player_start"), i18n("Previous"), actionCollection(), "controls_previous");
	actionControlPlayPause = new KAction(KIcon("player_play"), i18n("Play"), actionCollection(), "controls_play_pause");
	connect(actionControlPlayPause, SIGNAL(triggered(bool)), this, SLOT(play()));
	actionControlStop = new KAction(KIcon("player_stop"), i18n("Stop"), actionCollection(), "controls_stop");
	connect(actionControlStop, SIGNAL(triggered(bool)), player, SLOT(stop()));
	actionControlNext = new KAction(KIcon("player_end"), i18n("Next"), actionCollection(), "controls_next");

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

void Kaffeine::play()
{
	if ( actionControlPlayPause->isCheckable() ) {
		if ( actionControlPlayPause->isChecked() )
			player->togglePause( true );
		else
			player->togglePause( false );
	}
	else {
		player->play();
	}	
}

void Kaffeine::newMediaState( MediaState status )
{
	switch (status) {
		case MediaPlaying:
			stateChanged( "playing" );
			actionControlPlayPause->setIcon( KIcon("player_pause") );
			actionControlPlayPause->setText( i18n("Pause") );
			actionControlPlayPause->setCheckable( true );
			actionControlPlayPause->setChecked( false );
			break;
		case MediaPaused:
			stateChanged( "paused" );
			break;
		case MediaStopped:
			stateChanged( "stopped" );
			actionControlPlayPause->setCheckable( false );
			actionControlPlayPause->setText( i18n("Play") );
			actionControlPlayPause->setIcon( KIcon("player_play") );
			break;
		default:
			break;
	}
}

void Kaffeine::actionQuit()
{
	close();
}
