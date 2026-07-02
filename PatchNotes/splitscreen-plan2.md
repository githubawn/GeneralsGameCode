# Splitscreen Plan 2 — Implementation Handbook

Companion to `splitscreen-plan.md` (the *what* and *why*). This document is the
*how*: mechanical, file-level instructions written for an implementing agent that
has not read the rest of the conversation. Follow the work packages in order.
Line numbers are anchors from July 2026 on `feature/sdl3-input-backport`; if a
line moved, search for the quoted identifier instead.

---

## 0. Ground rules for the implementing agent

1. **Work only in `GeneralsMD/` and `Core/`.** Do not touch `Generals/` (port
   happens later, as a separate pass).
2. **Single-player must stay behaviorally identical** after every work package.
   With one seat and no `-splitscreen` flag, the game must play exactly as before.
3. **Never modify GameLogic determinism.** Anything under `GameLogic/` that reads
   per-frame state must not branch on seat/viewport/render state. Client (render,
   UI, input) code may. If you are unsure which side a file is on:
   `GameLogic/` = sim, `GameClient/` + `GameEngineDevice/` = client.
4. **Do not delete or replace singletons.** Every refactor here keeps the global
   pointer alive and forwarding (patterns in §2).
5. **No new GUI designs.** The in-match UI is the existing `ControlBar.wnd`,
   instanced and repositioned (WP8). Do not invent menus, radial UIs, or HUDs.
6. Compile after each numbered step where feasible. Config: the existing CMake
   presets in `CMakePresets.json` (ask the user which preset they build with
   before starting; do not guess).
7. New files copy the GPL header block from a neighboring file in the same
   directory (use the line-comment style found in recently touched files like
   `SDL3Input.h`, not the old block-banner style).
8. New engine code that is device-independent goes in
   `Core/GameEngine/{Include,Source}/Common/` or `.../GameClient/`; SDL3-specific
   code goes in `Core/GameEngineDevice/{Include,Source}/SDL3Device/GameClient/`;
   W3D-specific rendering goes in `Core/GameEngineDevice/.../W3DDevice/`.
   Register new .cpp files in the `CMakeLists.txt` of the owning target
   (`Core/GameEngine/CMakeLists.txt`, `Core/GameEngineDevice/...`): find where a
   sibling file is listed and add yours in sorted order.
9. Every new run-time behavior is gated: read a `Bool m_splitscreenEnabled` style
   flag (add to `GlobalData` alongside similar debug flags; INI-parse optional).
   Flag off ⇒ code path inert.
10. Commit per work package (or smaller), message prefix `splitscreen:`.

---

## 1. Concepts and invariants (memorize)

- **Seat** = a local human. `0..MAX_SEATS-1` (`MAX_SEATS = 8`). Owns: one input
  device, one virtual cursor, one UI context (selection etc.), an assigned game
  `playerIndex`, and a pointer to the `View` it looks through.
- **Viewport** = a screen rectangle with its own `View`/camera. Seats map N:1 to
  viewports (two seats may share a viewport — supported, not user-exposed).
- **Player slot** = existing sim concept (`Player`, `playerIndex`,
  `GameInfo`/`GameSlot`). Seats map N:1 to players (two seats may command one
  army — supported, not user-exposed).
- **Invariant A**: seat 0 is always bound to whatever
  `ThePlayerList->getLocalPlayer()` returns, so legacy code keeps working.
- **Invariant B**: every `GameMessage` that crosses into GameLogic carries the
  correct `m_playerIndex` for the *seat that issued it* (§WP5).
- **Invariant C**: when a network game is active (`TheNetwork != nullptr`), the
  seat count is forced to 1. No splitscreen code runs. (LAN must not regress.)

---

## 2. The singleton playbook

The engine is built on `The*` global singletons. There are ~10 that assume "one
local player". **Four sanctioned patterns** — every singleton task in this doc
names which pattern to apply. Do not invent a fifth.

### Pattern A — Primary alias (cheapest; default)

Keep the global exactly as is; it now means "seat 0's instance". Used when the
singleton's consumers are legacy paths that only ever cared about the primary
local player (score screen, EVA, stats).

```cpp
// no code change at the singleton; multi-seat-aware code goes through
// TheSeatManager->getSeat(i)->... instead of the global.
```

