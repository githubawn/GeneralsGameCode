# Splitscreen bug sweep — handoff #3 (2026-08-01)

Successor to `splitscreen-bugfix-handoff2.md`. That file's remaining open items (5.1-5.5) are
untouched by this round — this round is a fresh batch of **13 findings from 11 user reports**,
researched via 5 parallel agent passes (no code written until research was presented and the user
confirmed scope), now being implemented one at a time. **1 of 13 is committed; 12 are still open.**

User context for this round: gamepad is player 4; players 2 and 3 are AI spectator (observer)
seats, not human.

## 1. Where things stand

Branch `splitscreen-documents`. Commit `c2a26a4e8` lands finding #2/#3 (broadcast half) below.
Everything else in this file is diagnosed with file:line citations but **not yet implemented**.

Build: Release only, win32 x86 — see `splitscreen-progress.md` §1 for the exact recipe
(`Enter-VsDevShell` + `ninja -f build-Release.ninja z_generals` from `build/win32`, prepend the
VS Installer dir to PATH first). Verified working this session.

## 2. The 13 findings, mapped to the user's 11 reports

Each item below is a self-contained fix — implement, build, move to the next. None of these are
verified at runtime yet (same caveat as every prior round).

### #1 — Communicator button always opens in the main window
`GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Diplomacy.cpp:89-90` — file-static
`theLayout`/`theWindow`, zero seat plumbing anywhere in the file (confirmed by grep: no "seat" hits
at all). `ControlBarCallback.cpp:411-414` calls `ToggleDiplomacy(FALSE)` on `GBM_SELECTED` with no
seat argument, unlike the `GBM_MOUSE_ENTERING`/`LEAVING` handlers right above it which already
route through `ControlBarInstances::fromWindow(control)`. `Diplomacy.wnd` opens at its authored
absolute screen position. **Fix shape:** thread the acting seat from the `GBM_SELECTED` handler
into `ToggleDiplomacy`/`ShowDiplomacy`; turn `theLayout`/`theWindow` and the per-slot widget
statics (lines 63-85) into `[MAX_SEATS]` arrays, position each within its owning seat's viewport.

### #2/#3 splash — "you have been defeated" / "you are victorious" full-screen popups
`ScriptActions::doDefeat`/`doLocalDefeat`/`doVictory` (`ScriptActions.cpp:207-268`) share one
**static** `GameWindow *m_messageWindow` (`ScriptActions.h:84`) and pick the window using
`ThePlayerList->getLocalPlayer()` (lines 214, 238) — the non-seat-aware singleton. User wants
**both** defeat and victory scoped to the proper player's own viewport (clarified 2026-08-01 — the
earlier "keep victorious as-is" only meant "don't change which mechanism", not "leave it
centered"). **Fix shape:** same pattern as the broadcast fix already committed — resolve
`rts::getSeatIndexForPlayer()` for the player the script action concerns, position/parent the
popup within that seat's viewport instead of the whole display. `doVictory`/`doDefeat`/
`doLocalDefeat` are three near-identical functions sharing one static; the array-per-seat
approach (mirroring `SeatUIContext`) is the same shape used for the broadcast text — a
`GameWindow* m_messageWindow[MAX_SEATS]` likely works for all three call sites at once.

