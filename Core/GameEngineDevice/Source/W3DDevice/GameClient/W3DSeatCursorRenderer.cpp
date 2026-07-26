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

// W3DSeatCursorRenderer.cpp
//
// See W3DSeatCursorRenderer.h for the design notes. The cursor art is a small
// solid tinted arrow (right-triangle pointer with a black outline), assembled
// from the Display's own 2D primitives (drawFillRect / drawLine). Those route
// through W3DDisplay's configured Render2DClass batch, whose Add_Quad/Add_Rect
// modulate by the per-quad vertex color (pre-flight V1) - so no shader is needed
// to tint each seat's cursor its own color.

#include "W3DDevice/GameClient/W3DSeatCursorRenderer.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SeatManager.h"
#include "GameClient/Color.h"
#include "GameClient/Display.h"
#include "GameClient/Image.h"
#include "GameClient/Mouse.h"
#include "W3DDevice/GameClient/W3DMouse.h"   // MAX_2D_CURSOR_ANIM_FRAMES
#include "WW3D2/assetmgr.h"
#include "WW3D2/texture.h"

// Fallback seat colors used until a seat is assigned a game player (then the
// player's house color wins). Eight visually distinct, opaque colors; index 0 is
// the keyboard/mouse seat, which draws the OS cursor and so is not normally shown.
static const Color fixedSeatPalette[MAX_SEATS] =
{
	GameMakeColor(255, 255, 255, 255),  // seat 0 - white  (mouse; not usually drawn)
	GameMakeColor( 80, 160, 255, 255),  // seat 1 - blue
	GameMakeColor(255,  80,  80, 255),  // seat 2 - red
	GameMakeColor( 80, 220, 100, 255),  // seat 3 - green
	GameMakeColor(255, 220,  60, 255),  // seat 4 - yellow
	GameMakeColor(255, 140,  40, 255),  // seat 5 - orange
	GameMakeColor(200, 100, 255, 255),  // seat 6 - purple
	GameMakeColor( 60, 230, 230, 255),  // seat 7 - cyan
};

// The seat's tint: its player's house color once it has one, else the fallback.
// Alpha is forced opaque so the cursor is always visible regardless of how the
// source color packed its alpha.
static Color seatCursorColor(const LocalSeat* seat)
{
	if (seat->m_playerIndex >= 0 && ThePlayerList)
	{
		Player* player = ThePlayerList->getNthPlayer(seat->m_playerIndex);
		if (player)
			return player->getPlayerColor() | 0xFF000000;
	}

	Int idx = seat->m_seatIndex;
	if (idx < 0 || idx >= MAX_SEATS)
		idx = 0;
	return fixedSeatPalette[idx];
}

// Blend a seat's identity color 10% over white. Every seat draws the game's real cursor art, so
// the tint has to stay faint: these cursors are themselves color-coded (a red attack cursor, a
// green repair cursor) and a strong tint would misread as a different cursor state. 10% is just
// enough to tell whose pointer is whose.
static Color seatTintColor(const LocalSeat* seat)
{
	static const Real TINT_STRENGTH = 0.10f;

	const Color seatColor = seatCursorColor(seat);
	const Int sr = (seatColor >> 16) & 0xFF;
	const Int sg = (seatColor >>  8) & 0xFF;
	const Int sb = (seatColor      ) & 0xFF;

	const Int r = (Int)(255.0f * (1.0f - TINT_STRENGTH) + sr * TINT_STRENGTH);
	const Int g = (Int)(255.0f * (1.0f - TINT_STRENGTH) + sg * TINT_STRENGTH);
	const Int b = (Int)(255.0f * (1.0f - TINT_STRENGTH) + sb * TINT_STRENGTH);

	return GameMakeColor(r, g, b, 255);
}

