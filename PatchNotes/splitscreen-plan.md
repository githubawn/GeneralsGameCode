# Splitscreen Plan — 1–8 Local Players (Controller-First)

Goal: console-level local multiplayer for Generals/Zero Hour. 1–8 players on one
machine, controllers only at first (mouse/keyboard as a seat device later), true
splitscreen viewports, per-player shader-tinted cursors, drop-in/drop-out join/leave
that is reflected live in skirmish setup (claiming open/bot slots) and in-match.

Primary target: `GeneralsMD` (Zero Hour) on the `feature/sdl3-input-backport` line,
with new device-independent code in `Core/GameEngine` and SDL3 code in
`Core/GameEngineDevice/.../SDL3Device`. Port to `Generals` afterwards (code layout
is parallel).

---

## 1. Core architecture: Seat ≠ Viewport ≠ Player slot

Three concepts, deliberately decoupled so shared-screen and co-op configurations
work later without rework:

| Concept | What it is | Owned by |
|---|---|---|
| **Seat** (`LocalSeat`, new) | A local human: bound input device(s), a virtual cursor, a selection/UI context, and an assigned game player index | `TheSeatManager` (new subsystem) |
| **Viewport** | A screen rectangle with its own `View` (camera) | `ViewportLayoutManager` (new) |
| **Player slot** | The existing sim concept: `Player` / `GameSlot` / `playerIndex` | existing `ThePlayerList` / `GameInfo` |

Mappings are N:1 in both directions by design:

- Seats → Viewport: normally 1:1 (true splitscreen, the shipped config), but two
  seats may share one viewport (shared-screen; **not user-exposed yet**, just
  supported by the abstraction).
- Seats → Player: normally 1:1, but two seats may map to the same `playerIndex`
  (two humans co-controlling one army). The sim does not care — commands are just
  messages stamped with a player index (see §2). Selection is per-seat, so two
  seats on one army don't fight over selection state.

`LocalSeat` sketch:

```
class LocalSeat
{
    Int             m_seatIndex;        // 0..7, stable for the session
    SeatDeviceId    m_device;           // SDL gamepad instance id (later: kb/mouse)
    Int             m_playerIndex;      // game Player this seat commands, -1 in menus
    VirtualCursor   m_cursor;           // position, velocity, visibility
    SeatUIContext*  m_ui;               // selection, hints, placement mode (see §5)
    View*           m_view;             // tactical view this seat looks through
};
```

Seat 0 is special only in legacy-compat terms: `ThePlayerList->getLocalPlayer()`
(216+ call sites) keeps returning seat 0's player so untouched code paths keep
working; multi-seat-aware code asks the seat instead. New helper:
`ThePlayerList->isLocalHuman(playerIndex)` for "any seat owns this player" checks
(audio, EVA, score screen).

---

## 2. Why this is feasible: what the engine already does

Verified in the codebase:

- **The sim is already multi-player-per-process.** Every `GameMessage` carries
  `m_playerIndex` ("the Player who issued the command",
  `GeneralsMD/.../Common/MessageStream.h:684`), and the logic dispatcher resolves
  the acting player from it (`Core/.../GameLogicDispatch.cpp:353`). Network games
  are exactly this: many players' commands in one deterministic sim. Splitscreen
  reuses that path with zero netcode.
- **Visibility is tracked per player.** `PartitionManager` keeps
  `m_shroudedness[MAX_PLAYER_COUNT]`, `getShroudStatusForPlayer(playerIndex)`,
  `Object::getShroudedStatus(playerIndex)` (`PartitionManager.h:331,395,517`).
  Only the *render* side assumes a single local player (§6).
- **Multiple views per display already work.** `Display` keeps a view list and
  draws each attached view (`Core/.../Display.cpp:106,360`); `W3DView` supports
  sub-rect viewports via `m_3DCamera->Set_Viewport` (`W3DView.cpp:219,238` — used
  today for letterboxing). `TheTacticalView` is just the first attached view
  (`GeneralsMD/.../InGameUI.cpp:1370`).