### #2/#3 broadcast — "Player X has been defeated" text — **DONE, commit `c2a26a4e8`**
`InGameUI::m_uiMessages[]`/`m_messagePosition` were a flat member drawn once at a fixed
display-relative position (`InGameUI.cpp` old lines 4059-4094). Moved into `SeatUIContext` (one
array per seat, matching the established Pattern B). `postDraw()` now draws each seat's own queue
relative to that seat's own viewport origin (`LocalSeat::m_view->getOrigin()`), skipping a seat
with nothing queued. New `InGameUI::messageForSeat(seat, label, ...)` lets a logic-side caller
(where `m_activeSeat` is always 0, since it's only meaningful during input translation) name the
seat explicitly. `VictoryConditions.cpp:210` now resolves `rts::getSeatIndexForPlayer(p->getPlayerIndex())`
and routes there, falling back to seat 0 exactly as before when no local seat watches that player.
Every other `message()`/`addMessageText()` call site is untouched (default seat still resolves to
`m_activeSeat`), so single-view play is byte-identical. **This also supplies the plumbing #6 (under-attack text) needs** — route through `messageForSeat`/`addMessageText(..., seat)` the same way.

### #4 — Control-bar theme disappears for every seat when player 1 is defeated
`Player::killPlayer()` (`Player.cpp:2019`) is guarded by `isLocalPlayer()` (`Player.cpp:1905-1908`,
`== ThePlayerList->getLocalPlayer()`, only ever true for player 1) and on defeat calls
`TheControlBar->setControlBarSchemeByPlayerTemplate(FactionObserver)` (`Player.cpp:2083`). That
mutates `ControlBarSchemeManager::m_currentScheme` (`ControlBarScheme.h:289`), which **every
seat's `ControlBar` shares** — `initAsSeatInstance` (`ControlBar.cpp:1420`) points all 8 bars at
`TheControlBar`'s manager. Per-frame skin paint (`W3DControlBar.cpp:646-720`) resolves the correct
per-bar window but then draws through the one shared scheme object, so player 1's defeat
reskins every other seat's bar to blank Observer. A second latent instance of the same bug:
`rts::changeLocalPlayer()` (`Core/GameEngine/Source/Common/GameUtility.cpp:234`) has the identical
unscoped `TheControlBar->setControlBarSchemeByPlayer(player)` call.
**Fix shape:** give each seat's `ControlBar` its own `ControlBarSchemeManager` instance (or at
minimum don't let one bar's defeat-triggered scheme change propagate to bars sharing the manager)
— `applySchemeForBarPlayer()` already exists as the per-bar entry point; `killPlayer()` should call
through the *defeated player's own seat's bar* if one exists, not the global `TheControlBar`.