Apply to: `ThePlayerList->getLocalPlayer()` (216+ sites — you will NOT visit
them; they keep meaning seat 0), `TheTacticalView` (becomes seat 0's view),
`TheMouse` (stays the OS mouse, which belongs to seat 0 until the mouse/kb phase).

### Pattern B — Context-object extraction

Move per-local-player *state* out of the singleton into a context class; the
singleton owns `MAX_SEATS` contexts plus accessors; all existing methods forward
to context 0 so no call site breaks; new seat-aware code calls
`getContext(seat)` explicitly.

```cpp
// Before (InGameUI):
DrawableList m_selectedDrawables;

// After:
class SeatUIContext { public: DrawableList m_selectedDrawables; /* ... */ };
SeatUIContext m_seatContexts[MAX_SEATS];
SeatUIContext* getSeatContext(Int seat) { return &m_seatContexts[seat]; }
// legacy accessor, unchanged signature:
const DrawableList* getAllSelectedDrawables() const
  { return &m_seatContexts[0].m_selectedDrawables; }  // seat 0 forwarding
```

Apply to: `TheInGameUI` (WP4), `TheControlBar` (WP8, as whole-instance
duplication rather than inner contexts — see there).

### Pattern C — Scoped render switch (RAII)

For singletons consulted *during rendering* that cache "the local player index":
give them a settable index and switch it per view pass, restoring afterwards.
The sim never sees the switch (render only — Invariant in §0.3).

```cpp
class ScopedRenderPlayer
{
public:
    ScopedRenderPlayer(Int playerIndex)
    {
        m_prev = TheParticleSystemManager->getLocalPlayerIndex();
        TheParticleSystemManager->setLocalPlayerIndex(playerIndex);
        // ...same for other render-side consumers (see WP7 table)
    }
    ~ScopedRenderPlayer() { /* restore m_prev everywhere */ }
private:
    Int m_prev;
};
// in the per-view render loop:
{ ScopedRenderPlayer swap(view->getRenderPlayerIndex());  drawView(view); }
```

Apply to: `TheParticleSystemManager` (`setLocalPlayerIndex` already exists,
`Core/.../GameClient/ParticleSys.h:790`), the W3D scene visibility check
(`W3DScene.cpp:626` reads a `localPlayerIndex` — trace where that variable comes
from and route it through the scoped value), `W3DShroud` texture selection (WP7),
`TheGhostObjectManager` (WP7 — partially; it also needs Pattern B for storage).

### Pattern D — Instance-scoped window lookup

For `TheWindowManager` name-key collisions when the same `.wnd` is instanced N
times. Rule: **never resolve a ControlBar child window with a `nullptr` parent**.

```cpp
// Before (ambiguous with N instances):
GameWindow* w = TheWindowManager->winGetWindowFromId(
    nullptr, TheNameKeyGenerator->nameToKey("ControlBar.wnd:MoneyDisplay"));

// After (scoped to one instance's tree):
GameWindow* w = TheWindowManager->winGetWindowFromId(
    m_barRootWindow /*this instance's root*/, key);
```

Apply to: all `"ControlBar.wnd:*"` lookups (WP8). **Verified caveat**
(`GameWindowManager.cpp:654-678`): `winGetWindowFromId(parent, id)` recurses the
parent's children but then **walks `m_next` trailing siblings too** — with N bar
instances as siblings, a child missing from instance 1 would silently resolve to
instance 2's window. Therefore write a strict
`findChildById(GameWindow* root, Int id)` that checks `root`, then recurses only
into `root->winGetChild()` descendants (never `m_next` at the root level), and
use it for **every** per-instance lookup in WP8. Scoped-lookup precedent in the
codebase: `IMEManager.cpp:575-582`.

### Anti-patterns (do not do these)

- Do **not** add a `seat` parameter to every function that touches a singleton.
  Only translators, InGameUI context accessors, and the render loop become
  seat-aware.
- Do **not** convert singletons to arrays-of-singletons at the global level
  (`TheInGameUI[8]`): keep one manager, N contexts inside.
- Do **not** cache `getLocalPlayer()` results in new code; ask the seat.
- Do **not** change `GameMessage` network serialization (`NetCommandMsg.cpp`) —
  the seat index never crosses the wire.

---

## 3. Work packages

### WP0 — Seat skeleton (new code only, zero behavior change)

New files:
- `Core/GameEngine/Include/Common/SeatManager.h`
- `Core/GameEngine/Source/Common/SeatManager.cpp`

Contents:

```cpp
enum : Int { MAX_SEATS = 8 };

enum SeatState CPP_11(: Int)
{ SEAT_UNBOUND, SEAT_BOUND, SEAT_IN_LOBBY, SEAT_IN_GAME, SEAT_DEVICE_LOST };

struct VirtualCursor
{
    ICoord2D pos;          // game-resolution coords, same space as MouseIO
    Bool     visible;
    Int      cursorType;   // mirrors MouseCursor enum values
};

class LocalSeat
{
public:
    Int           m_seatIndex;
    SeatState     m_state;
    Int           m_deviceId;     // SDL_JoystickID, -1 = none (kept as Int here; SDL types stay out of Core/GameEngine)
    Int           m_playerIndex;  // -1 outside a match
    VirtualCursor m_cursor;
    View*         m_view;         // null until WP6
    // SeatUIContext lives in InGameUI (WP4), fetched by index — do not store a pointer here
};

class SeatManager : public SubsystemInterface
{
public:
    // SubsystemInterface: init/reset/update
    LocalSeat* getSeat(Int i);
    Int  getBoundSeatCount() const;
    Int  bindSeatToDevice(Int deviceId);   // returns seat index or -1; refuses if TheNetwork
    void unbindSeat(Int seatIndex);
    Bool isJoiningAllowed() const;         // false when TheNetwork != nullptr  (Invariant C)
};
extern SeatManager* TheSeatManager;
```

Wire-up: instantiate alongside other subsystems in
`GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp` — find where a similar
client subsystem (e.g. `TheRadar` or `TheInGameUI`) is `initSubsystem(...)`-ed and
mirror that, including the shutdown/reset ordering. Seat 0 initializes `BOUND`
with `m_deviceId = -1` (keyboard/mouse implicit owner).

Acceptance: builds; game runs unchanged; a `DEBUG_LOG` on init prints seat table.

### WP1 — SDL3 gamepad hotplug and per-pad state

File: `Core/GameEngineDevice/Source/SDL3Device/GameClient/SDL3Input.cpp` (+ `.h`).

Current single-pad code to remove/replace: `openFirstGamepad()`,
`closeGamepad()`, the single `SDL_Gamepad* m_gamepad`, the single
`GamepadState m_state` (`SDL3Input.h:136–148,152–156,177`).

Steps:
1. Replace with `std::map<SDL_JoystickID, PadEntry>` where
   `PadEntry { SDL_Gamepad* pad; GamepadState state; }`.
2. In the SDL event pump, handle `SDL_EVENT_GAMEPAD_ADDED` (open, insert) and
   `SDL_EVENT_GAMEPAD_REMOVED` (close, erase, and call
   `TheSeatManager`-notify via a device-independent callback: add
   `SeatManager::onDeviceDisconnected(Int deviceId)` → seat goes
   `SEAT_DEVICE_LOST`).
3. Keep `processGamepadInput()` per pad; move the deadzone/edge-detect logic into
   a function taking `PadEntry&`.
4. **Delete the virtual-event injection for gamepads**
   (`virtualPulseKey`/`virtualPulseMouse` and their gamepad callers). Gamepad
   input no longer pretends to be the mouse. (Keep the functions only if
   something other than gamepads uses them; check callers first.)
5. Instead, each frame build a `SeatInputState` snapshot per pad:

```cpp
struct SeatInputState
{
    Real leftX, leftY, rightX, rightY;   // post-deadzone, -1..1
    Bool buttonDown[SEAT_BUTTON_COUNT];  // engine-side logical buttons
    Bool buttonPressed[SEAT_BUTTON_COUNT], buttonReleased[SEAT_BUTTON_COUNT]; // edges
};
```

   Define logical buttons engine-side (`SEAT_BUTTON_CONFIRM`, `_CANCEL`,
   `_JOIN`, `_COMMAND_BAR`, …) in a new
   `Core/GameEngine/Include/GameClient/SeatInput.h`; SDL3Input translates
   `SDL_GamepadButton` → logical. Hand snapshots to
   `TheSeatManager->setSeatInput(seatForDevice(id), snapshot)`.
6. Unbound pads: if a pad has no seat and its CONFIRM/JOIN button was pressed,
   call `TheSeatManager->bindSeatToDevice(id)` **only if**
   `TheSeatManager->isJoiningAllowed()` *and* the current game state allows it
   (WP9 wires the lobby check; until then leave a TODO and bind only when a
   debug flag is set).

Acceptance: debug overlay (add to an existing debug display under
`Core/.../System/Debug/`) lists pads, seats, live axes/buttons; hotplug works;
single-player with only the mouse is unaffected.

### WP2 — Seat-tagged raw input into the message stream

Goal: seat cursors move and produce the same `MSG_RAW_MOUSE_*` events real mice
produce, tagged with their seat.

1. Add to `GameMessage` (`GeneralsMD/Code/GameEngine/Include/Common/MessageStream.h`,
   near `m_playerIndex` at :684):

```cpp
Int m_seatIndex;   // client-only; which local seat generated this (default 0)
Int getSeatIndex() const { return m_seatIndex; }
void friend_setSeatIndex(Int i) { m_seatIndex = i; }
```

   Initialize `m_seatIndex = 0` in the constructor (`MessageStream.cpp:55`).
   **Do not** serialize it anywhere (`NetCommandMsg.cpp`, `Recorder.cpp` remain
   untouched — grep both to confirm they copy only type/args/playerIndex).
2. Reference for event shape: `Mouse::createStreamMessages()`
   (`Core/GameEngine/Source/GameClient/Input/Mouse.cpp:669`) — see how it appends
   `MSG_RAW_MOUSE_POSITION`, button up/down/drag, with pixel/delta arguments.
3. New `SeatManager::createStreamMessages()` — insertion point verified:
   `GameClient::update`, `GameClient.cpp:610-611`
   (`TheMouse->UPDATE(); TheMouse->createStreamMessages();`) — insert the seat
   call immediately after line 611 (keyboard equivalent is at :599-600). For
   each seat with a bound pad,
   - integrate cursor: `pos += leftStick * speed * dt` (reuse
     `DEFAULT_CURSOR_SPEED`, precision-mode halving from the deleted WP1 code),
     clamp to that seat's viewport rect (fullscreen until WP6);
   - append the same `MSG_RAW_MOUSE_*` messages with identical argument layout,
     then `msg->friend_setSeatIndex(seat)`;
   - map logical buttons: CONFIRM → left click, CANCEL → right click, camera on
     right stick appended as the wheel/position messages `LookAtTranslator`
     expects (open `LookAtXlat.cpp` and match what it consumes for scroll).
4. OS mouse messages keep `m_seatIndex = 0` implicitly.

Acceptance: with flag on and 1 pad bound to seat 0 (dev shortcut), the game is
fully playable by controller through the *existing* translators — because seat
0's tagged events are indistinguishable from mouse events. This proves the tag
plumbing without any translator changes.

### WP3 — Software cursors with house-color tint

New: `Core/GameEngineDevice/Include+Source/W3DDevice/GameClient/W3DSeatCursorRenderer.*`.

1. Hook: end of the 2D/UI render, after windows are drawn. Find where
   `TheWindowManager->winRepaint()` (or equivalent final UI draw) is invoked in
   `W3DDisplay::draw` and render cursors after it.
2. For each bound seat with `m_cursor.visible`, draw a quad at `m_cursor.pos`
   sized to the cursor image. Source art: the cursor definitions from `Mouse`
   INI data (`Core/.../Input/Mouse.cpp` parses them; the SDL3 branch already
   decodes `.ANI` — reuse that decode to get RGBA frames, upload once to a
   `TextureClass`).
3. Tint — verified, no shader needed:
   `Render2DClass::Add_Quad(..., unsigned long color = 0xFFFFFFFF)` modulates
   the texture by a per-quad vertex color
   (`GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.h:118-127`). Draw
   each cursor through a `Render2DClass` with the seat color as the quad color.
4. Color source: `seat->m_playerIndex >= 0 ?`
   `ThePlayerList->getNthPlayer(idx)->getPlayerColor()` (verified:
   `Color Player::getPlayerColor() const`, `Player.h:251`)
   `: fixedPalette[seatIndex]`. Define
   `static const Color fixedPalette[MAX_SEATS]`.
5. Seat 0 keeps the OS hardware cursor when it is mouse-driven; when seat 0 is
   pad-driven, hide the OS cursor (`SDL3Mouse::setVisibility(false)`) and draw
   seat 0's software cursor too.
6. Cursor *type* (arrow/attack/move): mirror what `Mouse::setCursor` receives —
   translators call `TheMouse->setCursor(...)`; in WP5 the per-seat hit-test sets
   `seat->m_cursor.cursorType` instead. Until WP5, all seats draw the arrow.

Acceptance: 4 pads → 4 tinted arrows moving independently over the main menu.

### WP4 — `SeatUIContext`: extract per-player UI state from `TheInGameUI`

File: `GeneralsMD/Code/GameEngine/{Include/GameClient/InGameUI.h, Source/GameClient/InGameUI.cpp}`.
Pattern B. This is the largest mechanical step — do it as several commits.

1. Create `SeatUIContext` (nested in `InGameUI` or its own header) and move these
   members into it (locate each in `InGameUI.h`; the list below is the minimum,
   move obviously-coupled siblings with them):
   - `m_selectedDrawables` (`InGameUI.h:729`) and any parallel selection
     count/flags
   - hint state (`m_moveHint`/attack hint arrays and indices)
   - build placement state: the placement icon/ghost members used by
     `PlaceEventTranslator` (`m_pendingPlaceType`, `m_pendingPlaceSourceObjectID`,
     `m_placeIcon`, angle/anchor fields — search `pendingPlace` in the header)
   - waypoint/attack-move/force-move UI mode booleans
   - `m_mousedOverDrawableID` / hint-drawable tracking
2. Add `SeatUIContext m_seatContexts[MAX_SEATS]` and
   `SeatUIContext* getSeatContext(Int seat)`.
3. Convert every existing method that touched the moved members to forward to
   `m_seatContexts[0]` **preserving its public signature** (Invariant: no call
   site outside `InGameUI.cpp` changes in this WP).
4. Add seat-parameterized variants used from WP5 on:
   `selectDrawable(Drawable*, Int seat)`, `deselectDrawable(Drawable*, Int seat)`,
   `getAllSelectedDrawables(Int seat)`, `setPendingPlaceType(..., Int seat)` etc.
   The old signatures call the new ones with `seat = 0`.
5. `Drawable` selection flag: find the "selected" bool on `Drawable`
   (`GeneralsMD/.../GameClient/Drawable.h`, search `friend_setSelected` or
   `isSelected`). Replace with `UnsignedByte m_selectedSeatMask` +
   `isSelectedByAnySeat()`, `isSelectedBySeat(Int)`. Legacy `isSelected()`
   forwards to `isSelectedBySeat(0)` — then grep all `isSelected(` callers and
   classify: *render* callers (selection decal, health bar) should use
   `AnySeat`; *command/UI* callers keep seat 0 until WP5 touches them. Selection
   decal color: where the decal is drawn (search `selection` in
   `W3DDevice/GameClient` drawable draw code), tint by the *lowest set seat bit*
   for now.
6. Group hotkeys (`m_groups`/ctrl-groups): leave global (keyboard-only feature);
   add `// TODO(splitscreen): per-seat groups` comment.

Acceptance: builds; single-player selection, placement, hints all behave
identically (manual smoke: select, box-select, build a structure, set rally).

### WP5 — Seat-aware translators + command stamping

Files: `GeneralsMD/Code/GameEngine/Source/GameClient/MessageStream/`
`{SelectionXlat,CommandXlat,LookAtXlat,PlaceEventTranslator,GUICommandTranslator,WindowXlat}.cpp`.
Translator registration and priorities: `GameClient.cpp:291–309`.

Core rule — inside every translator, replace the two implicit globals with
seat-derived values:

```cpp
// at the top of translateGameMessage(), for raw input messages:
Int seat = msg->getSeatIndex();
SeatUIContext* ui = TheInGameUI->getSeatContext(seat);
Player* seatPlayer = ThePlayerList->getNthPlayer(TheSeatManager->getSeat(seat)->m_playerIndex);
View* seatView = TheSeatManager->getSeat(seat)->m_view;   // == TheTacticalView for seat 0
```

Then:
1. **Selection** (`SelectionXlat.cpp`): all reads/writes of the global selection
   go through `ui`; picking through `seatView->pickDrawable(...)` (the view is
   viewport-aware already — `W3DView.cpp:584,652` do origin conversion).
   `getLocalPlayer` sites (6) become `seatPlayer` where they mean "the acting
   player" (ownership checks), stay global where they mean "observer/replay
   camera" (read each site; when in doubt it is the acting player).
2. **Commands** (`CommandXlat.cpp`, 27 sites): same substitution. **Every**
   `TheMessageStream->appendMessage(<logic command>)` in a translator gets
   `msg->friend_setPlayerIndex(seatPlayer->getPlayerIndex());` immediately after
   creation. To catch misses, add once in
   `GameLogicDispatch.cpp:353` (where `msgPlayer` is resolved):
   `DEBUG_ASSERTCRASH` if the message playerIndex differs from
   what a seat 0 default would produce *while more than one seat is bound* —
   simplest robust form: assert `msg->getPlayerIndex()` corresponds to a
   human-controlled player when `TheSeatManager->getBoundSeatCount() > 1`.
3. **Camera** (`LookAtXlat.cpp`, 4 sites): operates on `seatView` instead of
   `TheTacticalView`. Scroll-at-screen-edge: use the seat's viewport rect
   (`seatView` origin/size), not display size.
4. **Placement** (`PlaceEventTranslator.cpp`): pending-place state now lives in
   `ui` (moved in WP4).
5. **Windows/GUI** (`WindowXlat.cpp`): route seat cursor events into
   `TheWindowManager` only for windows the seat owns (their ControlBar instance,
   WP8). Until WP8, let only seat 0's events reach the window system to avoid
   two cursors fighting over one control bar: early-return `KEEP_MESSAGE` /
   `DESTROY` semantics — match how the translator currently passes messages.
6. Cursor type feedback: wherever these translators call
   `TheMouse->setCursor(...)`, also set
   `TheSeatManager->getSeat(seat)->m_cursor.cursorType`; guard the `TheMouse`
   call with `seat == 0` so pads don't flicker the OS cursor.

Seat→player mapping recipe (verified — use for the dev-start here and WP9):
slot *i* becomes the side named `"player%d" % slotIdx`
(`GameLogic::startNewGame`, `GameLogic.cpp:1384-1412`), so after logic start:

```cpp
AsciiString name; name.format("player%d", slotIdx);
Int playerIndex = ThePlayerList->findPlayerWithNameKey(NAMEKEY(name))->getPlayerIndex();
```

(same pattern as `NetCommandMsg.cpp:172`).

Acceptance (dev config, one shared fullscreen view): 2 pads, 2 seats bound to 2
different players of a 2-human-slot skirmish (WP9 provides the lobby; until
then, hardcode a debug start that assigns seat 1 → player slot 1). Each seat
selects and commands only its own army. Save a replay, play it back, verify.

### WP6 — `ViewportLayoutManager` + N tactical views

New: `GeneralsMD/Code/GameEngine/{Include,Source}/GameClient/ViewportLayout.*`
(GameClient because it manipulates `View`s; keep W3D types out — it only uses the
`View`/`Display` interfaces).

1. Reference for creating the tactical view:
   `InGameUI.cpp:1370–1383` (`createView`, `TheDisplay->attachView`,
   `setWidth/Height/DefaultView`). `Display` draws every attached view
   (`Core/.../Display.cpp:360`); `W3DView` maps origin/size to a camera
   sub-viewport (`W3DView.cpp:219,238` — `Set_Viewport` normalizes by display
   size; also `setOrigin`, search it in `W3DView.cpp`).
2. Layout table (screen fractions, after reserving ControlBar strips per WP8):
   - 1 seat: 1×1 · 2: 2×1 (side by side) · 3–4: 2×2 (3 seats: slot 4 shows a
     letterboxed idle background, simplest) · 5–6: 3×2 · 7–8: 4×2.
3. `applyLayout(Int seatCount)`: creates/destroys views via the same
   `createView()` factory, attaches, sets origin/size, assigns
   `seat->m_view`, and stores per-view `m_renderPlayerIndex` (new small field —
   add to `View` with default = local player index).
   Seat 0's view **is** `TheTacticalView` (Pattern A): never destroy it; resize
   it.
4. Call sites: on match start (after players exist), and on join/leave (WP9).
5. Divider bars: draw 2px lines between viewports in the same late-2D hook as
   WP3 cursors.
6. Audit per-view global state: camera shake, letterbox/cinematic. Verified:
   letterbox is **Display-global**, not per-view (`Display.h:182-185,221-223` —
   `enableLetterBox`, `m_letterBoxEnabled`; consumed at `W3DView.cpp:344` via
   `TheDisplay->isLetterBoxed()`). Make `enableLetterBox` a no-op while more
   than one seat is bound.

Acceptance: dev command sets 2/4/8 fake seats → screen splits correctly, each
view independently scrollable by its seat (WP5 camera routing), single-seat
still fullscreen-identical.

### WP7 — Per-viewport render player (shroud, ghosts, particles, hiding)

This is Spike A first: build steps 1–3 for **two** views before generalizing.

Where the per-view pass runs: the draw call chain from `Display::draw` view loop
into `W3DView`/`W3DScene`. Wrap each view's 3D draw with Pattern C:

```cpp
ScopedRenderPlayer swap(view->getRenderPlayerIndex());
```

`ScopedRenderPlayer` must switch, in order:

| # | System | Existing hook | Work |
|---|---|---|---|
| 1 | Object hiding | `W3DScene.cpp:626` `obj->getShroudedStatus(localPlayerIndex)` | Trace `localPlayerIndex`'s origin in that file; make it read the scoped value (a static/member on the scene set by the RAII object). Sim data is already per-player — render-only change. |
| 2 | Particles | `TheParticleSystemManager->setLocalPlayerIndex` (`ParticleSys.h:790`) | Already settable; add getter if missing; swap/restore. |
| 3 | Terrain shroud | `W3DShroud` (search class in `Core/GameEngineDevice/.../W3DDevice`) | Today: one shroud texture updated from one player's `PartitionManager` data (`getShroudStatusForPlayer`, `PartitionManager.h:331`). Change: `W3DShroud` keeps one texture per *distinct* render player among seats (map playerIndex→texture, created lazily), updates each from that player's cells, and `ScopedRenderPlayer` selects which texture binds. |
| 4 | Ghost objects | `GhostObjectManager` (`GhostObject.h:82` single `m_localPlayer`; W3D impl `GeneralsMD/.../W3DGhostObject.cpp:949`) | Hardest. Storage is per-ghost render snapshots for one player. Change `W3DGhostObjectManager` to keep snapshot sets keyed by playerIndex for every *local human* player (register the set of local player indices at match start: `TheSeatManager` → `TheGhostObjectManager->setLocalPlayerIndices(...)`). Creation/deletion events currently gated by `if (playerIndex == m_localPlayer)` become `if (isLocalIndex(playerIndex))` with per-index storage. Rendering: draw only the current render player's set (check inside the ghost render-object submit; Pattern C value). Sim save/load: `GhostObjectManager` participates in snapshots (`Snapshot` interface) — keep the on-disk format identical for single-local (xfer only index-0 set when one local player; version-bump the xfer if the multi-set must persist; simplest correct answer: rebuild non-primary sets from `m_everSeenByPlayer` sim data on load — see `PartitionManager.h:523` — and persist only the primary, preserving format). |
| 5 | Stealth/occlusion tint | search `getLocalPlayer` in `GeneralsMD/.../W3DDevice/GameClient/` drawable/model draw modules (e.g. stealth opacity) | Replace with scoped render player. |

Also per-view 2D overlays (health bars, rally lines, waypoint paths): these draw
in view space already if they go through the view's world-to-screen — verify
each uses the *current* view's transform, not `TheTacticalView` (`InGameUI.cpp`
draw helpers; fix any hard reference by passing the view down or using the
current-render-view scoped pointer).

