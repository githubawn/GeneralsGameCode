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

// SeatManager.cpp
//
// Device-independent local-seat model. See SeatManager.h for the design notes.

#include "PreRTS.h" // must be first

#include "Common/SeatManager.h"
#include "Common/Debug.h"
#include "Common/RenderLeakProbe.h"
#include "Common/MessageStream.h"
#include "GameClient/DebugDisplay.h"
#include "GameClient/Display.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/View.h"
#include "GameNetwork/NetworkDefs.h" // TheNetwork
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/NameKeyGenerator.h" // seat->player lookup by "player%d" slot name
#include "GameLogic/GameLogic.h"

SeatManager* TheSeatManager = nullptr;

// Splitscreen input-routing diagnostics (see SeatManager.h).
Int g_dbgSeatMsgCount[MAX_SEATS] = { 0 };
Int g_dbgLastClickSeat = -99;
Int g_dbgLastActiveSeat = -99;
Int g_dbgShroudFills = 0;
Int g_dbgShroudLastPlayer = -99;
Int g_dbgShroudClearCells = -99;
Int g_dbgLobbyClaims = 0;
Int g_dbgLobbyLastSlot = -99;
Int g_dbgSecondaryShroudRenders = 0;
Int g_dbgShroudBindSecondary = 0;
Int g_dbgShroudBindPrimary = 0;
Int g_dbgSeat1AimFound = -1;
Int g_dbgSeat1AimX = -99, g_dbgSeat1AimY = -99;
Int g_dbgSeat1CamX = -99, g_dbgSeat1CamY = -99;
Int g_dbgForceSecondaryClear = 0; // isolation test (off): force 2nd-viewport fog fully clear
Int g_dbgRenderAimStatus = -99;   // LOGICAL shroud status (0=CLEAR,1=FOG,2=SHROUD) for the render player at its own base cell
Int g_dbgRenderAimPlayer = -99;   // which player index the above status was sampled for
Int g_dbgAimCellX = -99, g_dbgAimCellY = -99; // partition cell under the seat-1 base (set by the probe)
Int g_dbgSrcLevelAtBase = -99;    // TEXTURE shroud level (0..255) actually written at the base cell for the render player (255=lit)
Int g_dbgBindOverridePlayer = -99; // render override AT the actual terrain shroud bind (getShroudTexture)
Int g_dbgBindSrcAtBase = -99;      // src shroud level at the base cell AT the terrain shroud bind
Int g_dbgObjRenderPlayer = -99;    // localPlayerIndex used when the scene renders objects (per view)

// Virtual-cursor speed, in game-resolution pixels per frame at full stick
// deflection (~60fps assumed). WP2 uses a fixed per-frame step; a dt-based
// version can replace this later. Precision mode (left trigger) slows it down.
static const Real SEAT_CURSOR_STEP = 15.0f;
static const Real SEAT_CURSOR_PRECISION_SCALE = 0.35f;

// Per-seat camera control (right stick scrolls, d-pad rotates/zooms). Tuned to feel close to the
// keyboard/mouse scroll rate; the stick is analog so these are the full-deflection rates.
static const Real SEAT_CAMERA_SCROLL_STEP  = 14.0f;
static const Real SEAT_CAMERA_ROTATE_STEP  = 0.02f;   // radians per frame while held
static const Real SEAT_CAMERA_ZOOM_STEP    = 0.02f;
static const Real SEAT_CAMERA_ZOOM_MIN     = 0.35f;
static const Real SEAT_CAMERA_ZOOM_MAX     = 2.00f;

