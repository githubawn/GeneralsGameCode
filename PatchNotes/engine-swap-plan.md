# Plan: One merged exe running either engine, via namespace scaffolding

## Goal

Ship a single executable that can run **either** the *Generals* engine or the
*Zero Hour* (GeneralsMD) engine, let the player **switch between them by clicking
the logo in the main menu**, and extend that to **mods**.

**Scope (decided):**
- Work happens on the **`main`** branch.
- **Engine switching only — no multiplayer/online work** in this effort.
- All of it lives behind a **new build preset**; default builds stay byte-identical.
- **Keep the diff to existing files as small as possible** — ideally near zero (see
  "The mechanism": a build-time transform on generated copies, not edits to the
  originals).

## Context & hard constraints

- **One exe, no DLLs, both full engines inside it.**
- **Toolchains: VC6 and MSVC.** No `objcopy`/binutils; no linker-level symbol
  hiding. MSVC/VC6 linkers cannot localize or hide duplicate symbols in a static
  lib (`/FORCE:MULTIPLE` just picks one arbitrarily, which would silently merge
  the engines).
- Both engine trees (`Generals/Code`, `GeneralsMD/Code`) plus their per-engine
  copies of `Core/` define the **same global symbols and singletons**
  (`TheGameEngine`, `gAppPrefix`, `TheWin32Mouse`, `g_strFile`, ...). Linking both
  as-is produces duplicate-symbol errors.
- Today the two games are two separate executables (`g_generals`, `z_generals`),
  each statically compiling its own copy of `Core/` (the `corei_*` libraries are
  INTERFACE libraries, so Core source is compiled into each game rather than shared
  as one `.lib`). "Which game" is chosen at **compile time**.

Therefore the scaffold's core job is **compile-time symbol separation via
namespaces**, plus selecting and hosting one engine at a time. Engine *logic* is
untouched — this is purely a naming/wrapping layer, so gameplay determinism
(replays) is unaffected.

## The mechanism (namespaces via a generated tree, not edits to originals)

To separate the symbols we still wrap each engine in a namespace — Generals in
`namespace Gen { ... }`, Zero Hour in `namespace ZH { ... }` — so `TheGameEngine`
becomes `Gen::TheGameEngine` / `ZH::TheGameEngine`, two distinct symbols, no DLL.
**Spike 1.1 proved this compiles and links on both VC6 and MSVC (see below).**

To satisfy "smallest possible diff to existing files," the wrapping is **not** done
by editing the ~1,100 originals. Instead the new preset runs a **build-time
transform** that emits *generated* namespaced copies and compiles those:

- For each engine source/header, a script copies it into a generated dir
  (`build/<preset>/ns_gen/{Gen,ZH}/...`) and inserts `namespace X {` after the
  leading `#include` block plus a matching `}` at EOF. Originals are untouched.
- `#line` directives in the generated copies map diagnostics/debugging back to the
  real source paths, so errors and breakpoints still point at the originals.
- The merged target's include paths point at the original include dirs, so includes
  resolve unchanged.

Wrapping discipline the transform enforces: **`#include`s stay at global scope; the
namespace opens after them.** Standard/STLport/third-party headers must never be
pulled inside a namespace, and no engine file may include another engine header
after opening its namespace (or you get `ZH::ZH::...`). The bulk is mechanical; a
tail of files that violate include order (or use `extern "C"`, callbacks, or macros)
either get handled by the transform or, worst case, need a *tiny* edit to the
original — kept to the minimum.

Net effect on existing tracked files: essentially only `CMakePresets.json` (add the
preset) and a small hook in the top-level `CMakeLists.txt`/build option to enable the
merged target. Everything else — the transform script, the scaffold `main`, the new
target — is new files.

---

## Spike 1.1 results (MSVC — PASS; VC6 — PASS, incl. real engine code + STLport)

Run 2026-07-01. **Bottom line: the go/no-go gate is fully cleared.** Both MSVC 2022
and VC6 (SP6) accept namespace-based separation of identically-named colliding
globals AND compile real engine code wrapped in a namespace, both emitting the SAME
mangled symbols (`?TheGameEngine@Gen@@...` vs `?TheGameEngine@ZH@@...`). Artifacts in
`scratch/`.

