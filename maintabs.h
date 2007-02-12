/*
 * maintabs.h
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

#ifndef MAINTABS_H
#define MAINTABS_H

#include <config.h>

#include "tabmanager.h"

class MediaWidget;

class StartTab : public TabBase
{
public:
	explicit StartTab(TabManager *tabManager_);
	~StartTab();

private:
	void internalActivate() { }
};

class PlayerTab : public TabBase
{
public:
	PlayerTab(TabManager *tabManager_, MediaWidget *mediaWidget_);
	~PlayerTab();

private:
	void internalActivate();

	MediaWidget *mediaWidget;
};

#endif /* MAINTABS_H */
