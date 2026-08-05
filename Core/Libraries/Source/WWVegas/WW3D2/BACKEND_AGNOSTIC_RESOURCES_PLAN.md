# Making WW3D2 resource classes backend-agnostic — plan

Goal: let `DX9ExBackend` (stacked on top of this branch, see `topic/dx9ex-skeleton`)
create, lock, bind, and draw with its own `IDirect3DDevice9Ex` resources,
without breaking the DX8 reference path or VC6 builds.

## Resource-class survey

| Class | Pattern | D3D8 exposure |
|---|---|---|
| `VertexBufferClass` / `IndexBufferClass` | **Clean.** Base holds no D3D pointer; `DX8VertexBufferClass`/`DX8IndexBufferClass` hold it; `SortingVertexBufferClass`/`SortingIndexBufferClass` are already non-D3D siblings. `DX8Wrapper` dispatches via type-tag + `static_cast` centrally (`dx8wrapper.cpp:2098-2352`), not virtual calls on the resource. | Easy — purely additive. |
| `TextureBaseClass` | **Not clean.** Base stores `IDirect3DBaseTexture8 *D3DTexture` directly, and its **virtual `Apply(unsigned stage)`** (`texture.cpp:920,960`) itself calls `DX8Wrapper::Set_DX8_Texture(...)` — the D3D8 call is issued from inside the resource class, unlike buffers where the backend dispatches. | Base-class hard-typed *and* binding call site lives in the resource class. |
| `SurfaceClass` | **Not clean, same shape as `TextureBaseClass`.** Single concrete class (no base/derived split) holding `IDirect3DSurface8 *D3DSurface`, with ~10 internal `LockRect` call sites. No non-D3D8 sibling to model after. | Hardest — no existing split. |
| `ShaderClass` | **Not a problem.** Plain packed-bitfield render-state descriptor, holds no D3D pointer. `DX8Wrapper::Apply_Render_State_Changes()` translates bits to D3D8 states. | Needs only a backend-side translator. |
| `DX8Wrapper`'s internal render state | **Load-bearing, monolithic.** `RenderStateStruct render_state` plus a second low-level texture shadow-cache are private/static to `DX8Wrapper`. `DX9ExBackend` can't reach into them — needs its own independent cache and dirty-tracking (~lines 2200-2400, 1150-1420 of dx8wrapper.cpp as reference). | Architecture decision, not a mechanical port. |

## TextureBaseClass design: recommended approach

**Option A (recommended): opaque handle + backend-tagged accessor.** Replace
`IDirect3DBaseTexture8 *D3DTexture` with `void *GpuHandle` in the base class.
Keep existing accessor names (`Peek_D3D_Base_Texture()`, etc.) as thin typed
casts for the DX8 path — zero-overhead, no call-site changes needed as long as
they only run under `DX8Backend`. Add DX9-flavored casts guarded the same way
`IS_VS6_BUILD` is. Move the `SetTexture` call **out of `TextureClass::Apply()`**
into each backend's own `Apply_Render_State_Changes()`, mirroring how buffers
already work. Apply the same treatment to `SurfaceClass`.

Rejected: a base/derived split like the buffer classes (Option B) — `TextureClass`
carries too much shared bookkeeping (format, filter, mip/pool state, multiple
constructors used throughout `textureloader.cpp`'s ~2500-line background-thread
pipeline); doubling 4 texture classes × 2 backends isn't worth it. Also rejected:
a backend-owned side-table keyed by resource pointer (Option C) — needs
creation/destruction notification hooks that don't exist yet, plus a lookup on
every bind; bigger lift than Option A for the same result.

## Call-site inventory (blast radius)

- **Vertex/index buffers** — small, contained: `dx8wrapper.cpp` dispatch (~6
  sites), `dx8vertexbuffer.cpp`/`dx8indexbuffer.cpp` creation/lock. Two files
  **bypass the abstraction entirely** and need separate handling:
  `Core/GameEngineDevice/.../Water/W3DWaterTracks.cpp` (calls
  `Get_DX8_Vertex_Buffer()->Lock()` directly with `D3DLOCK_NOOVERWRITE`/`DISCARD`)
  and `Core/GameEngineDevice/.../W3DSnow.h/.cpp` (owns a raw
  `IDirect3DVertexBuffer8*` member, no `VertexBufferClass` at all).
