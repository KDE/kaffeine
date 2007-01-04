#include <KStandardAction>

#include "kaffeine.h"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	KCmdLineLastOption
};

// FIXME no Qt::WA_DeleteOnClose
Kaffeine::Kaffeine() : KMainWindow(0, (Qt::WindowFlags) 0)
{
	KStandardAction::open(this, SLOT(close()), actionCollection(), "file_open");
	KStandardAction::quit(this, SLOT(close()), actionCollection(), "file_quit");

	createGUI();

	show();
}

Kaffeine::~Kaffeine()
{
}

void Kaffeine::updateArgs()
{
	// FIXME
}
