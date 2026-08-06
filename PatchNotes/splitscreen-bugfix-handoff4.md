# Splitscreen bug sweep — handoff #4 (2026-08-06)

Successor to `splitscreen-bugfix-handoff3.md`. That file listed 13 findings with 1 committed
and 12 open. This round **re-verified all 12 against the tree before writing any code**, using
11 parallel read-only agents briefed to *refute* rather than confirm. That was worth doing:

> **4 confirmed as written, 6 partly wrong, 1 refuted outright.**
> Seven of eleven prescribed fix shapes would have shipped a no-op, a regression, or a fix for
> a cause that does not exist.

**Do not skip the verification step in the next round either.** Every wrong diagnosis in
handoff3 was written with confident file:line citations, and the citations were *accurate* —
it was the reasoning about them that was wrong. Accurate citations are not evidence of a
correct diagnosis.

## 1. Where things stand

Branch `splitscreen-documents`. Six fixes landed this round, each compiled and relink-verified
on a Windows host (Release win32 x86, VS 2022 BuildTools, MSVC 14.44):

| finding | commit | what actually landed |
|---|---|---|
| #7 lasso | `66e2f73b7` | + the latent stuck-lasso bug drawing it exposed |
| #5 observer HQ | `7c67ce432` | via a new `getCommandingSeatIndexForPlayer()` |
| #9 placement | `d14e21c39` | clear-only sites; arm/consume pair deliberately deferred |
| #4 bar scheme | `ce879e9b5` | per-bar recorded scheme + defeated-seat observer skin |
| #13 shell radar | `79adf2a5c` | stale-instance resolution, not the claimed root count |
| #10 cursor shape | `45077fb27` | real cause was `isLocallyControlled`, not `TheMouse` |
| #6 under-attack | `f06510e91` | narrow: gate + 4 messages + radar glow + EVA |
| #8 probe | `3bc73deea` | instrumentation only, `GX_CLICKPROBE`; no fix attempted |
| #2/#3 splash | `3cd83c6e6` | reposition **and** the missing seat>0 trigger |

**Still open and deliberately NOT attempted: #1 and #11, plus #9's arm/consume pair.**
All three are large, none can be runtime-verified from a Mac, and each has a failure mode a
one-match smoke test would miss — #1's `forgetBarLayout` omission crashes only on the SECOND
match (and `ResetDiplomacy` runs on every teardown, single-view included); #11's routing fix
without the update-func fix leaves a seat's popup permanently on screen; #9's arm side without
the consume side leaves a pad seat unable to place anything at all. Landing any of them blind
would have compromised testing of the nine that did land. Their full plans are in §6.

Baseline exe SHA256 before any change: `1C25A9BE5518C472943E860558AE8C4FCCC943875CF6AED48362C2F37BD38362`.
After the six: `BD35E6BE7A851DDE8ECABD0B223485E0B91CAF8C8A97F79A8B6ECD82338EF1ED`.
**The exe size never changed** (9,159,168 bytes at every step) — gate on the SHA, never the size.

**Nothing in this round is runtime-verified.** Every fix is a static argument plus a clean
compile. Four findings remain open, below.

## 2. Build recipe that actually works

The handoff3 recipe is right but omits the trap that cost the most time here.

```
Enter-VsDevShell -DevCmdArguments '-arch=x86 -host_arch=x64'
cmake --preset win32          # Ninja Multi-Config, non-vcpkg
ninja -f build-Release.ninja z_generals    # from build/win32
```

* **Use the cmake/ninja that ship with VS BuildTools**, not whatever is on `PATH`.
  A winlibs cmake 4.3.2 on `PATH` fails configure: it is built against OpenSSL with no default
  CA bundle, so the SDL3 `FetchContent` download dies with cURL **status 60**. It ignores
  `CURL_CA_BUNDLE`; only `CMAKE_TLS_CAINFO` works. VS's cmake 3.31 downloads it with no config.
  This is **not** a certificate problem on the box — `git` works because it uses schannel, and
  the box sees a genuine `github.com` → Sectigo chain.
  cmake 4.x also drops `cmake_minimum_required` < 3.5, which this codebase will trip over.