**VC6 (SP6, `C:\code\vc6\VC6SP6\VC98`):**
- *Concept proof* (`scratch/ns_spike_vc6.cpp`, C++98-safe): compiled with VC6
  `CL /GX`, linked (explicit `LINK` — VC6's implicit link-through-cl didn't fire in
  this shell, an environment quirk, not a code issue), ran both modes, and the
  colliding global appears as two distinct symbols matching MSVC's mangling.
- *Real-code probe* (`scratch/real_ns_probe.cpp` via the **existing `build/vc6`
  preset's exact flags** — `-DUSING_STLPORT=1`, `/Zm1000 /GX /O2`,
  `/FIUtility/CppMacros.h`, STLport at `build/vc6/_deps/stlport-src`): real engine
  headers (`PreRTS.h` + `AsciiString` + full `Common` web) with a `namespace ZH { }`
  block compiled clean (`CL_EXIT=0`); object exports `?TheGameEngine@ZH@@...` and
  `?RunEngine@ZH@@YAHXZ`. **VC6 handles the wrapping on real engine code with
  STLport.** The "remaining VC6 item" is now done.

Below, MSVC results (same run):

- **Concept proof (`scratch/ns_spike.cpp`):** two "engines" each defining the SAME
  global names (`class GameEngine`, `GameEngine* TheGameEngine`, `RunEngine()`),
  wrapped in `namespace Gen` / `namespace ZH`, linked into ONE exe with a selector
  `main`. Built clean, ran both modes, and `dumpbin /SYMBOLS` showed the colliding
  global as two distinct symbols: `?TheGameEngine@Gen@@...` vs `?TheGameEngine@ZH@@...`.
  Confirms the linking model: namespaces separate the duplicate symbols, no DLLs.

- **Real-code probe (`scratch/real_ns_probe.cpp`):** structured like a real engine
  `.cpp` — `Utility/CppMacros.h` then `PreRTS.h` at global scope (pulling in the
  whole `Common` header web), then a `namespace ZH { ... }` block using a real engine
  type (`AsciiString`) plus a colliding `TheGameEngine` singleton and `RunEngine()`.
  Compiled clean (`CL_EXIT=0`) with the real includes/defines; object exports
  `?TheGameEngine@ZH@@...` and `?RunEngine@ZH@@YAHXZ`. Confirms the wrapping
  discipline (includes at global scope, definitions inside the namespace) works on
  real engine code.

**Findings that feed Stage 1:**
- The wrapping discipline is mandatory and confirmed: **all `#include`s stay at
  global scope; the namespace opens after them.**
- **The PCH must be namespaced per engine too.** The build force-includes
  `cmake_pch.hxx` (`/FI`), which pulls `CppMacros.h` + `PreRTS.h` at global scope.
  Core headers reached via the PCH (e.g. `AsciiString`) are defined globally, so
  wrapping a header that the PCH already included is defeated by `#pragma once`.
  Each engine needs its own namespaced PCH.
- **VC6 PASSED on both the concept proof and real engine code + STLport** (see
  above) — the fragile-namespace risk is retired. No compiler-capability blockers
  remain for the bulk codemod.

## Stage 1 — The scaffolding (this is the real work)

Everything below is gated by a **new preset + build option** (`RTS_BUILD_MERGED`);
default builds are untouched.

**1.1 Prove the wrapping on a vertical slice.** *(DONE — concept proof + real-code
probe pass on BOTH MSVC and VC6; see Spike 1.1 results.)*

**1.2 New preset + build option (small, additive).**
Add `win32-merged` and `vc6-merged` configure presets (inheriting `win32` / `vc6`)
plus an `RTS_BUILD_MERGED` option. This is the *only* meaningful change to existing
tracked files (`CMakePresets.json`, one guard in top-level `CMakeLists.txt`). When
off, nothing changes; when on, the merged target is built.

**1.3 Build-time transform → generated namespaced tree (no edits to originals).**
A new script, run by the merged target, copies each engine's source + headers **and
its compiled-in copy of `Core/`** into `build/<preset>/ns_gen/{Gen,ZH}/...`,
inserting `namespace Gen {` / `namespace ZH {` after the include block and `}` at
EOF, with `#line` directives back to the originals. Includes resolve via the
original include dirs. **Originals stay byte-for-byte unchanged.**

**1.4 Namespaced PCH per engine.**
Because the build force-includes `cmake_pch.hxx` (`CppMacros.h` + `PreRTS.h`) at
global scope, each engine needs its own generated PCH that opens the namespace after
those includes (Spike finding). Generated alongside the tree in 1.3.

