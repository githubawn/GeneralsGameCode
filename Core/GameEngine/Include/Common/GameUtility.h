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
