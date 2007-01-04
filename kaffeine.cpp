#include <KLocalizedString>
#include <KMenu>
#include <KMenuBar>

#include "kaffeine.h"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	KCmdLineLastOption
};

// FIXME no Qt::WA_DeleteOnClose
Kaffeine::Kaffeine() : KMainWindow(0, (Qt::WindowFlags) 0)
{
	KMenu *fileMenu = new KMenu(i18n("&File"), this);

	menuBar()->addMenu(fileMenu);
	menuBar()->addMenu(helpMenu());

	show();
}

Kaffeine::~Kaffeine()
{
}

void Kaffeine::updateArgs()
{
	// FIXME
}