### #5 — AI observer spectators have their HQ auto-selected
`GameLogic::startNewGame`'s `findAndSelectCommandCenter` (`GameLogic.cpp:2468-2481`) computes
`rts::getSeatIndexForPlayer(obj->getControllingPlayer()->getPlayerIndex())` (`GameUtility.cpp:159-179`)
with **no check of `LocalSeat::m_observer`** (`SeatManager.h:150`). An observer seat watching a
live AI army resolves to a real seat index exactly like a human-controlled seat, so
`GameLogic::selectObject` (`GameLogic.cpp:2790-2792`) selects that AI's command center into the
observer's own selection context — and since round 3k's fix made every `ControlBar` correctly
read its own seat's selection, the (now-correct) per-seat highlighting faithfully shows it as
selected. **Fix shape:** exclude observer seats in `getSeatIndexForPlayer` (or add a variant/flag
that `findAndSelectCommandCenter`'s caller uses) so an observed AI's own HQ is never
auto-selected into the watching seat's context.

### #6 — Unit-under-attack text only for player 1, and shows in main window
Two stacked defects. **(a)** `Object::isLocallyControlled()` (`Object.cpp:1727-1730`,
`== ThePlayerList->getLocalPlayer()`) gates the `attemptDamage` call to `TheRadar->tryUnderAttackEvent()`
(around `Object.cpp:1945-1952`) — so the event never even fires for a unit not controlled by
whichever player that singleton resolves to. The correct pattern is one function away:
`Object::isLocallyViewed()` (`Object.cpp:1743-1746`) already uses `rts::getObservedOrLocalPlayer()`
correctly. **(b)** Where it does fire, `Radar::tryUnderAttackEvent` (`Radar.cpp:1043-1109`) resolves
`rts::getObservedOrLocalPlayer()` from GameLogic-side code (render-only-safe helper, always
answers player 1 outside a render pass — the "third disguise" bug class from progress.md round
3g/h/i/103ff6bb5), and `Player::isLocalPlayer()` (`Player.cpp:1905-1908`) has the same defect a
third time at `Radar.cpp:1093`. **(c)** The resulting `TheInGameUI->message(...)` calls
(`Radar.cpp:1073,1081,1099`) go through the now-fixed per-seat feed from #2/#3 — but need to pass
the concerned seat explicitly via `messageForSeat`, same as the defeat fix.
**Fix shape:** replace `isLocallyControlled()` in the `attemptDamage` gate with a per-seat test
(does *some* local seat control this object — likely a new `Object::isLocallyControlledByAnySeat()`
or loop `rts::getSeatIndexForPlayer` at the call site); replace the two `Radar.cpp` singleton
reads with `rts::getSeatIndexForPlayer(obj->getControllingPlayer()->getPlayerIndex())`; route the
three `message()` calls through `messageForSeat(seat, ...)`.

### #7 — Gamepad lasso not drawn
`W3DInGameUI.cpp:403,459-471` — `W3DInGameUI::draw()` runs once per frame (not per view, unlike
the per-view loop at lines 408-427 just below it for move/attack hints) and only checks/draws
`m_seatContexts[0].m_isDragSelecting`/`.m_dragSelectRegion`. The underlying per-seat state is
already correct (`InGameUI::beginAreaSelectHint`/`endAreaSelectHint`, `InGameUI.cpp:2531-2532,2540`,
write `m_seatContexts[m_activeSeat]` correctly). **Fix shape:** loop all `MAX_SEATS` (or just the
active-seat list) in `W3DInGameUI::draw()` and call `drawSelectionRegion()` for each seat with
`m_isDragSelecting` true; coordinates are already absolute screen pixels so no viewport scoping
needed beyond reading the right seat's data.

### #8 — Gamepad can only select via drag, not a direct click
Genuinely different code paths, not a simple wrong-accessor swap. A plain click collapses to a
single `pickDrawable()` ray-cast (`W3DView.cpp:2469-2472`) that returns null — killing the entire
selection, `SelectionXlat.cpp:616-619` — if `getWindowUnderCursor` (`W3DView.cpp:2555-2564`) hits
*any* non-see-through window at that pixel, including a stray fragment of another seat's
oversized, display-authored-then-scaled control bar (the residual risk `winSeatOwnsWindow`'s own
comment names, `GameWindowManager.cpp:271-275`). Drag-select never calls `pickDrawable` at all — it
projects every drawable against the drag rect instead (`W3DView.cpp:2475-2514`), so stray window
geometry can't block it. **Fix shape:** make the click-path window gate consistent with drag —
either apply `winSeatOwnsWindow` filtering inside `pickDrawable`'s `getWindowUnderCursor` check (it
currently has none beyond "any window blocks"), or fall through to the projection-based pick when
the only window hit isn't one this seat owns. Verify with the seat overlay/`splitscreen_input.log`
whether `getWindowUnderCursor` is actually returning a non-null window at the click pixel before
assuming this diagnosis over a residual state-machine cause.

