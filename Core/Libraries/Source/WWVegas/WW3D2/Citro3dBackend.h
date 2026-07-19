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

    // TheSuperHackers @bugfix githubawn 18/07/2026 Phase 3 Milestone 4.1: the
    // general-heap OOM chased for most of this session turned out to be a
    // load-vs-draw timing gap, not a real memory shortfall. Ensure_Texture's
    // GPU-upload-then-release-CPU-scratch only ran from Set_Texture, i.e. at
    // DRAW time -- but match load places all ~663 map objects (and their
    // textures/meshes) before the world is ever actually drawn a single
    // frame, so every one of them sat with its full CPU-side decode buffer
    // retained for the whole load. texture.cpp already calls this exact
    // hook the moment a texture finishes LOADING (TextureBaseClass::
    // Set_D3D_Base_Texture), completely decoupled from whether/when it's
    // ever drawn -- DX8Backend's default no-ops it. Overriding it here
    // triggers the same upload+release Ensure_Texture already does, just at
    // the correct (load, not draw) time.
    virtual RenderResource Register_Loaded_Texture(TextureBaseClass * texture) override;

    // TheSuperHackers @feature githubawn 18/07/2026 Redirects subsequent 2D
    // UI draws (Render2DClass -- the same path menus/HUD/radar/text already
    // use) to the top screen's citro3d render target instead of the default
    // bottom one. Purely a target switch: the 2D shader/orthographic
    // transform is unchanged, so anything drawn while this is active still
    // uses whatever logical coordinate space the caller set up (see
    // GGC_DrawTopScreenOverlay in W3DDisplay.cpp for the actual usage and
    // its aspect-ratio caveat). Always leave this false when done -- normal
    // per-frame UI drawing assumes the bottom target.
    virtual void Set_Top_Screen_Active(bool active) override;

    virtual void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                             const void * data,
                                             unsigned int size_bytes) override;
    virtual void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                            const void * data,
                                            unsigned int size_bytes) override;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) override;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) override;

    // -- 3D world draw path (Phase 3 Milestone 4) ------------------------------
    //
    // Static (mesh) vertex/index buffers, as opposed to the per-frame
    // Dynamic* path above used only by Render2DClass. Captured lazily on
    // first bind (same "lock the stub D3D8 buffer, copy into a GPU-visible
    // allocation, cache by engine pointer" pattern BgfxBackend already uses
    // for the same problem) since these are created once per unique mesh
    // and drawn every frame thereafter, unlike the 2D path's per-frame data.

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream) override;
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) override;

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) override;
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) override;
    virtual void Set_World_Identity() override;
    virtual void Set_View_Identity() override;
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar) override;

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

    // -- 3D world draw path (Phase 3 Milestone 4) ------------------------------
    //
    // Static VB/IB cache, keyed by engine pointer like m_textureCache above.
    // Captured once (linearAlloc'd copy of the stub D3D8 buffer's CPU data)
    // and reused every subsequent Set_Vertex_Buffer/Set_Index_Buffer call for
    // the same mesh. Not release-hooked yet (VertexBufferClass/IndexBufferClass
    // have no Release_Cached_* callback into IRenderBackend the way textures
    // do) -- acceptable for now because these are deduplicated per unique
    // mesh sub-object by WW3DAssetManager's prototype cache, not per Object
    // instance, so this is bounded by asset diversity, not live object count.
    struct GGCStaticBufferEntry
    {
        void * data;
        unsigned int sizeBytes;
    };
    std::map<const VertexBufferClass *, GGCStaticBufferEntry> m_staticVBCache;
    std::map<const IndexBufferClass *, GGCStaticBufferEntry> m_staticIBCache;
    const void * m_currentVertexData;
    unsigned int m_currentVertexStride;
    const void * m_currentIndexData;
    unsigned int m_currentIndexSizeBytes;

    // World/View/Projection, cached as the engine's own row-major layout
    // (m_xMtx[row*4+col] = m[row][col]) so this header does not need to pull
    // in the full Matrix4x4/Matrix3D definitions -- only Draw_Triangles (in
    // the .cpp) needs to convert these into a citro3d C3D_Mtx.
    float m_worldMtx[16];
    float m_viewMtx[16];
    float m_projMtx[16];

    // TheSuperHackers @todo githubawn 17/07/2026 Per user direction: land the
    // 3D draw path first so world geometry is visible at all, then disable
    // texture sampling for 3D draws specifically (2D UI/fonts are unaffected)
    // so a match can be viewed untextured while DXT decode (see Ensure_Texture)
    // is still unported -- see docs/3ds-port-plan.md.
    static const bool kGgc3DTexturingEnabled = false;

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
