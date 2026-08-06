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

#pragma once

#include "Lib/BaseType.h"

#include <array>
#include <vector>
#include <SDL3/SDL.h>

#include "GameClient/Mouse.h"

// Splitscreen: one decoded frame of a cursor, kept in memory as tightly-packed ARGB8888.
//
// Seat cursors are drawn by us, not by the OS, so an SDL_Cursor is no use to them - it is opaque
// and only the window manager can draw it. Of the 37 cursor states, 27 ship NO texture art at all
// (only Data\Cursors\*.ani and Art\W3D\*.W3D), so a seat cursor had nothing to draw for any of
// them and silently fell back to the arrow - which is why a pad seat could not tell garrison from
// move from waypoint. The .ani pixels are the only redistributable-free source we have: they are
// the player's own installed game data, decoded at runtime and never written back to disk.
struct CursorFrameRGBA
{
	Int m_width;
	Int m_height;
	std::vector<UnsignedByte> m_pixels;	///< w*h*4, ARGB8888, tightly packed

	CursorFrameRGBA() : m_width(0), m_height(0) {}
};

struct AnimatedCursor
{
	SDL_Cursor* m_cursor;

	// Retained copy of what IMG_LoadAnimation_IO decoded. It used to be freed immediately after
	// SDL_CreateColorCursor took it; keeping it costs a few KB per cursor state and is what lets a
	// seat cursor draw real art instead of falling back to the arrow.
	std::vector<CursorFrameRGBA> m_frames;
	Int m_hotSpotX;
	Int m_hotSpotY;

	AnimatedCursor()
		: m_cursor(nullptr), m_hotSpotX(0), m_hotSpotY(0)
	{}
	~AnimatedCursor()
	{
		if (m_cursor)
		{
			SDL_DestroyCursor(m_cursor);
			m_cursor = nullptr;
		}
	}

	SDL_Cursor* getCursor() const { return m_cursor; }
	Int getFrameCount() const { return (Int)m_frames.size(); }
	const CursorFrameRGBA* getFrame(Int i) const
	{
		return (i >= 0 && i < (Int)m_frames.size()) ? &m_frames[i] : nullptr;
	}
};

class SDL3CursorManager
{
public:
	static void init();
	static void shutdown();

	static SDL_Cursor* getCursor(Mouse::MouseCursor cursor, int direction);

	// Splitscreen: the decoded frames behind that cursor, for callers that must draw it
	// themselves rather than hand it to the window manager. Null if the .ani was absent or
	// failed to decode.
	static const AnimatedCursor* getAnimatedCursor(Mouse::MouseCursor cursor, int direction);

	// Internal loader used by Mouse implementation
	static void initResources(Mouse* mouse);

private:
	static AnimatedCursor* loadANI(const char* filepath);
	static AnimatedCursor* m_cursorResources[Mouse::NUM_MOUSE_CURSORS][MAX_2D_CURSOR_DIRECTIONS];
};
