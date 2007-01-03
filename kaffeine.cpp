#include "kaffeine.h"

// no Qt::WA_DeleteOnClose
Kaffeine::Kaffeine() : KMainWindow(0, (Qt::WindowFlags) 0)
{
	show();
}

Kaffeine::~Kaffeine()
{
}
