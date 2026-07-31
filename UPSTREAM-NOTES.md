# Upstream notes — ci/wasm-build

Working notes for what on this branch is a general fix versus web-port scaffolding.
The intent is that `ci/wasm-build` eventually goes up as **one** PR, so nothing here
should be sent piecemeal.

---

## Already open against bobtista (opened prematurely — close or fold in)

These were raised as individual PRs before the one-big-PR plan was clear. They should
probably be closed and folded into the single PR instead.

| PR | Commit | What |
|----|--------|------|
| [#3](https://github.com/bobtista/GeneralsGameCode/pull/3) | `755d205cc` | GLES/WebGL scene composite sampled upside down |
| [#4](https://github.com/bobtista/GeneralsGameCode/pull/4) | `459a406e1` + `045bd8c43` | A1R5G5B5 terrain atlas expanded to BGRA8 off Windows |
| [#5](https://github.com/bobtista/GeneralsGameCode/pull/5) | `4c121ad92` | `%hs` kept printable when rewriting wide formats for libc |

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

### `459a406e1` + `045bd8c43` — A1R5G5B5 terrain atlas
`BGR5A1` is not a native GLES format, so bgfx converted on upload, over-read the
source buffer and painted coloured speckle across the terrain. Three linked places
all need changing (`GetBgfxTextureUploadFormat`, `IsTerrainAtlasTexture`,
`UploadTerrainAtlasMips`), which is why partial fixes appear to do nothing.

Trunk still maps `A1R5G5B5 -> BGR5A1`. The Switch branches were stripped for upstream:
trunk has zero `__SWITCH__` references, so they would have been unbuildable dead code,
and dropping them also removed three now-unreachable `|| RGBA8` acceptances.

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

### `5445eb081` — case-correct the leading path component
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

The backported function is now byte-identical to trunk's. The path literal itself was
deliberately left lowercase to match trunk exactly — the filesystem layer is the single
fix.

Reproduced on wasm in **both** this port and the vibecode port; not a lost re-port fix.
Hidden on Windows, macOS (case-insensitive APFS) and WSL over `/mnt/c`.

---

## Web-only (only meaningful if the Emscripten target goes up too)

- `140c2badc` — run windowed on the web. Startup asked for fullscreen unless `-win`,
  and the web build has no command line. The browser never granted it, but the request
  sized the drawing buffer to the desktop mode while the page presented the canvas at
  its `100vw/100vh` CSS box, so pointer coordinates were scaled against the wrong
  extent and clicks missed. Guarded to `__EMSCRIPTEN__`.
- All `ci(web)` / `build(web)` / `feat(web)` commits: Emscripten target, CI workflow,
  Vercel deploy, OPFS data picker, LAN-over-WebSocket relay, `serve.py`, `shell.html`.

---

## Needs a decision before it can go anywhere

### `53da6ba0b` — texture loader with no loader thread
Currently guarded to `__EMSCRIPTEN__`, so on trunk it would be dead code. But trunk's
`ThreadClass::Execute()` returns immediately under `_UNIX`, so Linux/macOS/Android have
no texture loader thread either and may share the latent problem. Either generalise the
guard to `_UNIX` after confirming it actually manifests there, or hold it until the web
target goes up.

### Missing-texture-untextured work
`798789565`, `6c90583cb`, reverted by `c174ee79c`. Tangled history — needs unpicking
before it is presentable.

---

## Open, unresolved

- **`fixFilenameFromWindowsPath` scope.** Trunk's fix repairs a mis-cased leading
  component, but the mis-cased literal in `SidesList` is still there. Worth deciding
  whether path literals should also be corrected, or whether the filesystem layer is
  the intended single point of tolerance.
- **`Data/Scripts/Scripts.ini`** never copies into OPFS on the hosted web build. It is
  whitelisted in `RETAIL_MANIFEST` but is the only one of the 123 entries that does not
  land. Benign — no engine reference to it; it is a WorldBuilder file.
- **Hosted data whitelist coverage.** The hosted build only reaches four `Data\`
  subfolders (Cursors, WaterPlane, Scripts, INI). The local `serve.py` serves the whole
  `Data\` tree minus `Movies\`. Anything a real install keeps elsewhere under `Data\` is
  silently absent from the hosted build.
