/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

// TheSuperHackers @build githubawn 10/07/2026 PS2 (Emotion Engine / Graphics
// Synthesizer) render backend. See docs/ps2-port-plan.md, Tier 0.
//
// Inherits DX8Backend for the same reason BgfxBackend does (see
// BgfxBackend.h): DX8Wrapper's render_state tracking is still read by the
// sorting renderer and other subsystems that have not been migrated to
// route exclusively through IRenderBackend. render-backend.cmake forces
// GGC_BGFX_STANDALONE=ON whenever GGC_RENDER_BACKEND=ps2, so DX8Wrapper
// initializes a StubD3D8Device instead of a real D3D8 device -- there is
// no D3D8 reference popup on PS2, same as the bgfx-standalone builds on
// Android/Linux/macOS/iOS.
//
// This is a Tier 0 stub: it does not submit any draws to the GS yet. The
// goal is only to let WW3D2/GameClient compile and link on PS2 so the
// Phase 1 headless sim-boot milestone is reachable. Real gsKit/VU1
// rendering is Phase 3 work and will override the draw-call methods here
// as they are implemented.

#pragma once

#include "DX8Backend.h"

class PS2Backend : public DX8Backend
{
public:
    PS2Backend();
    virtual ~PS2Backend();
};