// Resolve the art for a cursor state into a drawable Image.
//
// The mouse system tracks a single cursor - one loaded texture set, one hot-spot - so we cannot
// ask it to draw ours. Note the cursors are NOT in the mapped-image collection: Mouse.ini gives
// each one a raw texture name ("Texture = SCCPointer"), which the mouse loads through
// WW3DAssetManager as SCCPointer.tga, or SCCPointer0000.tga.. for animated ones. So we build a
// lightweight Image per cursor state naming that texture and let W3DDisplay::drawImage resolve
// it through the same asset manager. Built once per (cursor, frame) and cached for the session.
static const Image *findCursorImage(Int cursorType, Int frame, const CursorInfo **infoOut)
{
	static const Int CURSOR_SIZE_FALLBACK = 32;  // only if the texture cannot be measured

	if (TheMouse == nullptr)
		return nullptr;

	const CursorInfo *info = TheMouse->getCursorInfo( cursorType );
	if (info == nullptr || info->textureName.isEmpty())
		return nullptr;

	static Image *s_cache[Mouse::NUM_MOUSE_CURSORS][MAX_2D_CURSOR_ANIM_FRAMES];
	if (cursorType < 0 || cursorType >= Mouse::NUM_MOUSE_CURSORS)
		return nullptr;
	if (frame < 0 || frame >= MAX_2D_CURSOR_ANIM_FRAMES)
		frame = 0;

	// Only cache once the texture could actually be MEASURED. Cursor textures are not necessarily
	// resident the first time a seat cursor is drawn, and caching a guessed size then leaves every
	// seat cursor permanently the wrong scale next to player 1's.
	static Bool s_cacheSizeKnown[Mouse::NUM_MOUSE_CURSORS][MAX_2D_CURSOR_ANIM_FRAMES];

	if (s_cache[cursorType][frame] == nullptr || !s_cacheSizeKnown[cursorType][frame])
	{
		AsciiString file;
		if (info->numFrames <= 1)
			file.format("%s.tga", info->textureName.str());          // single frame, no suffix
		else
			file.format("%s%04d.tga", info->textureName.str(), frame); // animated

		Image *image = s_cache[cursorType][frame];
		if (image == nullptr)
		{
			image = newInstance(Image);
			image->setName( file );
			image->setFilename( file );

			Region2D uv;
			uv.lo.x = 0.0f; uv.lo.y = 0.0f;
			uv.hi.x = 1.0f; uv.hi.y = 1.0f;
			image->setUV( &uv );

			ICoord2D fallback;
			fallback.x = CURSOR_SIZE_FALLBACK;
			fallback.y = CURSOR_SIZE_FALLBACK;
			image->setImageSize( &fallback );

			s_cache[cursorType][frame] = image;
		}

		// Measure the real cursor texture rather than assuming a size - Mouse.ini declares none.
		// Keep retrying until it succeeds: the texture is not necessarily resident on the first
		// frame a seat cursor is drawn, and locking in the fallback then leaves every seat cursor
		// permanently out of scale next to player 1's.
		TextureClass *tex = WW3DAssetManager::Get_Instance()->Get_Texture( file.str(), MIP_LEVELS_1 );
		if (tex != nullptr)
		{
			SurfaceClass *surface = tex->Get_Surface_Level();
			if (surface != nullptr)
			{
				SurfaceClass::SurfaceDescription desc;
				surface->Get_Description( desc );
				if (desc.Width > 0 && desc.Height > 0)
				{
					ICoord2D size;
					size.x = desc.Width;
					size.y = desc.Height;
					image->setImageSize( &size );
					s_cacheSizeKnown[cursorType][frame] = TRUE;
				}
				surface->Release_Ref();
			}
			tex->Release_Ref();
		}
	}

	*infoOut = info;
	return s_cache[cursorType][frame];
}

// Ticks once per rendered frame; drives cursor animation. A render-frame counter rather than a
// clock keeps this free of any timing dependency, which matters because this file is shared Core
// device code.
static Int s_cursorAnimTick = 0;

// Which animation frame a cursor should be showing right now. Animated cursors (the scroll and
// attack pointers) otherwise sit frozen on frame 0.
static Int currentCursorFrame(const CursorInfo *info)
{
	static const Int RENDER_FRAMES_PER_CURSOR_FRAME = 4;

	if (info == nullptr || info->numFrames <= 1)
		return 0;

	return (s_cursorAnimTick / RENDER_FRAMES_PER_CURSOR_FRAME) % info->numFrames;
}

// Draw the game's OWN art for this seat's cursor, so a seat cursor changes shape with context
// (attack, move, select, ...) exactly as player 1's does. A cursor state with no art defined
// falls back to the game's DEFAULT cursor rather than to a stand-in shape, so a seat always
// shows real cursors.
static void drawSeatCursor(const LocalSeat* seat)
{
	const CursorInfo *info = nullptr;
	const CursorInfo *probe = TheMouse ? TheMouse->getCursorInfo( seat->m_cursor.cursorType ) : nullptr;

	const Image *image = findCursorImage( seat->m_cursor.cursorType, currentCursorFrame( probe ), &info );
	if (image == nullptr)
	{
		// This cursor state has no art defined - show the game's DEFAULT cursor rather than a
		// stand-in shape, so a seat always displays real cursors like player 1 does.
		probe = TheMouse ? TheMouse->getCursorInfo( Mouse::ARROW ) : nullptr;
		image = findCursorImage( Mouse::ARROW, currentCursorFrame( probe ), &info );
	}
	if (image == nullptr)
		return;

	// Place the art so its hot-spot lands on the cursor position, matching the OS cursor.
	const Int left = seat->m_cursor.pos.x - info->hotSpotPosition.x;
	const Int top  = seat->m_cursor.pos.y - info->hotSpotPosition.y;

	TheDisplay->drawImage( image, left, top,
		left + image->getImageWidth(), top + image->getImageHeight(), seatTintColor(seat) );
}

void W3DSeatCursorRenderer::render()
{
	if (!TheSeatManager || !TheSeatManager->isSplitscreenEnabled())
		return;
	if (!TheDisplay)
		return;

	++s_cursorAnimTick;

	for (Int i = 0; i < MAX_SEATS; ++i)
	{
		LocalSeat* seat = TheSeatManager->getSeat(i);
		if (!seat || !seat->m_cursor.visible)
			continue;

		// A seat only owns a cursor while it owns a viewport. Without this the seat cursors
		// survive into the shell/main menu, where the seats still exist but their views are gone.
		if (seat->m_view == nullptr)
			continue;

		// visible is only ever set for a bound seat driving its own cursor, so
		// seat 0 (OS mouse) and unbound seats are naturally skipped here.
		drawSeatCursor(seat);
	}
}
