/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// W3DSeatCursorRenderer.h
//
// Per-seat software cursors for splitscreen (WP3 of the splitscreen plan; see
// PatchNotes/splitscreen-plan2.md). Draws each bound local seat's virtual cursor
// as a small tinted pointer in the 2D/UI pass, after the windows and the OS mouse
// are drawn so the seat cursors sit on top. Seat 0 keeps the OS hardware cursor;
// only seats whose VirtualCursor is visible (integrated in
// SeatManager::createStreamMessages) are drawn here.
//
// The tint is the seat's player house color when it has one, else a fixed
// per-seat fallback palette. Purely additive: this touches neither the mouse, the
// window system, nor input routing. Runs only while splitscreen dev mode is on.

#pragma once

#include "Lib/BaseType.h"

// Draws all visible per-seat software cursors. Call at the end of the 2D/UI pass.
// No-op unless TheSeatManager exists and splitscreen dev mode is enabled.
class W3DSeatCursorRenderer
{
public:
	static void render();
};