Radar: parameterize `TheRadar`'s draw by player (`Core/.../Radar.cpp` has one
`getLocalPlayer`) — one radar per ControlBar instance (WP8 consumes this).

Acceptance (Spike A gate): 2 viewports, 2 players on a fog-heavy map; each view
shows only its player's vision; build a structure in one player's fog → ghost
appears only in the correct viewport; save/load and replay still work.

### WP8 — ControlBar multi-instance (no new GUIs)

Files: `GeneralsMD/.../GameClient/GUI/ControlBar/*.cpp`,
`GameClient/InGameUI.cpp:4209` (`createControlBar` /
`winCreateFromScript("ControlBar.wnd")` at :4212, `recreateControlBar` at :5996),
callbacks in `GUI/GUICallbacks/ControlBarCallback.cpp` +
`ControlBarCommandProcessing.cpp`.

Layout rules (fixed requirements):
- Bar docks to the horizontal screen edge nearest its seat's viewport row:
  bottom-row viewports → bottom (classic), top-row → top, **vertically
  mirrored**.
- 2-seat shared screen: seat 0 bottom, seat 1 identical copy mirrored on top.
- 8 seats (4×2): four quarter-width bars along the top (mirrored) + four along
  the bottom. `ViewportLayoutManager` (WP6) reserves these strips.

