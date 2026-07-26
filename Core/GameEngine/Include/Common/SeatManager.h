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

// Device ids for FAKE seats from the dev harness (-splitscreendev <n>). Not real SDL joystick
// ids - those are non-negative - but distinct from SEAT_DEVICE_NONE so the lobby-claim code
// treats a fake seat as device-backed and gives it a slot. Seat i gets SEAT_DEVICE_FAKE_BASE - i.
enum { SEAT_DEVICE_FAKE_BASE = -100 };

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
	Int            m_lobbySlot;    // skirmish slot this seat claimed in the lobby, -1 = none
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

	// Dev harness: pre-bind 'count' seats with no real device, starting at seat 1 (seat 0 is
	// always the keyboard/mouse). Lets seat layouts and per-seat rendering be exercised without
	// owning that many pads. Real controllers join into whatever seats are left.
	void bindFakeSeats(Int count);
	void unbindSeat(Int seatIndex);

	// A device disappeared (unplugged); its seat, if any, goes SEAT_DEVICE_LOST.
	void onDeviceDisconnected(Int deviceId);

	// As above, and also releases the device from the seat-0 role if it held it. This is
	// what the backend calls on an unplug now that the seat layer owns every pad.
	void onDeviceRemoved(Int deviceId);

	// True when a device pressing "join" may claim a seat right now: splitscreen
	// dev mode is enabled and no network game is active (Invariant C).
	Bool isJoiningAllowed() const;

	// Find the seat bound to a device, or -1.
	Int getSeatForDevice(Int deviceId) const;

	// The device backend hands one frame of logical input per bound seat.
	void setSeatInput(Int seatIndex, const SeatInputState& state);

	// Every pad goes through here, every frame, splitscreen or not. The seat layer owns
	// the whole device population: it decides which seat a pad belongs to, and the answer
	// is the ONLY thing the backend branches on. Before this there were two parallel input
	// systems - the legacy pad->mouse/keyboard injection and the seat path - each deciding
	// for itself which pad it was entitled to; a per-seat control bar is not clickable
	// until one layer owns every device.
	//
	// Returns the seat that owns this device:
	//   >0  a seat of its own - it drives that seat's cursor and seat-tagged messages,
	//       and the backend must NOT inject anything for it.
	//    0  seat 0, the keyboard/mouse seat: the backend runs its legacy injection, which
	//       is now simply "what seat 0 does with a pad" rather than a separate system.
	//   -1  no seat: another pad is already acting for seat 0, so this one stays idle
	//       until it joins.
	Int routeDeviceInput(Int deviceId, const SeatInputState& state);

	// The device currently acting for seat 0 (the one allowed to drive the OS
	// mouse/keyboard), or SEAT_DEVICE_NONE.
	Int getSeat0DeviceId() const { return m_seat0DeviceId; }

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

	// When a full-screen menu (e.g. the Escape/quit menu) is up, seat cursors are
	// freed from their viewport so they can reach it; they re-confine when it closes.
	void setCursorsUnconfined(Bool b) { m_cursorsUnconfined = b; }
	Bool areCursorsUnconfined() const { return m_cursorsUnconfined; }

	// Seat 0's pointer is the OS cursor, which the operating system draws over the whole
	// window - it is the one cursor the seat layer does not own, and the one that shows up
	// in everybody else's viewport. Confining it relies on the window manager honouring a
	// clip rect, which is not something the game can guarantee. So while the screen is
	// split, hide the OS cursor and let seat 0 draw a software cursor like every other
	// seat: the seat's cursor position is already clamped to its own viewport by the
	// engine, so it cannot escape no matter what the OS does with the physical pointer.
	// Off => the OS cursor comes back exactly as it was (single-player is untouched).
	void setSeat0UsesSoftwareCursor(Bool on);
	Bool seat0UsesSoftwareCursor() const { return m_seat0SoftwareCursor; }

private:
	Int findFreeSeat() const;
	void logSeatTable() const;
	void updateSeat0Cursor();

	LocalSeat m_seats[MAX_SEATS];
	Bool      m_enabled;           // splitscreen dev mode
	Int       m_connectedDevices;  // open input devices (debug overlay)
	Bool      m_cursorsUnconfined; // free seat cursors from viewports (menu open)
	Bool      m_seat0SoftwareCursor; // seat 0 draws its own cursor; OS cursor hidden
	Int       m_seat0DeviceId;     // pad acting for seat 0, SEAT_DEVICE_NONE if none
};

extern SeatManager* TheSeatManager;

// Splitscreen input-routing diagnostics (shown live in the seat debug overlay). Written
// from the input/message paths so a single screenshot shows where routing breaks:
//  - g_dbgSeatMsgCount[i]: stream messages seat i has emitted (createStreamMessages)
//  - g_dbgLastClickSeat:   seatIndex stamped on the most recent cooked click (MetaEvent)
//  - g_dbgLastActiveSeat:  active seat during the most recent seat>0 translation
//  - g_dbgShroudFills / g_dbgShroudLastPlayer: per-view shroud refills + last player filled
extern Int g_dbgSeatMsgCount[MAX_SEATS];
extern Int g_dbgLastClickSeat;
extern Int g_dbgLastActiveSeat;
extern Int g_dbgShroudFills;
extern Int g_dbgShroudLastPlayer;
extern Int g_dbgShroudClearCells; // revealed (non-shrouded) cell count for the non-local player's fill
extern Int g_dbgLobbyClaims;      // times the skirmish menu claimed a slot for a controller seat
extern Int g_dbgLobbyLastSlot;    // last skirmish slot claimed by a controller seat
extern Int g_dbgSecondaryShroudRenders; // times W3DShroud::render() wrote the SECOND viewport's fog texture
extern Int g_dbgShroudBindSecondary;    // times the terrain BOUND the secondary fog texture (getShroudTexture)
extern Int g_dbgShroudBindPrimary;      // times the terrain bound the primary fog texture
extern Int g_dbgSeat1AimFound;          // did the seat-1 viewport find player-2's base to aim at? (1/0)
extern Int g_dbgSeat1AimX, g_dbgSeat1AimY;   // world pos the seat-1 camera was aimed at (player-2 base)
extern Int g_dbgSeat1CamX, g_dbgSeat1CamY;   // current world pos the seat-1 camera looks at
// ISOLATION TEST: when 1, the 2nd viewport's fog is forced all-CLEAR (fully lit) regardless
// of real shroud data. If the right view then lights up, the terrain IS sampling dst2 (bug is
// dst2 content); if still black, the terrain is NOT sampling dst2 (bug is bind/UV/pass order).
extern Int g_dbgForceSecondaryClear;
extern Int g_dbgRenderAimStatus; // LOGICAL shroud status (0=CLEAR,1=FOG,2=SHROUD) for the render player at its own base cell
extern Int g_dbgRenderAimPlayer; // which player index g_dbgRenderAimStatus was sampled for
extern Int g_dbgAimCellX, g_dbgAimCellY; // partition cell under the seat-1 base (set by the probe)
extern Int g_dbgSrcLevelAtBase;  // TEXTURE shroud level (0..255) actually written at the base cell for the render player
extern Int g_dbgBindOverridePlayer; // render override AT the actual terrain shroud bind (getShroudTexture)
extern Int g_dbgBindSrcAtBase;      // src shroud level at the base cell AT the terrain shroud bind
extern Int g_dbgObjRenderPlayer;    // localPlayerIndex used when the scene renders objects (per view)