### #9 — Gamepad selecting a unit drops player 1's building placement
`ControlBar::onDrawableDeselected` (`ControlBar.cpp:2585`) calls the **legacy 2-arg**
`InGameUI::placeBuildAvailable(nullptr, nullptr)`, which hardcodes seat 0
(`InGameUI.cpp:3407-3411`) instead of the already-existing seat-aware 3-arg overload — even though
`m_seatIndex` is used one line above it in the same function (`getSelectCount(m_seatIndex)`).
Dispatch up to that point is correct (`InGameUI.cpp:3712-3713,3762-3763` route through
`ControlBarInstances::get(seat)`), so seat 1's own `onDrawableDeselected` runs — it just clears
seat 0's placement as a side effect. **Related, same root:** the arm-placement call sites
(`ControlBarCommandProcessing.cpp:266,311,353`, `GUI_COMMAND_DOZER_CONSTRUCT` and two special-power
variants) have the identical legacy 2-arg pattern from inside `ControlBar` methods that already
have `m_seatIndex` in scope — likely why a non-primary seat's own build button doesn't arm *its
own* placement either (matches the known deferred item "handleBuildPlacements runs only for seat
0's context"). **Fix shape:** change all of these call sites to pass `m_seatIndex` to the 3-arg
`placeBuildAvailable`.

### #10 — Cursors don't switch to the right shape
Both the write path (`InGameUI::setMouseCursor`, `InGameUI.cpp:574-598`) and the renderer
(`W3DSeatCursorRenderer.cpp:272-331`, resolves per-type art via `findCursorImage`) are already
correct and seat-scoped — this was fixed in an earlier round and the task's assumption that it was
still WP3-only (position/tint, no shape) is outdated. The real defect is two singleton reads
**inside** the otherwise-correctly-scoped hover/context-detection code:
`InGameUI::createCommandHint`/`createMouseoverHint` (`InGameUI.cpp:2905-2998`, `:2622-2653`) gate
on `TheMouse->getMouseStatus()` — **seat 0's real hardware pointer** — to decide "is the pointer
over a UI window" (`InGameUI.cpp:2949`, `:2628-2631`), even when evaluating a different seat's
hover. Whenever seat 0's mouse rests on its own HUD (common), every other seat's cursor gets
force-reset to `ARROW` regardless of what it's actually hovering. `createCommandHint` also reads
`rts::getObservedOrLocalPlayer()` at `InGameUI.cpp:2916` (the render-only helper) for the
shroud-based attack/move cursor swap instead of `getCommandActingPlayer()`. **Fix shape:** replace
`TheMouse->getMouseStatus()` in both functions with the acting seat's own cursor position (seat 0
keeps `TheMouse`; seat>0 reads `TheSeatManager->getSeat(m_activeSeat)->m_cursor.pos`), and replace
the `getObservedOrLocalPlayer()` read with `getCommandActingPlayer()`.

