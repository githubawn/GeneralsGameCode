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

// TheSuperHackers @build githubawn 10/07/2026 PS2 (Emotion Engine / Graphics
// Synthesizer) render backend. See docs/ps2-port-plan.md, Tier 0.
//
// Inherits DX8Backend for the same reason BgfxBackend does (see
// BgfxBackend.h): DX8Wrapper's render_state tracking is still read by the
// sorting renderer and other subsystems that have not been migrated to
// route exclusively through IRenderBackend. render-backend.cmake forces
// GGC_BGFX_STANDALONE=ON whenever GGC_RENDER_BACKEND=ps2, so DX8Wrapper
// initializes a StubD3D8Device instead of a real D3D8 device -- there is
// no D3D8 reference popup on PS2, same as the bgfx-standalone builds on
// Android/Linux/macOS/iOS.
//
// TheSuperHackers @build githubawn 11/07/2026 Tier 0 rendering bring-up:
// real gsKit initialization + Clear() so the GS actually displays
// something, proving the hardware output pipeline works before any real
// geometry submission exists. Draw_Triangles/Set_Vertex_Buffer/etc. are
// still the inherited DX8Backend no-ops -- nothing is drawn yet, only
// cleared. See PS2Backend.cpp for the gsKit call sequence (based on
// gsKit's own examples/basic/basic.c).
//
// TheSuperHackers @build githubawn 12/07/2026 Tier 1: minimal fixed-function
// software T&L + untextured flat/gouraud-shaded triangle submission. This
// engine only ever uses D3D8's classic fixed-function pipeline (see
// dx8wrapper.cpp's single SetVertexShader(fvf) call, which selects FVF mode,
// not a compiled shader) so a small state machine mirroring World/View/
// Projection + FVF-driven vertex decode is enough -- no shader compiler
// needed, unlike bgfx. Capture_Vertex_Data/Capture_Index_Data are generic
// IRenderBackend hooks (see dx8vertexbuffer.cpp/dx8indexbuffer.cpp) already
// called for every backend, so real geometry bytes arrive here for free.
// Texture sampling is explicitly out of scope for this pass -- see
// docs/ps2-port-plan.md Tier 1 for the follow-up.

#pragma once

#include "DX8Backend.h"
#include "matrix4.h"

#include <gsKit.h>

#include <vector>
#include <unordered_map>

struct gsGlobal;

class PS2Backend : public DX8Backend
{
public:
    PS2Backend();
    virtual ~PS2Backend();

    virtual void Initialize(void * hwnd, int width, int height) override;
    virtual void Shutdown() override;

    virtual void Begin_Scene() override;
    virtual void End_Scene(bool flip_frame) override;
    virtual void Flip_To_Primary() override;
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil) override;

    virtual void Capture_Vertex_Data(const VertexBufferClass * vb,
                                     const void * data, unsigned int size_bytes) override;
    virtual void Capture_Index_Data(const IndexBufferClass * ib,
                                    const void * data, unsigned int size_bytes) override;
    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream) override;
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) override;

    // TheSuperHackers @build githubawn 12/07/2026 Instrumentation confirmed
    // the shell/menu screen submits essentially 100% of its geometry through
    // this dynamic-buffer path (DynamicVBAccessClass/DynamicIBAccessClass),
    // not the static VertexBufferClass path -- so this is the one that
    // actually matters for getting anything visible on screen.
    virtual void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                             const void * data, unsigned int size_bytes) override;
    virtual void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                            const void * data, unsigned int size_bytes) override;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) override;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) override;
    virtual void Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                    const DynamicIBAccessClass & dyn_ib,
                                    unsigned short polygon_count,
                                    unsigned short vertex_count) override;

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) override;
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) override;
    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;

    // TheSuperHackers @build githubawn 12/07/2026 Texture support. Textures
    // loaded from files go through Register_Loaded_Texture (not
    // Create_Texture -- that hook is for explicitly-created buffers/render
    // targets). Under GGC_BGFX_STANDALONE, DX8Wrapper's StubD3D8Device still
    // stores genuine pixel data (the legacy TGA/DDS loader writes real bytes
    // via LockRect/UnlockRect during load) even though nothing is GPU-backed
    // -- BgfxBackend's EnsureBgfxTexture proves this pattern already works;
    // PS2Backend mirrors it (Peek_D3D_Texture -> LockRect -> convert to
    // RGBA8 -> gsKit GSTEXTURE) instead of uploading to bgfx.
    virtual RenderResource Register_Loaded_Texture(TextureBaseClass * tex) override;
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture) override;

