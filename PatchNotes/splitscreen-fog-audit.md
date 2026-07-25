# Splitscreen Per-View Fog — Full System Audit

## RESOLUTION (2026-07-25) — **it was never the fog**

**Root cause: the terrain *draw window* is GLOBAL, not per-view.**

`HeightMapRenderObjClass` only builds geometry for a sliding window of tiles around one
center. That center is global state on `TheTerrainRenderObject`, set by
`W3DView::updateTerrain()` (W3DView.cpp:3695):

```cpp
TheTerrainRenderObject->setTerrainDrawSize(drawWidth, drawHeight);
TheTerrainRenderObject->updateCenter(m_3DCamera, &cameraPivot, it);
```

which is called only from camera-update paths (W3DView.cpp:854 in `setCameraTransform`,
:1398 on `doesNeedFullUpdate()`). With two viewports, **whichever view moved its camera last
owns the window for every viewport**. The right viewport was drawing the terrain tiles for
player 1's region, so at player 2's base there was simply *no terrain geometry* — black.

### Evidence that closed it (the 5 labelled states)

| State | Player 1's camera | Right viewport |
|---|---|---|
| 1 | at its own base | black ground |
| 2 | moved over player 2 | ground appears |
| 3 | unit moved to player 2 | both viewports fine |
| 4 | camera moved away | **buildings floating on black ground** |
| 5 | unit moved away | same, buildings static |

State 4 is decisive: player 2's *objects* render correctly while the ground under them is
black. Objects honour the per-view render-player override; terrain geometry doesn't exist to
render at all. The right viewport's terrain tracked player 1's camera exactly.

### Instrumentation that ruled the fog out entirely

Every fog signal was already correct before the fix:

| Probe | Value | Meaning |
|---|---|---|
| `terrainOverride` | 3 | the render-player override IS live at the actual terrain shroud bind |
| `srcAtBase` / `TEXlvl` | 255 | the shroud texture genuinely contains "lit" at player 2's base |
| `LOGICvisAtBase` | 0 (`CELLSHROUD_CLEAR`) | player 2 has real logical vision at its base |
| `objRenderPlayer` | 3 | scene objects render with player 2's vision |
| `EYEht` / `aim` | 328 / (1063,428) | the right camera is correctly perched over player 2's base |

### Fix

Re-centre the terrain on **this** view's camera immediately before its render, in
`W3DView::draw()` (Core/GameEngineDevice/.../W3DView.cpp, just before `doRender`):

```cpp
if (TheTerrainRenderObject && TheDisplay && TheDisplay->getFirstView()
        && TheDisplay->getNextView( TheDisplay->getFirstView() ))
    updateTerrain();
```

Gated on multi-view, so single-player is byte-identical. Safe/cheap because `updateCenter`
early-outs when the draw origin hasn't moved. (`View::getNextView()` is protected —
use the public `Display::getNextView(View*)`.)

### Superseded theory (kept for the record)

An earlier resolution blamed the secondary shroud texture `m_pDstTexture2` (a `POOL_DEFAULT`
texture created mid-frame in `setActiveShroudTarget`) and switched to a single shared texture.
**That diagnosis was wrong** — the right view stayed black afterwards. The single-shared-texture
change was kept anyway because it is simpler and demonstrably correct (`srcAtBase=255` proves
the per-view refill works):

- `W3DShroud::getShroudTexture()` — always returns `m_pDstTexture`.
- `W3DDisplay::prepareShroudForView()` — `setActiveShroudTarget(FALSE)` always; refill + render into primary.
- `m_pDstTexture2` / `setActiveShroudTarget(TRUE)` is now dormant.

Built clean (generalszh.exe, SDK 10.0.28000.0). **Needs in-game verification** of states 1 and 4:
the right viewport should now show lit terrain *under* player 2's buildings.

### Lesson for the rest of splitscreen

Per-view correctness needs an audit of **all global render state that a view mutates**, not just
fog. The terrain draw window was one; look for others (LOD/detail centres, water plane, anything
else keyed off "the" camera) before assuming a per-view symptom is a visibility problem.

