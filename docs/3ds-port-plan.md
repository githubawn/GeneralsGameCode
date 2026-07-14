# New Nintendo 3DS Port Plan

Branch: `3ds-port` (off `android-macos-iphone-windows64-vibecode-test`)
Status: planning — no implementation started.

Goal: get Zero Hour (`z_generals`) running on the **New** Nintendo 3DS using the
same devkitPro installation as the Switch port (`C:/code/devkitPro`), which
already contains devkitARM, libctru, and citro3d/citro2d. `portlibs/3ds` is
currently empty (no SDL, no zlib) — those must be installed or self-built.

## Reality check

The New 3DS is the most constrained target attempted so far. The plan is built
around finding out **cheaply** whether it is viable before investing in the
expensive parts.

- **CPU**: 4x ARM11 MPCore @ 804MHz, ARMv6K, VFPv2 float, no NEON, 32-bit.
  ILP32 matches win32 assumptions, and the wasm32 work already flushed out most
  32-bit issues.
- **RAM (make-or-break)**: a CIA installed with New-3DS extended memory mode
  gets roughly a 124–178MB application region; a plain `.3dsx` under Homebrew
  Launcher gets less. Memory is what killed the previous small-RAM console
  attempt, so this plan front-loads a memory go/no-go gate before any renderer
  work. Old 3DS (64–96MB) is flatly impossible and is out of scope.
- **GPU**: PICA200 — **bgfx has no 3DS backend and cannot have one** (PICA has
  no fragment shaders, only 6 fixed texture-combiner (texenv) stages plus
  programmable vertex shaders written in PICA assembly). The entire bgfx path
  used on Android/macOS/iOS/Switch/WASM is unusable. A new citro3d render
  backend is the dominant work item.
- **Screen**: 400x240 top screen vs. the game's 800x600 minimum resolution.
  UI readability is a real open problem even if everything else works.

## Phase 0 — Branch + toolchain bring-up (cheap, ~a day)

1. ~~New branch `3ds-port`~~ (done).
2. Install missing 3DS portlibs via devkitPro's bundled msys2 pacman
   (`3ds-zlib`, etc. — the `3ds-dev` core group is already installed).
3. Write `cmake/toolchains/nintendo-3ds.cmake` modeled directly on
   `cmake/toolchains/nintendo-switch.cmake`:
   - `arm-none-eabi-gcc`/`g++` from `devkitARM/bin`
   - `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -D__3DS__`
   - `-fno-strict-aliasing` — REQUIRED, same GCC miscompile risk as Switch
     (VC6-era type punning throughout the codebase)
   - `-specs=3dsx.specs`, `CMAKE_FIND_ROOT_PATH` on libctru + portlibs/3ds
   - the same `--start-group`/`--end-group` link-rule override as Switch
     (circular deps between engine libs)
   - `RTS_GAMEMEMORY_ENABLE OFF` — the custom GameMemory pool allocator
     corrupts memory under devkitPro GCC + static libstdc++ (proven on Switch);
     use system malloc/free
   - all tools OFF
4. Add a `3ds-generalsmd` configure/build preset to `CMakePresets.json`
   (model on `switch-generalsmd-sdl3-bgfx`, but with the citro3d backend once
   it exists; expect to need the explicit-paths configure workaround the
   Switch preset needs).
5. Verify a hello-world `.3dsx` boots in Citra (and on hardware if available).

## Phase 1 — Go/no-go gates (before investing in a renderer)

1. **Memory gate**: build a trivial app that allocates-and-touches heap until
   failure, as both `.3dsx` and CIA with N3DS extended memory mode. If we
   cannot get **>= ~150MB** of real usable heap, STOP — the port is not
   viable, and only a day or two was spent.
2. **Compile gate**: build the GameEngine core libraries headless against a
   stubbed backend. `StubD3D8Device.{h,cpp}` on this branch is
   backend-agnostic (no-op D3D8 COM interfaces under `GGC_BGFX_STANDALONE`)
   and reusable; the standalone define may need generalizing. Expect
   GCC/newlib issues, not architectural ones.
3. **Renderer scoping memo**: enumerate the DX8 fixed-function states the game
   actually uses (terrain multi-texture, alpha blend, vertex color, water,
   roads, shadows) and map each onto PICA texenv stages; flag anything
   unrepresentable before committing to Phase 3.

## Phase 2 — Platform layer

- **Window/input**: SDL3 has an official N3DS port (see SDL's
  `docs/README-n3ds.md`). Build it from our SDL3 fork the same way the Switch
  build clones its patched SDL3. Fallback: libctru `hidScanInput` directly.
- **Clock**: check `timeGetTime()` first thing. The Switch/WASM
  `CLOCK_BOOTTIME=0` bug ("menus draw and click but never navigate", because
  Shell::update's 30Hz gate never passes) is likely present; guard
  `CLOCK_MONOTONIC` under `__3DS__` in time_compat, same as Switch/Emscripten.
- **Data**: SD card at `sdmc:/generalszh`, mirroring the Switch
  `GGC_SWITCH_SD_DATA` approach with a `GGC_3DS_SD_DATA` define. The full
  3.4GB data set is not needed if textures get reduced anyway (Phase 4).
  Remember ZH reuses base-Generals art: `ZH_Generals/Textures.big`, `W3D.big`,
  `Terrain.big` must ship or units go magenta and terrain/water vanish.
- **Audio/video**: `SAGE_USE_OPENAL OFF` (no OpenAL port for 3DS),
  `RTS_BUILD_OPTION_FFMPEG OFF` initially. SDL3 audio via ndsp can come later.

## Phase 3 — citro3d renderer (the big one)

- Add `citro3d` as a third `GGC_RENDER_BACKEND` value in
  `cmake/render-backend.cmake`, implementing the same standalone-backend seam
  `BgfxBackend` uses (IRenderBackend + DX8Wrapper driven against the stub
  device).
- One PICA vertex shader (world-view-proj transform + per-vertex color/UV),
  compiled with picasso at build time. Translate DX8 texture-stage state to
  texenv stages per draw call.
- **Textures**: PICA does not support DXT/S3TC. Decompress DXT to
  RGB565/RGBA4 and swizzle to the PICA tiled (Morton) layout at load time.
  ETC1 offline conversion is the fallback if memory demands it. Max texture
  size 1024x1024 is sufficient for this game. TGA loading already works on
  32-bit (the LP64 TGA footer bug does not apply to ILP32).
- Milestones, in order, each a reassessment checkpoint:
  1. clear screen at 400x240
  2. 2D UI quads (Render2D path)
  3. main menu navigable
  4. terrain renders
  5. units render
- Per project rules: every change guarded under `__3DS__` (or the citro3d
  backend define) — no unguarded edits to shared files.

## Phase 4 — Memory + fit (only if Phase 3 reaches in-game)

- Texture downscale pipeline and memory-pool tuning (expect this to be the
  bulk of the remaining work; the previous small-RAM port's lesson is that
  texture scratch and pool over-provisioning are the big levers).
- Render at native 400x240 — the low fill-rate cost is the one thing the
  weak GPU has going for it.
- **UI readability at 400x240**: scaling the 800x600 layout down will hurt
  text legibility; needs its own design pass. Biggest UX unknown. The
  bottom touch screen (320x240) could take some UI (command bar?) as a
  stretch goal.

## Honest expectations

Best realistic outcome: menus plus a small skirmish map at modest framerates,
New 3DS only. The two likeliest kill conditions are the Phase 1 memory gate
and texenv coverage for the terrain/water paths — both are resolved within
the first week, before the expensive renderer work begins.
