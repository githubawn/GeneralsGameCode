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

// RenderLeakProbe.h
//
// Splitscreen render-leak diagnostic.
//
// Every viewport draws the SAME 3D scene, so anything that decides "should this be
// drawn" from global state keyed to "the local player" leaks one seat's vision into
// another seat's viewport. Three separate sessions guessed at which decision was
// wrong and were wrong every time, so this probe reports the decision instead.
//
// How it works: point the mouse at the leaking thing. Once per frame the probe takes
// the mouse pixel as its target; each viewport whose rect CONTAINS that pixel becomes
// "probed". While a probed view draws, every render decision made about an object that
// projects near the target pixel appends a row saying what was decided and why. The
// rows are published to the seat debug overlay at the start of the next frame.
//
// A row reads:
//   <dist>px <path> <name> own=P<n> ss=<n> ghost=P<n> -> <decision>
// where
//   path     = which code path was making the decision (scene main loop, occluder
//              pass, translucent pass, particle pass, ...)
//   own      = the player index that owns the object (-1 = none/unknown)
//   ss       = ObjectShroudStatus as seen by THIS view's render player
//              (0=INVALID 1=CLEAR 2=PARTIAL_CLEAR 3=FOGGED 4=SHROUDED - check the
//              enum in Object.h if it moves)
//   ghost    = player whose fogged memory is standing in for the object, -1 = not a ghost
//   decision = DREW... or SKIP... with the reason
//
// So a leak is diagnosed by reading one line: if a building the viewport's player
// cannot see reports "ss=3 -> DREW", the shroud test never ran on that path; if it
// reports no row at all, the pixels came from something that is not a scene object
// (a shadow, a decal, a 2D overlay).

#pragma once

#include "Lib/BaseType.h"

namespace RenderLeakProbe
{

enum { PROBE_MAX_ROWS = 8 };

/// Master switch. Off => every entry point below is a cheap no-op.
Bool isEnabled();
void setEnabled(Bool enabled);

/// Latch this frame's target pixel (the OS mouse) and publish the previous frame's
/// rows for the overlay to read. Called once per frame from Display::drawViews.
void beginFrame();

/// Called around each view's draw. The view becomes "probed" when it contains the
/// target pixel; only a probed view records anything.
void beginView(Int viewIndex, Int renderPlayerIndex, Int originX, Int originY, Int width, Int height);
void endView();

/// TRUE while a probed view is drawing. Every recording site tests this first, so the
/// cost of the probe in the other viewports is one predictable branch.
Bool isViewProbed();

/// TRUE if this screen position is close enough to the target pixel to be worth a row.
Bool wantsPixel(Real screenX, Real screenY);

/// Append a decision row. Safe to call when not probed (it is dropped).
void record(Real screenX, Real screenY, const char* path, const char* name,
	Int ownerPlayer, Int shroudStatus, Int ghostOwner, const char* decision);

/// Convenience for the common "one printf-ish decision string" case.
void recordf(Real screenX, Real screenY, const char* path, const char* name,
	Int ownerPlayer, Int shroudStatus, Int ghostOwner, const char* decisionFormat, ...);

/// Tally a shadow decision for the view being drawn. Unlike the rows above this counts in EVERY
/// view, not just the probed one, because "shadows are missing in the other viewports" is a
/// question about the views the mouse is not in.
void countShadow(Bool drawn);

/// Same tally for stencil/volume shadows, which are a completely separate list from the decal
/// shadows above - unit shadows are usually these, so counting only decals answers the wrong
/// question.
void countVolumeShadow(Bool drawn);

/// Whether the shadow pass was even allowed to run in this view (W3DShadowManager::isShadowScene).
void noteShadowPass(Bool decalPassRan, Bool stencilPassRan);

/// What seat 0's software cursor resolved to this frame. Reported verbatim on the overlay so a
/// wrong-looking cursor can be attributed to (or cleared of) the seat cursor renderer at a glance.
void noteSeatCursor(Int seatIndex, Int cursorType, const char* imageName, Int width, Int height, Bool drew);

// --- readback for the debug overlay (reports the frame that just finished) ---
Int getShadowsDrawn(Int viewIndex);
Int getShadowsSkipped(Int viewIndex);
Int getVolumeShadowsDrawn(Int viewIndex);
Int getVolumeShadowsSkipped(Int viewIndex);
Int getShadowPassRan(Int viewIndex);      ///< bit 0 = decal pass ran, bit 1 = stencil pass ran
const char* getSeatCursorReport();

Int getRowCount();
const char* getRow(Int index);
Int getProbeX();
Int getProbeY();
Int getProbedViewIndex();   ///< -1 when the target pixel is in no view
Int getProbedViewPlayer();  ///< render player index of that view
Int getViewCount();
Int getConsideredCount();   ///< scene objects examined in the probed view (sanity check)

} // namespace RenderLeakProbe