// Seat debug overlay. Installed by SeatManager::update() while splitscreen dev
// mode is on; drawn by the display's debug-display facility. Lists the seat
// table and each seat's live input so pads/hotplug/binding can be verified.
static void SeatDebugDisplay(DebugDisplayInterface* dd, void* /*userData*/, FILE* /*fp*/)
{
	if (!dd || !TheSeatManager)
		return;

	// activeSeats = how many seats updateSeatViewports will tile (seat 0 + bound seats
	// with a player). display + inGame/shell reveal whether the split branch even runs.
	Int activeSeats = 1; // seat 0 always
	for (Int j = 1; j < MAX_SEATS; ++j)
	{
		LocalSeat* sj = TheSeatManager->getSeat(j);
		if (sj && sj->m_playerIndex >= 0 && sj->m_deviceId != SEAT_DEVICE_NONE)
			++activeSeats;
	}

	dd->setCursorPos(0, 0);
	dd->setTextColor(DebugDisplayInterface::YELLOW);
	dd->printf("SPLITSCREEN DEV  pads=%d  display=%dx%d  inGame=%d shell=%d  activeSeats=%d  (A/Start to join)\n",
		TheSeatManager->getConnectedDeviceCount(),
		TheDisplay ? TheDisplay->getWidth() : -1, TheDisplay ? TheDisplay->getHeight() : -1,
		TheGameLogic ? (Int)TheGameLogic->isInGame() : -1,
		TheGameLogic ? (Int)TheGameLogic->isInShellGame() : -1,
		activeSeats);
	dd->printf("  ROUTE lastClickSeat=%d lastActiveSeat=%d | SHROUD fills=%d lastPlayer=%d nonLocalClear=%d\n",
		g_dbgLastClickSeat, g_dbgLastActiveSeat, g_dbgShroudFills, g_dbgShroudLastPlayer, g_dbgShroudClearCells);
	dd->printf("  LOBBY claims=%d lastSlot=%d | 2ndShroudRenders=%d bindPrim=%d bindSec=%d\n",
		g_dbgLobbyClaims, g_dbgLobbyLastSlot, g_dbgSecondaryShroudRenders,
		g_dbgShroudBindPrimary, g_dbgShroudBindSecondary);
	dd->printf("  SEAT1CAM aim=(%d,%d) EYEht=%d | P%d LOGICvis=%d TEXlvl=%d(255=lit)\n",
		g_dbgSeat1AimX, g_dbgSeat1AimY, g_dbgSeat1CamY,
		g_dbgRenderAimPlayer, g_dbgRenderAimStatus, g_dbgSrcLevelAtBase);
	dd->printf("  RENDER@BIND: terrainOverride=%d srcAtBase=%d | objRenderPlayer=%d (should be seat-1 player, not local)\n",
		g_dbgBindOverridePlayer, g_dbgBindSrcAtBase, g_dbgObjRenderPlayer);

	// Cursor leak readout: where seat 0's pointer actually is and what it is confined to.
	// If pos leaves the confine rect, the confinement is not being applied; if the confine
	// rect is the whole display, updateSeatViewports never narrowed it for this frame.
	if (TheMouse)
	{
		Int cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;
		TheMouse->getConfineRegion(&cx0, &cy0, &cx1, &cy1);
		const MouseIO *io = TheMouse->getMouseStatus();
		dd->printf("  MOUSE pos=(%d,%d) confine=(%d,%d)-(%d,%d) visible=%d cursor=%d\n",
			io ? io->pos.x : -1, io ? io->pos.y : -1, cx0, cy0, cx1, cy1,
			(Int)TheMouse->getVisibility(), (Int)TheMouse->getMouseCursor());
	}

	// Render-leak probe: point the mouse at whatever is leaking and read the decisions
	// made about it in that viewport. See Common/RenderLeakProbe.h for how to read a row.
	if (RenderLeakProbe::isEnabled())
	{
		dd->setTextColor(DebugDisplayInterface::GREEN);
		dd->printf("  PROBE @(%d,%d) view=%d renderPlayer=P%d views=%d considered=%d rows=%d\n",
			RenderLeakProbe::getProbeX(), RenderLeakProbe::getProbeY(),
			RenderLeakProbe::getProbedViewIndex(), RenderLeakProbe::getProbedViewPlayer(),
			RenderLeakProbe::getViewCount(), RenderLeakProbe::getConsideredCount(),
			RenderLeakProbe::getRowCount());
		for (Int r = 0; r < RenderLeakProbe::getRowCount(); ++r)
			dd->printf("   %s\n", RenderLeakProbe::getRow(r));

		// Shadows drawn/skipped per view. A view showing 0/N drew none of the shadows it was
		// asked about, which is a different fault from a view that was never asked (0/0).
		// NB view index is position in the display list, which attachView PREPENDS to - so the
		// LAST index is seat 0's tactical view, not the first.
		// decal shadows / stencil-volume shadows (unit shadows are the latter) / whether each
		// pass was allowed to run at all (pass=3 means both ran, 0 means neither).
		dd->printf("  SHADOWdecal");
		for (Int v = 0; v < RenderLeakProbe::getViewCount() && v < 8; ++v)
			dd->printf(" v%d=%d/%d", v, RenderLeakProbe::getShadowsDrawn(v), RenderLeakProbe::getShadowsSkipped(v));
		dd->printf("\n  SHADOWvol  ");
		for (Int v = 0; v < RenderLeakProbe::getViewCount() && v < 8; ++v)
			dd->printf(" v%d=%d/%d", v, RenderLeakProbe::getVolumeShadowsDrawn(v), RenderLeakProbe::getVolumeShadowsSkipped(v));
		dd->printf("\n  SHADOWpass ");
		for (Int v = 0; v < RenderLeakProbe::getViewCount() && v < 8; ++v)
			dd->printf(" v%d=%d", v, RenderLeakProbe::getShadowPassRan(v));
		dd->printf("\n  SEATCURSOR %s\n", RenderLeakProbe::getSeatCursorReport());
		for (Int b = 0; b < RenderLeakProbe::getControlBarReportCount(); ++b)
			dd->printf("  %s\n", RenderLeakProbe::getControlBarReport(b));
	}

	dd->setTextColor(DebugDisplayInterface::WHITE);

	for (Int i = 0; i < MAX_SEATS; ++i)
	{
		LocalSeat* s = TheSeatManager->getSeat(i);
		if (!s)
			continue;
		if (s->m_state == SEAT_UNBOUND && i != 0)
			continue;

		const SeatInputState& in = s->m_input;
		dd->printf("seat %d  st=%d dev=%d ply=%d  L(%.2f,%.2f) R(%.2f,%.2f) LT%.2f RT%.2f  A%d B%d X%d Y%d\n",
			i, (Int)s->m_state, s->m_deviceId, s->m_playerIndex,
			in.leftX, in.leftY, in.rightX, in.rightY, in.leftTrigger, in.rightTrigger,
			(Int)in.buttonDown[SEAT_BUTTON_CONFIRM], (Int)in.buttonDown[SEAT_BUTTON_CANCEL],
			(Int)in.buttonDown[SEAT_BUTTON_ACTION], (Int)in.buttonDown[SEAT_BUTTON_ALT_ACTION]);

		// Full linkage readout (control -> seat i -> game slot i -> army -> viewport),
		// so a single glance shows exactly which link is broken. The " =LOCAL!" flag
		// means this seat is bound to the mouse's own player (the "linked to player 1"
		// bug); "vp=NONE" means no viewport was created for it yet.
		const Int localIdx = (ThePlayerList && ThePlayerList->getLocalPlayer())
			? ThePlayerList->getLocalPlayer()->getPlayerIndex() : -1;
		const char* localFlag = (s->m_playerIndex >= 0 && s->m_playerIndex == localIdx) ? " =LOCAL!" : "";
		if (s->m_view != NULL)
		{
			Int ox = 0, oy = 0;
			s->m_view->getOrigin(&ox, &oy);
			dd->printf("        gameSlot=%d lob=%d  army=P%d(engineIdx%d)%s  vp=(%d,%d %dx%d)  msgs=%d\n",
				i, s->m_lobbySlot, i + 1, s->m_playerIndex, localFlag, ox, oy, s->m_view->getWidth(), s->m_view->getHeight(),
				g_dbgSeatMsgCount[i]);
		}
		else
		{
			dd->printf("        gameSlot=%d lob=%d  army=P%d(engineIdx%d)%s  vp=NONE  msgs=%d\n",
				i, s->m_lobbySlot, i + 1, s->m_playerIndex, localFlag, g_dbgSeatMsgCount[i]);
		}
	}
}

