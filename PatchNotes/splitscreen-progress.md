# Splitscreen — Progress Tracker & Decision Log

Living document. The implementing agent updates this file **every session**:
tick checkboxes, append decisions, record human-checkpoint results. Read this
file at the start of every session to recover state. Docs:
`splitscreen-plan.md` (design) → `splitscreen-plan2.md` (work packages) →
`splitscreen-conventions.md` (idioms, build, testing) → this file (state).

## 1. Environment (fill once, first session)

- [ ] Build preset confirmed with user: `____________` (default guess: `win32-vcpkg-debug`)
- [ ] Build target name for Zero Hour exe: `____________`
- [ ] Game install / run directory: `____________`
- [ ] Launch flags for windowed dev testing: `____________`
- [ ] How INI/data changes reach the game dir: `____________`
- [ ] Local replay-check command (from `check-replays.yml`): `____________`

## 2. Pre-flight verification — ALL ANSWERED 2026-07-02 (by code reading)

| # | Question | Needed by | Answer |
|---|---|---|---|
| V1 | 2D draw color multiply? | WP3 | **Yes.** `Render2DClass::Add_Quad(..., unsigned long color = 0xFFFFFFFF)` modulates texture by per-quad vertex color (`GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.h:118-127`). **No shader needed** — pass the seat color as the quad color. |
| V2 | `winGetWindowFromId(parent, id)` scoping? | WP8 | **Subtree + trailing siblings** (`GameWindowManager.cpp:654-678`): starts at the given window, recurses children, **then walks `m_next` siblings** — a missing child would silently match a later bar instance. → A strict `findChildById(root, id)` helper that recurses only into `root->winGetChild()` is **mandatory** for WP8. Scoped-lookup precedent: `IMEManager.cpp:575-582`. |
| V3 | Slot index → `playerIndex` mapping? | WP5, WP9 | `GameLogic::startNewGame` (`GeneralsMD/.../GameLogic.cpp:1384-1412`): slot *i* becomes side dict with `playerName = "player%d" % slotIdx`; `ThePlayerList->newGame()` builds Players from `TheSidesList`. Recipe: `ThePlayerList->findPlayerWithNameKey(NAMEKEY(fmt("player%d", slotIdx)))->getPlayerIndex()` — same as `NetCommandMsg.cpp:172`. |
| V4 | Is adding `m_seatIndex` serialization-safe? | WP2 | **Yes.** Recorder writes exactly type + playerIndex + args (`Recorder.cpp:711-712` write, `:1338-1340` read); NetCommandMsg likewise reconstructs from type/args/sender. `m_seatIndex` stays client-side; replay/net-received messages default to seat 0, which is correct (they never re-enter client translators). |
| V5 | Mid-game `setPlayerType(PLAYER_COMPUTER, TRUE)`? | WP9 | **Mechanism exists, one known hazard.** `Player.cpp:738-755`: deletes `m_ai`, creates `AISkirmishPlayer(this)`. BUT skirmish AI scripts are duplicated/qualified only at map load in `initFromDict` for slots that are AI *at that time* (`Player.cpp:863-886`); human slots get civilian scripts (`:829-858`). Mid-game human→AI takeover ⇒ AI likely scriptless/passive. Probable fix: when splitscreen enabled, also duplicate+stash skirmish scripts for human slots at load (or run the qualify step at takeover). **Spike B verifies runtime behavior of both flip directions.** |
| V6 | Letterbox state location? | WP6 | **Display-global, not per-view**: `Display.h:182-185,221-223` (`enableLetterBox`, `m_letterBoxEnabled`, fade); consumed at `W3DView.cpp:344` via `TheDisplay->isLetterBoxed()`. Multi-seat: make `enableLetterBox` a no-op when >1 seat bound. |
| V7 | Insertion point for seat stream messages? | WP2 | `GameClient::update` — `GameClient.cpp:610-611` (`TheMouse->UPDATE(); TheMouse->createStreamMessages();`). Insert `TheSeatManager->createStreamMessages()` immediately after :611. Keyboard equivalent at :599-600. |
| V8 | `ControlBarResizer` arbitrary scale? | WP8 | **No** — INI-driven two-state only (`ControlBarResizer.h:63-89`: per-named-window `default` and `alt` size/pos). WP8 writes its own recursive uniform-scale helper; the resizer is precedent, not a tool. |
| V9 | House color getter? | WP3 | `Color Player::getPlayerColor() const` (`Player.h:251`, returns `m_color`). |
| V10 | `GameMessage` ctor safe in shell? | WP2 | **Yes in practice**: the shell already constructs GameMessages today; `getLocalPlayer()` asserts non-null (`PlayerList.h:120`) and `setLocalPlayer` handles first-call null (`PlayerList.cpp:315-319`). Rule: never construct a `GameMessage` before `ThePlayerList` init. |

## 3. Decision log (append-only; one line each: date, WP, decision, why)

- 2026-07-02 · plan · Seat ≠ Viewport ≠ Player abstractions; shared-screen & co-op internal-only (user)
- 2026-07-02 · plan · AI takeover on leave; cursor = house color; lobby+in-game controller scope only (user)
- 2026-07-02 · WP8 · No new GUIs — classic ControlBar instanced; top bars vertically mirrored; 8P = 4+4 quarter-width (user)
- 2026-07-02 · WP9 · LAN must keep working; `CommandTransport` seam formalized around `TheCommandList` (user)
- 2026-07-02 · test · User has real controllers; 3 pads = test ceiling (4–8 seats add no new code paths — layout only, cover with fake seats) (user)
- 2026-07-02 · all · Pre-flight V1–V10 resolved by code reading (see §2); WP9 gains the skirmish-script-stash fix candidate from V5

