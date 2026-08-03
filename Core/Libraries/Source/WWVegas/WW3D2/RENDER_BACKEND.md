# IRenderBackend / DX9Ex — branch base and known gaps

`topic/dx9ex` is currently stacked on `bobtista/feat/render-backend-interface-skeleton`
(the 11-commit foundational IRenderBackend + DX8Backend adapter), **not** on
`bobtista/topic/render-backend-interface` (PR #2613). This file tracks why, and
what that trade gives up so it can be picked back up deliberately instead of
by accident.

## Why skeleton instead of PR #2613's branch

`topic/render-backend-interface` has a real bug on its current tip
(`c2435cbb5`, confirmed against a fresh fetch, not a stale local ref):
`W3DShaderManager.cpp` calls `g_renderBackend->Override_Terrain_Blend(...)`,
`Override_Texcoord_Index(...)`, `Override_Alpha_Blend_Enable(...)`, and
`Override_Blend(...)` (introduced in commit `6a5409977`,
"feat(ww3d2): extend IRenderBackend with ZBias/FillMode/DepthTest/DepthFunc/ColorWriteMask
and migrate callers"), but none of those four methods were ever declared on
`IRenderBackend`. It does not compile on Windows/MSVC as-is.

Given a choice between fixing that upstream bug in place vs. starting the
DX9Ex work clean, the decision was to rebuild the small DX9Ex-specific surface
(`DX9ExBackend.cpp/h`, `RenderBackend.cpp` factory wiring, `GlobalData.h`
selection enum, `cmake/dx9.cmake`) directly against skeleton, mirroring
`DX8Backend.cpp/h` 1:1, rather than merge/cherry-pick the old work forward.

## What skeleton does NOT have (known gaps, in case they matter later)

### Caller migrations (the actual reason topic/render-backend-interface exists)

These commits, present on `topic/render-backend-interface` but **not** on
skeleton, migrated dozens of call sites off `DX8Wrapper::Set_DX8_Render_State`
and onto `g_renderBackend`. Without them, terrain shading, shroud rendering,
and screen filters in `W3DShaderManager.cpp` / `W3DShroud.cpp` still talk to
`DX8Wrapper`/D3D8 directly, so a non-DX8 backend (bgfx, or eventually a real
DX9Ex device) will not actually intercept those draws:

- `cf12396ea` — route W3D subsystems and shadow/scene callers through
  `g_renderBackend`; extends `IRenderBackend` with blend/stencil/cursor/alpha
  state (`Set_Blend_Op`, `Set_Blend_Factors`, `Set_Color_Write_Enable`,
  `Set_Alpha_Blend_Enable`, `Show_Hardware_Cursor`,
  `Set_Hardware_Cursor_Image/Position`, `Set_Stencil_*`).
- `10d5d6636` — deprecate remaining `DX8Wrapper` game-code callers in favor of
  `IRenderBackend`.
- `6a5409977` — extends `IRenderBackend` with `Set_Z_Bias`, `Set_Fill_Mode`,
  `Set_Depth_Test_Enable`, `Set_Depth_Write_Enable`, `Set_Depth_Func`,
  `Set_Color_Write_Mask`; migrates `W3DShaderManager.cpp` callers. **Also
  introduces the four undeclared `Override_*` calls — the bug above.**
- `b0084567c` — routes `W3DShroud`'s texture upload through
  `IRenderBackend::Upload_Texture_Region` (POOL_DEFAULT surfaces can't be
  locked directly).
- `951a79613` — null-guards `g_renderBackend` in `addShadow` and shroud
  render paths.

Net effect: skeleton's `IRenderBackend` has **52 virtual methods**; the
current PR #2613 tip has **74** (plus the 4 that should exist but don't).

### General upstream drift (unrelated to rendering)

Skeleton's merge-base with `upstream/main` is the same commit as its
merge-base with `topic/render-backend-interface` (`fe72137f3`) — skeleton was
branched once and never rebased forward. As of this writing it is **42
commits behind `upstream/main`**, vs. 14 for the PR #2613 branch (which has
been periodically rebased — its commits carry "(cherry picked from commit
...)" notes). Notable things missing from skeleton as a result, in case a
conflict or missing symbol traces back here:

- SDL3 windowing backend (`GeneralsMD/Code/Main/SDL3Main.cpp`, ~286 lines,
  doesn't exist on skeleton at all).
- OpenAL audio manager (device/source/stream management, decoded-PCM sample
  cache) and FFmpeg movie-audio wiring.
- macOS build/deploy scripts (`scripts/build/macos/*`), `docs/BUILD/GETTING_THE_GAME_FILES.md`.
- `cmake/bgfx.cmake`, `cmake/openal.cmake`, `cmake/sdl3.cmake`.
- A batch of `unify(*)`: Move-to-Core commits (ww3d2, particlesys, bezier,
  commandline, precompiled, etc.) and assorted bugfixes (see
  `git log --oneline remotes/bobtista/bobtista/feat/render-backend-interface-skeleton..upstream/main`
  for the full list of 42).
- `BgfxBackend.cpp/h` exist on skeleton as files but are **not wired into**
  `RenderBackend.cpp`'s factory (no `#if defined(GGC_RENDER_BACKEND_BGFX)`
  branch there, unlike PR #2613).

## Current state of DX9ExBackend (as of this handoff)

`Backend/DX9ExBackend.cpp/h` now has a **real device lifecycle**:
`Initialize(window, width, height)` dynamically loads `d3d9.dll`, resolves
`Direct3DCreate9Ex` via `GetProcAddress`, and calls it — **forced, no
fallback to plain `Direct3DCreate9`** (explicit decision: systems without
D3D9Ex support should select `-dx8`/`GraphicsBackend=DX8` instead of silently
downgrading). On success it calls `CreateDeviceEx` with
`D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED |
D3DCREATE_FPU_PRESERVE`, windowed-only, backbuffer format taken from
`GetAdapterDisplayModeEx`, depth/stencil `D3DFMT_D24S8` with a `D3DFMT_D16`
fallback if the driver can't do D24S8. `Shutdown()` releases both the device
and the `IDirect3D9Ex` interface. This is entirely self-contained — it does
**not** touch `DX8Wrapper`'s device or any of its static state.

Also implemented for real against the D3D9Ex device: `Begin_Scene`/
`End_Scene` (via `PresentEx`, device-lost detected from the `HRESULT`)/
`Clear`/`Set_Viewport`/`Set_Transform`/`Get_Transform` (world/view/projection,
using the existing backend-agnostic `To_D3DMATRIX`/`To_Matrix4x4` helpers
from `WWMath/matrix4.h` — those already work with D3D9's `D3DMATRIX` since
the struct layout is identical between the D3D8 and D3D9 headers)/
`Set_Light`/`Set_Ambient`/`Set_Fog`/`Set_Gamma`. World/view identity tracking
is a plain bool pair owned by the backend instance (not shared with
`DX8Wrapper::render_state`, deliberately — see Phase 0 in
`BACKEND_AGNOSTIC_RESOURCES_PLAN.md`).

**Still stubbed (empty bodies), matching `BgfxBackend`'s precedent, because
they need real D3D9 resources and `VertexBufferClass`/`IndexBufferClass`/
`TextureBaseClass`/`SurfaceClass` are still hard-typed to D3D8 COM
interfaces:** `Set_Vertex_Buffer` (both overloads), `Set_Index_Buffer` (both
overloads), `Set_Index_Buffer_Index_Offset`, `Set_Shader`, `Get_Shader`,
`Set_Material`, `Set_Texture`, `Apply_Render_State_Changes`,
`Invalidate_Cached_Render_States`, `Draw_Triangles` (both overloads),
`Draw_Strip`, `Set_Vertex_Shader`, `Set_Pixel_Shader`, `Create_Render_Target`,
`Set_Render_Target_With_Z`, `Is_Render_To_Texture`, `Set_Shadow_Map`,
`Get_Shadow_Map`. `Get_Back_Buffer`/`Get_Back_Buffer_Format` are also stubbed
for the same reason (would need to wrap `IDirect3DSurface9`/convert
`D3DFORMAT` — no `SurfaceClass`/`WW3DFormat` conversion path exists for D3D9
yet).

## NOT done yet — explicit gaps at handoff

1. ~~**`cmake/dx9.cmake` is not wired into the build at all.**~~ **Done and
   compile-verified:** `cmake/render-backend.cmake` includes `cmake/dx9.cmake`
   when `GGC_RENDER_BACKEND=dx9ex`, and `corei_ww3d2` links the `d3d9`
   target. `z_generals` (GeneralsMD) now builds clean with the default
   `GGC_RENDER_BACKEND=dx9ex`, producing a working `generalszh.exe`.

   Getting there required fixing a real crossover bug: the shared PCH for
   `z_ww3d2`/`g_ww3d2` force-includes `dx8wrapper.h` -> `d3d8.h`, and
   `d3d8types.h`/`d3d9types.h` define dozens of identically-named
   enums/structs (`_D3DPRESENT_PARAMETERS_`, `_D3DLIGHTTYPE`, `IDirect3D9Ex`
   and friends only exist behind a `DIRECT3D_VERSION >= 0x0900` gate that
   `d3d8.h` had already pinned to `0x0800`, etc.) — the two headers cannot
   both be visible in one translation unit. `DX9ExBackend.cpp` is DX9Ex-only
   code and has no business seeing D3D8 types at all, so the fix is
   structural, not a macro workaround: `Backend/DX9ExBackend.cpp` now has
   `SKIP_PRECOMPILE_HEADERS ON` in both `GeneralsMD/.../WW3D2/CMakeLists.txt`
   and `Generals/.../WW3D2/CMakeLists.txt`, so it never inherits the shared
   PCH and never sees `d3d8.h` in the first place. This is the same
   DX8/DX9-isolation principle `topic/backend-agnostic-resources` exists to
   extend to the resource classes.
2. ~~**VC6 guard is not in place.**~~ **Done:** `Backend/DX9ExBackend.cpp/h`
   are only added to `corei_ww3d2` when `NOT IS_VS6_BUILD AND
   GGC_RENDER_BACKEND STREQUAL "dx9ex"`. VC6 builds always get `dx8` forced
   via `render-backend.cmake`.
3. ~~**Default backend selection is not implemented.**~~ **Done (compile-time
   only):** `GGC_RENDER_BACKEND` defaults to `dx9ex` on modern toolchains and
   `dx8` on VC6; `GGC_RENDER_BACKEND_DX9EX=1` / `GGC_RENDER_BACKEND_DX8=1`
   propagate globally and into `RenderBackend.cpp`'s `#if`.
4. **The `-dx8`/`-dx9ex` command-line flags and `Options.ini`
   `GraphicsBackend` runtime parsing** (item 3 of the original implementation
   plan) still don't exist — `GlobalData::m_renderBackend` is declared but
   nothing reads or writes it. Right now backend selection can only ever be
   compile-time (once item 3 above lands).
5. **Resource-class work** (`BACKEND_AGNOSTIC_RESOURCES_PLAN.md` Phases 1-2)
   hasn't started. Until then, `DX9ExBackend` can create a real device, clear
   the screen, and set transforms/lights, but cannot draw any actual game
   geometry (no vertex/index/texture binding).
6. **Caller-migration gap** (this file's earlier section) is unchanged: even
   once resource classes are fixed, `W3DShaderManager.cpp`/`W3DShroud.cpp`
   etc. still call `DX8Wrapper` directly for blend/stencil/cursor/terrain
   state, bypassing `g_renderBackend` entirely, because skeleton never got
   those caller-migration commits.

## Next steps, roughly in order

1. ~~Wire `cmake/dx9.cmake` and compile-verify `DX9ExBackend.cpp`.~~ **Done** —
   `z_generals` builds and links clean with `GGC_RENDER_BACKEND=dx9ex` (the
   default).
2. ~~Exclude `Backend/DX9ExBackend.cpp/h` from VC6 builds.~~
3. ~~Add cmake option + `GGC_RENDER_BACKEND_DX9EX` default-on for modern
   compilers, off for VC6.~~
4. Add the `-dx8`/`-dx9ex` command-line flags and `Options.ini`
   `GraphicsBackend` parsing that select `GGC_RENDER_BACKEND_DX9EX` /
   `TheGlobalData->m_renderBackend` at runtime.
5. Start `BACKEND_AGNOSTIC_RESOURCES_PLAN.md` Phase 1 (`VertexBufferClass`/
   `IndexBufferClass` — additive, no base-class changes needed) so
   `DX9ExBackend` can actually bind and draw geometry.
6. Decide whether to re-derive the caller-migration commits (blend/stencil/
   cursor/terrain-override plumbing) on top of this branch, or accept that
   DX9Ex — like skeleton's DX8Backend — won't actually see those draw calls
   until that's done.
