#include <KAction>
#include <KFileDialog>
#include <KLocalizedString>
#include <KStandardAction>
#include <KToolBar>

#include "kaffeine.h"
#include "kaffeine.moc"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	// FIXME add options
	KCmdLineLastOption
};

Kaffeine::Kaffeine() : currentState(stateAll)
{
	// FIXME workaround
	setAttribute(Qt::WA_DeleteOnClose, false);

	/*
	 * initialise media widget
	 */

	player = new MediaWidget( this );
	connect(player, SIGNAL(newState(MediaState)), this, SLOT(newMediaState(MediaState)));

	setCentralWidget(player);

	/*
	 * initialise gui elements
	 */

	KAction *action = KStandardAction::open(this, SLOT(actionOpen()), actionCollection());
	actionCollection()->addAction("file_open", action);
	action = KStandardAction::quit(this, SLOT(actionQuit()), actionCollection());
	actionCollection()->addAction("file_quit", action);

	actionControlPrevious = createAction("controls_previous", i18n("Previous"), KIcon("player_start"), statePrevNext | statePlaying);
	actionControlPlayPause = createAction("controls_play_pause", i18n("Play"), KIcon("player_play"), stateNone);
	connect(actionControlPlayPause, SIGNAL(triggered(bool)), this, SLOT(actionPlayPause()));
	actionControlStop = createAction("controls_stop", i18n("Stop"), KIcon("player_stop"), statePlaying);
	connect(actionControlStop, SIGNAL(triggered(bool)), player, SLOT(stop()));
	actionControlNext = createAction("controls_next", i18n("Next"), KIcon("player_end"), statePrevNext);

	KAction *ac = new KAction(actionCollection());
	actionCollection()->addAction("controls_volume", ac);
	ac->setDefaultWidget( player->getVolumeSlider() );

	ac = new KAction(actionCollection());
	actionCollection()->addAction("position_slider", ac);
	ac->setDefaultWidget( player->getPositionSlider() );

	createGUI();

	// FIXME workaround
	addToolBar(Qt::BottomToolBarArea, toolBar("main_controls_toolbar"));
	addToolBar(Qt::BottomToolBarArea, toolBar("position_slider_toolbar"));

	setCurrentState(stateNone);

	show();
}

Kaffeine::~Kaffeine()
{
}

void Kaffeine::updateArgs()
{
	// FIXME implement
}

void Kaffeine::actionOpen()
{
	// FIXME do we want to be able to open several files at once or not?
	KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Open file"));
	if (url.isValid())
		player->play(url);
}

void Kaffeine::actionPlayPause()
{
	if (currentState.testFlag(statePlaying))
		if (actionControlPlayPause->isChecked())
			player->togglePause(true);
		else
			player->togglePause(false);
	else {
		// FIXME do some special actions - play playlist, ask for input ...
		player->play();
	}
}

void Kaffeine::actionQuit()
{
	close();
}

void Kaffeine::setCurrentState(stateFlags newState)
{
	if (currentState == newState)
		return;

	stateFlags changedFlags = currentState ^ newState;

	// starting / stopping playback is special
	if (changedFlags.testFlag(statePlaying))
		if (newState.testFlag(statePlaying)) {
			actionControlPlayPause->setIcon(KIcon("player_pause"));
			actionControlPlayPause->setText(i18n("Pause"));
			actionControlPlayPause->setCheckable(true);
		} else {
			actionControlPlayPause->setIcon(KIcon("player_play"));
			actionControlPlayPause->setText(i18n("Play"));
			actionControlPlayPause->setCheckable(false);
		}

	foreach (stateFlags key, flaggedActions.keys()) {
		bool currentEnabled = ((currentState & key) != stateNone);
		bool newEnabled = ((newState & key) != stateNone);
		if (currentEnabled ^ newEnabled)
			foreach (KAction *value, flaggedActions.values(key))
				value->setEnabled(newEnabled);
	}

	currentState = newState;
}

void Kaffeine::newMediaState(MediaState status)
{
	switch (status) {
		case MediaPlaying:
			setCurrentState(currentState | statePlaying);
			break;
		case MediaPaused:
			break;
		case MediaStopped:
			setCurrentState(currentState & statePrevNext);
			break;
		default:
			break;
	}
}
