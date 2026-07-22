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

// TheSuperHackers @feature githubawn 20/07/2026 Forward-declared the same way
// texture.h/textureloader.h/missingtexture.h already do, so this header does
// not have to include d3d8.h just to name the pointer type stored in
// GGCTextureCacheEntry below.
struct IDirect3DTexture8;

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

    // -- Sorted / translucent draw path -----------------------------------------
    //
    // TheSuperHackers @feature githubawn 20/07/2026 SortingRendererClass (particles,
    // explosions, health bars, translucent decals -- all present at low detail) drives
    // these hooks around its depth-sorted draw loop (see sortingrenderer.cpp's
    // Flush_Sorting_Pool/Apply_Render_State and Flush()'s non-pooled branch). Mirrors
    // BgfxBackend's identically-named overrides (BgfxBackend.cpp ~4448-4527); see the .cpp
    // for why Begin/End + Capture_Sorted_Batch_Transforms need real handling here (the
    // engine sets the D3D world/view transform via a direct DX8Wrapper::_Set_DX8_Transform
    // call for this path, bypassing Set_Transform entirely, so m_worldMtx/m_viewMtx would
    // otherwise be stale for every sorted draw).
    virtual void Begin_Sorted_Batch_Pass() override;
    virtual void End_Sorted_Batch_Pass() override;
    virtual void Capture_Sorted_Batch_Transforms(const Matrix4x4 & world,
                                                 const Matrix4x4 & view) override;
    virtual void Capture_Sorted_Batch_Light(const RenderBackendLight & light, bool enabled) override;

    // TheSuperHackers @feature githubawn 20/07/2026 DX8Wrapper::Draw_Sorting_IB_VB's direct-bind
    // sorting-VB/IB path (dx8wrapper.cpp ~2138) -- a separate entry point from the
    // Begin/End_Sorted_Batch_Pass draws above, see the .cpp for the engine-flow analysis of
    // which path is actually live in this build. Mirrors BgfxBackend::Submit_Sorted_Draw
    // (BgfxBackend.cpp ~5655).
    virtual void Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                    const DynamicIBAccessClass & dyn_ib,
                                    unsigned short polygon_count,
                                    unsigned short vertex_count) override;

    // TheSuperHackers @feature githubawn 20/07/2026 Rigid-mesh shared-container sub-range
    // writes (VertexBufferClass::AppendLockClass / IndexBufferClass::AppendLockClass --
    // dx8vertexbuffer.cpp/dx8indexbuffer.cpp). Without these, a shared static VB/IB container
    // that gets a NEW mesh appended into it AFTER this backend already cached+bound it once
    // (see m_staticVBCache/m_staticIBCache below) would keep serving the stale first-bind
    // snapshot forever, silently missing whatever was appended later. See the .cpp for why an
    // in-place partial update (not a full re-capture) is both correct and sufficient here.
    virtual void Capture_Vertex_Sub_Range(const VertexBufferClass * vb,
                                          const void * data,
                                          unsigned int start_vertex,
                                          unsigned int size_bytes) override;
    virtual void Capture_Index_Sub_Range(const IndexBufferClass * ib,
                                         const void * data,
                                         unsigned int start_index,
                                         unsigned int size_bytes) override;

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

    // TheSuperHackers @bugfix githubawn 20/07/2026 This was never overridden, so the
    // per-mesh-part base vertex the engine sets between draws that share one vertex
    // buffer (DX8Wrapper::Set_Index_Buffer_Index_Offset) was silently dropped, and
    // every part after the first resolved its indices against the wrong vertices.
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset) override;

    // TheSuperHackers @feature githubawn 19/07/2026 Destroy-time invalidation
    // for m_staticVBCache/m_staticIBCache below -- see the IRenderBackend.h
    // declaration and the header comment on those maps for why this was
    // missing before.
    virtual void Release_Cached_Vertex_Buffer(const VertexBufferClass * vb) override;
    virtual void Release_Cached_Index_Buffer(const IndexBufferClass * ib) override;

    // -- Basic scene lighting (crude CPU flat-shade, no 3D vertex shader) -----
    //
    // TheSuperHackers @feature githubawn 20/07/2026 None of these three were
    // overridden before, so 3D geometry always drew with a hardcoded opaque-
    // white vertex color regardless of the game's actual lights -- see
    // Draw_Triangles' use of the members below and Compute_Flat_Light_Color's
    // definition in the .cpp for the crude ambient+dominant-light CPU combine
    // this feeds. Mirrors what BgfxBackend::Set_Light_Environment/Set_Material/
    // Set_Ambient (BgfxBackend.cpp) extract from the engine, minus everything
    // that requires a programmable vertex/fragment shader (per-vertex N.L,
    // multiple simultaneous lights, material color-source selection) -- this
    // backend cannot edit vs_2d.v.pica (shared with the 2D UI path), so there
    // is no shader stage available to do real per-vertex lighting math in.
    virtual void Set_Material(const VertexMaterialClass * material) override;
    virtual void Set_Ambient(const Vector3 & color) override;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) override;

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) override;
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) override;
    virtual void Set_World_Identity() override;
    virtual void Set_View_Identity() override;
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar) override;

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;

    // TheSuperHackers @feature githubawn 20/07/2026 Triangle-strip draw path -- only known
    // reachable caller is W3DWater.cpp's water grid (g_renderBackend->Draw_Strip(0,
    // m_numIndices-2, 0, mx*my)), out of scope for this backend per the low-detail rendering
    // restrictions (water/smudge effects excluded). Implemented anyway, mirroring
    // Draw_Triangles' non-sorted 3D/2D setup exactly (see the .cpp for why this duplicates
    // rather than shares that code), so any OTHER strip-based draw that reaches this backend
    // is not silently dropped either.
    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count) override;