- **Human↔AI control flips exist.** `Player::setPlayerType(PLAYER_COMPUTER, skirmish)`
  is already used (`Player.cpp:738,863`), giving a hook for AI-takeover on leave
  and human-takeover on join.
- **Replays already record per-player commands** (`Recorder.cpp:1340` stamps
  playerIndex on playback), so splitscreen replays work if we stamp correctly.

The single-local-player assumption lives in the **client**, not the sim. The work
is therefore: input routing, per-seat UI state, per-viewport rendering, and lobby
flow — scaffolding around an already-capable core.

---

## 3. Phase 0 — Input foundation: gamepads → seats

Current state (`Core/GameEngineDevice/.../SDL3Input.h/.cpp`): `SDL3InputManager`
opens **the first gamepad only** (`openFirstGamepad`) and fakes keyboard/mouse by
injecting virtual SDL events into the single mouse/keyboard buffers. That model
cannot scale to 8 players and gets replaced.

Work:

1. **Gamepad hotplug**: handle `SDL_EVENT_GAMEPAD_ADDED/REMOVED`; keep a table of
   open `SDL_Gamepad*` keyed by `SDL_JoystickID`. Remove `openFirstGamepad`.
2. **`TheSeatManager`** (new, in `Core/GameEngine/Source/Common/`, device-agnostic):
   - Seat lifecycle: `Unbound → Bound (device claimed) → InLobby → InGame`.
   - "Press A/Start on an unbound pad" claims a seat (only in contexts that allow
     joining: skirmish lobby, in-match join overlay).
   - Device disconnect → seat enters `DeviceLost` (pause + "reconnect controller"
     toast for in-game seats, console-style).
3. **Per-seat input snapshot**: each frame, per gamepad, produce a
   `SeatInputState` (sticks with deadzone, buttons edge-detected, triggers).
   The existing `GamepadState` struct is the starting point, made per-instance.
4. **Per-seat virtual cursor**: cursor position integration (left stick, precision
   mode, speed constants already exist as `DEFAULT_CURSOR_SPEED` etc.) moves into
   `LocalSeat::m_cursor`. **Stop injecting into the OS mouse stream** — seat
   events flow through a new path (§5). The OS mouse remains a plain mouse for
   frontend menus (driven by whoever holds it; formally it is seat 0's device
   later, in the mouse/kb phase).
5. **`InputRoute` abstraction**: `SeatInputState` is produced by an interface, not
   by SDL directly, so a `KeyboardMouseRoute` can be added later without touching
   consumers. This is the designed-in hook for the mouse/kb stage.

Deliverable / acceptance: debug overlay listing seats, bound pads, live stick and
button state for 8 pads; hotplug and unplug work.

---

## 4. Phase 1 — Multi-cursor rendering with shader tint

One OS hardware cursor exists; 8 cursors must be **software-rendered**. Current
SDL3 path uses `SDL_Cursor` (`SDL3Mouse::m_activeSDLCursor`) — kept for the plain
OS mouse in frontend menus, unused for seats.

Work:

1. **`W3DSeatCursorRenderer`** (new, `Core/GameEngineDevice/.../W3DDevice`): draws
   each active seat's cursor as a textured quad in the 2D/UI pass, after all UI,
   per frame. Reuses existing cursor art (`Mouse.ini` cursor definitions; ANI
   decoding already exists from the `SDL3Cursor loadANI` work) uploaded to
   textures with frame animation.
2. **Tint shader**: small pixel shader (or fixed-function TFACTOR modulate as
   fallback) multiplying the cursor texture by a per-seat color constant.
   Grayscale-authored or luminance-keyed variants of the cursor art may be needed
   for clean tinting — evaluate per cursor; the tint applies to the cursor's
   accent regions.
3. **Color source**: the seat's player **house color** once chosen (lobby color
   pick or in-game `Player` color); before any color exists, a fixed fallback
   palette (P1..P8). Colors update live when the lobby color changes.
4. Per-seat cursor *type* state (attack/select/move variants) comes from that
   seat's own hit-testing (§5), not the global `Mouse` singleton.

