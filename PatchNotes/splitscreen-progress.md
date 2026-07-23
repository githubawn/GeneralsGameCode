# Splitscreen — Progress Tracker & Decision Log

Living document. The implementing agent updates this file **every session**:
tick checkboxes, append decisions, record human-checkpoint results. Read this
file at the start of every session to recover state. Docs:
`splitscreen-plan.md` (design) → `splitscreen-plan2.md` (work packages) →
`splitscreen-conventions.md` (idioms, build, testing) → this file (state).

## 1. Environment (fill once, first session)

- [~] Build preset confirmed with user: `____________` — **NEEDS USER CONFIRM.**
  A configured build already exists at `build/win32` (CMake generator
  `Ninja Multi-Config`, `RTS_BUILD_OPTION_SDL3:BOOL=ON`). Compiles verified there
  with MSVC 2022. `build/win32-vcpkg-debug` does NOT exist yet. Ask the user
  whether to keep using `build/win32` or configure the doc's `win32-vcpkg-debug`.
- [ ] Build target name for Zero Hour exe: `____________` (lib targets seen:
  `z_gameengine`, `z_gameenginedevice`; exe target name still to confirm)
- [ ] Game install / run directory: `____________`
- [ ] Launch flags for windowed dev testing: `____________`
- [ ] How INI/data changes reach the game dir: `____________`
- [ ] Local replay-check command (from `check-replays.yml`): `____________`

