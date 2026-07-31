# Splitscreen bug sweep — handoff #2 (2026-07-30)

Successor to `splitscreen-bugfix-handoff.md`, which is CLOSED — its twelve bugs are all fixed and
committed. This one carries the **nine reports from the second live-play round**, which are also all
fixed and committed, and **none of the twenty-one is verified at runtime**. So the next session's
first job is not to write code.

Full write-ups of every fix, with the reasoning, are in `splitscreen-progress.md` §3 under the two
2026-07-30 entries. This file is the short version plus what is still open.

---

## 1. Where things stand

Branch `splitscreen-documents` (ZH only; `Generals/` is deliberately untouched and `g_generals` does
not build — user: "i dont care about generals for now").

| Commit | What |
|---|---|
| `56872d8fd` | Build fix (branch did not compile), the match-restart crash, "easy army" naming, Random CPU on the load screen |
| `63f19476d` | The other eight of round 1: bar clipping, world-overlay clipping + projection, generals screen, superweapon strip, slide-in origin, load-screen badges, cursor size |
| `09fd4aa2c` | All nine of round 2 (below) |
| `83ca81b0a` | §5.1: per-seat window hit-testing — a seat can press its own control bar |

Nothing is pushed. The observer-seat harness change (fake seats watch live AI armies instead of
taking them over — see §3) is **uncommitted in the working tree**.

## 2. Build

win32 Debug, x86. The `-arch=x86` matters — an x64 dev shell breaks the link. The Bash tool has no
MSVC on PATH, so builds must run from a VS dev environment:

```powershell
# $vs = the VS BuildTools install root, e.g. `vswhere -latest -property installationPath`
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -DevCmdArguments "-arch=x86 -host_arch=x86" -SkipAutomaticLocation
Set-Location <repo>\build\win32
& "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -f build-Debug.ninja z_generals
```

Exe: `build/win32/GeneralsMD/Debug/generalszh.exe` (~18 MB), relative to the repo root.

**Do not go looking for the game install directory** — the user declined that search and probing the
filesystem or registry trips a permission prompt. Ask for it if it is needed; `splitscreen-progress.md`
§1 has the line waiting.

## 3. How to run the next test — this matters more than it looks

- **`-splitscreendev` with NO count** puts a real pad on **seat 1**. Use this for anything about pad
  input. A run on 2026-07-27 produced a whole round of conclusions that could not have been valid,
  because it used a count and so "player 2" was a harness seat with no controller behind it.
- **`-splitscreendev <n>`** creates n FAKE seats. A real pad joining then evicts only the HIGHEST one,
  so it lands on seat 7 (= "player 8"), not on the seat that holds a player of interest.
- **`-splitscreendevquiet [n]`** is the same but leaves the seat debug overlay off. The overlay
  is drawn at the top-left, which in a split game is player 1's own viewport, so it cannot be read
  around while judging how the game looks. Use plain `-splitscreendev` when a probe row is wanted.
- **Fake seats now WATCH live AI armies** (2026-07-31). They no longer take the army off the AI, so
  the extra viewports show a real match being played instead of a base standing still - which is
  what makes the visual checks in §4 answerable at all. Each observer camera **locks onto one unit
  and follows it**, swapping to a newly chosen unit on a randomised 10-15s timer that alternates
  between the army's own base (production, construction) and its unit furthest into an enemy base
  (the fighting). It eases, so a swap pans rather than cuts. An empty lobby slot is filled with an
  Easy AI so the seat has something to watch; occupied slots are never touched.
- **`-splitscreendevtakeover [n]`** (new) restores the old behavior - fake seats convert their army
  AI→human and it stands still. Use it only when the thing under test *is* the hand-off, which is
  what a real pad joining does. It is the mode hot-swapping will be built on.

## 4. What needs verifying (nothing here is confirmed)

Grouped so one run can cover several.