Deliverable: 8 tinted cursors moving independently on screen at once (over a menu
or empty map), colors following lobby color picks.

---

## 5. Phase 2 — Per-seat command routing and UI context (sim-facing correctness)

This is the highest-fan-out refactor. Today:

- `GameMessage`'s constructor stamps `ThePlayerList->getLocalPlayer()`
  (`MessageStream.cpp:57`) — every command is attributed to "the" local player.
- Selection is one global list: `InGameUI::m_selectedDrawables` (`InGameUI.h:729`),
  plus a global selected flag on `Drawable`.
- The translator chain (`SelectionXlat`, `CommandXlat` (27 `getLocalPlayer` sites),
  `LookAtXlat`, `PlaceEventTranslator`, `GUICommandTranslator`) reads the global
  mouse/keyboard stream and the global selection.

Work:

1. **Seat-stamped messages**: raw input events entering the message stream carry a
   seat id; when a translator emits a logic command it stamps
   `msg->friend_setPlayerIndex(seat->m_playerIndex)`. The `GameMessage` ctor
   default remains (legacy paths = seat 0), but every translator-emitted command
   is explicitly stamped. Add a debug assert for logic-crossing messages with
   unstamped indices.
2. **`SeatUIContext`** (extracted from `InGameUI` global state), per seat:
   - selected drawables list + group hotkey squads (per-seat squads later; squads
     are keyboard-centric, low priority for controller phase)
   - hint/feedback state, attack-move/waypoint modes
   - build placement state (`PlaceEventTranslator` / build ghost) — placement is
     per seat
   - camera intent (handed to that seat's `View`)
3. **Per-seat selection flags on `Drawable`**: replace the single "selected" bool
   with an 8-bit seat mask; selection decals tint per seat house color (matches
   cursor tint). `TheInGameUI->selectDrawable` (`InGameUI.h:449`) grows a seat
   parameter; the no-seat overload forwards to seat 0.
4. **Translator instancing**: run the selection/command/look-at translators per
   seat against that seat's `SeatInputState` + cursor + view, instead of one chain
   fed by the OS mouse. The OS mouse path stays wired to seat 0 during transition
   so the game remains playable at every commit.
5. **Picking per seat**: `View::pickDrawable` etc. already take screen coords and
   exist per view — each seat picks through its own view with its own cursor
   coords (viewport-relative conversion exists: `W3DView.cpp:584,652`).

Acceptance: 2 seats on one shared fullscreen view (dev config), each selecting
and commanding **their own army** independently in a skirmish vs each other;
replay of that match plays back correctly; a co-op dev toggle (2 seats, same
playerIndex) works with independent selections.

Note: this phase is exactly the abstraction the user wants for non-exposed
shared-screen/co-op — it falls out of seat-vs-viewport decoupling for free.

---

## 6. Phase 3 — True splitscreen viewports

1. **`ViewportLayoutManager`**: computes rects for the active seat count and
   creates/attaches one `W3DView` per viewport via `TheDisplay->attachView`
   (multi-view loop already exists, `Display.cpp:360`):
   - 1: fullscreen · 2: vertical halves · 3–4: 2×2 (3rd config: one wide bottom)
   - 5–6: 3×2 · 7–8: 4×2
   - Layout re-flows live on join/leave with a short animated transition.
   - `Set_Viewport` per camera (`W3DView.cpp:219`); divider bars drawn in UI pass.
2. **Per-viewport "render-local player"**: the render loop introduces a *current
   render player index* switched per view pass:
   - **Object hiding**: `W3DScene.cpp:626` uses `getShroudedStatus(localPlayerIndex)`
     — the sim data is already per-player; the render pass just switches the index.
   - **Terrain shroud**: `W3DShroud` holds one shroud texture for one player →
     one shroud texture per *distinct local player* (≤8; small, e.g. map-cell
     sized luminance textures), select per view pass.
   - **Ghost objects** (fogged building snapshots): `GhostObjectManager` tracks
     ghosts for a single `m_localPlayer` (`GhostObject.h:82`,
     `W3DGhostObject.cpp:949`). Extend to maintain ghost render-objects for a
     *set* of local player indices. Ghosts are client-side visuals keyed off
     per-player sim data (`m_everSeenByPlayer`), so this is memory/bookkeeping,
     not determinism risk — but it is the **highest-risk render item**; spike it
     early (see §12).
   - **Particles/stealth visibility**: `TheParticleSystemManager->setLocalPlayerIndex`
     (`ParticleSys.h:790`) and stealth-object render checks switch per pass.
   - **Snow/water/letterbox/camera-shake**: per-view state audit.
3. **Per-viewport UI overlays**: health bars, rally points, waypoint lines, drop
   zones render only in viewports whose seat should see them; radar (§7) per
   viewport.
4. **Audio**: one listener — follow seat 0's camera for positional audio initially
   (console games commonly center or use loudest-viewport heuristics; ship simple,
   iterate). "Is this event for the local player?" checks
   (unit responses, EVA) become `isLocalHuman(playerIndex)`; per-seat EVA voice
   throttling so 8 "unit lost" lines don't stack.
5. **Performance**: N full scene passes. The user has confirmed target hardware is
   not a concern for this engine; still, add a per-view frustum culling sanity
   check and a splitscreen particle LOD scalar.

Acceptance: 8 controllers, 8 viewports, 8 armies in one skirmish; each viewport
shows correct per-player shroud/fog and ghost buildings; steady frame rate.

---

## 7. Phase 4 — Per-seat classic ControlBar instances

**No new GUIs.** Every seat gets its own instance of the classic `ControlBar`,
driven by that seat's virtual cursor (hover/click via Phase 1/2) — cursor
emulation makes the existing bar controller-usable as-is.

Placement rules:

- Bars dock to the horizontal screen edge nearest the seat's viewport row:
  bottom-row viewports → bottom edge (classic), top-row viewports → top edge,
  **vertically mirrored**.
- Mirroring is container-level only: child window Y positions flip within the bar
  (`y' = barHeight − y − childHeight`); leaf widgets (buttons, portraits, text)
  stay upright; background bar art draws with flipped V coordinates (pre-flipped
  art variants where a draw path can't flip UVs).
- 2-player shared screen: seat 0 bar at the bottom, seat 1 an identical copy
  mirrored at the top.
- 8-player (4×2): four quarter-width bars along the top (mirrored), four along
  the bottom. Bars scale to viewport width via the window system's
  creation-resolution scaling plus the existing `ControlBarResizer`.
  `ViewportLayoutManager` (§6) reserves the top/bottom bar strips so viewports
  sit between the bar rows.

Refactor work (instancing, not redesign):

1. **`TheControlBar` singleton → per-seat instances.** Seat 0 keeps the global
   alias for legacy paths; per-player state reads (the 30 `getLocalPlayer` sites
   in `ControlBar.cpp` — money, power, build queue, sciences) become "the
   instance's player".
2. **Window instancing.** The bar UI is one script layout,
   `TheWindowManager->winCreateFromScript("ControlBar.wnd")`
   (`InGameUI.cpp:4212`), and code finds its windows by **global name key**
   (e.g. `"ControlBar.wnd:MoneyDisplay"`, `InGameUI.cpp:2021`) — ambiguous with
   N copies. Fix: resolve names **scoped to the instance's window tree** (pass
   the instance root to `winGetWindowFromId` instead of `nullptr`) and cache the
   handles per instance. `recreateControlBar()` (`InGameUI.cpp:5996`) shows
   teardown/rebuild already works at runtime — reuse it for join/leave re-flow.
3. **Callback routing.** GUI callbacks (`ControlBarCallback.cpp`,
   `ControlBarCommandProcessing.cpp`) resolve the *owning instance* from the
   window that fired, then act on that instance's seat/player instead of
   `TheControlBar`/`getLocalPlayer`.
4. **Radar**: `TheRadar` renders shroud/blips for one player — parameterize the
   render per bar instance's player (`Core/.../Radar.cpp` has one
   `getLocalPlayer` site).
