# Upstream notes — ci/wasm-build

Working notes for what on this branch is a general fix versus web-port scaffolding.
The intent is that `ci/wasm-build` eventually goes up as **one** PR, so nothing here
should be sent piecemeal.

---

## Already open against bobtista

Raised as individual PRs before the one-big-PR plan was settled. Left open as-is —
they are not the route this work is going up by, and nothing here depends on them.
The commit hashes are the ones on the bobtista branches; the equivalent work on this
branch is listed in the next section.

| PR | What |
|----|------|
| [#3](https://github.com/bobtista/GeneralsGameCode/pull/3) | GLES/WebGL scene composite sampled upside down |
| [#4](https://github.com/bobtista/GeneralsGameCode/pull/4) | A1R5G5B5 terrain atlas expanded to BGRA8 off Windows |
| [#5](https://github.com/bobtista/GeneralsGameCode/pull/5) | `%hs` kept printable when rewriting wide formats for libc |

All three cherry-picked cleanly onto `bobtista/topic/trunk` and had the `githubawn`
handle stripped from code comments.

---

## General fixes on this branch (not web-specific)

Candidates for the eventual PR. Each is self-contained and guarded.

### `755d205cc` — GLES composite V-flip
Render targets are bottom-left origin on GLES/WebGL, top-left on DX11/Metal. The
composite vertex shader built one fixed set of texcoords and the backend passed a
hardcoded `0` where the flip flag belongs, so the whole 3D scene came out mirrored
while the UI stayed upright. Cap-driven (`originBottomLeft`), so DX11/Metal pass `0.0`
and are byte-for-byte unaffected.

Trunk still has the bug — `vs_scene_composite.sc` there is byte-identical to the
pre-fix parent, and `BgfxBackend.cpp` still has the hardcoded `0.0f`.

### `ebd422631` — A1R5G5B5 terrain atlas
`BGR5A1` is not a native GLES format, so bgfx converted on upload, over-read the
source buffer and painted coloured speckle across the terrain. Three linked places
all need changing (`GetBgfxTextureUploadFormat`, `IsTerrainAtlasTexture`,
`UploadTerrainAtlasMips`), which is why partial fixes appear to do nothing.

Trunk still maps `A1R5G5B5 -> BGR5A1`. The Switch branches were stripped for upstream:
trunk has zero `__SWITCH__` references, so they would have been unbuildable dead code,
and dropping them also removed three now-unreachable `|| RGBA8` acceptances.

Vibecode frees its intermediate buffer with `bgfx::release()`, which does not exist in
this bgfx revision. `CopyTextureLevel` is asked for the final upload format instead — it
already knows how to widen `A1R5G5B5` — so there is no second allocation.

### `4c121ad92` — `%hs` wide-format rewrite
`translateWideFormat` emitted length modifiers as it read them, so `%hs` reached musl,
which rejects it: `vswprintf` returns -1 and the string comes out empty. `GameText`
formats its missing-label placeholder as `MISSING: '%hs'`, so any INI naming an absent
label produced an empty string and `INI::parseAndTranslateLabel` threw — killing
startup. Reachable with retail data: `CommandMap.ini` asks for `GUI:SaveView5`, which
`generals.csf` does not define. Windows keeps its own `vswprintf` path.

Trunk does not have this.

---

## Backported FROM trunk into this branch

### `45a413a82` — case-correct the leading path component
Backported from `bobtista/topic/trunk` (fixed there 29/07/2026 on Linux); this branch
forked earlier and missed it. `fixFilenameFromWindowsPath` copied the first component
of a relative path through verbatim before its case-insensitive traversal, so a
mis-cased leading directory could never be repaired.

`SidesList` spells the skirmish scripts path `"data\Scripts\SkirmishScripts.scb"` with
a lowercase `d` while it is `Data` on disk. On a case-sensitive filesystem the open
failed, and since the skirmish team records are cleared immediately before that parse,
they stayed empty → the skirmish AI got no teams → null default team → its starting
base belonged to no team → `hasAnyObjects()` false → defeated on frame 0 → **instant
win**. The score screen still showed 1 unit / 1 building because `onUnitCreated` /
`onStructureCreated` are called explicitly, independent of team membership.

The path literal is deliberately left lowercase: the filesystem layer is the single
point of tolerance, and the function is byte-identical to trunk's.

Reproduced on wasm in **both** this port and the vibecode port; not a lost re-port fix.
Hidden on Windows, macOS (case-insensitive APFS) and WSL over `/mnt/c`.
Skirmish plays normally since this landed.

---

## Web-only (only meaningful if the Emscripten target goes up too)

- `53da6ba0b` — texture loader with no loader thread. Guarded to `__EMSCRIPTEN__` and
  staying that way: it travels with the web target in the single PR, so it is not dead
  code there. Trunk's `ThreadClass::Execute()` also returns immediately under `_UNIX`,
  so Linux/macOS/Android may share the latent problem — widening the guard is a
  separate change that needs the failure demonstrated on one of those first.
- `7bce11538` — disable the automatic fullscreen on the web. Startup asked for
  fullscreen unless `-win`, and the web build has no command line. The browser never
  granted it, but the request sized the drawing buffer to the desktop mode while the
  page presented the canvas at its `100vw/100vh` CSS box, so pointer coordinates were
  scaled against the wrong extent and clicks missed. Guarded to `__EMSCRIPTEN__`.

  Known limit, deliberately not addressed: this covers startup only.
  `W3DDisplay::setDisplayMode` is passed `TheDisplay->getWindowed()`, which is still
  false on the web, so changing resolution from the Options menu takes the `else`
  branch and calls `SDL_SetWindowFullscreen(true)` plus a mode switch again. Doing that
  properly means deciding whether the web target should own the container outright
  rather than pretending to have a window - a separate piece of work, not a patch.
- `cede40a5b` — manual reference build of the branch the port came from, artifact only,
  no deploy. Scaffolding for comparing the two ports; goes or stays with the web target.
- All `ci(web)` / `build(web)` / `feat(web)` commits: Emscripten target, CI workflow,
  Vercel deploy, OPFS data picker, LAN-over-WebSocket relay, `serve.py`, `shell.html`.

---

## Open

- **Hosted data whitelist coverage.** The hosted build only reaches four `Data\`
  subfolders (Cursors, WaterPlane, Scripts, INI). The local `serve.py` serves the whole
  `Data\` tree minus `Movies\`. Anything a real install keeps elsewhere under `Data\` is
  silently absent from the hosted build.
- **`GgcRuntimeFlags` scaffolding is still live** (`66985ba06`, April). `GGC_TRACE`
  breadcrumbs, the `GGC_BGFX_RENDERER` override and `GGC_POINTGROUP_DIAG` reach four
  files — `GgcRuntimeFlags.h`, `BgfxBackend.cpp`, `W3DVolumetricShadow.cpp`,
  `SDL3Main.cpp`. Env-var gated and off by default, but development instrumentation
  that should not go up. Deliberately not removed here: the same commit also flipped
  prewarm and mouse-grab defaults, so stripping it is a behaviour change to make on
  purpose, not a history rewrite three months back.

---

## Settled — do not reopen

- **Magenta missing-texture placeholder.** An attempt to bind the white fallback
  instead of the magenta placeholder on the web, and a second attempt to do the same
  where the pixels are read, were both chasing the pink terrain. The cause was the
  A1R5G5B5 atlas above. Both are removed from this branch; the placeholder behaves as
  it does everywhere else, and a genuinely absent asset shows magenta plus its
  `Missing texture <reason>: <file>` line in the browser console.
- **Skirmish instant win.** Fixed by `45a413a82`, not a lost re-port fix.
- **`Data/Scripts/Scripts.ini` never copying into OPFS.** Whitelisted, never lands, and
  no engine reference to it exists — it is a WorldBuilder file. Harmless.