private:
    struct CapturedVertexBuffer
    {
        std::vector<unsigned char> bytes;
        unsigned fvf;
        unsigned stride;
    };
    struct CapturedIndexBuffer
    {
        std::vector<unsigned short> indices;
    };
    struct CapturedTexture
    {
        std::vector<unsigned char> rgba8; // width*height*4, R,G,B,A byte order
        GSTEXTURE gsTex;
        bool valid;
        // TheSuperHackers @bugfix githubawn 13/07/2026 LRU eviction bookkeeping
        // (see EnsurePS2Texture) -- vramBytes is the GS VRAM footprint this
        // entry actually holds (width*height*4 for GS_PSM_CT32), lastUsedFrame
        // is stamped on every bind (hit or miss) so eviction can pick the
        // truly-stalest entry rather than just insertion order.
        unsigned vramBytes;
        unsigned lastUsedFrame;
    };

    // Returns nullptr if the texture couldn't be captured (unsupported
    // format, no D3D mirror yet, etc.) -- callers fall back to untextured.
    GSTEXTURE * EnsurePS2Texture(TextureBaseClass * tex);

    void DrawCapturedTriangle(const unsigned char * v0,
                              const unsigned char * v1,
                              const unsigned char * v2,
                              unsigned fvf,
                              unsigned locationOffset,
                              unsigned diffuseOffset,
                              unsigned texOffset,
                              bool hasTexCoord);

    // TheSuperHackers @bugfix githubawn 13/07/2026 Flush (gsKit_queue_exec)
    // the current oneshot drawbuffer if it has less than a safety threshold
    // of space left, so a single heavy frame can never overrun the fixed
    // 1MB/buffer pool (which corrupts heap memory -- see Initialize()). Safe
    // to call between primitives (never mid-packet); the mid-frame kick draws
    // to the same back framebuffer, only End_Scene's sync_flip presents it.
    void MaybeFlushOneshot();

    struct gsGlobal * m_gsGlobal;

    std::unordered_map<const VertexBufferClass *, CapturedVertexBuffer> m_vbCache;
    std::unordered_map<const IndexBufferClass *, CapturedIndexBuffer> m_ibCache;
    std::unordered_map<const DynamicVBAccessClass *, CapturedVertexBuffer> m_dynVbCache;
    std::unordered_map<const DynamicIBAccessClass *, CapturedIndexBuffer> m_dynIbCache;

    enum BindMode { BIND_NONE, BIND_STATIC, BIND_DYNAMIC };
    BindMode m_bindMode;

    const VertexBufferClass * m_boundVB;
    const IndexBufferClass * m_boundIB;
    const DynamicVBAccessClass * m_boundDynVB;
    const DynamicIBAccessClass * m_boundDynIB;
    unsigned short m_indexBaseOffset;

    std::unordered_map<const TextureBaseClass *, CapturedTexture> m_textureCache;
    TextureBaseClass * m_boundTexture;

    // TheSuperHackers @bugfix githubawn 13/07/2026 gsKit_TexManager_bind's
    // own internal VRAM allocator hangs outright once cumulative bound-
    // texture VRAM exceeds the GS's real 4MB budget within a single frame
    // (gsKit_TexManager_nextFrame(), our only eviction trigger, runs once
    // per Begin_Scene -- too coarse when one frame alone needs to bind more
    // distinct textures than fit). Proactively evict the least-recently-used
    // entries ourselves (via gsKit_TexManager_free) before we ever hand
    // gsKit_TexManager_bind a texture that would push us over budget, so its
    // buggy/absent mid-frame eviction path is never exercised. See
    // EnsurePS2Texture.
    unsigned m_textureCacheVramBytes;
    unsigned m_currentFrameIndex;

    // TheSuperHackers @build githubawn 13/07/2026 3D world draws (terrain,
    // units) are the ones most likely to blow the memory/VRAM budget on
    // real PS2 hardware, and are the least essential visually compared to
    // 2D UI (menu buttons/text must stay legible; a flat-shaded unit is
    // still readable). Toggle so 2D UI keeps real textures while 3D world
    // draws fall back to untextured flat/gouraud shading -- see
    // DrawCapturedTriangle. Investigation ongoing into the actual memory
    // cost split (docs/ps2-port-plan.md); default false (3D textures off)
    // pending that data.
    bool m_enable3DTextures;

    Matrix4x4 m_world;
    Matrix4x4 m_view;
    Matrix4x4 m_projection;
};