* The target is `z_generals`; the **output is `generalszh.exe`**, not `z_generals.exe`.
* `GeneralsReplays` submodule does not need initialising to build.

### Driving it over SSH

* The default shell is PowerShell. `ssh $WIN 'powershell -Command -' < script.ps1` feeds the
  script **line by line**, so any multi-line `if {}` block is split into fragments that do
  nothing — **it exits 0 and prints nothing**, indistinguishable from success. A clone
  "succeeded" that way with no repo on disk. Use base64 `-EncodedCommand`.
* Do **not** set `$ErrorActionPreference='Stop'` around native git. git writes ordinary progress
  ("Already on 'splitscreen-documents'") to stderr and PowerShell promotes that to a terminating
  error. Gate on `$LASTEXITCODE`.
* Commit identity: a clone with an `https://` remote does **not** match the
  `includeIf hasconfig:remote.*.url:git@github-personal:*/*` rule and silently falls back to the
  global default account. Set `user.name`/`user.email` locally in the clone before committing.

## 3. The refutation — #8 is not what handoff3 says it is

**#8 (gamepad can only select via drag, not a direct click) — REFUTED. The prescribed fix is
already in the tree.**

handoff3 says `pickDrawable`'s `getWindowUnderCursor` check "currently has none beyond 'any
window blocks'". That is factually wrong:

* `GameWindowManager.cpp:3863`, `:3888`, `:3913` each do `if (!winSeatOwnsWindow(window)) continue;`
* the function's own doc comment (`:3818-3829`) names `View::pickDrawable` and describes
  handoff3's exact hypothesis as **the bug it was added to kill**
* `MessageStream.cpp:1265-1266` calls `winBeginSeatInput(seatIdx)` before *every* translator,
  deliberately widened from WindowXlat-only, with a comment saying so because "those call
  `View::pickDrawable`". So `m_inputSeat >= 0` during the pick and the `< 0` bypass never fires.

Consequence: for `m_inputSeat > 0`, `winSeatOwnsWindow` returns FALSE for any window owned by no
bar or by a *different* seat's bar — and FALSE means `continue`, i.e. **does not block**. "A
stray fragment of another seat's oversized control bar" is architecturally impossible, not
merely unlikely.

