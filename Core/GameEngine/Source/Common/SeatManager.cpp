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
#include "Common/MessageStream.h"
#include "GameClient/DebugDisplay.h"
#include "GameClient/Display.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameNetwork/NetworkDefs.h" // TheNetwork

SeatManager* TheSeatManager = nullptr;

// Virtual-cursor speed, in game-resolution pixels per frame at full stick
// deflection (~60fps assumed). WP2 uses a fixed per-frame step; a dt-based
// version can replace this later. Precision mode (left trigger) slows it down.
static const Real SEAT_CURSOR_STEP = 15.0f;
static const Real SEAT_CURSOR_PRECISION_SCALE = 0.35f;

// Seat debug overlay. Installed by SeatManager::update() while splitscreen dev
// mode is on; drawn by the display's debug-display facility. Lists the seat
// table and each seat's live input so pads/hotplug/binding can be verified.
static void SeatDebugDisplay(DebugDisplayInterface* dd, void* /*userData*/, FILE* /*fp*/)
{
	if (!dd || !TheSeatManager)
		return;

	dd->setCursorPos(0, 0);
	dd->setTextColor(DebugDisplayInterface::YELLOW);
	dd->printf("SPLITSCREEN DEV  pads connected: %d  (press A/Start on a pad to join)\n",
		TheSeatManager->getConnectedDeviceCount());
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
{
}

SeatManager::~SeatManager()
{
}

void SeatManager::init()
{
	for (Int i = 0; i < MAX_SEATS; ++i)
		m_seats[i].reset(i);

	// Seat 0 is always bound: it is the keyboard/mouse local player, the one
	// ThePlayerList->getLocalPlayer() refers to. It has no physical gamepad.
	m_seats[0].m_state    = SEAT_BOUND;
	m_seats[0].m_deviceId = SEAT_DEVICE_NONE;

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
	// Seat 0 is reserved for the keyboard/mouse; devices claim seats 1..N.
	for (Int i = 1; i < MAX_SEATS; ++i)
		if (m_seats[i].m_state == SEAT_UNBOUND)
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
	if (seat < 0)
	{
		DEBUG_LOG(("SeatManager: no free seat for device %d", deviceId));
		return -1;
	}

	m_seats[seat].m_deviceId = deviceId;
	m_seats[seat].m_state    = SEAT_BOUND;
	DEBUG_LOG(("SeatManager: bound device %d to seat %d", deviceId, seat));
	logSeatTable();
	return seat;
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

void SeatManager::setSeatInput(Int seatIndex, const SeatInputState& state)
{
	LocalSeat* s = getSeat(seatIndex);
	if (!s)
		return;

	// A reconnected device resumes its seat automatically once input flows again.
	if (s->m_state == SEAT_DEVICE_LOST)
		s->m_state = SEAT_BOUND;

	s->m_input = state;
}

void SeatManager::setSeatPlayerIndex(Int seatIndex, Int playerIndex)
{
	LocalSeat* s = getSeat(seatIndex);
	if (s)
		s->m_playerIndex = playerIndex;
}

void SeatManager::createStreamMessages()
{
	// WP3: integrate each bound seat's own virtual cursor from its left stick.
	// This is purely additive - it does NOT touch the OS mouse and does NOT append
	// any GameMessages (translators are not seat-aware until WP5). Each seat's
	// software cursor is drawn by W3DSeatCursorRenderer.
	if (!m_enabled || !TheDisplay)
		return;

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

		// Seed the cursor to screen center the first time this seat is used.
		if (!s.m_cursorInit)
		{
			s.m_cursorFX = width * 0.5f;
			s.m_cursorFY = height * 0.5f;
			s.m_cursorInit = TRUE;
		}

		// Precision mode (hold left trigger) slows the cursor for fine aiming.
		Real step = SEAT_CURSOR_STEP;
		if (s.m_input.leftTrigger > 0.5f)
			step *= SEAT_CURSOR_PRECISION_SCALE;

		s.m_cursorFX += s.m_input.leftX * step;
		s.m_cursorFY += s.m_input.leftY * step;

		if (s.m_cursorFX < 0.0f)   s.m_cursorFX = 0.0f;
		if (s.m_cursorFX > width)  s.m_cursorFX = width;
		if (s.m_cursorFY < 0.0f)   s.m_cursorFY = 0.0f;
		if (s.m_cursorFY > height) s.m_cursorFY = height;

		s.m_cursor.pos.x  = (Int)s.m_cursorFX;
		s.m_cursor.pos.y  = (Int)s.m_cursorFY;
		s.m_cursor.visible = TRUE;
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