// LocalSeat

LocalSeat::LocalSeat()
{
	reset(-1);
}

void LocalSeat::reset(Int seatIndex)
{
	m_seatIndex   = seatIndex;
	m_state       = SEAT_UNBOUND;
	m_deviceId    = SEAT_DEVICE_NONE;
	m_playerIndex = -1;
	m_lobbySlot   = -1;
	m_cursor      = VirtualCursor();
	m_cursorFX    = 0.0f;
	m_cursorFY    = 0.0f;
	m_cursorInit  = FALSE;
	m_view        = nullptr;
	m_input.clear();
}

void LocalSeat::clearMatchState()
{
	// Keep the device binding (pads stay bound between matches); drop per-match
	// state so a bound seat returns to a menu-ready condition.
	m_playerIndex = -1;
	m_view        = nullptr;
	if (m_state == SEAT_IN_GAME || m_state == SEAT_IN_LOBBY)
		m_state = SEAT_BOUND;
}

// SeatManager

SeatManager::SeatManager()
	: m_enabled(FALSE)
	, m_connectedDevices(0)
	, m_cursorsUnconfined(FALSE)
	, m_seat0SoftwareCursor(FALSE)
	, m_seat0DeviceId(SEAT_DEVICE_NONE)
{
}

void SeatManager::setSeat0UsesSoftwareCursor(Bool on)
{
	if (m_seat0SoftwareCursor == on)
		return;

	m_seat0SoftwareCursor = on;

	if (TheMouse)
		TheMouse->setVisibility(!on);

	if (!on)
	{
		m_seats[0].m_cursor.visible = FALSE;
	}
	else
	{
		updateSeat0Cursor();
	}
}

/** Mirror the OS pointer into seat 0's virtual cursor. The engine already clamps
	m_currMouse.pos to seat 0's confinement rect (Mouse::moveMouse), so this position is
	inside seat 0's viewport even when the physical pointer is not. */
void SeatManager::updateSeat0Cursor()
{
	if (!m_seat0SoftwareCursor || TheMouse == nullptr)
		return;

	const MouseIO* io = TheMouse->getMouseStatus();
	if (io == nullptr)
		return;

	LocalSeat& s = m_seats[0];
	s.m_cursor.pos        = io->pos;
	s.m_cursor.cursorType = (Int)TheMouse->getMouseCursor();
	s.m_cursor.visible    = TRUE;
}

SeatManager::~SeatManager()
{
}

void SeatManager::init()
{
	for (Int i = 0; i < MAX_SEATS; ++i)
		m_seats[i].reset(i);

	// Seat 0 is always bound: it is the keyboard/mouse local player, the one
	// ThePlayerList->getLocalPlayer() refers to. Its m_deviceId stays NONE because the
	// keyboard and mouse are its devices; a pad may additionally act for it (see
	// routeDeviceInput/m_seat0DeviceId), which is what the legacy injection now is.
	m_seats[0].m_state    = SEAT_BOUND;
	m_seats[0].m_deviceId = SEAT_DEVICE_NONE;
	m_seat0DeviceId       = SEAT_DEVICE_NONE;

	logSeatTable();
}