**Bar geometry / clipping** — hide the bar as player 1: only player 1's bar should move, nothing
should appear on player 5's screen, and player 1's viewport must NOT grow to fill the window.
The bar's show/hide animation should play on whichever player triggered it. Text in a docked bar
should be cut off at the viewport edge rather than running past it.

**World overlays** — build something near the edge of a viewport: the "Building %d%%" caption and the
health bar should stay inside that viewport. Each seat's own units should show their health bars in
their OWN viewport (this pass previously projected everything through seat 0's camera, so watch for
the opposite failure: seat 1's bars going missing rather than landing in the wrong place).

**Generals screen** — pressing the generals button on any seat should open THAT seat's screen, in
that seat's viewport, populated. The superweapon strip should exist for every seat, and its slide-in
should stay inside its own viewport.

**Pad input (the big one)** — with a pad on seat 1: left click selects, right click orders a move,
d-pad makes and recalls control groups 1-4, X is attack-move, LB queues. Do this **while the mouse
player is also clicking**, because the bug that was fixed only appears when two seats interleave.

**Decals** — a generals action's targeting ring should appear only in the arming player's viewport,
follow THAT player's cursor, and not delete anybody else's ring.

**Lobby / load screen** — every seat's slot shows "<name> (n)" and keeps it (no reverting to "Easy
AI"), every occupied slot gets a number badge on the map preview, and armies read correctly rather
than as "Random".

## 5. Still open — with what is already known

### 5.1 A seat's clicks cannot reach its own control bar — **CODE LANDED 2026-07-31, UNVERIFIED**
Was: `WindowTranslator::translateGameMessage` returned early for every `msg->getSeatIndex() != 0`, so
no seat but 0 interacted with the window system at all. Fixed in commit `83ca81b0a` — per-seat
hover/grab/captor state (`winBeginSeatInput`/`winEndSeatInput`), an ownership-scoped hit test
(`winSeatOwnsWindow`), tooltips reserved to the seat holding the OS mouse, and the mouse-lock check
moved to the acting seat's view. Full reasoning in `splitscreen-progress.md` §3 under 2026-07-31.

**What to test:** with a pad on seat 1 (`-splitscreendev`, no count), press buttons on the pad seat's
OWN bar — build a unit, open its build queue, use an ability. Then do it **while the mouse player is
also clicking its own bar**, which is the case the per-seat state exists for. Watch for: a press
landing on player 1's bar instead; player 1's tooltip flickering while the pad hovers; a button
staying stuck "pressed" after the other seat clicks. Keyboard input is still seat-0-only by design,
so a pad seat cannot type — that is not a bug.

### 5.2 Two recurring bug classes — audit these before hunting individual symptoms
Almost every bug in both rounds was one of three shapes. Recognising the shape is faster than
re-deriving each instance.

1. **A global name lookup with N identical layouts.** `winGetWindowFromId(nullptr, id)` walks every
   window in the manager, so with a bar per viewport it returns an arbitrary player's widget — in
   practice the most recently created one. This produced the "easy army" combo, the generals screen
   opening on player 8, the hide key toggling player 5's bar, and the money readout showing another
   player's cash. Remaining count: **22 in `GUI/ControlBar/`**, 1 in `ControlBarCallback.cpp`, 1 in
   `ControlBarPopupDescription.cpp`, 5 in `InGameUI.cpp`, and **60 `"ControlBar.wnd:..."` literal
   lookups outside `ControlBar.cpp`**. Shell-menu lookups are fine and are the bulk of the 291
   repo-wide; only in-game bar code matters. The fix is always the same: resolve through the owning
   bar (`findBarWindow`/`findBarWindowById`), or scope the lookup explicitly (`winFindChildById`).
2. **A singleton read during per-view or per-seat work.** `TheTacticalView` (205 references reachable
   from client/device code), `TheMouse`, `TheKeyboard`. These silently answer for seat 0. It caused
   the terrain draw window, the stencil-shadow quads, the radar view box, the radius cursor's aim,
   the overlay projection, and the shift check in selection. Two sanctioned answers already exist:
   swap the global at a choke point for the duration (`MessageStream::propagateMessages`,
   `W3DView::draw`), or ask the acting seat (`getCommandActingSeat`, `getCommandActingPlayer`,
   `getCommandActingView`, `getCommandActingShift`).
3. **A single-instance state machine in a translator.** This is the one that took longest to see.
   `MetaEventTranslator` and `CommandTranslator` kept click-versus-drag state per BUTTON, not per
   seat, so two seats clicking at once measured each other's coordinates and each other's timings.
   Both are now per seat. **Anything else in a translator that remembers something between messages
   is suspect** — `SelectionTranslator` still has `m_leftMouseButtonIsDown`, `m_selectFeedbackAnchor`,
   `m_dragSelecting`, `m_deselectFeedbackAnchor`, `m_lastClick` as single instances; `m_dragSeat`
   partially guards them but they have not been audited properly.

### 5.3 Smaller known holes
- **The build tooltip is one popup for every seat.** `theAnimateWindowManager` in
  `ControlBarPopupDescription.cpp` is a FILE STATIC, and so is the tooltip layout it animates. Its
  slide-in is also not bounded to a viewport (the bounds mechanism added for the superweapon strip is
  per-`AnimateWindowManager`, so it would just need wiring here).
- **`handleBuildPlacements` runs only for seat 0's context**, so only seat 0's placement preview
  follows a cursor.
- **One shared `ControlBarScheme`** — all bars get one skin, so eight seats playing different
  factions show one faction's artwork.
- **`isClick` timing for seat 0 is still the OS mouse's**, which is correct, but note the seat path
  now uses its own `timeGetTime()` clock; the two are different clocks and are never compared.

### 5.4 Debt outside splitscreen
The Generals (non-ZH) build has drifted further. Core now depends on GeneralsMD-only additions:
`GameMessage::friend_setSeatIndex`, `Shadow::setOwnerPlayerIndex`, and
`GameWindowManager::winFindChildById` (used by Core's `GameWindowTransitions.cpp`). The port pass has
to add all three to `Generals/`.

### 5.5 Never done
The round-1 handoff asked for bugs #5 and #6 (generals screen empty, power/ability actions missing)
to be **retested before being worked on**, in case they were symptoms of the match-restart crash.
That retest did not happen. Both fixes are correct independently, but if either symptom was a crash
artifact then those two changes are additive to a problem that had already gone.

## 6. Process rules that were learned the hard way

- **Scripted bulk edits keep biting.** In this round a regex rename in `CommandXlat.cpp` mangled the
  constructor and half the call sites, and the wrong-but-valid form still compiled in two places. A
  clean build proves nothing about a scripted edit. Read the diff. `splitscreen-progress.md`
  2026-07-26 lists the three specific failure modes (anchors inside comments, braceless `if` bodies,
  and editing a function the edited function is reachable from).
- **Two symptoms with one cause is the norm here, not the exception.** Both rounds had four such
  pairs. Before fixing the second of two similar-smelling reports, check whether the first explains it.
- **Instrument rather than guess.** Three consecutive sessions guessed at the fog/render leaks and
  were wrong every time; the render-leak probe (`Common/RenderLeakProbe.*`, point the mouse at the
  offending thing and read the row) settled it in one screenshot. The seat overlay does the same for
  seat/slot/player/viewport linkage.

## 7. Project conventions (must follow)

- `splitscreen-conventions.md` = engine idioms, build, test protocol. `splitscreen-plan.md` /
  `plan2.md` = design + WP0-WP9. `splitscreen-progress.md` = **living state, read and update every
  session.**
- No new GUIs; per-viewport classic ControlBar instances; LAN must keep working.
- Match 2003-era engine idioms; minimal diff; no new binary assets without asking.
- No `Co-Authored-By` trailers on commits in this repo.
- No ad-hoc null guards to paper over a crash — find the real lifetime bug.
- No local filesystem paths in committed docs.