Steps:
1. **Instance struct**: turn the `ControlBar` singleton class into an
   instantiable class; keep `TheControlBar` = instance 0 (Pattern A + B hybrid):
   add `ControlBarInstances` owner (array of `ControlBar*` indexed by seat,
   instance 0 aliased by the global). `ControlBar` gains `m_seatIndex`,
   `m_player` (the seat's player), `m_barRootWindow`.
2. **Per-instance windows**: `winCreateFromScript("ControlBar.wnd")` once per
   seat; capture the returned root (`ControlBarParent` — id used at
   `CommandXlat.cpp:3358`). Replace every
   `winGetWindowFromId(nullptr, key("ControlBar.wnd:*"))` in
   `ControlBar*.cpp` and `InGameUI.cpp` (:2021 money, :4320 RightHUD, :5944 idle
   worker, etc. — grep `"ControlBar.wnd:` across `GeneralsMD/`) with Pattern D
   scoped lookup + per-instance cached pointers. Build the cache in one
   `ControlBar::cacheWindowPointers()` run after creation.
3. **Callback routing**: window callbacks receive the `GameWindow*` that fired.
   Add `ControlBar* ControlBarInstances::fromWindow(GameWindow* w)` (walk `w`'s
   parents to a bar root, map root→instance). Every callback that used
   `TheControlBar` / `getLocalPlayer` resolves the instance and uses
   `instance->m_player`. The 30 `getLocalPlayer` sites in `ControlBar.cpp`
   become `m_player` (read each: sciences, money, power, build queue — all are
   "the bar's player").
4. **Geometry**: after creation, apply position/scale:
   - width scale: target = viewport width. Verified: `ControlBarResizer`
     (`ControlBarResizer.h:63-89`) is INI-driven and **two-state only**
     (per-named-window default/alt size+pos) — it cannot scale arbitrarily.
     Write a recursive uniform-scale helper (`winSetSize`/`winSetPosition` over
     the instance subtree); use the resizer's INI as the reference for which
     windows are safe to move.
   - **vertical mirror** for top-docked bars: one recursive pass over the
     instance subtree: for each child, `newY = parentHeight - oldY - childHeight`
     (positions only — never flip a widget's own contents; text/buttons stay
     upright).
   - background art: the bar bitmaps draw via the window user-draw functions
     (`W3DControlBar.cpp` in `GameEngineDevice`); add a "flip V" draw variant for
     mirrored instances (swap the V texture coords in the draw call).
5. **Update loop**: wherever `TheControlBar->update()`/`markDirty` is called,
   iterate all instances.
6. **Context switching** (`switchToContext`, `ControlBar.h:816`) already takes
   the selected drawable; it must use the instance's seat selection
   (`TheInGameUI->getSeatContext(instance->m_seatIndex)`).
7. **Input**: WP5 step 5 lifts the seat-0-only window guard: a seat's cursor
   events may hit only its own bar subtree (hit-test against
   `m_barRootWindow`'s rect before forwarding).
8. Single-seat: exactly one bottom instance, zero geometry changes (assert the
   transform is identity), classic behavior.

Spike C gate (do before the rest of WP8): two instances, bottom + mirrored top,
static player data, both clickable — proves name-key scoping + mirroring before
the full callback sweep.

Acceptance: 2-seat shared screen → bottom + mirrored top bars fully functional
by pad cursor (build, queue, abilities, rally); 8-seat → 4+4 quarter-width bars
showing correct per-player money/power/commands.

### WP9 — Lobby join/leave, in-match drop-in/out, transport guards

**Lobby** (`GeneralsMD/.../GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp`,
slots in `Core/.../GameNetwork/GameInfo.h:36` — `SLOT_OPEN/CLOSED/*_AI/PLAYER`):
1. While this menu is active, `TheSeatManager->isJoiningAllowed()` returns true
   (and `TheNetwork == nullptr` — skirmish only).
2. On bind of a new seat: claim first `SLOT_OPEN`; if none, convert the first AI
   slot (remember `{slotIdx, previousState}` on the seat for restore). Set the
   slot to `SLOT_PLAYER` with name "Controller N". Update the slot UI the same
   way the existing combo-box handlers do (mirror the code path the menu uses
   when the host changes a slot).
3. Seat CANCEL in lobby: restore remembered slot state, unbind seat.
4. Each joined seat's cursor (WP3) is confined to its slot row's controls
   (faction/color/team combos) — hit-test filter like WP8 step 7.
5. On game start: record `seat → playerIndex` from the started `GameInfo`
   (follow how slot index becomes player index in the game-start path — search
   for where `GameInfo` slots construct `Player`s), call
   `ViewportLayoutManager::applyLayout`, mark seats `SEAT_IN_GAME`.

**In-match leave**: seat holds CANCEL (1s) → confirm dialog (existing quit-style
messagebox) → append new `GameMessage::MSG_LOGIC_LOCAL_CONTROL_RELEASE`
(register the enum in `MessageStream.h` message list + its `CASE_LABEL` in
`MessageStream.cpp`; stamp with the seat's playerIndex). Handle in
`logicMessageDispatcher` (`GameLogicDispatch.cpp` — add a case next to similar
logic messages): `msgPlayer->setPlayerType(PLAYER_COMPUTER, TRUE)`.
Verified: this deletes `m_ai` and constructs `AISkirmishPlayer(this)`
(`Player.cpp:738-755`) — the mechanism exists. **Known hazard (Spike B target)**:
skirmish AI *scripts* are duplicated/qualified only at map load in
`Player::initFromDict` for slots that are AI at that time (`Player.cpp:863-886`);
human slots receive civilian scripts instead (`:829-858`), so a mid-game
human→AI flip likely yields a passive, scriptless AI. Expected fix: when
splitscreen is enabled, ALSO duplicate+stash the qualified skirmish scripts for
every human slot at load, and install them on takeover (sim-side, deterministic
— it runs identically on every machine/replay). Spike B must confirm both flip
directions before building the UI around them. Client-side on the same event:
unbind seat, re-flow layout.

**In-match join**: unbound pad presses JOIN → overlay listing AI players
(reuse a simple messagebox/listbox layout that already exists — e.g. the quit
menu layout pattern; still "no new GUI designs": assemble from existing gadget
windows) → `MSG_LOGIC_LOCAL_CONTROL_TAKE` stamped with the *target* playerIndex →
dispatcher flips `setPlayerType(PLAYER_HUMAN, FALSE)` (see `PlayerList.cpp:175`)
→ client binds seat to that playerIndex, re-flows, snaps camera to army
centroid. Both messages go through the stream ⇒ replays stay valid
(`Recorder.cpp:1340` path untouched).

**Transport guards + abstraction** (LAN must not regress):
1. Guards: `SeatManager::bindSeatToDevice` refuses when `TheNetwork != nullptr`
   or a network lobby is active; assert in `applyLayout` that
   `seatCount == 1` when networked.
2. `CommandTransport` scaffolding (pure passthrough — behavior identical):
   new `Core/GameEngine/{Include,Source}/Common/CommandTransport.*` wrapping the
   existing seam: local = commands already flow
   `MessageStream::propagateMessages` → `TheCommandList` (`MessageStream.cpp:1133`)
   → GameLogic; LAN = `Network::GetCommandsFromCommandList` (`Network.cpp:462`) /
   `RelayCommandsToCommandList` (`Network.cpp:185`) with the frame gate
   `TheNetwork->isFrameDataReady()` (`GameEngine.cpp:841`). Route the null-checks
   at `GameEngine.cpp:841,907` through
   `TheCommandTransport->isFrameReady()/update()`; `LocalTransport` returns
   ready-always; `LanLockstepTransport` delegates to `TheNetwork`. Nothing else
   changes. Do this as its own commit with a LAN smoke test.

Acceptance: full console loop — pads join at the skirmish screen (open slots
first, then bots, with restore on leave), match starts with correct viewports,
one player drops mid-game (AI continues), a new pad joins and takes a bot army;
replay of the session plays back; separate LAN game between two machines works
as before.

---

## 4. Singleton inventory (quick reference)

| Singleton | Assumption today | Pattern | WP |
|---|---|---|---|
| `ThePlayerList->getLocalPlayer()` | the one human | A (means seat 0; add `isLocalHuman(idx)` helper) | WP4/5 |
| `TheInGameUI` | one selection/hint/placement state | B (`SeatUIContext[8]`) | WP4 |
| `TheTacticalView` | one camera | A (seat 0's view); other seats via `seat->m_view` | WP6 |
| `TheMouse` / `SDL3Mouse` | one cursor | A (OS mouse = seat 0); seats use `VirtualCursor` | WP2/3 |
| `TheControlBar` | one bar, local player | A+B (instance array, global = instance 0) | WP8 |
| `TheParticleSystemManager` | `m_localPlayerIndex` | C (already settable) | WP7 |
| `TheGhostObjectManager` | `m_localPlayer` storage | B storage + C render-select | WP7 |
| `W3DShroud` | one shroud texture | B textures + C bind-select | WP7 |
| `TheRadar` | renders local player | parameterized draw per bar instance | WP7/8 |
| `TheWindowManager` name keys | one instance per `.wnd` | D (instance-scoped lookup) | WP8 |
| `TheNetwork` null-checks | net vs local scattered | `CommandTransport` seam | WP9 |
| `TheAudio` listener | one camera | A (seat 0's view); per-seat EVA throttle later | WP7 note |

## 5. Verification checklist (run per WP)

1. Build the user's preset; zero new warnings in touched files.
2. Single-player skirmish vs 1 AI: select, box-select, build, rally, attack —
   identical to pre-change behavior with the splitscreen flag OFF.
3. Replay: record a short skirmish, play it back, no desync
   (plus the repo's replay check workflow, `.github/workflows/check-replays.yml`).
4. WP-specific acceptance test listed in the WP.
5. From WP9 on: LAN smoke test (two instances/machines) per change to
   `MessageStream`, `Network`, or `CommandTransport`.
