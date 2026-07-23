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

// SeatManager.h
//
// Device-independent local-seat model for splitscreen (WP0 of the splitscreen
// plan; see PatchNotes/splitscreen-plan2.md). A "seat" is one local human: a
// claimed input device, a virtual cursor, a per-seat UI context (kept in
// InGameUI, fetched by index), and an assigned game player index.
//
// Invariants (from splitscreen-plan2.md §1):
//  - Seat 0 is always bound and corresponds to ThePlayerList->getLocalPlayer(),
//    so all legacy single-local-player code keeps working. Seat 0's device is
//    the keyboard/mouse (m_deviceId == -1).
//  - When a network game is active (TheNetwork != nullptr) joining is refused and
//    only seat 0 exists; no splitscreen behavior runs (LAN must not regress).
//
// This header stays free of any device/SDL includes so it can live in the core
// engine and be consumed by both the device layer and client code. All state
// here is strictly client-side and must never be read by GameLogic.

#pragma once

#include "Lib/BaseType.h"
#include "Common/SubsystemInterface.h"
#include "GameClient/SeatInput.h"

// FORWARD REFERENCES
class View;

// Up to 8 local humans on one machine.
enum { MAX_SEATS = 8 };

// Sentinel device id meaning "no physical device" (seat 0 = keyboard/mouse).
enum { SEAT_DEVICE_NONE = -1 };

// Lifecycle of a local seat.
enum SeatState CPP_11(: Int)
{
	SEAT_UNBOUND = 0,   // no device claimed
	SEAT_BOUND,         // device claimed, not yet placed into a lobby/game
	SEAT_IN_LOBBY,      // participating in skirmish setup
	SEAT_IN_GAME,       // controlling a player in a live match
	SEAT_DEVICE_LOST    // was active, device disconnected - awaiting reconnect
};

// A per-seat software cursor, in game-resolution coordinates (same space as
// MouseIO). Populated in WP2; declared here so the seat owns it.
struct VirtualCursor
{
	ICoord2D pos;
	Bool     visible;
	Int      cursorType;   // mirrors MouseCursor enum values

	VirtualCursor()
	{
		pos.zero();
		visible = FALSE;
		cursorType = 0;
	}
};

// A single local human. Owned by SeatManager. Members are public by design; the
// handbook's translator/render code reads them directly (m_playerIndex, m_view).
class LocalSeat
{
public:
	LocalSeat();

	void reset(Int seatIndex);
	void clearMatchState();   // clears per-match fields, keeps device binding

	Int            m_seatIndex;    // 0..MAX_SEATS-1, stable for the session
	SeatState      m_state;
	Int            m_deviceId;     // SDL_JoystickID, -1 = keyboard/mouse/none
	Int            m_playerIndex;  // game Player this seat commands, -1 in menus
	VirtualCursor  m_cursor;       // integer cursor snapshot (game-res coords)
	Real           m_cursorFX;     // sub-pixel cursor accumulator (WP2 integration)
	Real           m_cursorFY;
	Bool           m_cursorInit;   // cursor seeded from the mouse position yet?
	View*          m_view;         // tactical view this seat looks through (WP6)
	SeatInputState m_input;        // latest logical input (WP1)
};

// Global local-seat registry. A SubsystemInterface so it participates in the
// standard init/reset lifecycle; registered in GameEngine init before the client.
class SeatManager : public SubsystemInterface
{
public:
	SeatManager();
	virtual ~SeatManager();

	// SubsystemInterface
	virtual void init() override;
	virtual void reset() override;   // keeps device bindings, clears per-match state
	virtual void update() override;

	LocalSeat* getSeat(Int i);
	Int getBoundSeatCount() const;   // seats in BOUND/IN_LOBBY/IN_GAME

	// Claim a free seat for a device. Returns the seat index, or -1 if none is
	// free or joining is not currently allowed. Refuses in network games.
	Int  bindSeatToDevice(Int deviceId);
	void unbindSeat(Int seatIndex);

	// A device disappeared (unplugged); its seat, if any, goes SEAT_DEVICE_LOST.
	void onDeviceDisconnected(Int deviceId);

	// True when a device pressing "join" may claim a seat right now: splitscreen
	// dev mode is enabled and no network game is active (Invariant C).
	Bool isJoiningAllowed() const;

	// Find the seat bound to a device, or -1.
	Int getSeatForDevice(Int deviceId) const;

	// The device backend hands one frame of logical input per bound seat.
	void setSeatInput(Int seatIndex, const SeatInputState& state);

	// WP2: integrate bound-seat cursors and inject the same raw mouse messages a
	// real mouse produces, tagged with the seat index. Called from
	// GameClient::update right after TheMouse->createStreamMessages().
	void createStreamMessages();

	// Assign the game player a seat commands (set at match start).
	void setSeatPlayerIndex(Int seatIndex, Int playerIndex);

	// Splitscreen dev gate. Off by default; single-player behaves exactly as
	// before while off. Set from the GeneralsMD command-line/GlobalData wiring.
	void setSplitscreenEnabled(Bool enabled) { m_enabled = enabled; }
	Bool isSplitscreenEnabled() const { return m_enabled; }

	// Number of physical input devices the backend currently has open (for the
	// debug overlay). Pushed each frame by the device layer.
	void setConnectedDeviceCount(Int n) { m_connectedDevices = n; }
	Int  getConnectedDeviceCount() const { return m_connectedDevices; }

private:
	Int findFreeSeat() const;
	void logSeatTable() const;

	LocalSeat m_seats[MAX_SEATS];
	Bool      m_enabled;           // splitscreen dev mode
	Int       m_connectedDevices;  // open input devices (debug overlay)
};

extern SeatManager* TheSeatManager;