5. Single-seat (1 player) renders exactly one bottom bar — byte-identical classic
   behavior.

Acceptance: 2-seat shared-screen dev config shows a bottom bar and a mirrored top
bar, both fully functional by controller cursor (build, queue, abilities, rally
points); 8-player splitscreen shows 4+4 quarter-width bars, each displaying the
correct player's money/power/commands.

---

## 8. Phase 5 — Skirmish lobby join/leave (slot integration)

Current: `SkirmishGameOptionsMenu.cpp` edits `GameInfo`/`GameSlot`
(`SLOT_OPEN / SLOT_CLOSED / SLOT_EASY_AI / SLOT_MED_AI / SLOT_BRUTAL_AI /
SLOT_PLAYER`, `GameInfo.h:36`); slot 0 is the local human.

Work:

1. **Join**: on the skirmish setup screen, Start/A on an unbound gamepad binds a
   seat and claims a slot, preferring the first `SLOT_OPEN`; if none, converting
   the first AI slot (console behavior per requirements: joins replace open *or
   bot* slots — bots only when no open slot remains; remember the displaced bot's
   difficulty).
2. **Leave**: B/Back from an occupied seat releases the slot back to its prior
   state (restore the displaced bot, else `SLOT_OPEN`).
3. **Slot UI**: joined slots show "Controller N" + seat color chip; each joined
   seat gets a tinted cursor (Phase 1) *scoped to its own slot row* to pick
   faction, color, team, and toggle ready. Seat 0 (or the mouse) retains map
   selection and global options; all seats must "ready" before start.
