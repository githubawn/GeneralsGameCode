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

// RenderLeakProbe.cpp
//
// See RenderLeakProbe.h for what this is and how to read its output.

#include "PreRTS.h" // must be first

#include "Common/RenderLeakProbe.h"
#include "Common/SeatManager.h"
#include "GameClient/Mouse.h"

#include <stdarg.h>
#include <stdio.h>

namespace RenderLeakProbe
{

// How close (in screen pixels) an object has to project to the target pixel to be
// worth a row. Generous, because a building's bounding-sphere centre sits well above
// the pixel the eye picks out, and because a leak is easier to spot with too many
// rows than with too few.
static const Real PROBE_RADIUS = 70.0f;

enum { ROW_CHARS = 176 };

struct Row
{
	Real m_dist;
	char m_text[ROW_CHARS];
};

static Bool s_enabled = TRUE;

// The frame being recorded.
static Int  s_probeX = -1, s_probeY = -1;
static Bool s_viewProbed = FALSE;
static Int  s_viewIndex = -1;
static Int  s_viewPlayer = -1;
static Int  s_probedViewIndex = -1;
static Int  s_probedViewPlayer = -1;
static Int  s_viewCount = 0;
static Int  s_considered = 0;
static Row  s_rows[PROBE_MAX_ROWS];
static Int  s_rowCount = 0;

// The frame the overlay reads (published at beginFrame so the overlay never shows a
// half-filled frame, and so rows do not flicker as the draw order changes).
static Int  s_pubProbeX = -1, s_pubProbeY = -1;
static Int  s_pubViewIndex = -1;
static Int  s_pubViewPlayer = -1;
static Int  s_pubViewCount = 0;
static Int  s_pubConsidered = 0;
static Row  s_pubRows[PROBE_MAX_ROWS];
static Int  s_pubRowCount = 0;

Bool isEnabled()
{
	// Purely a splitscreen diagnostic: a single viewport cannot leak into another one,
	// so outside splitscreen dev mode the probe is inert and costs nothing.
	return s_enabled && TheSeatManager != nullptr && TheSeatManager->isSplitscreenEnabled();
}

void setEnabled(Bool enabled)
{
	s_enabled = enabled;
}

void beginFrame()
{
	if (!isEnabled())
	{
		s_probeX = s_probeY = -1;
		s_pubRowCount = 0;
		return;
	}

	// publish what the previous frame recorded
	s_pubProbeX        = s_probeX;
	s_pubProbeY        = s_probeY;
	s_pubViewIndex     = s_probedViewIndex;
	s_pubViewPlayer    = s_probedViewPlayer;
	s_pubViewCount     = s_viewCount;
	s_pubConsidered    = s_considered;
	s_pubRowCount      = s_rowCount;
	for (Int i = 0; i < s_rowCount; ++i)
		s_pubRows[i] = s_rows[i];

	// start a new frame, targeted at wherever the mouse is pointing
	s_rowCount        = 0;
	s_viewCount       = 0;
	s_considered      = 0;
	s_probedViewIndex = -1;
	s_probedViewPlayer = -1;
	s_viewProbed      = FALSE;

	if (TheMouse != nullptr)
	{
		const MouseIO* io = TheMouse->getMouseStatus();
		if (io != nullptr)
		{
			s_probeX = io->pos.x;
			s_probeY = io->pos.y;
		}
	}
}

void beginView(Int viewIndex, Int renderPlayerIndex, Int originX, Int originY, Int width, Int height)
{
	s_viewProbed = FALSE;

	if (!isEnabled())
		return;

	++s_viewCount;
	s_viewIndex  = viewIndex;
	s_viewPlayer = renderPlayerIndex;

	if (s_probeX < originX || s_probeX >= originX + width)
		return;
	if (s_probeY < originY || s_probeY >= originY + height)
		return;

	s_viewProbed       = TRUE;
	s_probedViewIndex  = viewIndex;
	s_probedViewPlayer = renderPlayerIndex;
}

void endView()
{
	s_viewProbed = FALSE;
}

Bool isViewProbed()
{
	return s_viewProbed;
}

Bool wantsPixel(Real screenX, Real screenY)
{
	if (!s_viewProbed)
		return FALSE;

	++s_considered;

	const Real dx = screenX - (Real)s_probeX;
	const Real dy = screenY - (Real)s_probeY;
	return (dx * dx + dy * dy) <= (PROBE_RADIUS * PROBE_RADIUS);
}

// Rows are kept sorted nearest-first and the list is capped, so the thing under the
// cursor always survives even when a crowded base fills the radius.
static void insertRow(Real dist, const char* text)
{
	Int slot = s_rowCount;
	if (slot >= PROBE_MAX_ROWS)
	{
		if (dist >= s_rows[PROBE_MAX_ROWS - 1].m_dist)
			return;
		slot = PROBE_MAX_ROWS - 1;
	}
	else
	{
		++s_rowCount;
	}

	while (slot > 0 && s_rows[slot - 1].m_dist > dist)
	{
		s_rows[slot] = s_rows[slot - 1];
		--slot;
	}

	s_rows[slot].m_dist = dist;
	strncpy(s_rows[slot].m_text, text, ROW_CHARS - 1);
	s_rows[slot].m_text[ROW_CHARS - 1] = 0;
}

void record(Real screenX, Real screenY, const char* path, const char* name,
	Int ownerPlayer, Int shroudStatus, Int ghostOwner, const char* decision)
{
	if (!s_viewProbed)
		return;

	const Real dx = screenX - (Real)s_probeX;
	const Real dy = screenY - (Real)s_probeY;
	const Real dist = (Real)sqrt((double)(dx * dx + dy * dy));

	char text[ROW_CHARS];
	snprintf(text, sizeof(text), "%3dpx %-9s %-22s own=P%-2d ss=%-2d ghost=P%-2d -> %s",
		(Int)dist,
		path != nullptr ? path : "?",
		name != nullptr ? name : "?",
		ownerPlayer, shroudStatus, ghostOwner,
		decision != nullptr ? decision : "?");
	text[ROW_CHARS - 1] = 0;

	insertRow(dist, text);
}

void recordf(Real screenX, Real screenY, const char* path, const char* name,
	Int ownerPlayer, Int shroudStatus, Int ghostOwner, const char* decisionFormat, ...)
{
	if (!s_viewProbed)
		return;

	char decision[96];
	va_list args;
	va_start(args, decisionFormat);
	vsnprintf(decision, sizeof(decision), decisionFormat, args);
	va_end(args);
	decision[sizeof(decision) - 1] = 0;

	record(screenX, screenY, path, name, ownerPlayer, shroudStatus, ghostOwner, decision);
}

Int getRowCount()          { return s_pubRowCount; }
const char* getRow(Int i)  { return (i >= 0 && i < s_pubRowCount) ? s_pubRows[i].m_text : ""; }
Int getProbeX()            { return s_pubProbeX; }
Int getProbeY()            { return s_pubProbeY; }
Int getProbedViewIndex()   { return s_pubViewIndex; }
Int getProbedViewPlayer()  { return s_pubViewPlayer; }
Int getViewCount()         { return s_pubViewCount; }
Int getConsideredCount()   { return s_pubConsidered; }

} // namespace RenderLeakProbe
