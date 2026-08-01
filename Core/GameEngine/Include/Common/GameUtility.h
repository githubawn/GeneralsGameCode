/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// For miscellaneous game utility functions.

class Player;
typedef Int PlayerIndex;

namespace rts
{

bool localPlayerHasRadar();
Player* getObservedOrLocalPlayer(); ///< Get the current observed or local player. Is never null.
Player* getObservedOrLocalPlayer_Safe(); ///< Get the current observed or local player. Is never null, except when the application does not have players.
PlayerIndex getObservedOrLocalPlayerIndex_Safe(); ///< Get the current observed or local player index. Returns 0 when the application does not have players.

void changeLocalPlayer(Player* player); //< Change local player during game. Must not pass null.
void changeObservedPlayer(Player* player); ///< Change observed player during game. Can pass null: is identical to passing the "ReplayObserver" player.

// Splitscreen (WP7): a scoped "render player" override. While set (>=0),
// getObservedOrLocalPlayerIndex_Safe() returns it, so a viewport's draw pass sees
// its own player's vision (shroud/fog/object-hiding). Set around each view's draw
// and cleared afterwards; -1 = no override (normal local/observed player).
void setRenderPlayerIndexOverride(Int playerIndex);
void clearRenderPlayerIndexOverride();

// Splitscreen: which local seat's viewport is being drawn right now, or 0.
//
// Derived from the render player rather than stored, so it follows the override above and needs no
// second thing to keep in sync. Seat 0 is the answer whenever no other seat's player is the one
// being rendered - which includes every single-viewport game, so callers can use it unconditionally.
//
// This is what per-seat UI feedback has to key off. Anything that asks "is this selected" or "is
// the mouse over this" while drawing is asking on behalf of ONE viewport, and answering with "any
// seat" or with seat 0's pointer is what put player 1's selection and hover highlights into all
// eight viewports at once.
Int getRenderSeatIndex();

// Splitscreen: which local seat commands the given player, or -1 if none does.
//
// Seat 0 always answers for ThePlayerList's local player, so in a single-viewport game this is
// exactly "is this the local player" and every caller below behaves as it always did. The reason
// it exists is that isLocallyControlled() answers ONLY for seat 0's player: with eight local
// players, "is this mine" and "is this somebody's, here, at this machine" stopped being the same
// question, and the logic-side client callbacks (select/deselect on level start, on death, on
// unit swap) were all asking the first one and acting on seat 0.
Int getSeatIndexForPlayer(PlayerIndex playerIndex);

// Splitscreen: the player a local seat commands, or -1 before it is bound (and in menus).
PlayerIndex getSeatPlayerIndex(Int seatIndex);

// Splitscreen: the screen rectangle of the view currently being drawn.
//
// Several full-screen render passes cover "the tactical view" by asking the TheTacticalView
// global for its rect - the stencil shadow darkening quad and the behind-building player-colour
// silhouettes both do. That global is seat 0's view, so with N viewports those passes paint over
// seat 0's rectangle no matter which view is actually being rendered, and every other viewport
// loses the effect entirely. Set around each view's draw; when unset this reports the full
// display, which is what a single-view game always wanted.
void setRenderViewRect(Int x, Int y, Int width, Int height);
void clearRenderViewRect();
void getRenderViewRect(Int* x, Int* y, Int* width, Int* height);

} // namespace rts