4. **Game start**: `GameInfo → Player` construction already maps slots to player
   indices; record `seat → playerIndex`, mark all seat players as local humans,
   keep slot-0 seat as legacy `getLocalPlayer`. Viewport layout spins up from the
   seat count.
5. Single-player campaign/other menus: unchanged (out of scope, controller phase
   covers skirmish lobby + in-match only, per requirements).

Acceptance: from the skirmish screen, plugging in pads and pressing A fills
slots (visibly, replacing open/bot slots), leaving restores them, and starting the
match spawns each seat controlling the right army in the right viewport.

---

## 9. Phase 6 — In-match drop-in/drop-out

1. **Leave (drop-out)**: hold Back (with confirm) →
   - seat unbinds, viewport layout re-flows
   - a new deterministic logic message (e.g. `MSG_LOGIC_LOCAL_CONTROL_RELEASE`,
     processed in `logicMessageDispatcher`) flips the player to AI via the
     existing `Player::setPlayerType(PLAYER_COMPUTER, TRUE)` path
     (`Player.cpp:863`) at a chosen difficulty (default: medium, or lobby-set).
     Going through the message stream keeps replays valid.
2. **Join (drop-in)**: Start/A on an unbound pad in-match → small overlay listing
   AI-controlled armies (and, if rejoining, the seat's former army first) →
   confirm → `MSG_LOGIC_LOCAL_CONTROL_TAKE` flips `PLAYER_COMPUTER → PLAYER_HUMAN`
   (`PlayerList.cpp:175` shows the human flip exists), seat binds to that
   playerIndex, layout re-flows, camera snaps to the army's center.
3. **Device loss** = soft drop: pause + reconnect prompt; timeout → treated as
   leave (AI takeover), consistent with the chosen leave policy.
4. Sim-side risk to spike: constructing/destroying `AIPlayer`/`AISkirmishPlayer`
   brains mid-game (`Player.h:770`) — `setPlayerType` handles it for the
   surrender/disconnect paths, but mid-game human-takeover (AI brain teardown,
   in-flight AI team behaviors) needs a focused test.

Acceptance: during a live 4-player match, one player drops (army keeps fighting
under AI), a new player joins and takes over a bot army; replay of the whole
session plays back.

---

## 10. LAN multiplayer: must keep working + transport abstraction

The command-transport seam **already exists**, and it is `TheCommandList`:

- **Solo/local**: translators → end of message stream → `TheCommandList` →
  GameLogic executes (`MessageStream.cpp:1133`).
- **LAN**: identical up to `TheCommandList`; then
  `Network::GetCommandsFromCommandList()` drains it and ships commands via the
  `ConnectionManager` (`Network.cpp:462`), and `RelayCommandsToCommandList(frame)`
  re-inserts *all* players' commands in lockstep order once frame data is ready
  (`Network.cpp:185`); GameLogic gates on `TheNetwork->isFrameDataReady()`
  (`GameEngine.cpp:841`).

Splitscreen only changes what happens **before** `TheCommandList` (per-seat
stamping, §5) — the LAN path is structurally untouched by everything in this plan.

Work:

1. **Non-regression guarantees.** In network games `TheSeatManager` binds exactly
   one seat (seat 0 = the network local player); join prompts never appear in
   LAN/online lobbies; all splitscreen render/UI paths are inert when a network
   game is active. No wire-protocol changes — LAN between two builds of this
   branch behaves exactly as before. LAN smoke test + the existing replay CRC
   workflow (`check-replays.yml`) run per phase.
2. **`CommandTransport` abstraction (new, scaffolding-only).** Formalize the seam
   so engine code stops null-testing `TheNetwork` ad hoc
   (`GameEngine.cpp:841,907`, several sites in `GameLogic.cpp`):

   ```
   class CommandTransport
   {
       virtual void collectLocalCommands() = 0;      // drain TheCommandList (or pass through)
       virtual Bool isFrameReady(UnsignedInt f) = 0; // lockstep gate; local: always TRUE
       virtual void relayFrameCommands(UnsignedInt f) = 0;
       virtual Bool isStalling() = 0;
   };
   ```

   `LocalTransport` = today's null-network passthrough (used by solo *and*
   splitscreen); `LanLockstepTransport` = thin delegate to the existing
   `Network`/`ConnectionManager`. Behavior stays identical — this is pure
   seam-naming so the two worlds can't silently entangle.
