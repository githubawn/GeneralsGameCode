# Splitscreen — Conventions, Environment & Test Protocol

Third companion doc. `splitscreen-plan.md` = design, `splitscreen-plan2.md` =
work packages, this doc = how to write code that fits this engine, how to build
and run, and how testing works when the implementer cannot play the game.
Read this **before** starting any work package.

---

## 1. Engine coding conventions (non-negotiable)

This is a 2003-era engine with its own runtime idioms. Matching them is not
style preference — some are load-bearing (memory pools, serialization).

### Types
- `Bool`, `Int`, `UnsignedInt`, `UnsignedByte`, `Real` (from `Lib/BaseType.h`) —
  never `bool/int/float` in engine-facing signatures (locals may use `bool` where
  neighboring code does; copy the file you are editing).
- Strings: `AsciiString` / `UnicodeString`. **Never `std::string`** in engine
  code. UI-visible text is `UnicodeString` and should come from
  `TheGameText->fetch("KEY")` with a fallback literal.
- Coordinates: `ICoord2D`, `Coord3D`, `IRegion2D`.

### Memory
- Classes deriving from `MemoryPoolObject` (most game objects, `GameMessage`)
  are created with `newInstance(ClassName)(args)` and destroyed with
  `deleteInstance(ptr)` — **never `new`/`delete`** for these. New engine classes
  that will be allocated frequently should use
  `MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ClassName, "ClassName")` (copy the
  pattern from `MessageStream.h:100`).
- Plain subsystems/singletons use `NEW ClassName` (the engine's `NEW` macro, see
  `TheControlBar = NEW ControlBar` in `InGameUI.cpp:1394`) and are deleted in the
  engine shutdown path.
- Translator allocation uses `MSGNEW("GameClientSubsystem") ClassName`
  (`GameClient.cpp:291`).
- STL containers are acceptable where already used nearby (this branch uses
  `std::map`, `std::array` in SDL3 code); prefer fixed arrays for
  per-seat data (`m_seatContexts[MAX_SEATS]`).

### Subsystems
- New managers implement `SubsystemInterface` (`init()`, `reset()`, `update()`).
  `reset()` must return the subsystem to a "no match loaded" state — it is called
  between games. Seat bindings survive `reset()` (pads stay bound between
  matches); per-match state (`m_playerIndex`, `m_view`) is cleared.
- Registration order matters. Register `TheSeatManager` after input
  (`TheKeyboard`/`TheMouse`) and before `TheInGameUI`. Find the
  `initSubsystem(...)` block in `GeneralsMD/.../Common/GameEngine.cpp` and read
  the existing order before inserting.

### Save/load & determinism
- Anything reachable from GameLogic that persists across frames is either part
  of the deterministic sim (then it must be in a `Snapshot`/xfer chain and
  identical across machines) or strictly client-side. **All splitscreen state is
  client-side.** Never make sim code read `TheSeatManager`,
  seat/viewport/cursor state, or `m_seatIndex`. The only sanctioned sim-facing
  artifacts are the two new `GameMessage` types in WP9 and per-message
  `m_playerIndex` stamping.
- `Snapshot` classes serialize via `xfer(...)` with version numbers — if a WP7
  change would alter `GhostObjectManager` xfer, prefer the rebuild-on-load
  option in plan2 WP7 table row 4 (keeps format identical).

### Misc
- Logging: `DEBUG_LOG(("format %d", x))` (double parens), asserts:
  `DEBUG_ASSERTCRASH(cond, ("msg"))`. No `printf`/`std::cout`.
- Comments: match the file. New files use `//` line comments (see recent
  `SDL3Input.h`), GPL header copied from a sibling file.
- Enum style for new enums: `enum Name CPP_11(: Int) { ... };`.
- Don't reformat code you aren't changing; keep diffs minimal.

---

## 2. Build & run

- Toolchain: MSVC 2022 (this branch does NOT need VC6 — VC6 presets exist for
  other work; ignore them).
- Preset: `win32-vcpkg` (Release-ish) or `win32-vcpkg-debug`. **Confirm with the
  user on first session**, then record the answer in
  `splitscreen-progress.md` §1 and stop asking.

```powershell
cmake --preset win32-vcpkg-debug
cmake --build build\win32-vcpkg-debug --target GeneralsXZH   # ZH target; confirm exact target name via: cmake --build ... --target help
```