Build note (verified 2026-07-22): the Bash shell lacks `midl.exe`/MSVC on PATH;
builds must run from a VS dev environment. Working recipe used this session:
`Enter-VsDevShell` (VS 2022 Community, `-arch=x64`) then
`ninja -f build-Debug.ninja <target>` in `build/win32`.

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
- 2026-07-22 · WP0 · Seat 0 = keyboard/mouse (m_deviceId = -1), always BOUND; gamepads claim seats 1..7 (findFreeSeat starts at 1). Per plan2 WP0 + conventions §Subsystems.
- 2026-07-22 · WP0 · SeatManager registered as a SubsystemInterface in GeneralsMD GameEngine.cpp immediately before TheGameClient (input+InGameUI live inside TheGameClient, so "after input / before InGameUI" both resolve to just-before-TheGameClient). reset() keeps device bindings, clears per-match state.
- 2026-07-22 · WP1 · **Deviation from plan2 WP1 step 4 ("delete gamepad injection"):** kept the legacy gamepad→mouse/keyboard injection but GATED it behind the splitscreen dev flag. Flag OFF ⇒ first pad drives the OS mouse exactly as today (zero single-player regression, satisfies ground rule 2 + plan.md §3 "playable at every commit"). Flag ON ⇒ pads route to seats via setSeatInput instead. Reversible; injection is deleted for good once WP2 (createStreamMessages) makes seat-0 controller play work through the message stream.
- 2026-07-22 · WP1 · Splitscreen gate lives on SeatManager (`m_enabled`, isSplitscreenEnabled) rather than reading TheGlobalData from Core code — avoids Core depending on a GeneralsMD-only GlobalData field (Generals build shares Core). The `-splitscreendev` command-line flag + GlobalData wiring that flips it is still TODO (WP1 remainder).
- 2026-07-22 · env · Build verified compiling via existing `build/win32` (Ninja Multi-Config, MSVC 2022 through Enter-VsDevShell). User confirmed: keep using `build/win32`. Preset alt (`win32-vcpkg-debug`) not needed.
- 2026-07-23 · WP3 · Per-seat software cursors, additive-only. (A) `SeatManager::createStreamMessages` now integrates each bound device-seat's (1..7) own virtual cursor from its left stick: fixed 15px/frame step (x0.35 when left trigger > 0.5), center-seeded via m_cursorInit, clamped to [0..TheDisplay width/height], writes m_cursor.pos + visible=TRUE. Does NOT touch TheMouse and appends NO GameMessages (translators seat-unaware until WP5). (B) New Core/GameEngineDevice W3DDevice files `W3DSeatCursorRenderer.{h,cpp}` (CMake SDL3 block) draw each visible seat cursor as a small solid tinted right-triangle arrow (tip = hot-spot) + 1px black outline, via `TheDisplay->drawFillRect/drawLine` — chose Display primitives over a hand-rolled Render2DClass for robustness (they set up shader/coord-range; still Add_Quad-tinted per V1, no shader). No cursor art assets added. (C) Tint = `getNthPlayer(m_playerIndex)->getPlayerColor()|0xFF000000` when the seat has a player, else an 8-color `fixedSeatPalette`. (D) Hooked in `W3DDisplay::draw` immediately after `TheMouse->DRAW()` (so cursors sit on top of UI+mouse), inside `#if RTS_SDL3_ENABLE`; whole renderer gated on `TheSeatManager && isSplitscreenEnabled()`. (E) OS-cursor hiding for a pad-driven seat 0 deliberately skipped (WP5 concern; WP3 additive only). Compile-verified (MSVC 2022, x86, build/win32 Debug): SeatManager.cpp.obj (z_gameengine), W3DSeatCursorRenderer.cpp.obj + W3DDisplay.cpp.obj (z_gameenginedevice) all built clean; W3DDisplay's only warning (C4018 @2043) is pre-existing and unrelated.
- 2026-07-23 · scope · **User scoping decision:** goal is mkb = player 1, one controller = player 2, shared screen. Build **WP3 → WP4 → WP5** (the player-2 core) then **WP6 (split viewports)**. **Defer WP7 (per-viewport fog/shroud/ghosts) and WP8 (per-seat control bars) and WP9 (lobby)** for now — split viewports without per-player fog is acceptable for this pass. Each WP delegated to a subagent, run sequentially in the working tree (WP0-2 uncommitted → no worktree isolation; WP5 depends on WP3+WP4).
- 2026-07-23 · WP2 · **Reverted the shared-mouse experiment** per user feedback: `SeatManager::createStreamMessages` is now empty and the SDL3 loop no longer `continue`s past legacy injection for a bound pad. The primary pad keeps the FULL legacy button mapping (all buttons restored). `GameMessage::m_seatIndex` tag infrastructure kept. The controller is player-1-via-mouse until WP5 moves it to its own seat/player.
- 2026-07-22 · WP2 · `createStreamMessages` makes the active pad seat drive the shared OS cursor (`TheMouse->setPosition` + a new `SDL3Mouse::setPosition` that warps the OS cursor) and emits seat-tagged raw mouse messages. Chosen over pure message emission because the visible cursor is the OS hardware cursor (engine `setPosition` alone wouldn't move it), so a pad wouldn't be *visibly* playable pre-WP3 otherwise. Reconciles WP2 acceptance ("indistinguishable from mouse events") with WP0's seat-0=kb/mouse model: a pad possesses the shared pointer as the local player until WP5 splits control per seat.
- 2026-07-22 · WP1 · Seat debug overlay reuses the existing `DebugDisplay` callback slot (`setDebugDisplayCallback`), auto-installed by `SeatManager::update()` only when the slot is free (won't stomp an active F-key debug display). Avoided new `MSG_META`/`CommandMap.ini` data changes. Overlay is RTS_DEBUG-only (that facility is), which is fine for dev.
- 2026-07-22 · WP1 · `-splitscreendev` gate lives on GlobalData (GeneralsMD only) and is pushed into SeatManager; SeatManager itself never reads GlobalData (keeps Core from depending on a GeneralsMD-only field, so the shared Core file stays clean for a Generals SDL3 build too).
- 2026-07-22 · WP0 · **VC6 gating (user directive):** the whole splitscreen feature depends on SDL3 (multiple mice / 8 controllers), so the seat model is useless on VC6. Corrects plan2 WP0's "device-independent, registered unconditionally": SeatManager/SeatInput are still SDL-free C++, but are compiled **only into SDL3 builds** — cmake entries gated behind `RTS_BUILD_OPTION_SDL3` (forced OFF for VC6), and the `GameEngine.cpp` include + `initSubsystem(TheSeatManager,...)` wrapped in `#if RTS_SDL3_ENABLE` (the same macro WinMain uses to pick SDL3GameEngine). VC6 compiles zero seat code. Verified win32 (SDL3 ON) still builds with the gate.

## 4. Work package status

Legend: `[ ]` not started · `[~]` in progress · `[x]` done+verified · `[!]` blocked (see log)

### WP0 — Seat skeleton
- [x] SeatManager.h/.cpp created, CMake registered (`Core/GameEngine`, both lists)
- [x] Subsystem wired into GameEngine init/reset order (before TheGameClient)
- [x] Debug: seat table logged on init (`SeatManager::logSeatTable`)
- [~] Build green, game boots unchanged — **compiles green** (SeatManager.cpp,
      GameEngine.cpp, SDL3Input.cpp objects built clean, MSVC 2022). Runtime
      boot not yet verified (needs run env, §1).

### WP1 — Gamepad hotplug + debug harness
- [x] Multi-pad map replaces `openFirstGamepad` / single `m_gamepad`
      (`std::map<SDL_JoystickID, PadEntry>` in SDL3Input)
- [x] ADDED/REMOVED events; `onDeviceDisconnected` → `SEAT_DEVICE_LOST`
- [~] Gamepad virtual key/mouse injection **gated, not removed** (see decision
      log 2026-07-22): preserved for flag-off single-player; bypassed when the
      splitscreen flag is on. Full deletion deferred to WP2.
- [x] `SeatInputState` + logical buttons (`Core/GameEngine/Include/GameClient/SeatInput.h`)
- [~] Debug harness (conventions §3): **seat overlay DONE** (`SeatDebugDisplay` in
      SeatManager.cpp, auto-installed via `TheDisplay->setDebugDisplayCallback`
      while the flag is on; shows connected-pad count + per-seat live axes/buttons,
      RTS_DEBUG builds). **Fake seats (Ctrl+Alt+F1..F8) + keyboard-possess
      DEFERRED** — clean impl needs either a small public `Keyboard::isKeyDown`
      accessor (`getKeyStatusData` is protected) or new `MSG_META` keys +
      `CommandMap.ini` entries; user tests with real pads so this is agent-only
      scaffolding, punted to when hardware-less testing is needed.
- [x] `-splitscreendev` command-line flag → `GlobalData::m_splitscreenEnabled` →
      `TheSeatManager->setSplitscreenEnabled()` (parsed in
      `parseCommandLineForEngineInit`, pushed right after the subsystem is created).
      Per-frame `TheSeatManager->UPDATE()` hook added in `GameClient::update`
      after the mouse (WP2's stream-message insertion point).
- [x] Build green; overlay shows live pad state (6 TUs compiled clean, MSVC 2022)

### WP2 — Seat-tagged raw input
- [x] V4, V7, V10 answered (V4 re-confirmed by grep this session: Recorder/NetCommandMsg copy only type/playerIndex/args)
- [x] `GameMessage::m_seatIndex` (client-only, default 0, getter/friend-setter, ctor init; NOT serialized)
- [x] `SeatManager::createStreamMessages()` — integrates the active pad seat's
      virtual cursor, moves the cursor via `TheMouse->setPosition` (SDL3 warps the
      OS cursor so it's visible), and injects seat-tagged `MSG_RAW_MOUSE_*`
      (CONFIRM→left, CANCEL→right, held-CONFIRM+move→drag). Called from
      `GameClient::update` right after the mouse's own `createStreamMessages`.
- [ ] **HC1 human checkpoint**: pad plays a normal skirmish → result: **PENDING (user to test)**

WP2 scope notes / known artifacts to check at HC1:
- Only the **lowest-index bound pad seat** drives the (single, shared) cursor and
  the game for now; seat 0 stays keyboard/mouse; extra pads become independently
  playable at WP5 (seat-aware translators) + WP3 (per-seat cursors). So test with
  **one pad**.
- Translators are not seat-aware yet, so the pad acts as the **local player**
  (its `m_seatIndex` tag is carried but not yet consumed — that's WP5).
- Cursor speed is a fixed per-frame step (`SEAT_CURSOR_STEP=15`, precision on left
  trigger), not dt-scaled yet — feel may need tuning.
- The **A-press that joins a seat** also emits a left-click that same frame (minor;
  can gate later). Right-stick camera pan not wired yet (WP5/LookAtXlat).

### WP3 — Software cursors + tint
- [x] V1, V9 answered (see §2)
- [x] `W3DSeatCursorRenderer` draws all bound seats post-UI (Core/GameEngineDevice
      W3DDevice file; hooked in W3DDisplay::draw after TheMouse->DRAW, RTS_SDL3-gated)
- [x] Per-seat cursor integration in `SeatManager::createStreamMessages` (seats 1..7,
      left-stick step 15px, x0.35 on left trigger, center-seeded, clamped to display)
- [x] Tint = house color (`getNthPlayer(m_playerIndex)->getPlayerColor()`) / 8-color
      fallback palette. **OS cursor hiding for a pad-driven seat 0 intentionally NOT
      done** — WP3 is additive-only (seat 0 stays keyboard/mouse; that is a WP5 item).
- [ ] **HC2 human checkpoint**: multiple tinted cursors → result: **PENDING (user to test)**

WP3 scope notes:
- Cursor art is a small solid tinted right-triangle arrow pointer (hot-spot at the
  tip) with a 1px black outline, drawn via `TheDisplay->drawFillRect`/`drawLine`
  (which route through W3DDisplay's configured Render2DClass batch — Add_Quad/Add_Rect
  modulate by the per-quad vertex color per V1, so the tint needs no shader). Chose
  the Display 2D primitives over a self-managed Render2DClass for robustness (they
  already handle shader/coord-range/state setup). No cursor texture/art assets added.
- Only seats with `m_cursor.visible == TRUE` draw; visible is set only for a bound
  seat with a real device in createStreamMessages, so seat 0 (OS mouse) and unbound
  seats are naturally skipped.
- Test with the splitscreen dev flag ON and ≥1 pad bound to a seat (1..7).

### WP4 — SeatUIContext extraction
- [x] `SeatUIContext` (nested in InGameUI, public) + `m_seatContexts[MAX_SEATS]`;
      `getSeatContext(Int)`; every legacy accessor forwards to seat 0
- [x] Selection, hints, placement, moused-over, UI-mode flags moved into SeatUIContext
- [x] `Drawable` selected-bool → seat mask; callers classified (render=AnySeat, command=seat 0)
- [x] Build green (SDL3 x86, build/win32 Debug): z_gameengine.lib + z_gameenginedevice.lib
      relinked clean; InGameUI.cpp/Drawable.cpp/W3DInGameUI.cpp recompiled with zero NEW
      warnings (only pre-existing C4018/C5055 in untouched functions).
- [ ] Single-player smoke identical (select/box/build/rally) — **PENDING (user to test)**

### WP5 — Seat-aware translators + stamping  (done via a SCOPED approach, not per-site)
- [x] Instead of editing 30+ translator sites, the whole existing translator chain is
      run through a scoped "active seat": `MessageStream::propagateMessages` sets
      `TheInGameUI->setActiveSeat(msg->getSeatIndex())` and a
      `TheSeatActingPlayerOverride` around each `translateGameMessage`, both restored
      immediately after (gated `#if RTS_SDL3_ENABLE`, only for seatIdx>0).
- [x] InGameUI legacy accessors now resolve `m_seatContexts[m_activeSeat]` (was `[0]`);
      m_activeSeat is 0 in all normal/render frames, so single-player is unchanged.
      → SelectionXlat/CommandXlat/PlaceEventTranslator become seat-aware for free
      (they call the same InGameUI methods; picking reads the msg pixel arg, which
      carries the seat cursor pos).
- [x] Command stamping: `GameMessage` ctor uses `TheSeatActingPlayerOverride` when set,
      so commands the translators create during a seat message are attributed to that
      seat's player. (No global setLocalPlayer swap — it fires becomingLocalPlayer.)
- [x] Seat message emission re-added to `SeatManager::createStreamMessages` (cursor +
      A=left / B=right, seat-tagged); a bound pad no longer drives the OS mouse
      (SDL3 `continue`s past legacy injection).
- [x] Dev seat→player mapping (no lobby yet = WP9): seat k → getNthPlayer(k) once a
      match is running. **Caveat: that player may be an AI that fights the controller;
      a clean human player 2 needs WP9.**
- [x] Build green (SDL3 x86): z_gameengine.lib + z_gameenginedevice.lib link clean.
- [ ] NOT done this pass: LookAtXlat camera per-seat (right-stick pan), WindowXlat
      seat-0 guard, per-site dispatcher assert, replay verify, meta/hotkey buttons for
      the controller (Y/D-pad/etc. — only cursor + A/B emit for a bound seat now).
- [ ] **HC3 human checkpoint**: controller selects & commands its mapped player's army
      independently of the mouse → result: **PENDING (user to test)**

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