The named alternative (handoff2 §5.3, `SelectionTranslator`'s own fields) does not explain it
either — the whole `MSG_MOUSE_LEFT_CLICK` case reads none of `m_leftMouseButtonIsDown`,
`m_selectFeedbackAnchor`, `m_dragSelecting`, `m_deselectFeedbackAnchor`, `m_lastClick`. The
click region comes from MetaEvent's per-seat `m_mouseDownPosition[seat][index]`.

**Best surviving hypothesis:** `getWindowUnderCursor` has three early returns *before* any seat
filter — `m_mouseCaptor` (`:3833`), `m_grabWindow` (`:3839`), `m_modalHead` (`:3846`). The first
two are swapped per seat by `winBeginSeatInput`. A stale `m_seatGrabWindow[padSeat]` left over
from a bar button press would make every later pick by **that one seat** return the grab window
and refuse — permanently, and for that seat only. That matches "click dead, drag alive, one
seat" exactly. Neither handoff3 nor the overlay mentions it.

**The verification handoff3 asks for cannot be performed.** It says to check
`splitscreen_input.log`, but the translator trace (`MessageStream.cpp:1278-1286`) filters to
`msg->getType() >= MSG_BEGIN_META_MESSAGES` (=177) while `MSG_MOUSE_LEFT_CLICK` is **163** — the
log is structurally blind to clicks. The seat overlay only reports `g_dbgLastClickSeat`.
**A probe has to be added first.** Per cooked left click from the pad seat, log: the seat tag and
`getCommandActingSeat()`; `isPoint` and the pixelRegion; whether `pickDrawable` returned null;
if null, whether `getWindowUnderCursor` returned non-null and **which exit produced it**, that
window's id/region, and `m_inputSeat`; and `drawablesThatWillSelect.size()`.
Readings: owned by the acting seat's own bar → the narrowed window theory; via the `m_grabWindow`
early return → the stale grab; both null → the ray-cast itself missed; `isPoint` FALSE → the
finding is misfiled entirely.

## 4. Findings whose prescribed fix was wrong

### #2/#3 splash — the prescribed fix ships a no-op. STILL OPEN.
handoff3 says to resolve `rts::getSeatIndexForPlayer()` "for the player the script action
concerns" and make `m_messageWindow[MAX_SEATS]`. **There is no such player, and the array would
only ever have `[0]` written.**

* MP victory/defeat scripts are appended to **one** side's list — `GameLogic.cpp:1607`,
  `TheSidesList->getSideInfo(0)->getScriptList()`. Not per side.
* Their conditions resolve through `m_localSlotNum` (`ScriptConditions.cpp:1752/1760/1768` →
  `VictoryConditions.cpp:400-424`, assigned at `:382-383` from `isLocalPlayer()`).
* So `doVictory`/`doDefeat`/`doLocalDefeat` fire **at most once per match, for seat 0 only**.
  Seats 1..7 never reach `ScriptActions` at all.
* `TheScriptEngine->getCurrentPlayer()` *is* live during `executeAction`, but because of the
  above it is side 0's player — threading it in would look correct and mislabel every splash.
* `GameLogic.cpp:1626-1686` has a commented-out block that would have built these per side. It
  also targeted `getSideInfo(0)`. **Do not resurrect it.**

This needs **two** deliverables: (a) reposition seat 0's existing splash into its own viewport —
pure positioning, fixes "the popup covers everybody's screen"; (b) a **new trigger** in
`VictoryConditions::update()` for seats 1..7, which is the one place that already detects defeat
per player (`:199-240`) and victory per alliance (`:184-196`), and which `c2a26a4e8` already
wired to `rts::getSeatIndexForPlayer` at `:213`. Karl scoped this to include (b).

Mechanism notes handoff3 omits: `winCreateFromScript` returns only the **first** top-level
window, so `closeWindows` already destroys only one root and a multi-root `.wnd` leaks today —
the reposition helper must iterate `WindowLayoutInfo::windows`, not assume one root. The `.wnd`
files themselves are **not in the repo** (they live in the install's `Window\` tree), so root
count cannot be verified statically. `TheRecorder->isMultiplayer()` is TRUE for skirmish, so
`VictoryConditions::update()`'s early-out at `:180` does not block the harness — verified,
because the whole plan depends on it.

### #9 — the prescribed fix would have made it worse. PARTLY LANDED.
handoff3 says "change all of these call sites to pass `m_seatIndex`", lumping the **arm** sites
in with the **clear** site. They are not equivalent. The whole legacy placement accessor family
forwards to a literal 0 (`InGameUI.cpp:3594,3608,3625,3652,3667,3685,3705`) — note this differs
from the *selection* family, which uses `m_activeSeat`. Because arm **and** consume are both
pinned to 0, seat N's build currently completes, wrongly, through seat 0's context. Moving only
the arm side leaves `PlaceEventTranslator` reading seat 0, so `getPendingPlaceType()` returns
nullptr and **seat N can no longer place anything at all**.

Landed: the three *clear-only* sites — `ControlBar.cpp:2585` (the reported bug),
`ControlBarCommandProcessing.cpp:197` and `CommandXlat.cpp:3909`, the latter two **missed by
handoff3**. The site list is **fifteen** 2-arg occurrences in GeneralsMD, not four.

**Still open — the arm/consume pair.** Needs `ControlBarCommandProcessing.cpp:266,311,353`
*together with* routing every placement read/write in `PlaceEventTranslator` through
`msg->getSeatIndex()`, **and** the three `TheTacticalView->screenToTerrain` calls at `:83,:179,:277`
through the acting seat's view — otherwise seat N's pixels are projected through seat 0's camera
and buildings land in the wrong world position.

### #10 — the stated symptom cannot happen. LANDED, different cause.
See `45077fb27`. The `getWindowUnderCursor` seat filter means seat N's lookup is essentially
always null — the **inverse** of handoff3's claim. Real cause: `Object::isLocallyControlled()`
read 4× across the two hover functions. **Live check:** cursor stuck on ARROW with exactly one
unit selected, but changing shape with two or more, confirms it.

### #13 — the structural claim is refuted. LANDED, different cause.
handoff3 says `recreateControlBar` rebuilds three window roots that `HideControlBar` cannot
reach. Extracting the shipped `ControlBar.wnd` from `WindowZH.big` shows **one** column-0
`WINDOW` block, `ControlBarParent`; `LeftHUD` (the radar's `DRAWCALLBACK`) and `RightHUD` are its
**children**. Hiding the parent hides the radar. Real cause: `createControlBar`'s
`HideControlBar()` runs while `TheControlBar` is still the **old** bar, and
`findBarWindowById` scopes strictly to that instance — so it hides the outgoing root and leaves
the incoming one, authored ENABLED, visible. Bug class 1 from the opposite direction: not a
global lookup returning an arbitrary instance, but a **scoped lookup pinned to a dead one**.

### #11 — partly dead code. STILL OPEN.
`static Bool useAnimation = FALSE;` (`ControlBarPopupDescription.cpp:101`) is never assigned
anywhere — unbounded grep returns reads only. The sole
`theAnimateWindowManager = NEW AnimateWindowManager` sits behind `if (useAnimation && ...)`, so
the pointer is permanently null and **there is no slide-in to bound**. This is the
`RETAIL_COMPATIBLE_CRC` pattern: a fix there ships nothing while still forcing an atomic
redeploy. Drop that sub-claim.

Also: **the stated trigger is wrong.** `GameWindowManager.cpp:1302` gates the whole
tooltip-callback path on `ownsSharedMouse = (m_inputSeat <= 0)`, and a pad seat runs with
`m_inputSeat >= 1`, so a gamepad seat **never** fires `commandButtonTooltip`. The live repro is
player 1 moving the OS mouse across another seat's bar. Reproduce it that way.

And `'a correctly-scoped basePos (via findBarWindow)'` is wrong — `getBackgroundMarkerPos()`
returns an **authored** coordinate captured once at init, mixed at `:663-664` against a **docked**
`winGetScreenPosition()`. `W3DControlBar.cpp:674-678` already carries the correction. Two
defects, not one.

Three further mechanisms required for coherence that handoff3 does not enumerate:
`ControlBarPopupDescription.cpp:249` reads `ThePlayerList->getLocalPlayer()` (line 570 of the
same function already uses the per-instance accessor); six sibling-walking `winGetWindowFromId`
lookups at `:560/:565/:582/:594/:600/:614`; and `ControlBarPopupDescriptionUpdateFunc`
(`:102-125`) drives the **global** `TheControlBar` while installed on every instance's layout —
dormant only because no seat>0 layout is ever shown, and **made live by the routing fix itself**.
Fix the routing without it and seat N's popup stays on screen permanently.

### #6 — correct, but widening the gate creates two new bugs. STILL OPEN, scoped narrow.
The fix shape is right (`getSeatIndexForPlayer`, not the `isLocallyViewed()` that handoff3's
point (a) suggests — that helper is render-only-safe and answers player 1 outside a render pass,
so copying it reproduces the bug being fixed; **delete that sentence from handoff3**).

Karl's call: **land narrow, log the rest.** The two consequences, both real:

1. **Cross-seat suppression.** `Radar::tryEvent` (`:1166-1215`) dedups against all events of the
   same type with no owner concept, and with `PRESERVE_RADAR_WARNING_SUPPRESSION` (=1) the
   suppression is **map-wide for 10 seconds**. Once every seat can raise the event, seat 0 being
   attacked silently swallows seat 3's warning. In an 8-seat game under fire, most seats get no
   warning — arguably worse than today's "only player 1 gets it".
2. **Radar blips leak across viewports.** `W3DRadar::drawEvents` draws every entry in the shared
   `m_event[]` into whichever radar is painting, with no owner filter. The "jump to last radar
   event" hotkey points every seat at the most recent event on the machine.

Fixing either properly wants an owner on `RadarEvent`, which is inside Radar's xfer chain
(`Radar.cpp:1436`) — conventions say stop and ask. Non-xfer alternative: a parallel client-only
`Int m_eventOwnerSeat[MAX_RADAR_EVENTS]`, cleared in `reset()`, never serialized.

Two more things handoff3 misses: a **fourth** `TheInGameUI->message()` at `Radar.cpp:1111` (the
cited range `1043-1109` stops 13 lines short of the real end at 1122), and
`TheControlBar->triggerRadarAttackGlow()` at `:1057` flashing **seat 0's** radar frame for every
seat's attack. Also `"isLocalPlayer() a third time at :1093"` is overstated — that test is
unconditionally TRUE today because the only caller's gate guarantees it, so the branch is dead;
it is latent, made live by step 1.

**Build-target note (pre-existing, not caused by any of this):** `Radar.cpp` compiles into
**both** game targets, but `messageForSeat`/`ControlBarInstances` exist only in GeneralsMD. The
`Generals/` target is **already unbuildable on this branch** — `Core/.../SelectionInfo.cpp:105-118`
calls `getCommandActingPlayer()` and `Object::isControlledByPlayer()`, both declared only under
`GeneralsMD/`. Build the ZH target only. Do **not** "fix" it with an `#ifdef`.

## 5. New findings, none of which are in handoff3

* **Placement icons leak across seats.** `InGameUI::~InGameUI` (`:1348`) and `InGameUI::reset`
  (`:2249`) call the 2-arg `placeBuildAvailable`, clearing seat 0 only. No seat is "acting"
  during teardown; both should loop all `MAX_SEATS` — `destroyPlacementIcons(Int seat = 0)`
  already exists. Today seats 1..7 leak their placement icon drawables across a match boundary.
* **`setGUICommand` is not seat-aware at all.** `ControlBar::onDrawableDeselected` (`:2577`) and
  `onDrawableSelected` call `TheInGameUI->setGUICommand(nullptr)`; `m_pendingGUICommand`
  (`InGameUI.h:842`) is a flat non-seat member with **no** seat-aware overload, and the body also
  writes the shared `m_mouseMode`. So seat N selecting still cancels seat 0's pending GUI
  command. Needs a `SeatUIContext` field + overload — Pattern B, same as `c2a26a4e8`.
* **`ControlBar.wnd` tree leaks on every resolution change.** `InGameUI.cpp:6841` looks up
  `"ControlBar.wnd"`, but **no window is ever named that** — every name is decorated
  `ControlBar.wnd:<something>`. The lookup returns nullptr and `deleteInstance` no-ops, so after
  *k* resolution changes there are *k+1* `ControlBarParent`s in `m_windowList`. Benign today only
  because global name lookups hit the newest head-inserted copy — which is exactly the ground
  that makes splitscreen name lookups ambiguous. **Do not fold the destroy into a fix:**
  `Radar::m_radarWindow` caches the old LeftHUD by global lookup at map load, and
  `ControlBarScheme`/`GameWindowTransitions` hold similar pointers, so destroying the old roots
  turns stale pointers into dangling ones. Wants its own change that re-resolves those caches.
* **`m_isScrolling`/`m_isSelecting`/`m_mouseMode`/`m_mouseModeCursor`/`m_pendingGUICommand` are
  single-instance** (`InGameUI.h:998-1001, :842`), not `SeatUIContext` fields. Seat 0
  right-drag-scrolling or lasso-dragging suppresses hint generation for **every** seat, and seat
  0's mouse mode selects the branch every seat takes.
* **`SelectionTranslator`'s raw-button cases have no seat guard at all**
  (`SelectionXlat.cpp:915-919`, `:928`), so seat 0 pressing while a pad holds its button steals
  `m_dragSeat` and `m_selectFeedbackAnchor`. Partially mitigated by the hand-back added in
  `66e2f73b7`, but the fields themselves are still shared.
* **`getSeatIndexForPlayer`'s doc comment lied** — it said "commands", the body has always meant
  "watches" (observers included). Corrected in `7c67ce432`, which also adds
  `getCommandingSeatIndexForPlayer` for the ownership sense. **Pick deliberately**: the defeat
  broadcast at `VictoryConditions.cpp:213` wants the watching form and would silently regress
  under a global change.

## 6. Suggested order for the next round

1. **#8 probe first** (it is instrumentation, not a fix, and everything else about #8 is guessing
   until it lands). Karl's call: add the probe, leave the fix.
2. **#2/#3** (a) reposition, then (b) the new per-seat trigger.
3. **#11**, remembering the routing fix requires the update-func fix in the same change.
4. **#1** — the only fully-confirmed finding still unimplemented. Note the extra defects the
   sweep found: a **third** file static at `Diplomacy.cpp:91`
   (`theAnimateWindowManager`); five global `winGetWindowFromId` lookups at `:220-227`; and
   `winSeatOwnsWindow`'s own comment (`GameWindowManager.cpp:264-266`) explicitly keeps diplomacy
   with seat 0 — so positioning the popup is **not enough**, a seat>0 could not press a button in
   it. That is why the fix must register the popup with the seat's `ControlBar`
   (`addBarLayoutWindows` + `redockAfterRootsChanged`), which is the only established mechanism
   for putting a non-`ControlBar.wnd` popup in a seat viewport — the generals screen and the
   special-power shortcut bar are the precedents. Also: `Diplomacy.cpp:63-85` is **not**
   homogeneous — `:63-70` and `:73-75` are `NameKeyType`s, identical for every instance, and must
   **not** become `[MAX_SEATS]`.
5. **#9** arm/consume pair, **#6** radar-event ownership — both need a decision first.

## 6b. Runtime verification — attempted, and what it found

**A fresh build of this branch does not run on the test box at all, and this is NOT caused by
any fix in this round.** Established by A/B, not assumption:

| binary | flags | verdict |
|---|---|---|
| fresh build, **baseline `f72603e6c`** (no fixes) | `-win` | **CRASHED** |
| fresh build, baseline | `-win -splitscreendev 1` | **CRASHED** |
| fresh build, HEAD (all six fixes) | `-win -splitscreendev 7` | **CRASHED** |
| pre-existing `GeneralsX-run\generalszh.exe` | `-win` | **RUNNING, healthy** |

The baseline crashing identically is what clears the six fixes. Crash record:

```
Release Crash at <ts>
; Reason Uncaught Exception during initialization.
```
Empty stack. **stdout and stderr are both 0 bytes, and no DXVK log is produced** — so it dies
before the graphics device or any engine logging is up. The working binary emits ~156 KB of
`[GX-ISSUE144]` font/tooltip logging and a DXVK log on the same box, same env, same data.

### Two measurement traps that produced wrong answers first — do not repeat them

1. **`Get-Process generalszh` is NOT a liveness test.** A crashed instance keeps its process
   alive while the "Technical Difficulties" modal is up. Scoring on process existence produced a
   completely bogus non-monotonic result (`seats=1` ok, `3` crash, `5` ok, `6` ok, `7` crash) and
   a wrong conclusion that the `-splitscreendev` flag was to blame. With a correct detector every
   count crashes, and so does no-flag. **Ground truth is the mtime of
   `Documents\Command and Conquer Generals Zero Hour Data\ReleaseCrashInfo.txt`** compared against
   the run start time.
2. **Launching over plain SSH always fails with `0xC0000005`** regardless of the binary — Miles
   opens the audio device even headless. Every run must go through an Interactive scheduled task
   (`GXSplit`, modelled on the existing `GXPlay`). An SSH-vs-task comparison is not an A/B.

### Also root-caused: `STATUS_DLL_NOT_FOUND` when relocating the exe
The build links against **its own** `binkw32.dll` / `mss32.dll`, staged under
`build/win32/_deps/{bink,miles}-build/Release/`. These differ by hash from the copies in the
existing run directory. Dropping the new exe beside the mismatched ones gives `0xC0000135` with
0 bytes of stderr. A run directory for a fresh build must take those two DLLs from that build.
(`C:\dev\GenSplit-run` and `C:\dev\GenSplit-base-run` are set up correctly this way.)

### What is still unknown
Why a fresh build dies that early. Not yet ruled out: a data/asset expectation this box does not
satisfy, or a build-configuration difference from however the branch author builds. Note the
working binary is from a *different fork* (GeneralsX), so it is not evidence that this branch has
ever run here. **No debugger is installed** — there is no `cdb.exe` under Windows Kits — so no
stack could be obtained. Next step is either installing the Debugging Tools for Windows and
catching the exception with a `.pdb` (one is produced next to the exe), or getting the branch
author's working run-directory layout and comparing.

## 6c. How to test what landed (for whoever has a pad)

Build: `-splitscreendev <n>`. The two sharpest falsifiable predictions first — if either fails,
that diagnosis is wrong and should be re-opened, not patched around.

* **#10 cursor.** With a pad seat, select **exactly one** of that seat's own units. Before the
  fix the cursor was pinned to ARROW; with **two or more** selected it changed shape normally.
  That asymmetry is the signature. If the seat's cursor is still stuck with 2+ selected, the
  `isLocallyControlled` diagnosis is incomplete.
* **#7 lasso.** A pad seat's drag box should now paint. Then, mid-drag on the pad, press the
  mouse on seat 0: the pad's box must **disappear**, not freeze. A frozen box means the
  hand-back in `RAW_MOUSE_LEFT_BUTTON_DOWN` did not fire.
* **#5.** An AI observer seat's control bar should start **empty** — no command centre selected.
* **#4.** Two seats on different factions should show **different** bar artwork. Then let player
  1 be defeated: only player 1's bar should go blank/observer, not all of them.
* **#9.** Arm a building placement on seat 0, then select a unit with the pad. Seat 0's
  placement must survive.
* **#2/#3.** On elimination a seat should get its own LocalDefeat splash **inside its own
  viewport**, and the other seats must keep playing — no global input freeze.
* **#6.** Attack a seat>0 unit: that seat gets the under-attack text in ITS viewport and ITS
  radar flashes. Known limitation, logged above: within 10s another seat's warning may be
  suppressed map-wide.
* **#13.** Main menu -> Options -> change resolution. No radar/bar over the shell map.
* **#8.** Run with `GX_CLICKPROBE=1` and click (not drag) with the pad, then read
  `splitscreen_input.log` for `[GXPICK]` / `[GXCLICK]` and follow the decision table in §3.

## 7. Process notes

* `getCommandActingSeat()` is at **global scope**, not in namespace `rts`. Writing
  `rts::getCommandActingSeat()` will not compile. Only `getSeatIndexForPlayer` /
  `getObservedOrLocalPlayer` are in `rts`.
* Line numbers in handoff3 had drifted by ~19 in `InGameUI.cpp` by the third fix of this round.
  Re-grep for the symbol; never trust a cached line number more than one fix in.
* `grep -c` exits 0 when it finds matches. zsh does not word-split unquoted parameters and
  aborts on unquoted globs like `--include=*.cpp` — the grep never runs and the empty output
  reads as "no matches".