- The built exe must run against a Zero Hour install directory (game assets are
  not in the repo). Ask the user once for the install/run directory and whether
  they use a `-win` windowed flag setup; record in progress doc. Typical dev
  flags: windowed mode and skipping intro movies — check
  `GlobalData.cpp`/command-line parsing (`parseCommandLine`) for supported flags
  (`-win`, `-noshellmap`, `-quickstart` and similar) rather than guessing.
- INI overrides: gameplay/UI data (`Mouse.ini`, control bar schemes) loads from
  the game's `Data\INI\` tree. Repo-side INI staging: check how the repo ships
  INI changes (search for a `Data/INI` folder or an install/copy step) — if
  none exists, INI changes must be documented in the progress file for the user
  to place manually.
- `.wnd` layouts (`ControlBar.wnd`) and window art also live in the game data
  dir (`Window\`, `Art\Textures\`). WP8's mirrored-art variants: prefer UV-flip
  in code over new asset files (no new binary assets without asking the user).

---

## 3. Debug harness (build this in WP0/WP1 — testing depends on it)

The user has real controllers available; **3 physical pads are the test
ceiling** — seat counts 4–8 exercise no new code paths, only layout math, so
they are verified with fake seats (below) plus one real pad. The harness still
matters: the implementing agent tests without hardware, and 4–8-seat layouts
need it. Flag-gated (`-splitscreendev` command-line flag or GlobalData bool):

1. **Fake seats**: debug hotkeys (register through `MetaEvent`/`MetaMap` the way
   existing `DEMO_*` debug keys are registered — see `MetaEvent.cpp:293` for the
   table) —
   - `Ctrl+Alt+F1..F8`: toggle-bind fake seat N (no device).
   - Fake seats produce a scripted `SeatInputState` (idle) unless driven below.
2. **Keyboard-driven virtual pad**: one fake seat at a time is "keyboard
   possessed" (`Ctrl+Alt+P` cycles): numpad 8/4/6/2 = left stick, numpad
   Enter = CONFIRM, numpad 0 = CANCEL, +/- = right stick zoom. This routes
   through `SeatManager::setSeatInput` exactly like a pad — translators cannot
   tell the difference.
3. **Seat debug overlay** (`Ctrl+Alt+S`): seat table (state, device, player,
   viewport rect, cursor pos), rendered via the same debug-display facility as
   existing overlays in `Core/.../System/Debug/`.
4. Every WP acceptance test below assumes: 2+ fake seats bound via hotkeys, one
   keyboard-possessed. Real-pad verification happens at human checkpoints only.

---

## 4. Test protocol — machine vs human

The implementer runs what it can; the human verifies feel/visuals. Rules:

- **Machine-verifiable (do every WP)**: builds clean; game boots to shell
  (launch, wait, check exit code / log output); replay-CRC workflow
  (`.github/workflows/check-replays.yml` logic can be run locally — see the
  workflow file for the command it runs; replicate that locally); grep-based
  invariants (no `getSeatIndex` under `GameLogic/`, no `std::string` in new
  files, every translator `appendMessage` followed by a stamp — write a small
  script for these in `scripts/` and run it per commit).
- **Human checkpoints (STOP and request testing — do not proceed past these
  without a result)**:
  - HC1 after WP2: "pad or keyboard-possessed seat 0 can play a normal skirmish"
  - HC2 after WP3: "multiple tinted cursors visible and smooth"
  - HC3 after WP5: "2 seats command 2 armies independently; replay OK"
  - HC4 after WP7 Spike A: "2 viewports show correct per-player fog & ghosts"
  - HC5 after WP8 Spike C: "mirrored top bar renders correctly and is clickable"
  - HC6 after WP9: full join/leave loop + separate LAN regression test
- Report results in the progress doc; a failed checkpoint reverts to the last
  green commit rather than stacking fixes on unknown state.

---

## 5. Escalation rules (when to stop and ask the user)

Stop and ask instead of guessing when:
1. A plan2 "verify first" check comes back *neither* of the anticipated options.
2. Any change would touch: network wire format, `Recorder` file format,
   `Snapshot`/xfer versions, or anything under `GameLogic/` beyond the two WP9
   messages and the dispatcher cases.
3. A WP step requires a new binary asset (texture, .wnd file).
4. Two consecutive attempts to fix the same build/test failure have failed.
5. The fix for a problem would exceed roughly double the scope the WP step
   describes.

Otherwise: make the smaller, more reversible choice, and log it in the decision
log (`splitscreen-progress.md` §3) with one line of reasoning.