- **Textures** — medium: `texture.h/.cpp` (~1900 lines, the base class and all
  `Apply()`/`Apply_New_Surface()` overrides), `textureloader.cpp` (~2550 lines,
  background-thread loading), `dx8texman.h` (device-lost recreation),
  `missingtexture.cpp/.h`. Downstream in `Core/GameEngineDevice`:
  `TerrainTex.cpp`, `W3DSmudge.cpp`, `W3DProfilerFrameCapture.cpp`,
  `W3DTreeBuffer.cpp`, `Water/W3DWater.cpp` read `Peek_D3D_Texture()`/
  `Peek_D3D_Surface()` or call `DX8Wrapper::_Get_D3D_Device8()` directly.
  `W3DShaderManager.cpp` owns **its own** static `IDirect3DTexture8*`/
  `IDirect3DSurface8*` members for render-to-texture postFX, entirely outside
  `TextureBaseClass`/`SurfaceClass`.
- **Surfaces** — `surfaceclass.cpp/.h` itself (~10 `LockRect` sites),
  `texture.cpp`'s `Get_Surface_Level`/`Get_D3D_Surface_Level`, plus
  `W3DProfilerFrameCapture.cpp`, `W3DMouse.cpp`, `W3DScreenshot.cpp`,
  `render2dsentence.cpp`, and two `Core/Tools/W3DView` files.
- **`DX8Wrapper`'s global device** (`static IDirect3DDevice8 *D3DDevice`,
  `_Get_D3D_Device8()`) is read directly, bypassing any resource-class
  abstraction, by `dx8vertexbuffer.cpp`, `dx8indexbuffer.cpp`, `W3DSnow.cpp`,
  `Water/W3DWater.cpp`, `W3DShaderManager.cpp`. Biggest correctness trap (see
  below) — resource **creation**, not just binding, is baked to the D3D8
  device today.

## Phase ordering

- **Phase 0** — decide render-state ownership: `DX9ExBackend` gets its own
  independent render-state cache/dirty-tracking (recommended, matches the
  "no fallback, forced Ex" spirit already chosen for the device) rather than
  extracting `DX8Wrapper::render_state` into a shared component. Design-only,
  should happen before Phase 1 code.
- **Phase 1** — `VertexBufferClass`/`IndexBufferClass`: add
  `DX9VertexBufferClass`/`DX9IndexBufferClass` siblings, a new buffer-type tag,
  and creation/lock code against the D3D9Ex device. `W3DSnow`'s raw buffer and
  `W3DWaterTracks.cpp`'s lock bypass are explicitly out of scope here —
  flagged as DX8-only until ported.

  **Correction (found while starting this phase): "no base-header changes"
  was wrong for `VertexBufferClass`.** Its header, `dx8vertexbuffer.h`,
  transitively pulls `<d3d8.h>` via `dx8fvf.h` (needed for `D3DFVF_*`
  bit-flag macros). A non-DX8 backend including it at all — even just to see
  the base class — hits the same D3D8/D3D9 type-collision crash fixed in
  `topic/dx9ex`. Rewriting `dx8fvf.h` to drop `<d3d8.h>` was tried and
  reverted: many unrelated files (`W3DBufferManager.cpp`, `W3DWater.h`,
  `WinMain.cpp`, ...) rely on it as an undocumented transitive backdoor to
  raw D3D8 types they never explicitly include — out of scope for this
  branch. Fixed narrowly instead: `VertexBufferClass`/`VertexBufferLockClass`
  are now split into their own header, `vertexbufferclass.h`, which only
  includes `WWLib/always.h`, `WWLib/refcount.h`, `WWDebug/wwdebug.h` — no
  `dx8fvf.h`, no `<d3d8.h>`. `dx8vertexbuffer.h` now `#include`s it instead of
  defining these classes itself; `DX8VertexBufferClass`/
  `SortingVertexBufferClass`/`DynamicVBAccessClass`/`dynamic_fvf_type` are
  unchanged and still pull `dx8fvf.h` as before. Pure move, zero behavior
  change, compile-verified.

  `IndexBufferClass` needed **no change at all** — `dx8indexbuffer.h` only
  includes `WWLib/always.h`, `WWDebug/wwdebug.h`, `WWMath/sphere.h`, none of
  which touch D3D8. It was already safe for a non-DX8 backend to include,
  despite the "dx8" in the filename.

  Still not done: the actual `DX9VertexBufferClass`/`DX9IndexBufferClass`
  siblings, the new buffer-type tag (currently in `dx8wrapper.h`, which is
  *not* D3D8-header-free and needs its own neutral home before `DX9ExBackend`
  can reference it), and creation/lock code against the D3D9Ex device.
