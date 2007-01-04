#include <KFileDialog>
#include <KLocalizedString>
#include <KStandardAction>

#include "kaffeine.h"

#include "kaffeine.moc"

const KCmdLineOptions Kaffeine::cmdLineOptions[] = {
	KCmdLineLastOption
};

// FIXME no Qt::WA_DeleteOnClose
Kaffeine::Kaffeine() : KMainWindow(0, (Qt::WindowFlags) 0)
{
	KStandardAction::open(this, SLOT(actionOpen()), actionCollection(), "file_open");
	KStandardAction::quit(this, SLOT(actionQuit()), actionCollection(), "file_quit");

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