---

Status: the RIGHT (player-2) viewport renders **black** in undiscovered areas even though
every measured signal says it should work. This document audits every system in the fog
render chain, lists every candidate bug with its verification status, and defines a
definitive isolation test.

## 1. Confirmed facts (from on-screen overlay, live game)

| Signal | Value | Meaning |
|---|---|---|
| `activeSeats` | 2 | split layout active |
| `seat1 vp` | (400,0 400x600) | right viewport correctly placed |
| `seat1 army` | P2 / engineIdx3 | seat 1 bound to the controller's army (index 3) |
| `nonLocalClear` | 3900 | player 3's shroud has 3900 revealed cells written to the src |
| `2ndShroudRenders` | ~270 (≈1/frame) | `W3DShroud::render()` DID target the secondary dst texture |
| `bindPrim / bindSec` | 1745 / 270 | terrain bound secondary ~1/frame, primary ~6-7/frame |
| `SEAT1CAM aimFound` | 1 | the view found player 3's base to aim at |
| `SEAT1CAM aim / cam` | (1362,1966)/(1362,1966) | the camera IS sitting on player 3's base |

Conclusion from facts: **camera is correct, binding of the secondary happens, the secondary
texture is written, and player 3 has vision.** Yet the terrain at (1362,1966) renders black,
which can only mean the *content* the terrain samples for the right view is fully-shrouded.

## 2. The fog data path (as-built)

```
GameLogic (per-frame, incremental)
  PartitionManager cell shroud state per player  ── getShroudStatusForPlayer(idx,x,y)
        │
        ▼   (splitscreen per view, in prepareShroudForView)
PartitionManager::refreshShroudForRenderPlayer()      [Core, my code]
  for each cell: TheDisplay->setShroudLevel(x,y, status-for-render-player)
        │
        ▼
W3DDisplay::setShroudLevel → W3DShroud::setShroudLevel   writes m_srcTextureData (CPU bits of m_pSrcTexture, a SYSTEMMEM surface)
        │
        ▼
W3DShroud::render(cam)                                  [my per-view target switch]
  dstTex = m_useSecondaryDst ? m_pDstTexture2 : m_pDstTexture
  _Copy_DX8_Rects(m_pSrcTexture → dstTex surface)       IDirect3DDevice8::CopyRects (sysmem→vidmem, immediate)
        │
        ▼
Terrain render (HeightMap::renderTerrainPass, in doRender within drawView, override active)
  W3DShroudMaterialPassClass::Install_Materials()
     W3DShaderManager::setTexture(0, shroud->getShroudTexture())   [my live-override pick]
  renderTerrainPass()  ── terrain modulated by the bound shroud texture → screen
```

Render frame order (W3DDisplay::draw):
1. `getShroud()->render(getFirstView camera)` once, m_useSecondaryDst=false → writes dst1
2. `drawViews()` iterates m_viewList = **[seat1View (right), TheTacticalView (left)]** (prepend order)
   - seat1 (right): override=3; prepareShroudForView → setActiveShroudTarget(true), fill src=player3, render→dst2; drawView → terrain binds getShroudTexture()=dst2
   - seat0 (left): override=-1; prepareShroudForView → setActiveShroudTarget(false), fill src=player1, render→dst1; drawView → terrain binds dst1

## 3. Subsystems involved

- **PartitionManager** (Core, GameLogic): authoritative per-player shroud; `getShroudStatusForPlayer`, `refreshShroudForLocalPlayer` (incremental for local), `refreshShroudForRenderPlayer` (my full per-view fill).
- **rts::render-player override** (Core, GameUtility): `setRenderPlayerIndexOverride` set per view by `Display::drawViews`; read by object visibility (works) and by my shroud target/bind selection.
- **W3DDisplay**: `draw()` frame flow; `setShroudLevel`/`clearShroud`(no-op); calls `getShroud()->render` once up front + `prepareShroudForView` per view.
- **W3DShroud**: `m_pSrcTexture` (SYSTEMMEM), `m_pDstTexture`/`m_pDstTexture2` (POOL_DEFAULT vidmem), `render()` (src→dst copy), `getShroudTexture()` (bind), `setShroudLevel` (writes src bits), `fillBorderShroudData`.
- **HeightMap / terrain render**: `renderTerrainPass` + shroud material pass; the visible terrain shroud.
- **W3DScene**: pushes `m_shroudMaterialPass`; object shroud via `getObservedOrLocalPlayerIndex_Safe` (this part demonstrably works per view).