void SeatManager::reset()
{
	// Bindings survive reset (a subsystem reset happens between matches); only
	// per-match state is cleared.
	for (Int i = 0; i < MAX_SEATS; ++i)
		m_seats[i].clearMatchState();
}

void SeatManager::update()
{
	// Cursor integration and stream-message emission arrive in WP2. For now, in
	// splitscreen dev mode, lazily install the seat debug overlay once the display
	// exists (do not stomp another active debug display).
	if (m_enabled && TheDisplay && TheDisplay->getDebugDisplayCallback() == NULL)
		TheDisplay->setDebugDisplayCallback(SeatDebugDisplay);
}

LocalSeat* SeatManager::getSeat(Int i)
{
	if (i < 0 || i >= MAX_SEATS)
		return nullptr;
	return &m_seats[i];
}

Int SeatManager::getBoundSeatCount() const
{
	Int count = 0;
	for (Int i = 0; i < MAX_SEATS; ++i)
	{
		SeatState s = m_seats[i].m_state;
		if (s == SEAT_BOUND || s == SEAT_IN_LOBBY || s == SEAT_IN_GAME)
			++count;
	}
	return count;
}

Bool SeatManager::isJoiningAllowed() const
{
	// Never allow local joins during a network game (Invariant C), and only when
	// splitscreen dev mode is on.
	if (TheNetwork != nullptr)
		return FALSE;
	return m_enabled;
}

Int SeatManager::findFreeSeat() const
{
	// Seat 0 is reserved for the keyboard/mouse; devices claim seats 1..N. Return
	// the LOWEST seat that is free OR was abandoned by a lost device, so controllers
	// pack at low seat indices. Without reclaiming DEVICE_LOST seats, a replugged pad
	// (SDL hands out a new instance id each time) would drift to seat 2, 3, ... and
	// then take the wrong slot/army. A genuine reconnect of the SAME id is handled
	// before this (getSeatForDevice), so reclaiming a lost seat here is safe.
	for (Int i = 1; i < MAX_SEATS; ++i)
		if (m_seats[i].m_state == SEAT_UNBOUND || m_seats[i].m_state == SEAT_DEVICE_LOST)
			return i;
	return -1;
}

Int SeatManager::getSeatForDevice(Int deviceId) const
{
	if (deviceId == SEAT_DEVICE_NONE)
		return -1;
	for (Int i = 0; i < MAX_SEATS; ++i)
		if (m_seats[i].m_deviceId == deviceId)
			return i;
	return -1;
}

Int SeatManager::bindSeatToDevice(Int deviceId)
{
	if (deviceId == SEAT_DEVICE_NONE)
		return -1;

	if (!isJoiningAllowed())
		return -1;

	// A reconnecting device reclaims its lost seat.
	Int existing = getSeatForDevice(deviceId);
	if (existing >= 0)
	{
		if (m_seats[existing].m_state == SEAT_DEVICE_LOST)
			m_seats[existing].m_state = SEAT_BOUND;
		return existing;
	}

	Int seat = findFreeSeat();

	// A REAL controller must never be locked out by the -splitscreendev fake seats. If every seat
	// is taken, evict the highest fake one and give this device its place: the fakes exist only to
	// pad the seat count for testing, and the map's slot limit means they can otherwise consume
	// every slot and leave a real pad with no seat, no player, and no working buttons at all.
	if (seat < 0)
	{
		for (Int i = MAX_SEATS - 1; i >= 1; --i)
		{
			if (m_seats[i].m_deviceId <= SEAT_DEVICE_FAKE_BASE)
			{
				seat = i;
				DEBUG_LOG(("SeatManager: real device %d evicts fake seat %d", deviceId, i));
				break;
			}
		}
	}

	if (seat < 0)
	{
		DEBUG_LOG(("SeatManager: no free seat for device %d", deviceId));
		return -1;
	}

	// Clear any stale state from a previously-lost device before this new one takes
	// the seat (playerIndex, lobbySlot, cursor, view), so the seat starts clean.
	m_seats[seat].reset(seat);
	m_seats[seat].m_deviceId = deviceId;
	m_seats[seat].m_state    = SEAT_BOUND;
	DEBUG_LOG(("SeatManager: bound device %d to seat %d", deviceId, seat));
	logSeatTable();
	return seat;
}

void SeatManager::bindFakeSeats(Int count)
{
	if (!m_enabled || count <= 0)
		return;

	// Seat 0 is always the keyboard/mouse, so fake seats start at 1. Whatever is left over stays
	// free, which is what lets a real controller join AFTER the fakes and land in a higher seat.
	Int bound = 0;
	for (Int i = 1; i < MAX_SEATS && bound < count; ++i)
	{
		if (m_seats[i].m_state != SEAT_UNBOUND)
			continue;

		m_seats[i].reset(i);
		m_seats[i].m_deviceId = SEAT_DEVICE_FAKE_BASE - i;
		m_seats[i].m_state    = SEAT_BOUND;
		++bound;
	}

	DEBUG_LOG(("SeatManager: pre-bound %d fake seat(s) of %d requested", bound, count));
	if (bound < count)
		DEBUG_LOG(("SeatManager: only %d seats available for fakes (MAX_SEATS=%d, seat 0 is keyboard/mouse)",
			MAX_SEATS - 1, MAX_SEATS));
	logSeatTable();
}