- **Phase 2** — `TextureBaseClass`/`SurfaceClass` opaque-handle refactor
  (Option A, applied to both — texture mip levels return `SurfaceClass*` so
  they're coupled). Largest, structural. The risky part isn't the type
  rename, it's relocating `Apply()`'s device call out of the resource class
  into backend-owned dispatch — a control-flow change that needs
  behavioral-equivalence testing against the DX8 path even before any DX9
  code exists. Also port `dx8texman.h`'s tracker pattern to a
  `DX9TextureTrackerClass`.
- **Phase 3** — wire real resource creation into `DX9ExBackend` against an
  actual `IDirect3DDevice9Ex`, plus its own render-state cache (Phase 0's
  decision made concrete).
- **Phase 4** — downstream `GameEngineDevice` call sites that bypass the
  abstraction outright (`W3DSnow`, `W3DWaterTracks`, `W3DShaderManager`'s
  render-to-texture, `TerrainTex`, `W3DSmudge`, `W3DTreeBuffer`, `W3DWater`,
  `W3DProfilerFrameCapture`). Given the `BgfxBackend` precedent of stubbing
  these as no-ops, the pragmatic first-milestone scope is: get core mesh
  rendering working, treat these seven as an explicit known-gap list (skip
  water sparkle/reflection, snow, smudge, terrain detail-blend, in-engine
  screenshot/profiler capture under non-DX8 backends) rather than porting all
  of them up front.

## Correctness traps

1. Resource **creation**, not just binding, is baked to
   `DX8Wrapper::_Get_D3D_Device8()` in `dx8vertexbuffer.cpp`/
   `dx8indexbuffer.cpp`/`textureloader.cpp` — `DX9ExBackend` needs its own
   creation helpers, not a way to inject a different device into
   `DX8Wrapper`'s static.
2. `TextureClass::Apply()` issues the device call itself (unlike buffers).
   Moving that to backend-owned dispatch (Phase 2) changes control flow, not
   just types — regression-test the DX8 path carefully after this move.
3. `W3DSnow` and `W3DWaterTracks.cpp` bypass the resource-class abstraction
   outright and need dedicated ports or explicit DX8-only gating; making the
   resource classes polymorphic doesn't fix them.
4. `W3DShaderManager` owns its own static D3D8 texture/surface pointers for
   render-to-texture postFX, entirely unabstracted — separate decision
   (port vs. stub) independent of the resource-class work.
5. `DX8Wrapper::render_state` is private/static and monolithic (Phase 0);
   duplicating its dirty-flag cache for DX9Ex risks the two state machines
   drifting apart over time as new render states get added to one and not
   the other.
6. Device-lost/reset recreation (`TextureTrackerClass::Recreate()`) assumes
   D3D8's lost-device model. D3D9Ex's `ResetEx`/`CheckDeviceState` model
   differs materially — don't port the *logic* verbatim, only the "notify
   textures on reset" *shape* is reusable.
7. Dynamic/locked buffer usage flags (`USAGE_DYNAMIC`, pool types, lock
   flags) encode D3D8-specific pool/lock semantics that aren't 1:1 under
   D3D9Ex — verify each flag's equivalent rather than assuming a direct
   mapping.

## Critical files

- `texture.h` / `texture.cpp`
- `surfaceclass.h` / `surfaceclass.cpp`
- `dx8wrapper.h` / `dx8wrapper.cpp`
- `dx8texman.h`
- `dx8vertexbuffer.h` / `dx8indexbuffer.h` (reference pattern to replicate)
- `Backend/DX8Backend.cpp` (shows the shape a backend's forwarding layer takes)
