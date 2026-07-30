# Splitscreen bug sweep — handoff (2026-07-30)

> **CLOSED 2026-07-30.** All twelve are now addressed and committed: the first four in
> `56872d8fd`, the remaining eight in the commit that follows it. What each fix turned out to be
> is written up in `splitscreen-progress.md` §3 under 2026-07-30; none of the twelve is verified at
> runtime. The research below is kept as-is because it is what the fixes were built from, and
> because the two retests it asks for (bugs #5 and #6) were never actually done.

Continuation of a 12-bug sweep reported from live play of an 8-seat splitscreen match
(`-splitscreendev 7 -win`). Four bugs are fixed; **eight remain**. Nothing is committed.

## Environment / build

Branch `splitscreen-documents` (ZH only — `Generals/` is deliberately untouched and `g_generals`
does **not** build on this branch; user: "i dont care about generals for now").

Build recipe (win32 Debug, x86 — the `-arch=x86` matters, x64 breaks the link):

```powershell
# $vs = the VS BuildTools install root, e.g. from `vswhere -latest -property installationPath`
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -DevCmdArguments "-arch=x86 -host_arch=x86" -SkipAutomaticLocation
Set-Location <repo>\build\win32
& "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -f build-Debug.ninja z_generals
```

Exe: `build/win32/GeneralsMD/Debug/generalszh.exe` (~18 MB, relative to the repo root).

## Uncommitted changes currently in the tree

- `GeneralsMD/.../GUI/ControlBar/ControlBar.cpp` — compile fix + crash fix
- `GeneralsMD/.../Include/GameClient/ControlBar.h` — `forgetBarWindows`/`forgetBarLayout` decls
- `Core/GameEngine/Source/GameNetwork/GameInfo.cpp` — slot naming + random reveal
- `GeneralsMD/.../Menus/SkirmishGameOptionsMenu.cpp` — seat slot label

Note: branch tip `265bea700` as pushed did **not** compile — `noteControlBar` gained a 10th param
(`scienceState`) with the sole call site left at 9 args. Fixed in `ControlBar.cpp:1753`.

## FIXED (verified compiling, not verified in game)

1. **Crash on match restart** — use-after-free. `ControlBar::m_barAuthoredGeom` caches raw
   `GameWindow*` and `dockToRect` writes through every entry every frame.
   `initSpecialPowershortcutBar` destroys+recreates `m_specialPowerLayout` (already registered in
   that cache) without dropping dead entries; it runs once per match start, so match 2 docked freed
   memory. The `AudioManager::~AudioManager` frames in the user's stack were whatever got allocated
   over the dead window — misattributed symbols, not an audio bug.
   Fix: `forgetBarWindows`/`forgetBarLayout` (walk subtree BEFORE destruction), called from the
   superweapon rebuild; `setBarLayoutWindows` clears the set it replaces; destructor drops pointers.
8/9. **"easy army" in lobby + load screen** — `GameSlot::setState` (`GameInfo.cpp:198`) only kept a
   supplied name for `SLOT_PLAYER`, discarding the seat's title. Now honors an explicit non-empty
   name. Label = keyboard player's name + " (n)", e.g. `Bob (2)`. Only the splitscreen call site
   passes a name with an AI state, so LAN/GameSpy unaffected.
10. **Random CPU shown as hidden on load screen** — concealment only protects against remote
   counter-picking; local skirmish has none. `getApparentPlayerTemplateDisplayName` now reveals the
   rolled army for slots belonging to `TheSkirmishGameInfo`, only once `m_playerTemplate >= 0`
   (so the lobby still shows "Random" before the roll).

## REMAINING (8) — with research already done

### #2 + #4 — same root cause: bar windows are never clipped to their seat's viewport
- #2: text like "building 1%" overflows past the side of the bar instead of being cut off.
- #4: pressing hide-control-bar for player 1 lowers the bar and it appears on **player 5's** screen
  — seat 4 sits directly under seat 0 in the 4x2 grid, so the bar simply spills past its viewport's
  bottom edge. `setLowControlBarConfig` (`ControlBar.cpp:~3970`) places it at 90% of display height
  via `placeBarWindow`, which is correct; nothing clips the result.
- Facility already exists: `TheDisplay->setClipRegion(IRegion2D*)` (`Display.h:121`) and
  `DisplayString::setClipRegion` (`DisplayString.h:97`); used already in
  `W3DDevice/.../GUICallbacks/W3DControlBar.cpp` (lines 246, 421, 597, 1037).
- Fix direction: push a per-seat clip rect (the bar's `m_barDockRect`) around the bar's draw.

### #12 — match-start slide-in crosses other viewports
User correction: the animation IS assigned to viewport 1 correctly; the problem is its **off-screen
origin** is computed against the full display rather than the seat's viewport rect, so the travel
path sweeps across seats 2-8. See `animateSpecialPowerShortcut` / `AnimateWindowManager`
`registerGameWindow(..., WIN_ANIMATION_SLIDE_*, ...)` (`ControlBar.cpp:4722`) and the `GenExpFade`
transition in `showPurchaseScience`.

### #5 — generals power screen opens empty, on player 8's viewport
### #6 — power/special-ability actions missing for ALL players
Both involve the science + superweapon layouts, the same two implicated in the crash (#1).
**Retest on the current build first** — a stale-pointer cache is a plausible cause of both, and
fixing a symptom of #1 would be wasted work.
Key finding if they persist: `initSpecialPowershortcutBar` has exactly **one** caller —
`GameLogic.cpp:2318`, `TheControlBar->initSpecialPowershortcutBar(localPlayer)` — i.e. only the
classic seat-0 bar, with the *local* player. Per-seat bars (created later in
`ControlBarInstances::syncToSeats` -> `initAsSeatInstance`, `ControlBar.cpp:1370`) never get their
superweapon bar initialised at all. `initAsSeatInstance` calls `initInstanceWindows()` but neither
`setControlBarSchemeByPlayer` nor `initSpecialPowershortcutBar`. Ordering matters: GameLogic runs
at match start, syncToSeats runs later from `InGameUI::updateSeatViewports`.
`initSpecialPowershortcutBar` already has a splitscreen-aware ownership test (`playerIsOurs`,
`ControlBar.cpp:~4090`).

### #7 — seat cursors correct art/colour but smaller than the stock cursor
`drawSeatCursor` (`Core/GameEngineDevice/.../W3DSeatCursorRenderer.cpp:248`) draws at the texture's
native size (`image->getImageWidth()/Height()`, SCCPointer.tga = 32x32). Player 1 gets the D3D
**hardware** cursor via `SetCursorProperties` (`W3DMouse.cpp:511`), which is not drawn through
`TheDisplay->drawImage` at all. The stock `RM_POLYGON` software path (`W3DMouse.cpp:521`) uses the
same native-size convention, so the discrepancy is specific to the hardware-cursor path. Needs an
in-game measurement of the real ratio — do not guess a multiplier.

### #11 — only player 1 gets a number badge on the load screen
`grep -rn "badge\|Badge"` over `Core/` and `GeneralsMD/` returns **nothing**, so whatever draws
player 1's badge is not called that. Identify it first. Load screen player rows are built in
`MultiPlayerLoadScreen::init` (`Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp:1258`), which
fills `StaticTextPlayer%d` / `StaticTextSide%d` / `ProgressLoad%d` / `ButtonMapStartPosition%d`;
start-position markers go through `positionStartSpots` / `updateMapStartSpots` (line ~1419) — the
number badge is most likely the map-preview start-position marker.

### #3 — player 1 builds something, moves camera, and it shows in other viewports
Not researched at all. Per-seat render ownership filter is not covering this path. Related existing
machinery: `Shadow::setOwnerPlayerIndex` / `getOwnerPlayerIndex` (ZH `Shadow.h`), consumed in
`W3DProjectedShadow.cpp:1385`; `View::setRenderPlayerIndex` (`InGameUI.cpp:5804`).

## Project conventions (must follow)

- `PatchNotes/splitscreen-conventions.md` + `splitscreen-progress.md` (living state — read/update
  each session). `splitscreen-plan.md` / `plan2.md` = design + WP0-WP9 handbook.
- No new GUIs; per-viewport classic ControlBar instances; LAN must keep working.
- Match 2003-era engine idioms; minimal diff; no new binary assets without asking.
- Do not add `Co-Authored-By` trailers to commits in this repo.
- No ad-hoc null guards to paper over crashes — find the real lifetime bug.