void SeatManager::unbindSeat(Int seatIndex)
{
	// Seat 0 (keyboard/mouse) is never unbound.
	if (seatIndex <= 0 || seatIndex >= MAX_SEATS)
		return;

	if (m_seats[seatIndex].m_state == SEAT_UNBOUND)
		return;

	DEBUG_LOG(("SeatManager: releasing seat %d (device %d)", seatIndex, m_seats[seatIndex].m_deviceId));
	m_seats[seatIndex].reset(seatIndex);
	logSeatTable();
}

void SeatManager::onDeviceDisconnected(Int deviceId)
{
	Int seat = getSeatForDevice(deviceId);
	if (seat <= 0)
		return;

	LocalSeat& s = m_seats[seat];
	DEBUG_LOG(("SeatManager: device %d for seat %d disconnected", deviceId, seat));
	s.m_state = SEAT_DEVICE_LOST;
	s.m_input.clear();
	logSeatTable();
}

void SeatManager::onDeviceRemoved(Int deviceId)
{
	// Free the seat-0 role so the next pad can take over the mouse/keyboard.
	if (m_seat0DeviceId == deviceId)
	{
		m_seat0DeviceId = SEAT_DEVICE_NONE;
		m_seats[0].m_input.clear();
	}

	onDeviceDisconnected(deviceId);
}