3. **Door open for splitscreen-over-LAN (deferred).** The wire protocol assumes
   one player per machine: received commands are attributed from the sender's
   connection identity (`NetCommandMsg.cpp:172`), and lobby slots are one-per-host.
   Combining would need per-command originating `playerIndex` on the wire plus
   multi-slot claiming per machine — out of scope, but per-seat stamping + the
   transport interface reduce it to a bounded protocol change later.

Acceptance: LAN skirmish between two machines on this branch plays desync-free
with splitscreen code compiled in; replay CRC workflow green; no join UI in
network lobbies.

---

## 11. Explicitly deferred (but designed for)

- **Mouse/keyboard as a seat device**: `InputRoute` (§3.5) is the plug point; the
  OS mouse/keyboard become claimable by a seat like a gamepad. MetaMap/hotkeys
  stay single-keyboard until then.
- **Shared-screen exposure**: seats-per-viewport > 1 works internally after §5/§6;
  UI toggle, camera arbitration polish, and edge-scroll rules come later.
- **Co-op (2 seats, 1 army) exposure**: command path and per-seat selection
  already permit it; needs only lobby UI ("join as ally on P1") when exposed.
- **Full frontend controller navigation**: out of scope this phase.
- **Splitscreen over LAN**: deferred — see §10.3; per-seat stamping plus the
  `CommandTransport` seam make it a bounded protocol change later. Plain
  single-seat LAN keeps working throughout (§10.1).

---

## 12. Risk register & early spikes