## 4. Candidate bugs (with verification status)

### VERIFIED-OK (ruled out)
- **Camera not on player 3's base** — ruled out: aim=cam=(1362,1966), aimFound=1.
- **Secondary texture never written** — ruled out: 2ndShroudRenders≈270.
- **Copy deferred / shared-src overwrite** — ruled out: CopyRects sysmem→vidmem is immediate.
- **Terrain never binds secondary** — ruled out: bindSec≈270 (≈1/frame).
- **Object visibility path** — works (uses same override, evaluated in scene render).

### NOT-YET-DISPROVEN (live suspects, priority order)
1. **`m_pDstTexture2` content is actually all-shrouded despite the copy.** The CopyRects into a POOL_DEFAULT texture that was **lazily created mid-render** (inside `setActiveShroudTarget` during `render()`) may not behave like the primary (created at init via `ReAcquireResources`). Possible: the mid-frame-created texture's surface isn't a valid CopyRects destination, or its interior is never actually updated. STRONGEST suspect.
2. **The bind returns a valid dst2 pointer, but the terrain samples it with the wrong texture-coordinate transform** for the right view (shroud UV uses `m_drawOriginX`/texture dims computed in `render()`; if the seat1 render() computed these against a stale heightmap draw-window, the UVs could map player 3's base to a shrouded texel). Medium suspect — visStartX/Y are force-set to full map, but `m_drawOriginX` is derived from them and used by the terrain shroud UV.
3. **`getShroudStatusForPlayer(3,...)` returns SHROUDED at the base even though nonLocalClear=3900.** The 3900 revealed cells might not include player 3's base cells if player 3's *look* (vision) hasn't been committed for the render frame. Low suspect (a player always sees its own units) but unverified at the specific base cells.
4. **Two src textures needed.** Even though the copy is immediate, if anything re-reads `m_pSrcTexture` after seat0's fill (e.g., a later terrain pass, water reflection, or the next frame's line-1 render) while sampling dst2, colors could smear. Low suspect.
5. **The right view runs an EXTRA terrain pass (reflection/scorch/bridge) that binds primary and draws over the shroud pass**, leaving the visible result as primary fog. The 7:1 bind ratio hints at multiple terrain passes; need to confirm which pass produces the visible pixels.

## 5. Definitive isolation test (next step)

Add a debug switch that, when the secondary target is active, **fills dst2 with all-CLEAR
(255 / fully lit)** instead of the real per-player data (bypass the src copy; write a solid
clear pattern straight into dst2).

- If the right view becomes **fully lit** → the terrain IS sampling dst2 correctly, so the
  bug is in the **src→dst2 content** (suspect #1: the mid-frame-created texture isn't being
  populated) → fix by allocating dst2 eagerly in `ReAcquireResources` (same as dst1) and/or
  verifying the CopyRects rect/pool.
- If the right view **stays black** → the terrain is NOT sampling dst2's content (suspect #2
  UV transform, or #5 an overdrawing pass) → investigate the terrain shroud UV + pass order.

This single test collapses the remaining suspect list to one branch. Implement behind the
existing splitscreen debug so single-player is untouched.

## 6. Notes / other open items (not fog)
- Skirmish: claim fires (claims=1, lob=1), but the slot shows as an AI difficulty combo; user
  wants it shown as "<player1 name> (2)" — needs a player-row presentation, not an AI combo.
- Per-view scroll not wired (controller can't scroll its viewport).
- Cursor animation hang from the seat-scoped setMouseCursor (cosmetic).
