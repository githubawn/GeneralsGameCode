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

// Draw a small arrow pointer whose hot-spot (tip) is the cursor position. The
// colored fill is a right-triangle scanned as horizontal 1px rows; a black
// outline along its three edges keeps it readable over any background.
static void drawArrowCursor(Int x, Int y, Color color)
{
	static const Color OUTLINE = GameMakeColor(0, 0, 0, 255);
	static const Int   SIZE    = 14;  // pointer height/width in pixels

	// solid tinted body
	for (Int row = 0; row < SIZE; ++row)
		TheDisplay->drawFillRect(x, y + row, row + 1, 1, color);

	// black outline: left (vertical) edge, hypotenuse, and bottom edge
	TheDisplay->drawLine(x, y, x, y + SIZE, 1.0f, OUTLINE);
	TheDisplay->drawLine(x, y, x + SIZE, y + SIZE, 1.0f, OUTLINE);
	TheDisplay->drawLine(x, y + SIZE, x + SIZE, y + SIZE, 1.0f, OUTLINE);
}

// Draw the game's OWN art for this seat's current cursor, so a seat cursor changes shape with
// context (attack, move, select, ...) exactly like the mouse does. The mouse system tracks a
// single cursor - one loaded texture set, one hot-spot - so we cannot ask it to draw ours; we
// look the cursor's image up the same way its polygon renderer does and blit it ourselves.
//
// The image is drawn untinted, because these are the real cursor graphics and recoloring them
// would misrepresent states that are themselves color-coded (a red attack cursor, say). Seat
// identity still reads from the tinted arrow when a cursor has no image defined.
//
// Returns FALSE if this cursor has no usable image, so the caller can fall back.
static Bool drawGameCursorImage(Int cursorType, Int x, Int y)
{
	if (TheMouse == nullptr || TheMappedImageCollection == nullptr)
		return FALSE;

	const CursorInfo *info = TheMouse->getCursorInfo( cursorType );
	if (info == nullptr || info->imageName.isEmpty())
		return FALSE;

	const Image *image = TheMappedImageCollection->findImageByName( info->imageName );
	if (image == nullptr)
		return FALSE;

	const Int w = image->getImageWidth();
	const Int h = image->getImageHeight();
	if (w <= 0 || h <= 0)
		return FALSE;

	// Place the art so its hot-spot lands on the cursor position, matching the OS cursor.
	const Int left = x - info->hotSpotPosition.x;
	const Int top  = y - info->hotSpotPosition.y;

	TheDisplay->drawImage( image, left, top, left + w, top + h );
	return TRUE;
}

void W3DSeatCursorRenderer::render()
{
	if (!TheSeatManager || !TheSeatManager->isSplitscreenEnabled())
		return;
	if (!TheDisplay)
		return;

	for (Int i = 0; i < MAX_SEATS; ++i)
	{
		LocalSeat* seat = TheSeatManager->getSeat(i);
		if (!seat || !seat->m_cursor.visible)
			continue;

		// visible is only ever set for a bound seat driving its own cursor, so
		// seat 0 (OS mouse) and unbound seats are naturally skipped here.
		if (drawGameCursorImage(seat->m_cursor.cursorType, seat->m_cursor.pos.x, seat->m_cursor.pos.y))
			continue;

		// No image for this cursor state - fall back to the tinted seat arrow.
		drawArrowCursor(seat->m_cursor.pos.x, seat->m_cursor.pos.y, seatCursorColor(seat));
	}
}