**1.5 Handle the tail.**
`extern "C"` entry points, WndProc/callbacks, declaration-macros, and files that
include an engine header mid-body. Prefer fixing these in the transform; only if
unavoidable, a *tiny* edit to the original (kept minimal per the scope constraint).

**1.6 Keep third-party libs shared and global.**
Miles, Bink, DX8, STLport, GameSpy, lzhl stay single, un-namespaced copies linked
once. They don't collide (one copy), and "one engine active at a time" (1.8) keeps
their global state sane.

**1.7 Uniform entry point per engine.**
Each engine exposes one function — `Gen::RunEngine(ctx)` / `ZH::RunEngine(ctx)` —
wrapping its existing `WinMain` body. Implemented in the generated tree / a new
per-engine shim file, not by editing the original `WinMain.cpp` where avoidable.

**1.8 The selector `main` + shared-resource ownership.**
A new `shell/` (new files only) provides the merged exe's real `WinMain`. It:
- reads the chosen game (arg or saved "active game" record),
- owns the one-per-process OS resources (window / HINSTANCE, graphics device,
  static-init order),
- calls the selected engine's `RunEngine`,
- enforces **one engine active at a time** — run one, fully tear down, then run
  the other.

**1.9 Runtime game identity.**
The scaffold passes the selected engine its identity — str/csf files, registry /
install paths, `gAppPrefix`, asset roots — so the same binary serves either game.

**Exit criteria:** with `RTS_BUILD_MERGED` on, the merged exe boots either game
(selected at startup); replays per game are **bit-identical** to today's standalone
builds (`GeneralsReplays/`, `check-replays.yml`); builds clean on VC6 and MSVC; and
with the option **off**, the tree and default build outputs are unchanged.

## Stage 2 — Click the logo to switch engine

With both engines in one exe, switching = the scaffold tears down the active engine
and calls the other's `RunEngine`, in the same process, scaffold-owned window
persisting across the transition. Wire it by adding a clickable gadget over the
logo in `MainMenu.wnd` and one more `NAMEKEY` handler in `MainMenu.cpp` that asks
the scaffold to swap the active game.

## Stage 3 — Mods as switch targets

Extend the scaffold's "active game" from `{engine}` to `{engine, modId}` (a mod =
an engine choice + overlaid asset/INI content, already supported). The logo click
becomes an engine/mod chooser using the same swap path.

## Online — OUT OF SCOPE

Multiplayer/online is explicitly **not** part of this effort. The scaffold keeps the
door open (it owns process-level state, so a session could later survive a swap), but
no online work is planned or designed here.

---

## Risks & where this bites

- **VC6 namespace fragility** — was the #1 risk; **retired** by Spike 1.1 (passes on
  VC6 incl. real code + STLport).
- **Include-order tail (1.5)** — the unbounded part of the effort; every "include
  after namespace open" must be handled by the transform (or a tiny original edit).
- **Generated-tree upkeep** — the transform must stay correct as sources change;
  `#line` directives keep diagnostics pointing at originals.
- **`extern "C"` / OS callbacks** — WndProc, thread entry points, exported symbols
  must stay outside the namespace.
- **Doubled Core** — each engine namespaces its own Core copy; larger binary, but
  no collision and no behavior change.
- **Determinism** — unaffected by design (naming only), but the replay check is the
  mandatory proof.

## Effort shape

Stage 1 is a large but low-*concept*, high-*volume* mechanical job, now front-loaded
into a **transform script** rather than hand edits: new preset (1.2) -> transform +
generated namespaced tree + per-engine PCH (1.3-1.4) -> tail handling (1.5) ->
scaffold + build wiring (1.6-1.9). Stages 2-3 are small once Stage 1 lands.

## Sequencing / go-no-go

1. **1.1 spike** — DONE (passes on VC6 + MSVC). Gate cleared.
2. New preset + `RTS_BUILD_MERGED` option (1.2), off by default.
3. Transform + generated namespaced tree + per-engine PCH (1.3-1.4); prove one real
   subsystem through the merged target on both compilers before scaling out.
4. Tail handling (1.5).
5. Scaffold `main` + per-engine entry points + merged link (1.6-1.9).
6. Logo switch (Stage 2).
7. Mods (Stage 3).

Online is out of scope.