void SeatManager::setSeatInput(Int seatIndex, const SeatInputState& state)
{
	LocalSeat* s = getSeat(seatIndex);
	if (!s)
		return;

	// A reconnected device resumes its seat automatically once input flows again.
	if (s->m_state == SEAT_DEVICE_LOST)
		s->m_state = SEAT_BOUND;

	// Levels (sticks, triggers, held buttons) are a snapshot and simply replace what was there.
	//
	// The EDGES are different, and getting this wrong is what made a pad seat able to move its
	// camera but never click. buttonPressed/buttonReleased are computed in the input backend
	// against the previous poll and are true for exactly ONE poll; the backend then advances its
	// own "previous" state. Whoever polls next therefore produces a state with every edge false.
	// Polling and consuming are not the same clock - the SDL event pump runs whenever the engine
	// services the OS, while createStreamMessages() runs once per client update - so an A-press
	// could be, and routinely was, overwritten before anything looked at it. Levels survived that
	// (they are re-read every poll), which is exactly why the stick worked and the buttons did
	// not.
	//
	// So edges are LATCHED here and cleared only by the consumer (consumeSeatInputEdges). A press
	// and a release arriving in the same window both stick, which is the honest reading of "this
	// button was tapped since you last looked".
	s->m_input.leftX  = state.leftX;
	s->m_input.leftY  = state.leftY;
	s->m_input.rightX = state.rightX;
	s->m_input.rightY = state.rightY;
	s->m_input.leftTrigger  = state.leftTrigger;
	s->m_input.rightTrigger = state.rightTrigger;

	for (Int b = 0; b < SEAT_BUTTON_COUNT; ++b)
	{
		s->m_input.buttonDown[b] = state.buttonDown[b];
		if (state.buttonPressed[b])
			s->m_input.buttonPressed[b] = TRUE;
		if (state.buttonReleased[b])
			s->m_input.buttonReleased[b] = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
/** Splitscreen: drop the latched edges once they have been turned into messages. See
	setSeatInput for why they are latched rather than overwritten. */
//-------------------------------------------------------------------------------------------------
void SeatManager::consumeSeatInputEdges(Int seatIndex)
{
	LocalSeat* s = getSeat(seatIndex);
	if (!s)
		return;

	for (Int b = 0; b < SEAT_BUTTON_COUNT; ++b)
	{
		s->m_input.buttonPressed[b]  = FALSE;
		s->m_input.buttonReleased[b] = FALSE;
	}
}

Int SeatManager::routeDeviceInput(Int deviceId, const SeatInputState& state)
{
	if (deviceId == SEAT_DEVICE_NONE)
		return -1;

	// A pad that already has a seat of its own keeps it. Note this is checked even with
	// splitscreen off, so a seat bound before the flag was cleared cannot silently revert
	// to driving the shared mouse.
	Int seat = getSeatForDevice(deviceId);
	if (seat > 0)
	{
		setSeatInput(seat, state);
		return seat;
	}

	// An unseated pad asking to join takes the next free seat, if joining is allowed.
	if (state.buttonPressed[SEAT_BUTTON_JOIN] || state.buttonPressed[SEAT_BUTTON_CONFIRM])
	{
		seat = bindSeatToDevice(deviceId);
		if (seat > 0)
		{
			setSeatInput(seat, state);

			// The button press that JOINED the seat has been spent joining. Leaving it latched
			// makes the seat's first act a click at wherever its cursor was seeded, which is how
			// a pad used to select something the instant it joined.
			consumeSeatInputEdges(seat);

			// It has stopped acting for seat 0, so the next unseated pad may take over.
			if (m_seat0DeviceId == deviceId)
				m_seat0DeviceId = SEAT_DEVICE_NONE;

			return seat;
		}
	}

	// Everything left over belongs to seat 0, the keyboard/mouse seat - but only ONE pad
	// at a time, or two of them would fight over the single OS pointer. The first unseated
	// pad to arrive holds the role until it joins a seat or is unplugged.
	if (m_seat0DeviceId == SEAT_DEVICE_NONE || getSeatForDevice(m_seat0DeviceId) > 0)
		m_seat0DeviceId = deviceId;

	if (m_seat0DeviceId != deviceId)
		return -1;

	// Seat 0's pad state is recorded like any other seat's, so the debug overlay and any
	// future seat-0 pad handling read it from the same place as every other seat. Nothing
	// consumes seat 0's edges - the backend's legacy injection handles its buttons directly -
	// so they are dropped here rather than latched, or they would never clear.
	setSeatInput(0, state);
	consumeSeatInputEdges(0);
	return 0;
}

void SeatManager::setSeatPlayerIndex(Int seatIndex, Int playerIndex)
{
	LocalSeat* s = getSeat(seatIndex);
	if (s)
		s->m_playerIndex = playerIndex;
}

// Splitscreen: which keyboard key a pad button stands in for.
//
// Deliberately a mapping to KEYS and not to commands. Everything downstream - CommandMap.ini,
// MetaEventTranslator, every MSG_META_* the game defines - is then shared with the keyboard, so
// there is exactly one input wiring to keep working rather than a keyboard one and a pad one.
// Re-binding a command in CommandMap.ini re-binds it for pads too.
//
// KEY_NONE means the button is not a key at all: the sticks and face buttons that act as mouse
// clicks, the shift-equivalent modifier, and the seat join/leave buttons are handled directly.
static Int seatButtonToKey(SeatButton button)
{
	switch (button)
	{
		case SEAT_BUTTON_ALT_ACTION:	return KEY_S;		// stop
		case SEAT_BUTTON_ACTION:		return KEY_T;		// select matching units
		case SEAT_BUTTON_CURSOR_CLICK:	return KEY_E;		// next idle worker
		case SEAT_BUTTON_CAMERA_RESET:	return KEY_SPACE;	// view command center
		case SEAT_BUTTON_COMMAND_BAR:	return KEY_TAB;		// toggle/focus the control bar
		case SEAT_BUTTON_LEAVE:			return KEY_ESC;		// cancel / menu

		// Not keys: clicks, the shift modifier, and seat management.
		case SEAT_BUTTON_CONFIRM:
		case SEAT_BUTTON_CANCEL:
		case SEAT_BUTTON_MODIFIER:
		case SEAT_BUTTON_JOIN:
		case SEAT_BUTTON_DPAD_UP:
		case SEAT_BUTTON_DPAD_DOWN:
		case SEAT_BUTTON_DPAD_LEFT:
		case SEAT_BUTTON_DPAD_RIGHT:
		default:
			return KEY_NONE;
	}
}

void SeatManager::createStreamMessages()
{
	// WP3: integrate each bound seat's own virtual cursor from its left stick.
	// This is purely additive - it does NOT touch the OS mouse and does NOT append
	// any GameMessages (translators are not seat-aware until WP5). Each seat's
	// software cursor is drawn by W3DSeatCursorRenderer.
	if (!m_enabled || !TheDisplay)
		return;

	// Seat 0's software cursor follows the OS pointer every frame while the screen is split.
	updateSeat0Cursor();

	const Real width  = (Real)TheDisplay->getWidth();
	const Real height = (Real)TheDisplay->getHeight();

	// Seats 1.. are physical devices (seat 0 is the keyboard/mouse, which keeps
	// the OS cursor and is never integrated here).
	for (Int i = 1; i < MAX_SEATS; ++i)
	{
		LocalSeat& s = m_seats[i];
		if (s.m_state != SEAT_BOUND && s.m_state != SEAT_IN_LOBBY && s.m_state != SEAT_IN_GAME)
			continue;
		if (s.m_deviceId == SEAT_DEVICE_NONE)
			continue;

		// Clean 1:1:1:1 linkage (control = player = army = viewport, all keyed by the
		// seat index): seat i drives game slot i, which GameLogic created as the player
		// named "player<i>" (seat 0 = kbd/mouse = slot 0 = the local player, handled
		// elsewhere). The skirmish lobby places a player in game slot i for each bound
		// seat i, so "player<i>" exists here; we bind the seat to it and convert it from
		// AI to human so the controller drives that army instead of fighting a brain.
		if (s.m_playerIndex < 0 && ThePlayerList && TheNameKeyGenerator && TheGameLogic
				&& TheGameLogic->isInGame() && !TheGameLogic->isInShellGame())
		{
			Player *local = ThePlayerList->getLocalPlayer();

			AsciiString pname;
			pname.format("player%d", i); // seat index == game slot
			Player *target = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(pname));

			if (target != NULL && target != local && target->isPlayableSide())
			{
				if (target->getPlayerType() == PLAYER_COMPUTER)
					target->setPlayerType(PLAYER_HUMAN, FALSE); // suppress the AI brain; the seat drives it
				s.m_playerIndex = target->getPlayerIndex();
				s.m_state = SEAT_IN_GAME;
				DEBUG_LOG(("SeatManager: seat %d -> game slot %d -> player index %d (now human)",
					i, i, s.m_playerIndex));
			}
		}

		// Clamp bounds: this seat's viewport rect if it has one (splitscreen), else
		// the full display. Keeps the cursor inside the seat's half and makes its
		// picking coordinates land in that view.
		Real minX = 0.0f, minY = 0.0f, maxX = width, maxY = height;
		if (s.m_view != NULL && !m_cursorsUnconfined)
		{
			Int ox = 0, oy = 0;
			s.m_view->getOrigin(&ox, &oy);
			minX = (Real)ox;
			minY = (Real)oy;
			maxX = (Real)(ox + s.m_view->getWidth());
			maxY = (Real)(oy + s.m_view->getHeight());
		}

		// Seed the cursor to the center of its viewport the first time.
		if (!s.m_cursorInit)
		{
			s.m_cursorFX = (minX + maxX) * 0.5f;
			s.m_cursorFY = (minY + maxY) * 0.5f;
			s.m_cursorInit = TRUE;
		}

		// Precision mode (hold left trigger) slows the cursor for fine aiming.
		Real step = SEAT_CURSOR_STEP;
		if (s.m_input.leftTrigger > 0.5f)
			step *= SEAT_CURSOR_PRECISION_SCALE;

		Int prevX = s.m_cursor.pos.x;
		Int prevY = s.m_cursor.pos.y;

		s.m_cursorFX += s.m_input.leftX * step;
		s.m_cursorFY += s.m_input.leftY * step;

		if (s.m_cursorFX < minX) s.m_cursorFX = minX;
		if (s.m_cursorFX > maxX) s.m_cursorFX = maxX;
		if (s.m_cursorFY < minY) s.m_cursorFY = minY;
		if (s.m_cursorFY > maxY) s.m_cursorFY = maxY;

		s.m_cursor.pos.x  = (Int)s.m_cursorFX;
		s.m_cursor.pos.y  = (Int)s.m_cursorFY;
		s.m_cursor.visible = TRUE;

		// WP5: emit seat-tagged raw mouse messages so the translators (run through
		// the scoped active seat) act on THIS seat's selection and player. Unlike
		// WP2 this never moves the OS mouse - the seat has its own on-screen cursor.
		if (!TheMessageStream)
			continue;

		const SeatInputState& in = s.m_input;
		const ICoord2D pos = s.m_cursor.pos;
		ICoord2D delta;
		delta.x = pos.x - prevX;
		delta.y = pos.y - prevY;
		// Pad modifiers: left shoulder = shift (queue / add-to-selection), right
		// trigger = ctrl - folded into the click modifier flags like the legacy pad.
		Int mods = TheKeyboard ? TheKeyboard->getModifierFlags() : 0;
		if (in.buttonDown[SEAT_BUTTON_MODIFIER])
			mods |= KEY_STATE_LSHIFT;
		if (in.rightTrigger > 0.5f)
			mods |= KEY_STATE_LCONTROL;
		const Int msgTime = TheMouse ? (Int)TheMouse->getMouseStatus()->time : 0;
		GameMessage* m = NULL;

		// Only emit a position message when the cursor actually moves. An idle
		// controller must stay silent so it cannot interfere with the mouse's
		// selection/drag on the shared translators.
		if (delta.x != 0 || delta.y != 0)
		{
			m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_POSITION);
			m->appendPixelArgument(pos);
			m->appendIntegerArgument(mods);
			m->friend_setSeatIndex(i);
		}

		if (in.buttonPressed[SEAT_BUTTON_CONFIRM])
		{
			m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN);
			m->appendPixelArgument(pos); m->appendIntegerArgument(mods); m->appendIntegerArgument(msgTime); m->friend_setSeatIndex(i);
			++g_dbgSeatMsgCount[i]; // diag: a click was emitted for this seat
		}
		else if (in.buttonReleased[SEAT_BUTTON_CONFIRM])
		{
			m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP);
			m->appendPixelArgument(pos); m->appendIntegerArgument(mods); m->appendIntegerArgument(msgTime); m->friend_setSeatIndex(i);
		}
		else if (in.buttonDown[SEAT_BUTTON_CONFIRM] && (delta.x != 0 || delta.y != 0))
		{
			m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_LEFT_DRAG);
			m->appendPixelArgument(pos); m->appendPixelArgument(delta); m->appendIntegerArgument(mods); m->friend_setSeatIndex(i);
		}

		if (in.buttonPressed[SEAT_BUTTON_CANCEL])
		{
			m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN);
			m->appendPixelArgument(pos); m->appendIntegerArgument(mods); m->appendIntegerArgument(msgTime); m->friend_setSeatIndex(i);
			++g_dbgSeatMsgCount[i]; // diag: a right-click was emitted for this seat
		}
		else if (in.buttonReleased[SEAT_BUTTON_CANCEL])
		{
			m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP);
			m->appendPixelArgument(pos); m->appendIntegerArgument(mods); m->appendIntegerArgument(msgTime); m->friend_setSeatIndex(i);
		}

		// Seat-tagged meta commands (routed to this seat's player/selection by the
		// scoped active seat in MessageStream::propagateMessages), mirroring the
		// Buttons are delivered as seat-tagged RAW KEY events, exactly as the keyboard delivers
		// them, rather than as a hand-picked list of MSG_META_* messages.
		//
		// This is the difference between one input wiring and two. Seat 0 gets every command the
		// game has for free, because its keys run through CommandMap -> MetaEventTranslator. A
		// pad that emits meta messages directly bypasses all of that, so every control has to be
		// re-implemented here and will always trail the keyboard. Feeding the same pipeline means
		// control groups, abilities, attack-move, waypoint mode and anything added later work for
		// a pad seat the moment they work for the keyboard, and are re-bindable in CommandMap.ini
		// like everything else.
		//
		// The seat tag is what keeps them apart: MessageStream scopes the acting seat and player
		// around each translation, and derived messages inherit it.
		for (Int b = 0; b < SEAT_BUTTON_COUNT; ++b)
		{
			const Int key = seatButtonToKey((SeatButton)b);
			if (key == KEY_NONE)
				continue;	// handled as a click, a modifier, or seat management

			if (in.buttonPressed[b])
			{
				m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_KEY_DOWN);
				m->appendIntegerArgument(key);
				m->appendIntegerArgument(KEY_STATE_DOWN | mods);
				m->friend_setSeatIndex(i);
				++g_dbgSeatMsgCount[i];
			}
			else if (in.buttonReleased[b])
			{
				m = TheMessageStream->appendMessage(GameMessage::MSG_RAW_KEY_UP);
				m->appendIntegerArgument(key);
				m->appendIntegerArgument(KEY_STATE_UP | mods);
				m->friend_setSeatIndex(i);
				++g_dbgSeatMsgCount[i];
			}
		}

		// Camera: right stick scrolls THIS seat's own view, d-pad rotates and zooms it. WP6 gave
		// every seat its own view, so camera control no longer has to fall back to the shared
		// tactical camera. The scroll vector is rotated into the view's own frame, so "up" on the
		// stick is always "up" on that seat's screen whatever its camera yaw happens to be.
		if (s.m_view != NULL)
		{
			if (in.rightX != 0.0f || in.rightY != 0.0f)
			{
				const Real angle = s.m_view->getAngle();
				const Real sinA  = Sin(angle);
				const Real cosA  = Cos(angle);
				const Real dx    = in.rightX * SEAT_CAMERA_SCROLL_STEP;
				// SDL reports stick-up as negative Y, and the view's forward axis already points
				// that way once rotated below - so passing it through un-negated scrolls the map
				// in the direction the stick is pushed.
				const Real dy    = in.rightY * SEAT_CAMERA_SCROLL_STEP;

				Coord2D scroll;
				scroll.x = dx * cosA - dy * sinA;
				scroll.y = dx * sinA + dy * cosA;
				s.m_view->scrollBy(&scroll);
			}

			if (in.buttonDown[SEAT_BUTTON_DPAD_LEFT])
				s.m_view->setAngle(s.m_view->getAngle() - SEAT_CAMERA_ROTATE_STEP);
			if (in.buttonDown[SEAT_BUTTON_DPAD_RIGHT])
				s.m_view->setAngle(s.m_view->getAngle() + SEAT_CAMERA_ROTATE_STEP);

			if (in.buttonDown[SEAT_BUTTON_DPAD_UP] != in.buttonDown[SEAT_BUTTON_DPAD_DOWN])
			{
				Real zoom = s.m_view->getZoom();
				zoom += in.buttonDown[SEAT_BUTTON_DPAD_UP] ? -SEAT_CAMERA_ZOOM_STEP : SEAT_CAMERA_ZOOM_STEP;
				if (zoom < SEAT_CAMERA_ZOOM_MIN) zoom = SEAT_CAMERA_ZOOM_MIN;
				if (zoom > SEAT_CAMERA_ZOOM_MAX) zoom = SEAT_CAMERA_ZOOM_MAX;
				s.m_view->setZoom(zoom);
			}
		}

		// Every edge above has now been turned into a message. Drop them; the levels stay.
		// MUST be last - 'in' aliases s.m_input.
		consumeSeatInputEdges(i);
	}
}

void SeatManager::logSeatTable() const
{
	DEBUG_LOG(("SeatManager: --- seat table (%d bound) ---", getBoundSeatCount()));
	for (Int i = 0; i < MAX_SEATS; ++i)
	{
		const LocalSeat& s = m_seats[i];
		if (s.m_state == SEAT_UNBOUND && i != 0)
			continue;
		DEBUG_LOG(("  seat %d: state=%d device=%d player=%d", i, (Int)s.m_state, s.m_deviceId, s.m_playerIndex));
	}
}