## 4. Work package status

Legend: `[ ]` not started · `[~]` in progress · `[x]` done+verified · `[!]` blocked (see log)

### WP0 — Seat skeleton
- [ ] SeatManager.h/.cpp created, CMake registered
- [ ] Subsystem wired into GameEngine init/reset order
- [ ] Debug: seat table logged on init
- [ ] Build green, game boots unchanged

### WP1 — Gamepad hotplug + debug harness
- [ ] Multi-pad map replaces `openFirstGamepad` / single `m_gamepad`
- [ ] ADDED/REMOVED events; `onDeviceDisconnected` → `SEAT_DEVICE_LOST`
- [ ] Gamepad virtual key/mouse injection removed (callers checked)
- [ ] `SeatInputState` + logical buttons (`SeatInput.h`)
- [ ] Debug harness: fake seats (Ctrl+Alt+F1..F8), keyboard-possessed pad, seat overlay (conventions §3)
- [ ] Build green; overlay shows live pad state

### WP2 — Seat-tagged raw input
- [ ] V4, V7, V10 answered
- [ ] `GameMessage::m_seatIndex` (client-only, default 0)
- [ ] `SeatManager::createStreamMessages()` (cursor integration + MSG_RAW_* emission)
- [ ] **HC1 human checkpoint**: seat-0-by-pad plays a normal skirmish → result: ____

### WP3 — Software cursors + tint
- [ ] V1, V9 answered
- [ ] `W3DSeatCursorRenderer` draws all bound seats post-UI
- [ ] Tint = house color / fallback palette; OS cursor hidden when seat 0 pad-driven
- [ ] **HC2 human checkpoint**: multiple tinted cursors → result: ____

### WP4 — SeatUIContext extraction
- [ ] `SeatUIContext` + `m_seatContexts[MAX_SEATS]`; legacy methods forward to [0]
- [ ] Selection, hints, placement, moused-over moved
- [ ] `Drawable` selected-bool → seat mask; callers classified (render=AnySeat, command=seat 0)
- [ ] Single-player smoke identical (select/box/build/rally)

### WP5 — Seat-aware translators + stamping
- [ ] V3 answered; dev-start assigns seat→slot for testing
- [ ] SelectionXlat / CommandXlat / LookAtXlat / PlaceEventTranslator seat-derived
- [ ] Every translator logic-command emission stamped; dispatcher assert added
- [ ] WindowXlat: seat-0-only window guard (until WP8)
- [ ] Replay record/playback of a 2-seat match verified
- [ ] **HC3 human checkpoint**: 2 seats, 2 armies, shared screen → result: ____

### WP6 — Viewport layout
- [ ] V6 answered
- [ ] `ViewportLayout.*`: layout table, apply/re-flow, seat→view assignment
- [ ] `View::m_renderPlayerIndex`; seat 0 view == TheTacticalView (resized, never destroyed)
- [ ] Dividers; letterbox disabled for multi-seat
- [ ] 1/2/4/8 fake-seat layouts render; single-seat pixel-identical

### WP7 — Per-viewport render player
- [ ] **Spike A first** (2 views, rows 1–3 of plan2 WP7 table) → result: ____
- [ ] `ScopedRenderPlayer` RAII wraps per-view draw
- [ ] W3DScene hiding · particles · shroud textures per player
- [ ] Ghost objects multi-local (storage per index, render-select, save/load strategy decided → log)
- [ ] Stealth/occlusion sweep of W3DDevice drawable modules
- [ ] **HC4 human checkpoint**: per-viewport fog & ghosts correct → result: ____

### WP8 — ControlBar instancing
- [ ] V2, V8 answered
- [ ] **Spike C first**: 2 instances, bottom + mirrored top, clickable → result: ____
- [ ] Instance array + `TheControlBar` = instance 0; window-pointer cache per instance
- [ ] All `"ControlBar.wnd:*"` lookups scoped (grep shows zero nullptr-parent lookups)
- [ ] Callback routing via `fromWindow`; 30 `getLocalPlayer` sites → `m_player`
- [ ] Geometry: dock/scale/mirror transforms; UV-flip bar art
- [ ] Per-seat window input hit-testing (lifts WP5 guard)
- [ ] Radar per instance
- [ ] **HC5 human checkpoint**: mirrored bar fully functional → result: ____

### WP9 — Join/leave + transport
- [ ] V5 (Spike B) answered → result: ____
- [ ] Lobby: join claims open→bot slots (restore state remembered); leave restores; slot UI updates
- [ ] Per-seat lobby cursors confined to own slot row
- [ ] Match start: seat→playerIndex map; layout applied
- [ ] `MSG_LOGIC_LOCAL_CONTROL_RELEASE/TAKE` + dispatcher cases + client bind/unbind/re-flow
- [ ] `CommandTransport` scaffolding (separate commit, passthrough-only)
- [ ] Network guards: bind refused when networked; layout asserts 1 seat
- [ ] **HC6 human checkpoint**: full join/leave loop + LAN regression → result: ____

## 5. Known deferred items (do not implement)

Mouse/keyboard seats (InputRoute exists, unused) · shared-screen/co-op UI
exposure · splitscreen-over-LAN · full frontend controller nav · per-seat group
hotkeys · Generals (non-ZH) port.