### #11 — Build-explanation tooltip mispositioned and oversized
`commandButtonTooltip()` (`ControlBar.cpp:128-133`) unconditionally calls the global
`TheControlBar->showBuildTooltipLayout()` instead of resolving the firing bar via
`ControlBarInstances::fromWindow(window)` (the exact mechanism WP8 built for this) — so hovering
any seat's button shows **seat 0's** tooltip, positioned off seat 0's marker. Compounded by
`populateBuildTooltipLayout` (`ControlBarPopupDescription.cpp:653-666`) mixing a correctly-scoped
`basePos` (via `findBarWindow`) against an **unscoped** `winGetWindowFromId(nullptr, ...)` lookup
for `"ControlBar.wnd:BackgroundMarker"`, plus a function-static `lastOffset` carrying state between
different bars' popups. Size: `m_buildToolTipLayout` is created per-instance
(`ControlBar.cpp:2176`) but is a **separate top-level `.wnd`**, never registered into the per-bar
`dockToRect`/`AuthoredWindowGeom` font-scaling pass (round 4(B)'s mechanism) — so it never shrinks
with its viewport. Also, `theAnimateWindowManager` (`ControlBarPopupDescription.cpp:97-101`) is
still the file-static flagged in handoff2 §5.3, and its slide-in never calls `setAnimationBounds`.
**Fix shape:** route `commandButtonTooltip` through `ControlBarInstances::fromWindow`; scope the
`BackgroundMarker` lookup through the owning instance; register the tooltip's windows into the
bar's `dockToRect` pass (or apply its scale/font transform directly); give each bar its own
`AnimateWindowManager` bounds. All four mechanisms already exist and are used elsewhere for
exactly this purpose.

### #12 — Civilian buildings hidden from gamepad1 until player 1 also discovers them — **SKIPPED PER USER, still active**
Read the full per-player shroud chain (`PartitionData::getShroudedStatus`,
`seatOwnerFilterHidesObject`, the per-seat evaluation loop in `GameClient::update`, ghost
snapshots) — everything is indexed purely by the viewing player; no AND-with-player-1 term found.
User confirmed 2026-08-01 this bug is still live but to skip it for now rather than keep
source-reading. **Next step when picked back up: verify live with the existing debug overlay
(per-seat shroud readout, `splitscreen-progress.md:139`) pointed at a specific civilian building
gamepad1 can't see** — do not resume with more static reading, per
`[[feedback-tracy-verify-before-fix]]`-style guidance (get a real repro before theorizing).

### #13 — Resolution change on the main menu shows the radar over the shell map
`OptionsMenu.cpp:881` / `MainMenu.cpp:727` call `TheInGameUI->recreateControlBar()`
unconditionally on a resolution change, with no shell-vs-match guard — even though the exact
guard idiom (`(TheGameLogic->isInGame() == FALSE) || (TheGameLogic->isInShellGame() == TRUE)`)
already exists in the same file for other resolution UI (`OptionsMenu.cpp:245,253,305`).
`recreateControlBar()` (`InGameUI.cpp:6765-6784`) rebuilds a bar with **3 window roots** (command
bar, RightHUD, radar — per the 2026-07-26 WP8 note), but `HideControlBar()`
(`ControlBarCallback.cpp:578-611`) only resolves/hides `ControlBarParent` — structurally can't
reach the other two roots. The radar draw itself (`W3DLeftHUDDraw`, `W3DControlBar.cpp:62-111`;
`W3DRadar::draw`, `W3DRadar.cpp:1645-1651`) has no in-game/in-shell guard at all, only
`rts::localPlayerHasRadar()`. **Fix shape:** guard the `recreateControlBar()` calls in both menu
files with the same `isInShellGame()` idiom already used nearby, OR add an in-game/in-shell check
directly to the radar draw path (belt-and-suspenders, matching how other HUD elements already
guard on `isInGame()`/`isInShellGame()` per `postDraw()`'s existing `drawGameTime`/`drawPlayerInfoList`
checks at `InGameUI.cpp:4045,4050`).

## 3. Suggested implementation order

Matches dependency and blast-radius, not report order:
1. ~~#2/#3 broadcast (done)~~
2. #2/#3 splash (same mechanism, same file, natural follow-on)
3. #6 (depends on #2/#3's `messageForSeat` plumbing)
4. #9 (small, single mechanism, high user-visible impact — "drops my placement" is jarring)
5. #7 (small, self-contained)
6. #5 (small, self-contained)
7. #4 (self-contained, needs a design call: per-seat scheme manager vs. gating the propagation)
8. #10 (self-contained)
9. #11 (touches the most call sites of this batch)
10. #1 (touches an entirely untouched subsystem, lowest risk of regressing other splitscreen work)
11. #13 (menu/shell lifecycle, unrelated to the rest — can be done any time)
12. #8 (needs live verification of the window-gate theory before committing to a fix shape)
13. #12 (deferred — needs a probe run first)

## 4. Process notes for this round

- Research was done via 5 parallel `general-purpose` agents, each briefed with the specific
  established bug classes from `splitscreen-progress.md`/`handoff2.md` so they wouldn't
  re-discover the same patterns from scratch. All 5 returned exact file:line citations; cross-
  checked against the live code while implementing #2/#3 broadcast (all citations were accurate
  after the fact, though line numbers will drift as more of this list lands — re-grep rather than
  trusting a cached line number more than one fix into this list).
- The #2/#3 broadcast fix moved `m_uiMessages`/`m_messagePosition` into `SeatUIContext` — the same
  "Pattern B" container already used for selection/hints/placement. Any future "single shared
  UI thing that should be per-seat" bug in this codebase should default to this same shape rather
  than inventing a new one.
- Build: Release, win32 x86, verified clean this session (`z_generals` target, 183 objects, only
  pre-existing warnings in touched files).