| Risk | Severity | Mitigation / spike |
|---|---|---|
| `GhostObjectManager` multi-local refactor (snapshot lifetimes, W3D render objects) | High | **Spike A (do first in Phase 3)**: 2 viewports, 2 render-player indices, verify ghost + shroud correctness on a fog-heavy map |
| `InGameUI`/`CommandXlat`/`ControlBar` global-state extraction (60+ sites) | High (LOC, regression surface) | Mechanical "context" extraction with seat-0 default; keep single-player path byte-identical where possible; do behind small PRs |
| `ControlBar.wnd` multi-instancing (global name-key lookups, singleton state, mirrored/scaled layout + art V-flip) | Medium-High | Instance-scoped window resolution first; **Spike C**: two bar instances (bottom + mirrored top) on one screen before any splitscreen wiring |
| Mid-game AI brain construction/teardown for takeover | Medium | **Spike B**: script-driven `setPlayerType` flips both directions mid-skirmish, soak test |
| Shroud texture per player (memory + terrain pass switching) | Medium | Measure: 8 × cell-resolution textures is small; verify terrain shader switch cost |
| Cinematics/scripted camera + letterbox vs layout manager | Medium | Multi-seat disables cinematic letterbox path; skirmish rarely uses it |
| One hardware cursor vs software cursors input lag | Low | Software cursors sampled at render time; precision mode already exists |
| Audio chaos with 8 local players | Low | Seat-0 listener + per-seat EVA throttle first pass |
| Replay/determinism regressions | Medium | Stamp asserts (§5.1) + existing replay check workflow (`check-replays.yml`) on every phase |
| LAN regression from stamping/transport scaffolding | Medium | `CommandTransport` refactor is pure passthrough (§10.2); single-seat guard in network games; LAN smoke test per phase |

Suggested build order: **Phase 0 → Spike A → Spike B → Phase 1 → 2 → 3 → 4 → 5 → 6**.
Phases 1 and 2 are independent and can proceed in parallel. Each phase lands
behind a `-splitscreen` dev flag / debug config until Phase 5 makes it user-facing.

---

## 13. Key file map

| Area | Files |
|---|---|
| Seats (new) | `Core/GameEngine/{Include,Source}/Common/SeatManager.*`, `LocalSeat.*` |
| SDL3 gamepads | `Core/GameEngineDevice/{Include,Source}/SDL3Device/GameClient/SDL3Input.*` |
| Cursor render (new) | `Core/GameEngineDevice/.../W3DDevice/GameClient/W3DSeatCursorRenderer.*`; art via `Mouse.ini` cursors |
| Message stamping | `GeneralsMD/.../Common/MessageStream.{h,cpp}` (`m_playerIndex`, ctor at `MessageStream.cpp:57`) |
| Command dispatch (read-only reference) | `Core/.../GameLogic/System/GameLogicDispatch.cpp:353` |
| Translators | `GeneralsMD/.../GameClient/MessageStream/{SelectionXlat,CommandXlat,LookAtXlat,PlaceEventTranslator,GUICommandTranslator}.cpp` |
| Selection/UI context | `GeneralsMD/.../GameClient/InGameUI.{h,cpp}` (`m_selectedDrawables` at `InGameUI.h:729`, view creation at `InGameUI.cpp:1370`) |
| Views/viewports | `Core/.../GameClient/Display.cpp` (view list), `Core/GameEngineDevice/.../W3DView.cpp` (`Set_Viewport` at 219) |
| Per-player render | `GeneralsMD/.../W3DDevice/GameClient/W3DScene.cpp:626`, `W3DShroud.*`, `{Generals,GeneralsMD}/.../W3DGhostObject.*`, `Core/.../ParticleSys.h:790` |
| Control UI | `GeneralsMD/.../GUI/ControlBar/*` (per-seat instancing), `ControlBar.wnd` name-key scoping in `InGameUI.cpp:4209–4332`, `GameWindowManager` |
| Lobby | `GeneralsMD/.../GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp`, `Core/.../GameNetwork/GameInfo.h` (slots) |
| Join/leave sim | `GeneralsMD/.../Common/RTS/Player.cpp:738` (`setPlayerType`), `PlayerList.cpp:175`, new `MSG_LOGIC_LOCAL_CONTROL_{TAKE,RELEASE}` |
| Command transport | `Core/.../GameNetwork/Network.cpp:462,185` (existing seam), `GeneralsMD/.../Common/GameEngine.cpp:841,907`, new `Core/.../Common/CommandTransport.*` |
