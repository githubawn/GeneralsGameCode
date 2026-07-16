/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// TheSuperHackers @feature githubawn 14/07/2026 Citro3dBackend — New
// Nintendo 3DS PICA200 backend. See docs/3ds-port-plan.md.
//
// Like BgfxBackend, this inherits from DX8Backend rather than IRenderBackend
// directly: DX8Wrapper keeps programming its render_state exactly as in the
// reference path, driving a no-op stub IDirect3DDevice8 (StubD3D8Device,
// active under GGC_BGFX_STANDALONE, which citro3d also builds with — see
// cmake/render-backend.cmake). Every IRenderBackend method this class does
// NOT override falls through to DX8Backend's forward-to-stub behavior, i.e.
// compiles and runs but draws nothing.
//
// TheSuperHackers @feature githubawn 15/07/2026 Phase 3 Milestone 2: 2D UI
// quads (Render2DClass — the game's menu/HUD/font-glyph draw path, see
// Core/Libraries/Source/WWVegas/WW3D2/render2d.cpp). Only the 2D path is
// implemented: draws are only actually submitted to the GPU when both world
// and view transforms are identity, which is how Render2DClass always draws
// (and is also how BgfxBackend itself infers "this is a UI draw" — see the
// is2D check in BgfxBackend::SubmitEngineDraw). Real 3D world geometry
// (terrain, units — Milestones 4-5) is not translated yet and is silently
// dropped, same as the pre-Milestone-2 stub behavior.
//
// This header MUST NOT be included from any VC6 translation unit, same
// restriction as BgfxBackend.h — GGC_RENDER_BACKEND=citro3d requires the
// devkitARM cross toolchain (see cmake/toolchains/nintendo-3ds.cmake), never
// VC6.

#pragma once

#include "DX8Backend.h"

#include <map>
#include <vector>

// citro3d's C3D_RenderTarget is an opaque struct pointer; forward-declare
// under its C typedef name so this header does not have to include
// citro3d.h (kept consistent with IRenderBackend.h's "forward declare, no
// heavy includes" convention).
struct C3D_RenderTarget_tag;
struct C3D_Tex_tag;
struct C3D_Mtx_tag;
struct shaderProgram_s_tag;
struct DVLB_s_tag;

class Citro3dBackend : public DX8Backend
{
public:
    Citro3dBackend();
    virtual ~Citro3dBackend();

    // -- Backend lifecycle ----------------------------------------------------
    //
    // Initialize calls gfxInitDefault + C3D_Init and creates the top/bottom
    // screen render targets. Shutdown tears them down before C3D_Fini.

    virtual void Initialize(void * hwnd, int width, int height) override;
    virtual void Shutdown() override;

    // -- Frame lifecycle ------------------------------------------------------
    //
    // Begin_Scene starts the citro3d frame and binds the top screen target.
    // Clear clears both screen targets to the requested color. End_Scene
    // ends the citro3d frame (submits the GPU command list + swaps).

    virtual void Begin_Scene() override;
    virtual void End_Scene(bool flip_frame) override;
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0) override;

    // -- 2D UI draw path (Phase 3 Milestone 2) ---------------------------------

    virtual void Set_Shader(const ShaderClass & shader) override;
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture) override;
    virtual void Release_Cached_Texture(TextureBaseClass * texture) override;
    virtual void Invalidate_Cached_Texture(TextureBaseClass * texture) override;

    virtual void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                             const void * data,
                                             unsigned int size_bytes) override;
    virtual void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                            const void * data,
                                            unsigned int size_bytes) override;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) override;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) override;

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;

private:
    void Ensure_Shader_Loaded();
    C3D_Tex_tag * Ensure_Texture(TextureBaseClass * texture);
    void Apply_Tex_Env(bool texturing_enabled);

    C3D_RenderTarget_tag * m_topTarget;
    C3D_RenderTarget_tag * m_bottomTarget;
    bool m_initialized;

    // Shader program (loaded once, Phase 3 Milestone 2's single 2D-UI vertex
    // shader — see shaders_3ds/vs_2d.v.pica).
    DVLB_s_tag * m_vshDvlb;
    shaderProgram_s_tag * m_program;
    int m_uniformTransform;
    bool m_shaderLoaded;

    // Current ShaderClass-derived draw state (Set_Shader), applied
    // immediately to citro3d's global GPU state (unlike DX8, citro3d has no
    // deferred "Apply_Render_State_Changes" step to hook).
    bool m_texturingEnabled;

    // Bound texture cache, keyed by the TextureBaseClass the engine's D3D8
    // texture loader already populated (see Ensure_Texture). Cleared per
    // entry from Release_Cached_Texture (TextureBaseClass dtor) so a later
    // allocation at the same address cannot alias a stale C3D_Tex.
    std::map<TextureBaseClass *, C3D_Tex_tag *> m_textureCache;
    C3D_Tex_tag * m_currentTexture;

    // Captured dynamic vertex/index data (Capture_Dynamic_* hooks), copied
    // into a linearAlloc'd (GPU-DMA-accessible) buffer each time the engine
    // unlocks a DynamicVBAccessClass/DynamicIBAccessClass write. A single
    // menu frame issues many draws (one Render2DClass::Render() call per UI
    // element), each with its own capture -- see Begin_Scene/m_pendingFrees
    // in the .cpp for why the old buffer cannot be freed immediately when a
    // new one is captured.
    void * m_dynamicVertexData;
    unsigned int m_dynamicVertexSizeBytes;
    void * m_dynamicIndexData;
    unsigned int m_dynamicIndexSizeBytes;
    unsigned short m_indexBaseOffset;

    // TheSuperHackers @bugfix githubawn 16/07/2026 C3D_DrawElements does not
    // copy vertex/index data -- it queues a GPU command referencing the
    // pointer, which the GPU only actually reads at frame-end
    // (C3D_FrameEnd). Freeing a captured buffer as soon as the NEXT
    // Capture_Dynamic_* call happens (the original design) frees memory
    // still referenced by earlier-in-the-same-frame draw commands that
    // haven't executed yet, corrupting/blanking every UI element except
    // whichever one happened to be captured last. Buffers freed during a
    // frame go here instead and are only actually linearFree'd at the next
    // Begin_Scene, by which point C3D_FrameBegin(C3D_FRAME_SYNCDRAW) has
    // guaranteed the previous frame's GPU work is complete.
    std::vector<void *> m_pendingFrees;
};