private:
    void Ensure_Shader_Loaded();
    C3D_Tex_tag * Ensure_Texture(TextureBaseClass * texture);
    void Apply_Tex_Env(bool texturing_enabled);

    // TheSuperHackers @feature githubawn 20/07/2026 See the Set_Material/
    // Set_Ambient/Set_Light_Environment declarations above -- combines
    // m_sceneAmbient and (if present) m_dominantLightColor into a single
    // flat RGB, pre-multiplied by 255 to match the shader's 1/255 diffuse
    // scale (see vs_2d.v.pica's colscale constant, referenced elsewhere in
    // the .cpp). Reads only members set by those three overrides -- never
    // calls into the engine, per this backend's existing rule that
    // Draw_Triangles (the only caller) must not query engine state directly.
    void Compute_Flat_Light_Color(unsigned char & outR, unsigned char & outG, unsigned char & outB) const;

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

    // TheSuperHackers @feature githubawn 20/07/2026 Alpha test (see
    // ShaderClass::Get_Alpha_Test() in shader.h) is applied immediately via
    // C3D_AlphaTest in Set_Shader, same as the blend/color-mask state above,
    // so it needs no member storage FOR THE GPU'S SAKE -- see
    // m_lastSrcBlendFactor/m_alphaTestEnabled below, though, which mirror it
    // into members anyway, purely so a later diagnostic can report it (they
    // change nothing about what reaches the GPU). Primary gradient and the
    // 3D-path depth/cull state below DO need storage for correctness, not just
    // diagnostics: Apply_Tex_Env and Draw_Triangles read them later, on a
    // separate call from Set_Shader, once per draw. Stored as plain int/bool
    // (not the ShaderClass:: enum types themselves) so this header does not
    // have to include shader.h just to name a nested enum -- ShaderClass is
    // only forward-declared via IRenderBackend.h.
    int m_primaryGradient;    // ShaderClass::PriGradientType, decoded for Apply_Tex_Env
    int m_depthCompare;       // ShaderClass::DepthCompareType, decoded for Draw_Triangles' 3D branch
    bool m_depthWriteEnabled; // ShaderClass::Get_Depth_Mask() == DEPTH_WRITE_ENABLE
    bool m_cullEnabled;       // ShaderClass::Get_Cull_Mode() == CULL_MODE_ENABLE (not yet acted on,
                               // see the kGgc3DCullMode @todo in Citro3dBackend.cpp's Draw_Triangles)

    // TheSuperHackers @feature githubawn 20/07/2026 Diagnostic-only mirror of the blend/alpha-test
    // state Set_Shader already computes and applies immediately via C3D_AlphaBlend/C3D_AlphaTest
    // (see the comment above m_texturingEnabled -- that state needs no member storage for its own
    // sake, since it is consumed the same call). These four exist ONLY so Draw_Triangles' new
    // [ggc-2dstate] log (see its own comment) can report what a given draw's blend/alpha-test
    // state actually was, without re-deriving it from the ShaderClass a second time (Draw_Triangles
    // is deliberately not allowed to query engine/ShaderClass state directly -- see
    // Compute_Flat_Light_Color's comment for the same rule applied to lighting). Stored as plain
    // int/bool rather than the citro3d GPU_BLENDFACTOR enum type, same reasoning as m_primaryGradient
    // above.
    int m_lastSrcBlendFactor;  // GPU_BLENDFACTOR passed to C3D_AlphaBlend's src args
    int m_lastDstBlendFactor;  // GPU_BLENDFACTOR passed to C3D_AlphaBlend's dst args
    bool m_alphaTestEnabled;   // shader.Get_Alpha_Test() != ShaderClass::ALPHATEST_DISABLE
    int m_alphaTestRef;        // 0-255 reference value passed to C3D_AlphaTest

    // TheSuperHackers @feature githubawn 20/07/2026 Lighting state (see
    // Set_Material/Set_Ambient/Set_Light_Environment above and
    // Compute_Flat_Light_Color's use of these in the .cpp). m_sceneAmbient
    // defaults to opaque white (not black/zero) so 3D geometry drawn before
    // the first real Set_Light_Environment call still looks the same as the
    // previous hardcoded-white behavior instead of going black.
    float m_sceneAmbient[3];
    float m_dominantLightColor[3]; // Diffuse color of LightEnvironmentClass's InputLights[0] --
                                    // already sorted "greatest contributor first" by that class
                                    // itself (see lightenvironment.h), so index 0 IS the dominant
                                    // light with no extra comparison needed here.
    bool m_hasDominantLight;       // False until a light_env with Get_Light_Count() > 0 is seen.
    bool m_materialLightingEnabled; // VertexMaterialClass::Get_Lighting() -- false disables even
                                     // this crude combine and falls back to opaque white, same as
                                     // an explicitly unlit D3D material would.

    // Bound texture cache, keyed by the TextureBaseClass the engine's D3D8
    // texture loader already populated (see Ensure_Texture). Cleared per
    // entry from Release_Cached_Texture (TextureBaseClass dtor) so a later
    // allocation at the same address cannot alias a stale C3D_Tex.
    //
    // TheSuperHackers @bugfix githubawn 20/07/2026 A TextureBaseClass* is not
    // a stable proxy for "the pixels this C3D_Tex was uploaded from" the way
    // Ensure_Texture originally assumed: (1) the async texture loader swaps
    // Peek_D3D_Texture()'s underlying IDirect3DTexture8* from a thumbnail to
    // the full image well after the first upload, and (2) some callers
    // rewrite an already-uploaded texture's pixels via LockRect/UnlockRect
    // without routing through Invalidate_Cached_Texture. Both left this cache
    // serving the very first upload forever (stale/black main-menu
    // background, loading-screen image, score-screen image, loading-bar
    // blocks). GGCTextureCacheEntry records what was actually uploaded --
    // the source IDirect3DTexture8* and its content version at upload time
    // (see StubD3D8Device's GGC_GetTextureContentVersion) -- so Ensure_Texture
    // can detect either change and re-upload instead of trusting the cache
    // hit blindly. tex == nullptr is the pre-existing negative-cache case
    // (format unsupported / C3D_TexInit failed); srcD3DTex/uploadedVersion
    // are still tracked for it so a later content/pointer change retries the
    // upload instead of caching the failure forever.
    struct GGCTextureCacheEntry
    {
        C3D_Tex_tag * tex;
        IDirect3DTexture8 * srcD3DTex;
        unsigned uploadedVersion;
    };
    std::map<TextureBaseClass *, GGCTextureCacheEntry> m_textureCache;
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
    // TheSuperHackers @bugfix githubawn 20/07/2026 This is a D3D8 BaseVertexIndex
    // (SetIndices' second argument), i.e. a value implicitly added to EVERY index
    // to pick the vertex: effective_vertex = VB[IB[start + i] + base]. It is NOT
    // an offset into the index array. See its use in Draw_Triangles.
    unsigned int m_indexBaseOffset;

    // -- 3D world draw path (Phase 3 Milestone 4) ------------------------------
    //
    // Static VB/IB cache, keyed by engine pointer like m_textureCache above.
    // Captured once (linearAlloc'd copy of the stub D3D8 buffer's CPU data)
    // and reused every subsequent Set_Vertex_Buffer/Set_Index_Buffer call for
    // the same mesh.
    //
    // TheSuperHackers @feature githubawn 19/07/2026 Now release-hooked via
    // Release_Cached_Vertex_Buffer/Release_Cached_Index_Buffer, called from
    // VertexBufferClass::~VertexBufferClass/IndexBufferClass::~IndexBufferClass
    // (see dx8vertexbuffer.cpp/dx8indexbuffer.cpp), same ABA-prevention
    // reasoning as m_textureCache/Release_Cached_Texture above. Entries are
    // erased there; the freed data is queued on m_pendingFrees rather than
    // linearFree'd immediately since in-flight draws this frame may still
    // reference it (same as the dynamic VB/IB path below).
    struct GGCStaticBufferEntry
    {
        void * data;
        unsigned int sizeBytes;
    };
    std::map<const VertexBufferClass *, GGCStaticBufferEntry> m_staticVBCache;
    std::map<const IndexBufferClass *, GGCStaticBufferEntry> m_staticIBCache;
    const void * m_currentVertexData;
    unsigned int m_currentVertexStride;
    // TheSuperHackers @diagnostic githubawn 20/07/2026 Size of the currently bound static vertex
    // buffer's GPU-visible copy. Tracked so the [ggc-3d] log can show whether the base-vertex
    // offset the GPU fetches from stays inside the allocation.
    unsigned int m_currentVertexSizeBytes;
    const void * m_currentIndexData;
    unsigned int m_currentIndexSizeBytes;

    // World/View/Projection, cached as the engine's own row-major layout
    // (m_xMtx[row*4+col] = m[row][col]) so this header does not need to pull
    // in the full Matrix4x4/Matrix3D definitions -- only Draw_Triangles (in
    // the .cpp) needs to convert these into a citro3d C3D_Mtx.
    float m_worldMtx[16];
    float m_viewMtx[16];
    float m_projMtx[16];

    // -- Sorted / translucent draw path -----------------------------------------
    //
    // TheSuperHackers @feature githubawn 20/07/2026 See Begin_Sorted_Batch_Pass/
    // Capture_Sorted_Batch_Transforms in the .cpp: SortingRendererClass sets the D3D world/view
    // transform via a direct DX8Wrapper::_Set_DX8_Transform call for sorted draws, bypassing
    // Set_Transform (and therefore m_worldMtx/m_viewMtx above) entirely. These are the
    // side-channel copies Draw_Triangles' 3D branch substitutes in instead, only while
    // m_inSortedBatchPass is true -- stored in the same row-major float[16] convention as
    // m_worldMtx/m_viewMtx so GGC_BuildWorldViewProjC3D (Draw_Triangles' .cpp) can consume
    // either pair identically.
    float m_sortWorldMtx[16];
    float m_sortViewMtx[16];
    // True between Begin_Sorted_Batch_Pass and End_Sorted_Batch_Pass (SortingRendererClass's
    // sorted-draw loop, both the pooled Flush_Sorting_Pool path and Flush()'s single-node
    // fallback -- see sortingrenderer.cpp). Gates the world/view substitution above; does NOT by
    // itself say which vertex/index buffer to read (see m_usingDynamicVertexBuffer below --
    // Flush_Sorting_Pool binds its combined geometry through the DYNAMIC Set_Vertex_Buffer
    // overload, while Flush()'s single-node fallback binds an ordinary STATIC mesh VB/IB, and
    // both are wrapped in the same Begin/End pair).
    bool m_inSortedBatchPass;
    // TheSuperHackers @feature githubawn 20/07/2026 True if the most recent Set_Vertex_Buffer
    // call was the DynamicVBAccessClass overload (2D UI draws, and SortingRendererClass::
    // Flush_Sorting_Pool's combined sort buffer), false if it was the static VertexBufferClass*
    // overload (ordinary 3D mesh geometry, including Flush()'s single-node sorted fallback).
    // Set_Vertex_Buffer always runs immediately before the Draw_Triangles/Draw_Strip call it
    // feeds, so this correctly reflects which of m_dynamicVertexData/m_currentVertexData is the
    // live geometry for whichever draw is about to fire. Only consulted by Draw_Triangles'
    // sorted-batch branch (see the .cpp) -- the ordinary is2D/3D branches decide their own buffer
    // source independently (is2D always implies dynamic in practice; the 3D branch is only
    // reached for genuinely static-bound draws).
    bool m_usingDynamicVertexBuffer;
    // TheSuperHackers @feature githubawn 20/07/2026 Captured by Capture_Sorted_Batch_Light
    // (mirrors BgfxBackend::Capture_Sorted_Batch_Light) but NOT currently read by any draw path:
    // SortingRendererClass's sorted geometry is always the 44-byte VertexFormatXYZNDUV2 dynamic
    // buffer (dynamic_fvf_type, dx8vertexbuffer.h), which already carries real per-vertex diffuse
    // -- the same "hasDiffuse" case Draw_Triangles' ordinary 3D branch already leaves as
    // buffer-color-only with no flat-light combine (see that branch's own comment). Stored
    // separately from m_dominantLightColor/m_hasDominantLight (rather than overwriting them) so a
    // sorted batch's light can never leak into and corrupt the ordinary opaque-3D flat-light
    // combine for draws that follow it in the same frame, in case a future change does start
    // consuming these.
    bool m_sortHasDominantLight;
    float m_sortDominantLightColor[3];

    // TheSuperHackers @feature githubawn 20/07/2026 DXT decode has landed (see
    // GGC_DecodeDxtToPica/Citro3dDxtDecode.h and Ensure_Texture's now-accepted DXT1-5
    // format checks) -- flipped back on now that real UVs are also loaded for 3D
    // draws (see Draw_Triangles' non-44-byte-stride branch below). Was temporarily
    // false per user direction (land 3D geometry visibility first, view a match
    // untextured while DXT decode was still unported) -- see docs/3ds-port-plan.md.
    static const bool kGgc3DTexturingEnabled = true;

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
