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

// TheSuperHackers @feature githubawn 14/07/2026 Citro3dBackend.
// docs/3ds-port-plan.md Phase 3 Milestone 1: clear screen at 400x240 on
// real PICA200 hardware/emulator. See Citro3dBackend.h for the
// DX8Backend-inheritance rationale (same pattern as BgfxBackend).
//
// TheSuperHackers @feature githubawn 15/07/2026 Phase 3 Milestone 2: 2D UI
// quad rendering (Render2DClass — menus, HUD, font glyphs). See
// Citro3dBackend.h for the identity-world/view 2D-detection rationale.

#include "Citro3dBackend.h"

#include "vector3.h"
#include "matrix3d.h"
#include "matrix4.h"
#include "shader.h"
#include "texture.h"
#include "StubD3D8Device.h"
// TheSuperHackers @feature githubawn 17/07/2026 Phase 3 Milestone 4: 3D world
// geometry. dx8vertexbuffer.h/dx8indexbuffer.h are per-game headers (see
// GeneralsMD/Code/.../dx8vertexbuffer.h, duplicated for Generals/Code) --
// this Core-level .cpp is compiled once per game target with that game's
// include dirs already on the search path, same as BgfxBackend.cpp's
// identical unqualified include of these two headers.
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "dx8wrapper.h"
#include "ww3d.h"
// TheSuperHackers @feature githubawn 20/07/2026 DXT/BC1-BC3 CPU decode (see
// Ensure_Texture below). DXTUtils.h is the same shared block-pitch math
// StubD3D8Device.cpp and BgfxBackendTextures.cpp already use; Citro3dDxtDecode.h
// is a new, citro3d-independent header living alongside this file (see its
// own top comment for why it is not itself restricted to the 3DS build).
#include "DXTUtils.h"
#include "Citro3dDxtDecode.h"

// TheSuperHackers @build githubawn 14/07/2026 The precompiled header pulls in
// win32_shims/winerror.h, which #defines ERROR_SUCCESS as a Win32 status
// code (0L). libctru's <3ds/applets/error.h> uses ERROR_SUCCESS as an actual
// enum identifier, so the stray macro corrupts that enum. This file is
// 3DS-only (compiled only under GGC_RENDER_BACKEND=citro3d), so it is safe
// to drop the Win32-compat macro before pulling in the real 3DS headers.
#ifdef ERROR_SUCCESS
#undef ERROR_SUCCESS
#endif

#include <3ds.h>
#include <citro3d.h>
#include <cstring>
#include <SDL3/SDL.h>

#include "vs_2d_shbin.h"

// TheSuperHackers @build githubawn 14/07/2026 Standard devkitPro 3DS
// transfer-engine flags for GPU-render-target -> LCD framebuffer copy.
// Verbatim from devkitPro's own citro3d examples
// ($DEVKITPRO/examples/3ds/graphics/gpu/both_screens); not engine-specific.
#define GGC_3DS_DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// -----------------------------------------------------------------------------
// PICA200 8x8-tile Morton (Z-order) swizzle
// -----------------------------------------------------------------------------
//
// TheSuperHackers @feature githubawn 15/07/2026 PICA200 textures are not
// stored linearly: memory is organized in 8x8-pixel tiles, and within each
// tile pixels are in Z-order. This is the standard, widely-documented 3DS
// homebrew swizzle (same algorithm citro3d's own offline tex3ds tool
// applies) — needed here because the engine's textures are decoded from
// .tga/.dds at runtime, not preprocessed offline.
namespace
{
    inline unsigned GGC_MortonInterleave(unsigned x, unsigned y)
    {
        static const unsigned char xlut[8] = { 0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15 };
        static const unsigned char ylut[8] = { 0x00, 0x02, 0x08, 0x0A, 0x20, 0x22, 0x28, 0x2A };
        return xlut[x & 7] | ylut[y & 7];
    }

    // Converts one A8R8G8B8/X8R8G8B8 (D3D BGRA-in-memory) source image into a
    // PICA-tiled GPU_RGBA8 (ABGR-in-memory) destination buffer, flipping V
    // (PICA/GL texture origin is bottom-left; the engine's source images are
    // top-left origin like every other backend already handles internally).
    void GGC_SwizzleBGRA8ToPicaRGBA8(const unsigned char * src, unsigned srcPitch,
                                     unsigned char * dst, unsigned width, unsigned height)
    {
        const unsigned tilesPerRow = (width + 7) / 8;
        for (unsigned y = 0; y < height; ++y)
        {
            const unsigned srcY = height - 1 - y; // V-flip
            const unsigned char * srcRow = src + srcY * srcPitch;
            for (unsigned x = 0; x < width; ++x)
            {
                const unsigned char * p = srcRow + x * 4; // B,G,R,A
                const unsigned tileX = x / 8, tileY = y / 8;
                const unsigned dstIndex = (tileY * tilesPerRow + tileX) * 64 + GGC_MortonInterleave(x, y);
                unsigned char * d = dst + dstIndex * 4;
                // PICA GPU_RGBA8 in-memory byte order is A,B,G,R.
                d[0] = p[3]; // A
                d[1] = p[0]; // B
                d[2] = p[1]; // G
                d[3] = p[2]; // R
            }
        }
    }

    // TheSuperHackers @feature githubawn 16/07/2026 The engine's UI TGA
    // loader (mainmenubackdrop/titlescreen/etc.) and the runtime font glyph
    // atlas both come through as 16-bit formats -- WW3D_FORMAT_R5G6B5 (no
    // alpha, the visible "2 white boxes" backdrop textures), WW3D_FORMAT_
    // A4R4G4B4 (the font atlas, needs alpha for glyph edges), and
    // WW3D_FORMAT_A1R5G5B5 (1-bit alpha, e.g. the main menu logo backdrop)
    // -- not the 32bpp formats the first pass of this backend handled.
    // These pack straight into the matching 2-byte-per-pixel PICA200 format
    // (GPU_RGB565/GPU_RGBA4/GPU_RGBA5551) instead of expanding to 4-byte
    // GPU_RGBA8, cutting GPU/linear-heap texture memory in half for these
    // formats with no quality loss (source and destination have identical
    // bit depth per channel --
    // this is a repack, not a compression). Bit layout confirmed against
    // 3dbrew's GPU/External_Registers ("most significant bits used first,
    // little-endian" + explicit GL_RGB565_OES/GL_RGB5_A1_OES/GL_RGBA4_OES
    // format cross-references) and cross-checked against the standard
    // GL_UNSIGNED_SHORT_5_6_5 / _5_5_5_1 / _4_4_4_4 packed-channel-order
    // convention those GLES type names imply; a native 16-bit store on this
    // little-endian ARM target then produces the documented in-memory byte
    // order automatically (same principle the existing RGBA8 path uses at
    // byte granularity: PICA's "reverse byte order, MSB first" description
    // is just little-endian storage of a value with the channel named first
    // in the highest bits, empirically confirmed for RGBA8 as A,B,G,R in
    // memory -- i.e. R is bits 31:24 of the logical value, A is bits 7:0).
    void GGC_SwizzleR5G6B5ToPicaRGB565(const unsigned char * src, unsigned srcPitch,
                                        unsigned char * dst, unsigned width, unsigned height)
    {
        const unsigned tilesPerRow = (width + 7) / 8;
        for (unsigned y = 0; y < height; ++y)
        {
            const unsigned srcY = height - 1 - y; // V-flip
            const unsigned short * srcRow =
                reinterpret_cast<const unsigned short *>(src + srcY * srcPitch);
            for (unsigned x = 0; x < width; ++x)
            {
                // Source and destination are both R5G6B5 -- straight repack,
                // no per-channel bit-replication needed.
                const unsigned short p = srcRow[x];
                const unsigned tileX = x / 8, tileY = y / 8;
                const unsigned dstIndex = (tileY * tilesPerRow + tileX) * 64 + GGC_MortonInterleave(x, y);
                reinterpret_cast<unsigned short *>(dst)[dstIndex] = p;
            }
        }
    }

    void GGC_SwizzleA4R4G4B4ToPicaRGBA4(const unsigned char * src, unsigned srcPitch,
                                        unsigned char * dst, unsigned width, unsigned height)
    {
        const unsigned tilesPerRow = (width + 7) / 8;
        for (unsigned y = 0; y < height; ++y)
        {
            const unsigned srcY = height - 1 - y; // V-flip
            const unsigned short * srcRow =
                reinterpret_cast<const unsigned short *>(src + srcY * srcPitch);
            for (unsigned x = 0; x < width; ++x)
            {
                const unsigned short p = srcRow[x];
                const unsigned short a4 = (p >> 12) & 0x0f;
                const unsigned short r4 = (p >> 8) & 0x0f;
                const unsigned short g4 = (p >> 4) & 0x0f;
                const unsigned short b4 = p & 0x0f;
                // GPU_RGBA4 logical value: R in bits 15:12 ... A in bits 3:0.
                const unsigned short packed =
                    static_cast<unsigned short>((r4 << 12) | (g4 << 8) | (b4 << 4) | a4);

                const unsigned tileX = x / 8, tileY = y / 8;
                const unsigned dstIndex = (tileY * tilesPerRow + tileX) * 64 + GGC_MortonInterleave(x, y);
                reinterpret_cast<unsigned short *>(dst)[dstIndex] = packed;
            }
        }
    }

    void GGC_SwizzleA1R5G5B5ToPicaRGBA5551(const unsigned char * src, unsigned srcPitch,
                                           unsigned char * dst, unsigned width, unsigned height)
    {
        const unsigned tilesPerRow = (width + 7) / 8;
        for (unsigned y = 0; y < height; ++y)
        {
            const unsigned srcY = height - 1 - y; // V-flip
            const unsigned short * srcRow =
                reinterpret_cast<const unsigned short *>(src + srcY * srcPitch);
            for (unsigned x = 0; x < width; ++x)
            {
                const unsigned short p = srcRow[x];
                const unsigned short a1 = (p >> 15) & 0x01;
                const unsigned short r5 = (p >> 10) & 0x1f;
                const unsigned short g5 = (p >> 5) & 0x1f;
                const unsigned short b5 = p & 0x1f;
                // GPU_RGBA5551 logical value: R in bits 15:11 ... A in bit 0.
                const unsigned short packed =
                    static_cast<unsigned short>((r5 << 11) | (g5 << 6) | (b5 << 1) | a1);

                const unsigned tileX = x / 8, tileY = y / 8;
                const unsigned dstIndex = (tileY * tilesPerRow + tileX) * 64 + GGC_MortonInterleave(x, y);
                reinterpret_cast<unsigned short *>(dst)[dstIndex] = packed;
            }
        }
    }

    // TheSuperHackers @feature githubawn 20/07/2026 DXT/BC1-BC3 decode (see
    // Ensure_Texture). Unlike the four Swizzle* functions above -- which
    // repack an already-16-bit-or-32-bit source straight into the matching
    // PICA format -- a block-compressed source has no 1:1 texel layout at
    // all until GGCDxt::DecodeBC1ColorBlock/DecodeBC2AlphaBlock/
    // DecodeBC3AlphaBlock (Citro3dDxtDecode.h) decode each 4x4 block. These
    // three small pack helpers take that decoder's 8-bit-per-channel RGBA8
    // output and quantize it down to the SAME 2-byte PICA formats the 16-bit
    // source formats above already upload as -- bit layouts copied verbatim
    // from the GPU_RGB565/GPU_RGBA4/GPU_RGBA5551 comments on those functions,
    // just built from 8-bit channels (>>3/>>2/>>4) instead of repacked
    // straight from an already-5/6/5-or-4/4/4/4 source.
    inline unsigned short GGC_PackRGB565FromRGBA8(unsigned char r, unsigned char g, unsigned char b)
    {
        const unsigned r5 = r >> 3, g6 = g >> 2, b5 = b >> 3;
        return static_cast<unsigned short>((r5 << 11) | (g6 << 5) | b5);
    }

    inline unsigned short GGC_PackRGBA4FromRGBA8(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        const unsigned r4 = r >> 4, g4 = g >> 4, b4 = b >> 4, a4 = a >> 4;
        return static_cast<unsigned short>((r4 << 12) | (g4 << 8) | (b4 << 4) | a4);
    }

    inline unsigned short GGC_PackRGBA5551FromRGBA8(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        // TheSuperHackers @info githubawn 20/07/2026 1-bit alpha threshold at
        // the midpoint (128) -- BC1's own punchthrough alpha is already
        // binary (0 or 255, see DecodeBC1ColorBlock), so this only matters
        // for the >=128 vs <128 boundary case, which never actually occurs
        // for DXT1 source data. Written as a threshold rather than a plain
        // shift so it still degrades sanely if this helper is ever reused
        // for a source with genuinely graduated alpha.
        const unsigned r5 = r >> 3, g5 = g >> 3, b5 = b >> 3, a1 = (a >= 128) ? 1u : 0u;
        return static_cast<unsigned short>((r5 << 11) | (g5 << 6) | (b5 << 1) | a1);
    }

    // TheSuperHackers @feature githubawn 20/07/2026 Decodes an entire DXT1-5
    // (BC1-BC3) compressed source image directly into a PICA-tiled 16-bit-
    // per-texel destination -- no full-image RGBA8 intermediate is ever
    // allocated (see Ensure_Texture's memory-budget comment for why that
    // matters on this hardware). Each 4x4 block is decoded once into a tiny
    // 16-texel stack buffer, then every texel is immediately scattered to its
    // final tiled destination slot -- the same per-texel Morton/tile math and
    // bottom-left-origin V-flip the four Swizzle* functions above use, just
    // driven per-block (4x4) instead of per-source-row.
    //
    // outFormat must be one of GPU_RGB565 (DXT1, no block used punchthrough
    // alpha), GPU_RGBA5551 (DXT1, at least one block did), or GPU_RGBA4
    // (DXT2-5, real multi-bit alpha) -- see the gpuFormat selection in
    // Ensure_Texture, which is responsible for choosing the right one BEFORE
    // calling this (this function trusts its caller and does not re-derive
    // the DXT1 punchthrough decision itself).
    void GGC_DecodeDxtToPica(const unsigned char * src, unsigned srcPitch, unsigned char * dst,
                             unsigned width, unsigned height, WW3DFormat fmt, GPU_TEXCOLOR outFormat)
    {
        const unsigned blockBytes = (fmt == WW3D_FORMAT_DXT1) ? 8u : 16u;
        const unsigned numBlocksX = (width + 3) / 4;
        const unsigned numBlocksY = DXT_SurfaceRows(height);
        const unsigned tilesPerRow = (width + 7) / 8;

        for (unsigned by = 0; by < numBlocksY; ++by)
        {
            const unsigned char * blockRow = src + by * srcPitch;
            for (unsigned bx = 0; bx < numBlocksX; ++bx)
            {
                const unsigned char * block = blockRow + bx * blockBytes;

                unsigned char r[16], g[16], b[16], a[16];
                if (fmt == WW3D_FORMAT_DXT1)
                {
                    GGCDxt::DecodeBC1ColorBlock(block, false, r, g, b, a);
                }
                else
                {
                    // TheSuperHackers @info githubawn 20/07/2026 Block layout for BC2/BC3
                    // (DXT2/3/4/5) is alpha-first: bytes 0-7 are the explicit/interpolated
                    // alpha block, bytes 8-15 are a BC1-layout color block (always decoded
                    // 4-color opaque -- see DecodeBC1ColorBlock's forceOpaqueColor doc).
                    GGCDxt::DecodeBC1ColorBlock(block + 8, true, r, g, b, nullptr);
                    if (fmt == WW3D_FORMAT_DXT2 || fmt == WW3D_FORMAT_DXT3)
                    {
                        GGCDxt::DecodeBC2AlphaBlock(block, a);
                    }
                    else // DXT4 / DXT5
                    {
                        GGCDxt::DecodeBC3AlphaBlock(block, a);
                    }
                }

                const unsigned blockOriginX = bx * 4;
                const unsigned blockOriginY = by * 4;
                for (unsigned ty = 0; ty < 4; ++ty)
                {
                    const unsigned py = blockOriginY + ty;
                    if (py >= height)
                    {
                        break; // Bottom edge block on a non-multiple-of-4 height: remaining rows don't exist.
                    }
                    const unsigned destY = height - 1 - py; // V-flip, same convention as the Swizzle* functions.
                    const unsigned tileY = destY / 8;
                    for (unsigned tx = 0; tx < 4; ++tx)
                    {
                        const unsigned px = blockOriginX + tx;
                        if (px >= width)
                        {
                            continue; // Right edge block on a non-multiple-of-4 width: remaining columns don't exist.
                        }
                        const unsigned texelIndex = ty * 4 + tx;
                        const unsigned tileX = px / 8;
                        const unsigned dstIndex = (tileY * tilesPerRow + tileX) * 64 + GGC_MortonInterleave(px, destY);

                        unsigned short packed;
                        if (outFormat == GPU_RGB565)
                        {
                            packed = GGC_PackRGB565FromRGBA8(r[texelIndex], g[texelIndex], b[texelIndex]);
                        }
                        else if (outFormat == GPU_RGBA5551)
                        {
                            packed = GGC_PackRGBA5551FromRGBA8(r[texelIndex], g[texelIndex], b[texelIndex], a[texelIndex]);
                        }
                        else // GPU_RGBA4
                        {
                            packed = GGC_PackRGBA4FromRGBA8(r[texelIndex], g[texelIndex], b[texelIndex], a[texelIndex]);
                        }
                        reinterpret_cast<unsigned short *>(dst)[dstIndex] = packed;
                    }
                }
            }
        }
    }

    // TheSuperHackers @feature githubawn 20/07/2026 Cheap up-front scan over
    // every BC1 block's 4-byte endpoint header ONLY (see
    // GGCDxt::BC1BlockHasPunchThroughAlpha) -- reads none of the pixel index
    // data, so this whole texture's worth of blocks are visited twice
    // (header-only here, full decode later in GGC_DecodeDxtToPica) rather
    // than decoding into a scratch buffer once just to inspect it and again
    // to actually write it out. Ensure_Texture needs this answer BEFORE it
    // can pick DXT1's PICA output format (opaque GPU_RGB565 vs GPU_RGBA5551)
    // and therefore before C3D_TexInit, so a single combined decode-and-
    // detect pass is not an option here the way it might be elsewhere.
    bool GGC_Bc1TextureHasPunchThroughAlpha(const unsigned char * src, unsigned srcPitch,
                                            unsigned width, unsigned height)
    {
        const unsigned numBlocksX = (width + 3) / 4;
        const unsigned numBlocksY = DXT_SurfaceRows(height);
        for (unsigned by = 0; by < numBlocksY; ++by)
        {
            const unsigned char * blockRow = src + by * srcPitch;
            for (unsigned bx = 0; bx < numBlocksX; ++bx)
            {
                if (GGCDxt::BC1BlockHasPunchThroughAlpha(blockRow + bx * 8))
                {
                    return true;
                }
            }
        }
        return false;
    }
}

Citro3dBackend::Citro3dBackend()
    : m_topTarget(nullptr)
    , m_bottomTarget(nullptr)
    , m_initialized(false)
    , m_vshDvlb(nullptr)
    , m_program(nullptr)
    , m_uniformTransform(-1)
    , m_shaderLoaded(false)
    , m_texturingEnabled(false)
    // TheSuperHackers @feature githubawn 20/07/2026 Defaults below match
    // ShaderClass::Reset()'s own defaults (shader.h) -- Set_Shader always
    // runs before the first Draw_Triangles call in practice, so these are
    // only ever visible if that assumption is ever violated.
    , m_primaryGradient(1)      // ShaderClass::GRADIENT_MODULATE
    , m_depthCompare(3)         // ShaderClass::PASS_LEQUAL
    , m_depthWriteEnabled(true) // ShaderClass::DEPTH_WRITE_ENABLE
    , m_cullEnabled(true)       // ShaderClass::CULL_MODE_ENABLE
    // TheSuperHackers @feature githubawn 20/07/2026 See the member comments in
    // Citro3dBackend.h -- diagnostic-only mirror of Set_Shader's blend/alpha-test decode.
    // Defaults match ShaderClass::Reset()'s own defaults (SRCBLEND_ONE/DSTBLEND_Zero/
    // ALPHATEST_DISABLE), same reasoning as m_primaryGradient's default above.
    , m_lastSrcBlendFactor(static_cast<int>(GPU_ONE))
    , m_lastDstBlendFactor(static_cast<int>(GPU_ZERO))
    , m_alphaTestEnabled(false)
    , m_alphaTestRef(0)
    // TheSuperHackers @feature githubawn 20/07/2026 See the member comments in
    // Citro3dBackend.h -- white ambient/no dominant light is the "no lighting
    // info seen yet" default that reproduces the old hardcoded-opaque-white
    // behavior until a real Set_Light_Environment/Set_Ambient call arrives.
    , m_hasDominantLight(false)
    , m_materialLightingEnabled(true)
    , m_currentTexture(nullptr)
    , m_dynamicVertexData(nullptr)
    , m_dynamicVertexSizeBytes(0)
    , m_dynamicIndexData(nullptr)
    , m_dynamicIndexSizeBytes(0)
    , m_indexBaseOffset(0)
    , m_currentVertexData(nullptr)
    , m_currentVertexStride(0)
    , m_currentVertexSizeBytes(0)
    , m_currentIndexData(nullptr)
    , m_currentIndexSizeBytes(0)
    // TheSuperHackers @feature githubawn 20/07/2026 See the member comments in
    // Citro3dBackend.h -- not in a sorted batch pass, and no dominant sort-batch light seen yet,
    // until Begin_Sorted_Batch_Pass/Capture_Sorted_Batch_Light say otherwise.
    , m_inSortedBatchPass(false)
    , m_usingDynamicVertexBuffer(false)
    , m_sortHasDominantLight(false)
{
    // Identity by default, matching DX8Backend's own tracked default (see
    // Is_World_Identity/Is_View_Identity), so an early 3D draw before the
    // first real Set_Transform call degenerates harmlessly instead of using
    // garbage matrix data.
    for (int i = 0; i < 16; ++i)
    {
        m_worldMtx[i] = m_viewMtx[i] = m_projMtx[i] = ((i % 5) == 0) ? 1.0f : 0.0f;
        m_sortWorldMtx[i] = m_sortViewMtx[i] = ((i % 5) == 0) ? 1.0f : 0.0f;
    }

    // TheSuperHackers @feature githubawn 20/07/2026 See m_sceneAmbient's header comment --
    // opaque white until real lighting data arrives, matching the pre-existing hardcoded color.
    for (int i = 0; i < 3; ++i)
    {
        m_sceneAmbient[i] = 1.0f;
        m_dominantLightColor[i] = 0.0f;
        m_sortDominantLightColor[i] = 0.0f;
    }
}

Citro3dBackend::~Citro3dBackend()
{
    if (m_initialized)
    {
        Shutdown();
    }
}

void Citro3dBackend::Initialize(void * hwnd, int /*width*/, int /*height*/)
{
    DX8Backend::Initialize(hwnd, 0, 0);

    if (m_initialized)
    {
        return;
    }

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    // PICA200 framebuffers are native portrait: width/height are swapped
    // relative to the screen's landscape resolution (top screen is
    // logically 400x240, but the render target is allocated 240x400).
    C3D_RenderTarget * top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT, GGC_3DS_DISPLAY_TRANSFER_FLAGS);
    C3D_RenderTarget * bottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(bottom, GFX_BOTTOM, GFX_LEFT, GGC_3DS_DISPLAY_TRANSFER_FLAGS);

    m_topTarget = reinterpret_cast<C3D_RenderTarget_tag *>(top);
    m_bottomTarget = reinterpret_cast<C3D_RenderTarget_tag *>(bottom);
    m_initialized = true;

    // TheSuperHackers @diagnostic githubawn 18/07/2026 Baseline linear-heap
    // usage right after C3D_Init + both render targets, before any menu
    // texture has loaded -- the other endpoint of the boot-time linear-heap
    // usage curve (see the matching log in Ensure_Texture).
    SDL_Log("[ggc-tex] #0 baseline after C3D_Init+RenderTargets linearFree=%u", (unsigned)linearSpaceFree());

    // TheSuperHackers @todo githubawn 17/07/2026 WW3D::Enable_Texturing(false)
    // was tried here as a diagnostic to isolate whether texture memory was
    // still the match-load OOM cause. REVERTED: with texturing off, nearly
    // every texture reference falls back to MissingTexture::_Create_Missing_
    // Surface() (missingtexture.cpp) -- a rarely-exercised code path that,
    // hammered at that volume, was itself corrupting state (CreateImageSurface
    // receiving garbage width/height/format from a GetDesc() call, unrelated
    // to the original OOM). The real OOM fix landed separately: static mesh
    // vertex/index buffers were being double-stored (once in the stub D3D8
    // buffer's permanent scratch, once in this backend's own GPU-visible
    // copy) -- see ReleaseVertexBufferCpuScratch/ReleaseIndexBufferCpuScratch
    // in StubD3D8Device.cpp/.h. Leave texturing on and let that fix carry the
    // weight; DXT decode is still a separate, unported gap (see Ensure_Texture).

    Ensure_Shader_Loaded();
}

void Citro3dBackend::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    for (std::map<TextureBaseClass *, GGCTextureCacheEntry>::iterator it = m_textureCache.begin();
         it != m_textureCache.end(); ++it)
    {
        // TheSuperHackers @bugfix githubawn 20/07/2026 Skip the negative-cache
        // case (tex == nullptr, see Ensure_Texture) -- C3D_TexDelete on a
        // texture that was never C3D_TexInit'd is not a real texture handle.
        if (it->second.tex != nullptr)
        {
            C3D_Tex * tex = reinterpret_cast<C3D_Tex *>(it->second.tex);
            C3D_TexDelete(tex);
            delete tex;
        }
    }
    m_textureCache.clear();
    m_currentTexture = nullptr;

    for (std::map<const VertexBufferClass *, GGCStaticBufferEntry>::iterator it = m_staticVBCache.begin();
         it != m_staticVBCache.end(); ++it)
    {
        linearFree(it->second.data);
    }
    m_staticVBCache.clear();
    for (std::map<const IndexBufferClass *, GGCStaticBufferEntry>::iterator it = m_staticIBCache.begin();
         it != m_staticIBCache.end(); ++it)
    {
        linearFree(it->second.data);
    }
    m_staticIBCache.clear();
    m_currentVertexData = nullptr;
    m_currentIndexData = nullptr;

    if (m_dynamicVertexData != nullptr)
    {
        linearFree(m_dynamicVertexData);
        m_dynamicVertexData = nullptr;
    }
    if (m_dynamicIndexData != nullptr)
    {
        linearFree(m_dynamicIndexData);
        m_dynamicIndexData = nullptr;
    }
    for (size_t i = 0; i < m_pendingFrees.size(); ++i)
    {
        linearFree(m_pendingFrees[i]);
    }
    m_pendingFrees.clear();

    if (m_shaderLoaded)
    {
        shaderProgramFree(reinterpret_cast<shaderProgram_s *>(m_program));
        delete reinterpret_cast<shaderProgram_s *>(m_program);
        m_program = nullptr;
        DVLB_Free(reinterpret_cast<DVLB_s *>(m_vshDvlb));
        m_vshDvlb = nullptr;
        m_shaderLoaded = false;
    }

    if (m_topTarget != nullptr)
    {
        C3D_RenderTargetDelete(reinterpret_cast<C3D_RenderTarget *>(m_topTarget));
        m_topTarget = nullptr;
    }
    if (m_bottomTarget != nullptr)
    {
        C3D_RenderTargetDelete(reinterpret_cast<C3D_RenderTarget *>(m_bottomTarget));
        m_bottomTarget = nullptr;
    }

    C3D_Fini();
    gfxExit();
    m_initialized = false;

    DX8Backend::Shutdown();
}

void Citro3dBackend::Begin_Scene()
{
    DX8Backend::Begin_Scene();

    if (!m_initialized)
    {
        return;
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // TheSuperHackers @bugfix githubawn 16/07/2026 Safe point to actually
    // free buffers queued by Capture_Dynamic_Vertex_Data/Index_Data during
    // the PREVIOUS frame -- C3D_FRAME_SYNCDRAW above just guaranteed that
    // frame's GPU commands (which may still reference them) finished
    // executing. See the m_pendingFrees comment in Citro3dBackend.h.
    for (size_t i = 0; i < m_pendingFrees.size(); ++i)
    {
        linearFree(m_pendingFrees[i]);
    }
    m_pendingFrees.clear();

    // TheSuperHackers @tweak githubawn 16/07/2026 Draw the interactive game
    // UI on the bottom screen, not the top. The New 3DS top screen has no
    // touch digitizer at all -- only the bottom screen does -- and an RTS
    // fundamentally needs mouse-equivalent click/drag interaction (unit
    // selection, drag-select boxes, command bar buttons), which only touch
    // can provide naturally here. The top screen (m_topTarget, still
    // created/cleared above) is reserved for a future secondary/spectator
    // view (extra map space, minimap) rather than anything clickable.
    C3D_FrameDrawOn(reinterpret_cast<C3D_RenderTarget *>(m_bottomTarget));
}

void Citro3dBackend::End_Scene(bool flip_frame)
{
    DX8Backend::End_Scene(flip_frame);

    if (!m_initialized)
    {
        return;
    }

    C3D_FrameEnd(0);
}

void Citro3dBackend::Set_Top_Screen_Active(bool active)
{
    if (!m_initialized)
    {
        return;
    }

    // TheSuperHackers @diagnostic githubawn 18/07/2026 Root-causing why the top-screen overlay's
    // text/radar are reportedly still appearing on the bottom screen -- confirming this redirect
    // is actually reaching distinct, non-null targets each call. Capped low since this fires
    // twice per frame (~7000+ times/minute uncapped) which would flood the log.
    {
        static int s_ggcTopScreenLogCount = 0;
        if (s_ggcTopScreenLogCount < 40)
        {
            ++s_ggcTopScreenLogCount;
            SDL_Log("[ggc-topscreen] Set_Top_Screen_Active(%d) top=%p bottom=%p ->target=%p",
                    (int)active, (void*)m_topTarget, (void*)m_bottomTarget,
                    active ? (void*)m_topTarget : (void*)m_bottomTarget);
        }
    }

    C3D_FrameDrawOn(reinterpret_cast<C3D_RenderTarget *>(active ? m_topTarget : m_bottomTarget));
}

void Citro3dBackend::Clear(bool clear_color, bool clear_z_stencil,
                           const Vector3 & color,
                           float dest_alpha, float z, unsigned int stencil)
{
    DX8Backend::Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);

    if (!m_initialized)
    {
        return;
    }

    unsigned char r = static_cast<unsigned char>(color.X * 255.0f);
    unsigned char g = static_cast<unsigned char>(color.Y * 255.0f);
    unsigned char b = static_cast<unsigned char>(color.Z * 255.0f);
    unsigned char a = static_cast<unsigned char>(dest_alpha * 255.0f);
    u32 clearColor = static_cast<u32>(r) << 24 | static_cast<u32>(g) << 16 |
                      static_cast<u32>(b) << 8 | static_cast<u32>(a);

    C3D_ClearBits clearBits = static_cast<C3D_ClearBits>(
        (clear_color ? C3D_CLEAR_COLOR : 0) | (clear_z_stencil ? C3D_CLEAR_DEPTH : 0));

    if (clearBits != 0)
    {
        C3D_RenderTargetClear(reinterpret_cast<C3D_RenderTarget *>(m_topTarget), clearBits, clearColor, 0);
        C3D_RenderTargetClear(reinterpret_cast<C3D_RenderTarget *>(m_bottomTarget), clearBits, clearColor, 0);
    }
}

// -----------------------------------------------------------------------------
// Shader lifecycle
// -----------------------------------------------------------------------------

void Citro3dBackend::Ensure_Shader_Loaded()
{
    if (m_shaderLoaded || !m_initialized)
    {
        return;
    }

    DVLB_s * dvlb = DVLB_ParseFile(const_cast<u32 *>(reinterpret_cast<const u32 *>(vs_2d_shbin)), vs_2d_shbin_size);
    if (dvlb == nullptr)
    {
        return;
    }

    shaderProgram_s * program = new shaderProgram_s;
    shaderProgramInit(program);
    shaderProgramSetVsh(program, &dvlb->DVLE[0]);

    m_vshDvlb = reinterpret_cast<DVLB_s_tag *>(dvlb);
    m_program = reinterpret_cast<shaderProgram_s_tag *>(program);
    m_uniformTransform = shaderInstanceGetUniformLocation(program->vertexShader, "transform");
    m_shaderLoaded = true;
}

// -----------------------------------------------------------------------------
// Draw state (ShaderClass -> citro3d fixed-function GPU state)
// -----------------------------------------------------------------------------
//
// TheSuperHackers @info githubawn 15/07/2026 Only decodes the subset the 2D
// UI path (Render2DClass::Get_Default_Shader / Enable_Alpha / Enable_Additive
// in render2d.cpp) ever actually sets: src/dst blend factors, color mask,
// and texturing enable. Depth test/write is hardcoded off below (2D UI never
// depth-tests, see Get_Depth_Compare()==PASS_ALWAYS / Get_Depth_Mask()==
// DEPTH_WRITE_DISABLE in Render2DClass::Get_Default_Shader — both constant
// for this whole draw path, so there's nothing to decode there).
//
// TheSuperHackers @feature githubawn 20/07/2026 Milestone 4/5: also decodes
// alpha test (applied immediately below, same as blend/color-mask), and
// primary gradient / depth-compare / depth-mask / cull-mode into members for
// Apply_Tex_Env and Draw_Triangles' 3D branch to read later (see the member
// comments in Citro3dBackend.h). The hardcoded C3D_DepthTest(false, ...)
// call at the end of this function is harmless dead state either way: both
// Draw_Triangles branches (2D and 3D) always issue their own C3D_DepthTest
// call immediately before drawing, so whatever this function leaves behind
// never actually reaches the GPU for a real draw.

void Citro3dBackend::Set_Shader(const ShaderClass & shaderIn)
{
    DX8Backend::Set_Shader(shaderIn);

    if (!m_initialized)
    {
        return;
    }

    ShaderClass shader = shaderIn;
    m_texturingEnabled = shader.Uses_Texture() != 0;

    GPU_BLENDFACTOR srcFactor = GPU_ONE;
    GPU_BLENDFACTOR dstFactor = GPU_ZERO;

    if (shader.Get_Color_Mask() == ShaderClass::COLOR_WRITE_ENABLE)
    {
        switch (shader.Get_Src_Blend_Func())
        {
            case ShaderClass::SRCBLEND_ZERO:                 srcFactor = GPU_ZERO; break;
            case ShaderClass::SRCBLEND_SRC_ALPHA:             srcFactor = GPU_SRC_ALPHA; break;
            case ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA:   srcFactor = GPU_ONE_MINUS_SRC_ALPHA; break;
            case ShaderClass::SRCBLEND_ONE:
            default:                                         srcFactor = GPU_ONE; break;
        }
        switch (shader.Get_Dst_Blend_Func())
        {
            case ShaderClass::DSTBLEND_ONE:                   dstFactor = GPU_ONE; break;
            case ShaderClass::DSTBLEND_SRC_COLOR:             dstFactor = GPU_SRC_COLOR; break;
            case ShaderClass::DSTBLEND_ONE_MINUS_SRC_COLOR:   dstFactor = GPU_ONE_MINUS_SRC_COLOR; break;
            case ShaderClass::DSTBLEND_SRC_ALPHA:             dstFactor = GPU_SRC_ALPHA; break;
            case ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA:   dstFactor = GPU_ONE_MINUS_SRC_ALPHA; break;
            case ShaderClass::DSTBLEND_ZERO:
            default:                                         dstFactor = GPU_ZERO; break;
        }
    }
    else
    {
        // Color writes disabled -- matches ShaderClass::Apply's DX8 path
        // (sf=ZERO, df=ONE : keep whatever is already in the color buffer).
        srcFactor = GPU_ZERO;
        dstFactor = GPU_ONE;
    }

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, srcFactor, dstFactor, srcFactor, dstFactor);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

    // TheSuperHackers @feature githubawn 20/07/2026 Diagnostic-only capture -- see the member
    // comments in Citro3dBackend.h. Does not change what was just applied to the GPU above.
    m_lastSrcBlendFactor = static_cast<int>(srcFactor);
    m_lastDstBlendFactor = static_cast<int>(dstFactor);

    // TheSuperHackers @feature githubawn 20/07/2026 Alpha test: mirrors
    // BgfxBackend.cpp's BuildAlphaTestUniforms exactly (ref 0x60/255, and the
    // compare direction inverted for shaders whose src blend is
    // SRCBLEND_ONE_MINUS_SRC_ALPHA -- see kDefaultAlphaTestRef there),
    // translated to citro3d's native fixed-function alpha test instead of a
    // shader-uniform-driven discard.
    if (shader.Get_Alpha_Test() != ShaderClass::ALPHATEST_DISABLE)
    {
        const int kAlphaTestRef = 0x60; // out of 255, matches BgfxBackend's kDefaultAlphaTestRef
        m_alphaTestEnabled = true;
        if (shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA)
        {
            C3D_AlphaTest(true, GPU_LEQUAL, 255 - kAlphaTestRef);
            m_alphaTestRef = 255 - kAlphaTestRef;
        }
        else
        {
            C3D_AlphaTest(true, GPU_GEQUAL, kAlphaTestRef);
            m_alphaTestRef = kAlphaTestRef;
        }
    }
    else
    {
        C3D_AlphaTest(false, GPU_ALWAYS, 0);
        m_alphaTestEnabled = false;
        m_alphaTestRef = 0;
    }

    // TheSuperHackers @feature githubawn 20/07/2026 Primary gradient (Apply_Tex_Env, called later
    // per-draw) and 3D depth/cull state (Draw_Triangles' 3D branch) -- see the member comments in
    // Citro3dBackend.h for why these are stored rather than applied immediately here.
    m_primaryGradient = static_cast<int>(shader.Get_Primary_Gradient());
    m_depthCompare = static_cast<int>(shader.Get_Depth_Compare());
    m_depthWriteEnabled = (shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE);
    m_cullEnabled = (shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_ENABLE);
}

// -----------------------------------------------------------------------------
// Basic scene lighting (crude CPU flat-shade -- no per-vertex GPU lighting)
// -----------------------------------------------------------------------------
//
// TheSuperHackers @feature githubawn 20/07/2026 Terrain and units drew fully
// unlit (flat constant color) before this: Set_Light_Environment, Set_Material,
// and Set_Ambient were never overridden, so they all fell through to
// IRenderBackend's no-op defaults (IRenderBackend.h) and the game's actual
// lights never reached this backend at all. This backend cannot edit
// vs_2d.v.pica (shared with the 2D UI path, per this file's own top comment
// and the shader-editing restriction on this task), so there is no vertex
// shader stage available to compute a real per-vertex N.L term on the GPU --
// unlike BgfxBackend, which has a full uber-shader that consumes the same
// LightEnvironmentClass data as real per-pixel/per-vertex lighting uniforms
// (see BgfxBackend::Set_Light_Environment, BgfxBackend.cpp ~line 6847, and
// its "Read current D3D light state per-draw" comment further down that
// file). The three overrides below instead just CAPTURE that same data into
// members; Compute_Flat_Light_Color (used from Draw_Triangles) combines them
// into one flat, per-DRAW (not per-vertex) RGB constant: scene ambient plus
// the single dominant light's diffuse color, clamped to [0,1]. There is no
// surface-normal term anywhere in this combine -- every triangle in a draw
// call gets the exact same flat color regardless of which way it faces.
//
// TheSuperHackers @todo githubawn 20/07/2026 Real per-vertex lighting (an
// actual N.L term, multiple simultaneous lights instead of just the
// dominant one, per-vertex point-light attenuation) needs a dedicated 3D
// vertex shader that takes the vertex normal as a real input and computes
// the light contribution on the GPU -- vs_2d.v.pica only has a straight
// pos/color/uv passthrough with no lighting math at all, and is shared with
// the 2D UI path so it cannot be repurposed here. This flat CPU combine is a
// deliberately crude stand-in until that shader exists.

void Citro3dBackend::Set_Material(const VertexMaterialClass * material)
{
    DX8Backend::Set_Material(material);

    // TheSuperHackers @feature githubawn 20/07/2026 The only bit of
    // VertexMaterialClass (vertmaterial.h) this crude combine reads --
    // everything else BgfxBackend's CaptureMaterialStateForBgfx extracts
    // (BgfxBackend.cpp ~line 5570: diffuse/ambient/emissive color-source
    // selection, opacity, per-source vertex-color routing) feeds a
    // programmable shader this backend does not have. Get_Lighting() alone
    // is enough to honor materials that explicitly ask to be unlit (e.g.
    // some decal/effect geometry) -- Compute_Flat_Light_Color falls back to
    // the same opaque white this backend already used everywhere when this
    // is false, rather than tinting something that was never meant to be lit.
    m_materialLightingEnabled = (material != nullptr) ? material->Get_Lighting() : true;
}

void Citro3dBackend::Set_Ambient(const Vector3 & color)
{
    DX8Backend::Set_Ambient(color);

    // TheSuperHackers @feature githubawn 20/07/2026 Mirrors
    // BgfxBackend::Set_Ambient (BgfxBackend.cpp ~line 6017) exactly: store
    // the scene ambient directly, no clamping here (Compute_Flat_Light_Color
    // clamps the final combined result once, not each input separately).
    m_sceneAmbient[0] = color.X;
    m_sceneAmbient[1] = color.Y;
    m_sceneAmbient[2] = color.Z;
}

void Citro3dBackend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    DX8Backend::Set_Light_Environment(light_env);

    // TheSuperHackers @feature githubawn 20/07/2026 Mirrors
    // BgfxBackend::Set_Light_Environment (BgfxBackend.cpp ~line 6847): a
    // null light_env leaves whatever was previously captured untouched (same
    // early-out BgfxBackend uses), rather than resetting to some default --
    // matches DX8Wrapper's own calling convention of only pushing a light
    // environment when one is actually being applied.
    if (light_env == nullptr)
    {
        return;
    }

    const Vector3 & ambient = light_env->Get_Equivalent_Ambient();
    m_sceneAmbient[0] = ambient.X;
    m_sceneAmbient[1] = ambient.Y;
    m_sceneAmbient[2] = ambient.Z;

    // TheSuperHackers @feature githubawn 20/07/2026 Unlike BgfxBackend (which loops all
    // MAX_LIGHTS=4 lights into per-light shader uniforms), this crude flat combine only has
    // room for ONE light's contribution. LightEnvironmentClass itself documents InputLights[]
    // as "Sorted list of input lights from the greatest contributor to the least" (see its own
    // member comment in lightenvironment.h) -- so index 0 IS already the dominant light with no
    // extra comparison needed; every other simultaneous light (fill lights, secondary point
    // lights, etc.) is simply dropped by this backend. Point vs. directional is not
    // distinguished either: both isPointLight(0)'s getPointDiffuse and the directional
    // Get_Light_Diffuse are just "this light's diffuse color" for the purposes of a combine
    // that has no direction/attenuation term to apply anyway (see the @todo above).
    const int lightCount = light_env->Get_Light_Count();
    m_hasDominantLight = lightCount > 0;
    if (m_hasDominantLight)
    {
        const Vector3 & dominant = light_env->isPointLight(0)
            ? light_env->getPointDiffuse(0)
            : light_env->Get_Light_Diffuse(0);
        m_dominantLightColor[0] = dominant.X;
        m_dominantLightColor[1] = dominant.Y;
        m_dominantLightColor[2] = dominant.Z;
    }
    else
    {
        m_dominantLightColor[0] = m_dominantLightColor[1] = m_dominantLightColor[2] = 0.0f;
    }
}

void Citro3dBackend::Compute_Flat_Light_Color(unsigned char & outR, unsigned char & outG, unsigned char & outB) const
{
    if (!m_materialLightingEnabled)
    {
        // Explicitly-unlit material (Set_Material) -- same opaque white this backend already
        // used for every no-diffuse 3D draw before this feature existed.
        outR = outG = outB = 255;
        return;
    }

    float r = m_sceneAmbient[0] + (m_hasDominantLight ? m_dominantLightColor[0] : 0.0f);
    float g = m_sceneAmbient[1] + (m_hasDominantLight ? m_dominantLightColor[1] : 0.0f);
    float b = m_sceneAmbient[2] + (m_hasDominantLight ? m_dominantLightColor[2] : 0.0f);

    r = (r < 0.0f) ? 0.0f : (r > 1.0f) ? 1.0f : r;
    g = (g < 0.0f) ? 0.0f : (g > 1.0f) ? 1.0f : g;
    b = (b < 0.0f) ? 0.0f : (b > 1.0f) ? 1.0f : b;

    // Pre-multiplied by 255 to match vs_2d.v.pica's colscale (1/255) constant -- see the same
    // reasoning already documented on the fixed-attribute C3D_FixedAttribSet calls this feeds
    // in Draw_Triangles.
    outR = static_cast<unsigned char>(r * 255.0f);
    outG = static_cast<unsigned char>(g * 255.0f);
    outB = static_cast<unsigned char>(b * 255.0f);
}

// -----------------------------------------------------------------------------
// Textures
// -----------------------------------------------------------------------------

C3D_Tex_tag * Citro3dBackend::Ensure_Texture(TextureBaseClass * texture)
{
    if (texture == nullptr)
    {
        return nullptr;
    }

    std::map<TextureBaseClass *, GGCTextureCacheEntry>::iterator found = m_textureCache.find(texture);
    if (found != m_textureCache.end())
    {
        // TheSuperHackers @bugfix githubawn 20/07/2026 Revalidate the cache
        // hit instead of trusting it blindly -- see GGCTextureCacheEntry's
        // comment in the header. Either the async texture loader swapped
        // Peek_D3D_Texture()'s underlying IDirect3DTexture8* (thumbnail ->
        // full image) or the same d3d texture's pixels were rewritten via
        // LockRect/UnlockRect since this entry was uploaded (GGC_
        // GetTextureContentVersion tracks the latter). Either means the
        // uploaded C3D_Tex no longer reflects what this TextureBaseClass
        // should show, so evict and fall through to a fresh upload below --
        // same eviction as Invalidate_Cached_Texture.
        IDirect3DTexture8 * currentD3DTex = texture->Peek_D3D_Texture();
        const bool d3dTexChanged = currentD3DTex != found->second.srcD3DTex;
        const bool contentChanged = !d3dTexChanged && found->second.srcD3DTex != nullptr
            && GGC_GetTextureContentVersion(currentD3DTex) != found->second.uploadedVersion;
        if (!d3dTexChanged && !contentChanged)
        {
            // Negative-cache hit (tex == nullptr) with nothing changed --
            // keep returning nullptr without retrying the failed upload.
            return found->second.tex;
        }

        // TheSuperHackers @bugfix githubawn 20/07/2026 Same guard as
        // Invalidate_Cached_Texture: when only the CONTENT version moved but this texture's
        // CPU-side pixels have already been released, the "new content" available to re-read is
        // the zeroed buffer LockRect reallocates -- re-uploading it would blank a good texture.
        // A changed d3d texture POINTER is different: that is the async loader swapping in a
        // genuinely new texture object with its own pixels, so that case still re-uploads.
        if (contentChanged && found->second.tex != nullptr
            && !GGC_TextureHasCpuScratch(currentD3DTex))
        {
            return found->second.tex;
        }

        if (found->second.tex != nullptr)
        {
            C3D_Tex * tex = reinterpret_cast<C3D_Tex *>(found->second.tex);
            if (m_currentTexture == found->second.tex)
            {
                m_currentTexture = nullptr;
            }
            C3D_TexDelete(tex);
            delete tex;
        }
        m_textureCache.erase(found);
        // Falls through to a fresh upload below, same as a first-time miss.
    }

    TextureClass * tex2d = texture->As_TextureClass();
    IDirect3DTexture8 * d3dTex = texture->Peek_D3D_Texture();
    if (tex2d == nullptr || d3dTex == nullptr)
    {
        // TheSuperHackers @diagnostic githubawn 18/07/2026 This early-out previously had no log
        // at all -- a texture that fails here (e.g. its D3D texture never finished being created)
        // silently never reaches the per-texture #%d log below, so it would never show up in past
        // greps for "supported=0" or "LockRect FAILED". Suspected cause of the Score Screen's big
        // white background rectangle (MainBackdrop, ScoreScreen.cpp) not rendering even though it
        // goes through the normal Image/texture path, unlike the Main Menu's black rectangle
        // (separate cause: shell map disabled, see GameLOD.cpp). Capped generously.
        static int s_ggcTexNullLogCount = 0;
        if (s_ggcTexNullLogCount < 100)
        {
            ++s_ggcTexNullLogCount;
            SDL_Log("[ggc-tex] Ensure_Texture EARLY-NULL tex=%p tex2d=%p d3dTex=%p",
                    (void*)texture, (void*)tex2d, (void*)d3dTex);
        }
        return nullptr;
    }

    const WW3DFormat fmt = tex2d->Get_Texture_Format();

    // TheSuperHackers @diagnostic githubawn 16/07/2026 Root-causing "logo
    // shows black instead of transparent, background missing" -- fires once
    // per distinct texture (cache miss only), so a live menu does not spam.
    {
        static int s_ggcTexLogCount = 0;
        if (s_ggcTexLogCount < 200)
        {
            ++s_ggcTexLogCount;
            // TheSuperHackers @diagnostic githubawn 18/07/2026 This diagnostic's
            // "first 200 distinct textures" window happens to cover exactly the
            // boot/main-menu asset-loading phase -- reused here to get a real
            // linear-heap-usage time series for that phase, since a fixed 24MB
            // linear heap broke boot entirely while a mid-match trace showed only
            // ~6.3MB actually in use. Need the BOOT-time peak, not the mid-match
            // one, before touching the heap split again.
            // TheSuperHackers @feature githubawn 20/07/2026 DXT1-5 now counts as
            // supported (CPU-decoded below, see GGC_DecodeDxtToPica) -- this used to be
            // the dominant "supported=0" case in this log (nearly all unit/terrain art
            // is DXT), which is why 3D world geometry drew flat white until now.
            SDL_Log("[ggc-tex] #%d tex=%p fmt=%d w=%d h=%d supported=%d linearFree=%u",
                    s_ggcTexLogCount, (void*)texture, (int)fmt,
                    tex2d->Get_Width(), tex2d->Get_Height(),
                    (int)(fmt == WW3D_FORMAT_A8R8G8B8 || fmt == WW3D_FORMAT_X8R8G8B8
                          || fmt == WW3D_FORMAT_R5G6B5 || fmt == WW3D_FORMAT_A4R4G4B4
                          || fmt == WW3D_FORMAT_A1R5G5B5
                          || fmt == WW3D_FORMAT_DXT1 || fmt == WW3D_FORMAT_DXT2 || fmt == WW3D_FORMAT_DXT3
                          || fmt == WW3D_FORMAT_DXT4 || fmt == WW3D_FORMAT_DXT5),
                    (unsigned)linearSpaceFree());
        }
    }

    // TheSuperHackers @diagnostic githubawn 20/07/2026 Separate, independently-capped log for
    // LARGE textures. The 200-entry counter above is entirely consumed by the boot/menu phase,
    // so it cannot see anything a match loads -- including the terrain tile atlas, the single
    // texture that decides whether terrain is textured at all (TerrainTextureClass, built at
    // match load, and the thing that was silently failing C3D_TexInit at 2048 wide until
    // TileData.h's TEXTURE_WIDTH was clamped for this platform). Anything this big is rare by
    // definition, so a size filter costs nothing in log volume while covering exactly the
    // atlases/backdrops that matter.
    // TheSuperHackers @diagnostic githubawn 20/07/2026 Threshold lowered 256 -> 128 to cover the
    // radar's own textures (W3DRadar builds m_terrainTexture/m_overlayTexture/m_shroudTexture at
    // exactly 128x128), which the 256 cut-off missed entirely. The radar area currently draws as
    // a solid white rectangle, and white is this GPU's output for an UNBOUND texture stage, so
    // the open question is whether those three ever reach a successful upload at all.
    if (tex2d->Get_Width() >= 128 || tex2d->Get_Height() >= 128)
    {
        static int s_ggcBigTexLogCount = 0;
        if (s_ggcBigTexLogCount < 40)
        {
            ++s_ggcBigTexLogCount;
            SDL_Log("[ggc-bigtex] #%d tex=%p fmt=%d w=%d h=%d linearFree=%u",
                    s_ggcBigTexLogCount, (void*)texture, (int)fmt,
                    tex2d->Get_Width(), tex2d->Get_Height(), (unsigned)linearSpaceFree());
        }
    }

    // TheSuperHackers @feature githubawn 20/07/2026 DXT1-5 (BC1-BC3) added to the
    // accepted-format list -- see GGC_DecodeDxtToPica/Citro3dDxtDecode.h. This was
    // previously the single biggest gap in this backend: nearly all unit/terrain art
    // is DXT-compressed, so before this every 3D-world texture bind fell into the
    // negative-cache path below and kGgc3DTexturingEnabled had to stay off entirely
    // (see its comment in Citro3dBackend.h).
    if (fmt != WW3D_FORMAT_A8R8G8B8 && fmt != WW3D_FORMAT_X8R8G8B8
        && fmt != WW3D_FORMAT_R5G6B5 && fmt != WW3D_FORMAT_A4R4G4B4
        && fmt != WW3D_FORMAT_A1R5G5B5
        && fmt != WW3D_FORMAT_DXT1 && fmt != WW3D_FORMAT_DXT2 && fmt != WW3D_FORMAT_DXT3
        && fmt != WW3D_FORMAT_DXT4 && fmt != WW3D_FORMAT_DXT5)
    {
        // Genuinely unsupported format (e.g. bumpmap U8V8/L6V5U5/X8L8V8U8, or a
        // palettized P8/A8P8 texture -- neither ever emitted for this engine's normal
        // art path). Cache a null entry so this texture is not retried every single
        // Set_Texture call.
        //
        // TheSuperHackers @bugfix githubawn 20/07/2026 Still record the
        // current d3d pointer/version even though there is no C3D_Tex --
        // Ensure_Texture's cache-hit revalidation above needs them to know
        // whether to retry this (e.g. the async loader later swaps in a
        // texture whose format IS supported).
        GGCTextureCacheEntry negativeEntry = { nullptr, d3dTex, GGC_GetTextureContentVersion(d3dTex) };
        m_textureCache[texture] = negativeEntry;
        return nullptr;
    }

    D3DLOCKED_RECT locked = { 0 };
    if (FAILED(d3dTex->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)) || locked.pBits == nullptr)
    {
        // TheSuperHackers @diagnostic githubawn 18/07/2026 This path previously failed
        // silently -- a texture that hits it draws with nothing bound, and this GPU's
        // texture unit defaults to solid white for an unbound stage, which is a strong
        // suspect for the "radar terrain area renders blank white" report (bezel/view-box
        // draw fine since those are untextured). The main Ensure_Texture log above is
        // capped at the first 200 distinct textures specifically to capture the boot/menu
        // phase (see its comment) and is long since exhausted by the time a match is
        // running, so this failure would otherwise be invisible. Separately and generously
        // capped so it stays useful for a live radar repro.
        static int s_ggcTexLockFailLogCount = 0;
        if (s_ggcTexLockFailLogCount < 50)
        {
            ++s_ggcTexLockFailLogCount;
            SDL_Log("[ggc-tex] LockRect FAILED tex=%p fmt=%d w=%d h=%d",
                    (void*)texture, (int)fmt, tex2d->Get_Width(), tex2d->Get_Height());
        }
        return nullptr;
    }

    const unsigned width = static_cast<unsigned>(tex2d->Get_Width());
    const unsigned height = static_cast<unsigned>(tex2d->Get_Height());

    // TheSuperHackers @feature githubawn 20/07/2026 Read here (rather than after
    // C3D_TexInit, where the equivalent read used to live for the non-DXT formats)
    // because DXT1's own gpuFormat choice below depends on scanning the source bits
    // -- see GGC_Bc1TextureHasPunchThroughAlpha. IMPORTANT: locked.Pitch for a
    // compressed format is StubD3D8Device.cpp's SurfacePitch(fmt, width), which for
    // IsCompressedFormat is exactly DXT_SurfacePitch(width, blockBytes) (DXTUtils.h)
    // -- i.e. the BLOCK-ROW pitch, not a per-texel byte pitch. Relied on directly
    // below (as srcPitch) rather than recomputed, same as BgfxBackendTextures.cpp's
    // CopyTextureLevel does against this identical stub.
    const unsigned char * srcBits = static_cast<const unsigned char *>(locked.pBits);
    const unsigned srcPitch = static_cast<unsigned>(locked.Pitch);

    // TheSuperHackers @feature githubawn 20/07/2026 DXT1 (BC1) alone has two
    // sub-modes with different alpha semantics baked into the SAME 8-byte block
    // layout (see Citro3dDxtDecode.h's DecodeBC1ColorBlock): most DXT1 art is fully
    // opaque and only ever uses the 4-color mode, but any texture with even one
    // block in the 1-bit-punchthrough mode needs an alpha channel to represent it.
    // Scanning every block's 4-byte header up front (no full decode) decides this
    // BEFORE gpuFormat/C3D_TexInit below need an answer, without a second full
    // decode pass over the whole texture -- see that function's own comment.
    const bool dxt1HasPunchThroughAlpha = (fmt == WW3D_FORMAT_DXT1)
        && GGC_Bc1TextureHasPunchThroughAlpha(srcBits, srcPitch, width, height);

    // TheSuperHackers @tweak githubawn 16/07/2026 Upload at the matching
    // 2-byte-per-pixel PICA200 format for the three 16-bit source formats
    // instead of always expanding to 4-byte GPU_RGBA8 -- halves GPU/linear-
    // heap texture memory for these with no quality loss (lossless repack,
    // see GGC_SwizzleR5G6B5ToPicaRGB565 and neighbors). A8R8G8B8/X8R8G8B8
    // still upload as GPU_RGBA8 (down-converting a 32bpp source is a real,
    // separately-auditable quality tradeoff, not a free win).
    //
    // TheSuperHackers @feature githubawn 20/07/2026 DXT1-5 (BC1-BC3) join the
    // 2-byte-per-pixel formats above instead of expanding to GPU_RGBA8 -- this is
    // the actual point of decoding on the CPU instead of just uploading a full RGBA8
    // expansion: nearly all unit/terrain art is DXT, and this hardware's linear heap
    // cannot afford a 4-byte-per-texel copy of all of it. DXT1 without punchthrough
    // alpha -> GPU_RGB565 (lossless w.r.t. BC1's own RGB565 endpoints in the common
    // case -- BC1 never had more color precision than that to begin with). DXT1 WITH
    // punchthrough alpha -> GPU_RGBA5551 (1-bit alpha is exactly what BC1's
    // punchthrough mode itself represents, so this is also lossless). DXT2-5 (real
    // per-texel alpha, 1-4 bits' worth of actual information depending on sub-mode)
    // -> GPU_RGBA4: exact for DXT2/3's explicit 4-bit alpha, and a deliberate,
    // acceptable quantization for DXT4/5's 8-bit interpolated alpha -- same
    // memory-over-precision tradeoff already made for the native 16-bit formats
    // above, just applied to a decoded-not-native alpha channel.
    const GPU_TEXCOLOR gpuFormat =
        (fmt == WW3D_FORMAT_R5G6B5) ? GPU_RGB565 :
        (fmt == WW3D_FORMAT_A4R4G4B4) ? GPU_RGBA4 :
        (fmt == WW3D_FORMAT_A1R5G5B5) ? GPU_RGBA5551 :
        (fmt == WW3D_FORMAT_DXT1) ? (dxt1HasPunchThroughAlpha ? GPU_RGBA5551 : GPU_RGB565) :
        (fmt == WW3D_FORMAT_DXT2 || fmt == WW3D_FORMAT_DXT3
         || fmt == WW3D_FORMAT_DXT4 || fmt == WW3D_FORMAT_DXT5) ? GPU_RGBA4 :
        GPU_RGBA8;

    C3D_Tex * tex = new C3D_Tex;
    if (!C3D_TexInit(tex, static_cast<u16>(width), static_cast<u16>(height), gpuFormat))
    {
        // TheSuperHackers @bugfix githubawn 20/07/2026 This previously left
        // the texture uncached, so it retried C3D_TexInit (and presumably
        // failed identically) on every single bind. Log once -- capped, this
        // is not expected to fire under normal linear-heap pressure -- and
        // cache a negative entry keyed off the current d3d pointer/version so
        // it is only retried if the source texture actually changes (same
        // revalidation as the unsupported-format negative cache above).
        static int s_ggcTexInitFailLogCount = 0;
        if (s_ggcTexInitFailLogCount < 50)
        {
            ++s_ggcTexInitFailLogCount;
            SDL_Log("[ggc-tex] C3D_TexInit FAILED w=%u h=%u fmt=%d linearFree=%u",
                    width, height, (int)gpuFormat, (unsigned)linearSpaceFree());
        }
        d3dTex->UnlockRect(0);
        delete tex;
        GGCTextureCacheEntry negativeEntry = { nullptr, d3dTex, GGC_GetTextureContentVersion(d3dTex) };
        m_textureCache[texture] = negativeEntry;
        return nullptr;
    }

    // TheSuperHackers @feature githubawn 20/07/2026 srcBits/srcPitch computed above
    // (needed early for the DXT1 punchthrough pre-scan); reused here unchanged.
    unsigned char * dstBits = static_cast<unsigned char *>(tex->data);
    if (fmt == WW3D_FORMAT_R5G6B5)
    {
        GGC_SwizzleR5G6B5ToPicaRGB565(srcBits, srcPitch, dstBits, width, height);
    }
    else if (fmt == WW3D_FORMAT_A4R4G4B4)
    {
        GGC_SwizzleA4R4G4B4ToPicaRGBA4(srcBits, srcPitch, dstBits, width, height);
    }
    else if (fmt == WW3D_FORMAT_A1R5G5B5)
    {
        GGC_SwizzleA1R5G5B5ToPicaRGBA5551(srcBits, srcPitch, dstBits, width, height);
    }
    else if (fmt == WW3D_FORMAT_DXT1 || fmt == WW3D_FORMAT_DXT2 || fmt == WW3D_FORMAT_DXT3
             || fmt == WW3D_FORMAT_DXT4 || fmt == WW3D_FORMAT_DXT5)
    {
        GGC_DecodeDxtToPica(srcBits, srcPitch, dstBits, width, height, fmt, gpuFormat);

        // TheSuperHackers @diagnostic githubawn 20/07/2026 Dedicated counter for compressed
        // uploads, deliberately NOT sharing the general per-texture cap above: that one is
        // capped at the first 200 distinct textures, which the menu/boot phase exhausts long
        // before a match starts, so it cannot answer "did any DXT texture ever upload in a
        // match?" -- the question that decides whether the black 3D world is a texturing
        // problem at all. Also samples one decoded texel so a decoder that runs but produces
        // all-black output is distinguishable from one that never runs.
        static int s_ggcDxtLogCount = 0;
        if (s_ggcDxtLogCount < 30)
        {
            ++s_ggcDxtLogCount;
            unsigned short firstTexel = 0;
            std::memcpy(&firstTexel, dstBits, sizeof(firstTexel));
            SDL_Log("[ggc-dxt] #%d fmt=%d gpuFmt=%d w=%u h=%u srcPitch=%u firstTexel=%04x linearFree=%u",
                    s_ggcDxtLogCount, (int)fmt, (int)gpuFormat, width, height, srcPitch,
                    (unsigned)firstTexel, (unsigned)linearSpaceFree());
        }
    }
    else
    {
        GGC_SwizzleBGRA8ToPicaRGBA8(srcBits, srcPitch, dstBits, width, height);
    }
    C3D_TexFlush(tex);
    // TheSuperHackers @feature githubawn 20/07/2026 Wrap/filter used to be hardcoded here
    // (GPU_NEAREST + GPU_CLAMP_TO_EDGE, unconditionally, for every texture -- including tiling
    // terrain/world materials that need REPEAT, which then visibly clamped instead of tiling).
    // Moved to Set_Texture instead, applied every bind from the engine's own per-texture
    // TextureFilterClass state (texture->As_TextureClass()->Get_Filter(), texturefilter.h) --
    // see Set_Texture's own comment for the full reasoning, including why per-BIND (not
    // per-upload, here) is the correct place for this. This upload path leaves the freshly
    // C3D_TexInit'd texture's param bits at whatever citro3d's own default is; Set_Texture
    // always overwrites them with the real value before this texture is ever actually bound for
    // a draw.
    d3dTex->UnlockRect(0);

    // TheSuperHackers @bugfix githubawn 16/07/2026 Root-caused the 3DS
    // match-load OOM (a genuine, reproducible calloc() failure -- see
    // StubD3D8Device.cpp's AllocScratch -- not corruption or a race, once a
    // diagnostic there stopped it from failing silently) to StubD3D8Device
    // permanently keeping a CPU-side copy of every texture's pixel data
    // alive in the general heap, on top of the GPU-side copy citro3d just
    // uploaded above into the linear heap -- double-storing every texture
    // for its whole lifetime. This swizzle read is the last time a WRITE-ONCE
    // texture's CPU-side pixels are needed (the GPU now has its own copy),
    // so release them.
    //
    // TheSuperHackers @bugfix githubawn 16/07/2026 EXCEPT the font glyph
    // atlas (A4R4G4B4): that texture is not write-once. render2dsentence.cpp
    // builds it up INCREMENTALLY -- each newly-rasterized glyph is written at
    // a fresh offset into the SAME buffer via Store_GDI_Char, relying on
    // every previously-rendered glyph still being there, and Invalidate_
    // Cached_Texture re-locks this exact texture every time a new character
    // is needed. LockRect's "lazily reallocate if released" fallback was
    // previously assumed safe for this ("LockRect lazily reallocates"), but
    // a lazy reallocation is a FRESH, empty buffer -- not a restore of
    // whatever was previously written -- so every glyph rendered before the
    // most recent release was silently discarded, visible as text
    // flickering/stuttering as the atlas kept effectively resetting. Keep
    // this one format's scratch alive for its whole lifetime, matching the
    // pre-optimization behavior; every other 2D UI format we handle really
    // is write-once (backgrounds, logos, icons) and keeps the memory win.
    if (fmt != WW3D_FORMAT_A4R4G4B4)
    {
        ReleaseTextureCpuScratch(d3dTex);
    }

    C3D_Tex_tag * result = reinterpret_cast<C3D_Tex_tag *>(tex);
    // TheSuperHackers @bugfix githubawn 20/07/2026 Read the content version
    // AFTER the LockRect/UnlockRect pair above completes (that lock is
    // D3DLOCK_READONLY, so it does not itself bump the version -- this is
    // just "what was the version at the moment these pixels were read").
    // Ensure_Texture's cache-hit revalidation compares against this on
    // every future bind.
    GGCTextureCacheEntry entry = { result, d3dTex, GGC_GetTextureContentVersion(d3dTex) };
    m_textureCache[texture] = entry;
    return result;
}

namespace
{
    // TheSuperHackers @feature githubawn 20/07/2026 TextureFilterClass::TxtAddrMode
    // (texturefilter.h) only ever has two values -- TEXTURE_ADDRESS_REPEAT and
    // TEXTURE_ADDRESS_CLAMP, no mirrored option exists in this engine -- so this
    // translation is a plain 1:1 mapping, no default-case guessing involved.
    // Mirrors what BgfxBackend::Set_Texture (BgfxBackend.cpp ~line 5980-5997) does with the
    // exact same accessor: it turns TEXTURE_ADDRESS_CLAMP into a BGFX_SAMPLER_*_CLAMP flag and
    // leaves everything else as bgfx's implicit WRAP default. citro3d has an explicit GPU_REPEAT
    // enumerator instead of an implicit default, so this is spelled out as a full switch, but the
    // semantics (CLAMP vs. REPEAT) are identical.
    GPU_TEXTURE_WRAP_PARAM GGC_TranslateAddrMode(TextureFilterClass::TxtAddrMode mode)
    {
        return (mode == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) ? GPU_CLAMP_TO_EDGE : GPU_REPEAT;
    }
}

void Citro3dBackend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Backend::Set_Texture(stage, texture);

    if (!m_initialized || stage != 0)
    {
        return;
    }

    // TheSuperHackers @todo githubawn 20/07/2026 Shroud/fog-of-war (Capture_Shroud_Texture,
    // Set_Shroud_Texture_Pass_Active -- see BgfxBackendTextures.cpp's
    // BgfxBackend::Capture_Shroud_Texture and BgfxBackend::Set_Shroud_Texture_Pass_Active in
    // BgfxBackend.cpp ~line 6228) is NOT ported here, deliberately. This backend only ever binds
    // stage 0 (the early-return two lines up) and has no notion of a second, independently
    // combined texture stage anywhere in Draw_Triangles/Apply_Tex_Env. Porting it for real would
    // need: (1) an actual stage-1 bind path here (this function currently drops every stage != 0
    // silently); (2) a second citro3d TEV stage in Apply_Tex_Env cascading GPU_MODULATE of
    // GPU_PREVIOUS against the shroud texture, analogous to the light-color-constant stage
    // considered (and skipped, see its own @todo) in Draw_Triangles' hasDiffuse branch; (3) a
    // camera-space-derived shroud UV per vertex -- BgfxBackend computes this in its vertex
    // shader from TCI_CAMERASPACEPOSITION + a texture-matrix-derived offset/scale uniform
    // (BgfxBackend.cpp's shroudParams block, ~line 7819-7873), which fundamentally needs
    // per-vertex shader math this backend has no shader stage to run (vs_2d.v.pica is a fixed
    // pos/color/uv passthrough, shared with the 2D UI path, off-limits to editing here -- same
    // restriction noted throughout the lighting feature above); and (4) a Capture_Shroud_Texture
    // override handling PARTIAL sub-rect texture updates (dst_x/dst_y/pitch into an
    // already-uploaded texture) -- this backend's whole texture path (Ensure_Texture) only knows
    // how to re-swizzle and upload a texture IN FULL, so a partial update would need either a
    // persistent CPU-side shadow copy to patch-then-re-swizzle-in-full, or a genuinely new
    // partial-swizzle code path, and how often the game actually calls this per frame (fog of war
    // update frequency) is not known without live measurement. None of this is verifiable without
    // running on real hardware/an emulator, which this environment cannot do -- skipped rather
    // than risk a half-working shroud (wrong UVs, stale partial texture data, or a silently
    // no-op'd TEV stage) shipping unverified. Fog of war currently just does not render on this
    // backend; it does not crash or corrupt anything else.

    // TheSuperHackers @diagnostic githubawn 19/07/2026 Logs BOTH the raw TextureBaseClass*
    // AND a static_cast to TextureClass* alongside m_texturingEnabled at the moment this
    // specific texture gets bound. TextureClass uses multiple inheritance
    // (class TextureClass : public W3DMPO, public TextureBaseClass -- see texture.h), so a
    // TextureBaseClass* and the TextureClass* that owns it are NOT the same numeric address
    // (the compiler applies a base-subobject offset). W3DRadar.cpp logs m_terrainTexture as a
    // TextureClass*, so cross-referencing against that must use the asTextureClass value below,
    // not the raw texture= value -- an earlier round of this diagnostic compared the wrong
    // pointer and wrongly concluded Set_Texture was never called for the radar's texture.
    {
        static int s_ggcSetTexLogCount = 0;
        if (texture != nullptr && s_ggcSetTexLogCount < 3000)
        {
            ++s_ggcSetTexLogCount;
            SDL_Log("[ggc-settex] #%d texture=%p asTextureClass=%p texturingEnabled=%d",
                    s_ggcSetTexLogCount, (void*)texture, (void*)static_cast<TextureClass*>(texture), (int)m_texturingEnabled);
        }
    }

    m_currentTexture = Ensure_Texture(texture);

    // TheSuperHackers @feature githubawn 20/07/2026 Wrap/filter, read from the engine's
    // per-texture TextureFilterClass (texture->As_TextureClass()->Get_Filter(), texturefilter.h)
    // and re-applied every bind, replacing the GPU_CLAMP_TO_EDGE/GPU_NEAREST this used to hardcode
    // unconditionally for every texture at UPLOAD time in Ensure_Texture (see that function's own
    // comment). Applied HERE, at bind time, for the same reason BgfxBackend::Set_Texture captures
    // its own sampler flags per-bind rather than once at texture-creation time: the engine can
    // change an ALREADY-uploaded TextureClass's filter/address mode at any point after upload --
    // this is not hypothetical, render2dsentence.cpp's font glyph atlas does exactly that
    // (Set_U_Addr_Mode/Set_V_Addr_Mode(TEXTURE_ADDRESS_CLAMP) + Set_Min_Filter/Set_Mag_Filter(
    // FILTER_TYPE_NONE), all called once right after the atlas texture is constructed, well after
    // any earlier upload could have baked in something else) -- a value captured once at upload
    // would go stale the moment that happens. C3D_TexSetFilter/C3D_TexSetWrap (c3d/texture.h) are
    // both plain inline bitfield writes on the C3D_Tex struct itself (`tex->param &= ~mask; tex->
    // param |= ...`), not GPU calls, so redoing this on every single bind costs nothing measurable.
    //
    // Wrap: TxtAddrMode only has TEXTURE_ADDRESS_REPEAT/CLAMP (see GGC_TranslateAddrMode above,
    // no mirrored option exists in this engine) -- translated 1:1 to GPU_REPEAT/GPU_CLAMP_TO_EDGE.
    // This is the actual bug fix motivating this change: tiling terrain and most world materials
    // request REPEAT and were visibly wrong (edge-clamped/stretched instead of tiled) under the
    // old hardcoded CLAMP-for-everything.
    //
    // Filter: TextureFilterClass::FilterType (FILTER_TYPE_NONE/FAST/BEST/DEFAULT) is D3D's
    // capability-driven filter-QUALITY preset system -- texturefilter.cpp's _Init_Filters builds a
    // full lookup table keyed off actual D3D driver caps (anisotropy support, mipmap availability,
    // etc.) that this backend has no equivalent hardware-capability query for, and reproducing it
    // would be pure guesswork. Rather than build an unverifiable parallel table, only the ONE
    // signal that actually matters for this backend's fixed-function (point vs. bilinear) filter
    // choice is read directly: FILTER_TYPE_NONE (point filtering explicitly requested) maps to
    // GPU_NEAREST, anything else (FAST/BEST/DEFAULT -- "some filtering wanted") maps to GPU_LINEAR.
    // This is precisely how the font glyph atlas is identified -- not by guessing at its texture
    // FORMAT (A4R4G4B4 is not exclusive to the atlas) but by reading the exact FILTER_TYPE_NONE
    // setting render2dsentence.cpp itself applies for it (see the comment above), which is the
    // same reason the old hardcoded GPU_NEAREST "kept small UI text crisp" -- it now stays crisp
    // because the engine explicitly asks for point filtering there, not because every texture is
    // forced to it. Normal world/UI textures that don't request FILTER_TYPE_NONE now get the
    // smooth GPU_LINEAR they actually ask for instead.
    if (m_currentTexture != nullptr)
    {
        TextureClass * t2d = texture->As_TextureClass();
        if (t2d != nullptr)
        {
            C3D_Tex * ctex = reinterpret_cast<C3D_Tex *>(m_currentTexture);
            TextureFilterClass & filter = t2d->Get_Filter();

            C3D_TexSetWrap(ctex, GGC_TranslateAddrMode(filter.Get_U_Addr_Mode()),
                           GGC_TranslateAddrMode(filter.Get_V_Addr_Mode()));

            const bool pointFilter = (filter.Get_Min_Filter() == TextureFilterClass::FILTER_TYPE_NONE)
                || (filter.Get_Mag_Filter() == TextureFilterClass::FILTER_TYPE_NONE);
            const GPU_TEXTURE_FILTER_PARAM gpuFilter = pointFilter ? GPU_NEAREST : GPU_LINEAR;
            C3D_TexSetFilter(ctex, gpuFilter, gpuFilter);
        }
        // t2d == nullptr should not happen here -- Ensure_Texture already required
        // texture->As_TextureClass() to succeed to produce a non-null m_currentTexture -- but if
        // it ever does, leave whatever wrap/filter this C3D_Tex already carries rather than crash.
    }
}

RenderResource Citro3dBackend::Register_Loaded_Texture(TextureBaseClass * texture)
{
    // TheSuperHackers @bugfix githubawn 18/07/2026 See the header comment.
    // texture.cpp calls this the instant a texture finishes LOADING
    // (TextureBaseClass::Set_D3D_Base_Texture), well before match-load's
    // ~663 map objects are ever actually drawn. Triggering the upload here
    // (same Ensure_Texture path Set_Texture already uses at draw time) means
    // every texture's CPU-side decode scratch gets released right after
    // load instead of sitting retained for the rest of the load sequence.
    // This backend tracks textures in its own m_textureCache (keyed by
    // TextureBaseClass*, cleaned up via Release_Cached_Texture/Invalidate_
    // Cached_Texture), not the generic RenderResource handle system, so
    // there is nothing meaningful to return here -- kInvalidRenderResource
    // matches DX8Backend's own default and correctly skips the
    // Destroy_Resource call in Set_D3D_Base_Texture.
    Ensure_Texture(texture);
    return kInvalidRenderResource;
}

void Citro3dBackend::Release_Cached_Texture(TextureBaseClass * texture)
{
    DX8Backend::Release_Cached_Texture(texture);

    if (texture == nullptr)
    {
        return;
    }

    std::map<TextureBaseClass *, GGCTextureCacheEntry>::iterator found = m_textureCache.find(texture);
    if (found == m_textureCache.end())
    {
        return;
    }

    if (found->second.tex != nullptr)
    {
        C3D_Tex * tex = reinterpret_cast<C3D_Tex *>(found->second.tex);
        if (m_currentTexture == found->second.tex)
        {
            m_currentTexture = nullptr;
        }
        C3D_TexDelete(tex);
        delete tex;
    }
    m_textureCache.erase(found);
}

void Citro3dBackend::Invalidate_Cached_Texture(TextureBaseClass * texture)
{
    // TheSuperHackers @bugfix githubawn 16/07/2026 render2dsentence.cpp
    // calls this every time the font glyph atlas is rebuilt with newly
    // rasterized characters (same TextureBaseClass*, new pixel data). This
    // backend never overrode it, so Ensure_Texture's cache kept returning
    // whatever C3D_Tex was uploaded the FIRST time that atlas texture was
    // seen (likely near-empty, before most glyphs existed) forever after --
    // matching the observed symptom exactly (everything else in the menu
    // renders once the buffer-lifetime fix landed, but no text ever
    // appears). Evicting the cache entry (same logic as
    // Release_Cached_Texture, just without invalidating the key itself --
    // the TextureBaseClass is still alive, only its pixel data changed)
    // forces Ensure_Texture to re-create and re-upload from the current
    // D3D8 mirror next time this texture is bound.
    if (texture == nullptr)
    {
        return;
    }

    std::map<TextureBaseClass *, GGCTextureCacheEntry>::iterator found = m_textureCache.find(texture);
    if (found == m_textureCache.end())
    {
        return;
    }

    // TheSuperHackers @bugfix githubawn 20/07/2026 Do not throw away a GPU texture we would be
    // unable to rebuild. Ensure_Texture releases each texture's CPU-side pixels right after
    // uploading them (ReleaseTextureCpuScratch, a deliberate memory saving on this platform), and
    // StubD3D8Texture::LockRect then lazily hands back a ZEROED buffer if anything locks it again.
    // So for a scratch-released texture, honouring this invalidation means deleting a correct GPU
    // image and re-uploading all-zeros in its place -- observed as large UI images (menu backdrop,
    // load screen, score screen) appearing for a moment and then vanishing. Keeping the existing
    // upload is strictly better: it is the only copy of those pixels left in the process. Textures
    // that genuinely can be re-read -- the font glyph atlas (exempt from the release) and the
    // radar's textures (they hold a live GetSurfaceLevel surface, which also blocks the release) --
    // still have real scratch here, so the paths that legitimately rewrite their pixels and call
    // this are unaffected and still re-upload.
    if (found->second.tex != nullptr && !GGC_TextureHasCpuScratch(found->second.srcD3DTex))
    {
        return;
    }

    if (found->second.tex != nullptr)
    {
        C3D_Tex * tex = reinterpret_cast<C3D_Tex *>(found->second.tex);
        if (m_currentTexture == found->second.tex)
        {
            m_currentTexture = nullptr;
        }
        C3D_TexDelete(tex);
        delete tex;
    }
    m_textureCache.erase(found);
}

// TheSuperHackers @bugfix githubawn 20/07/2026 Root-cause investigation for the reported "opaque
// black box around the loading-screen logo" / "progress-bar blocks don't appear" bug: this
// function used to configure most presets via C3D_TexEnvSrc/C3D_TexEnvFunc(env, C3D_Both, ...),
// i.e. one shared op+source pair forced onto BOTH the RGB and Alpha TEV combiners. Cross-checked
// against the actual reference decode, BgfxBackend::BuildTssOpsForShader (BgfxBackend.cpp
// ~1367-1494) and the fs_uber.sc fragment shader it feeds (~245-338): bgfx ALSO computes the exact
// same op for color and alpha in the MODULATE, DISABLE, and MODULATE2X presets (only GRADIENT_ADD
// genuinely diverges: ADD for color, MODULATE for alpha) -- so forcing C3D_Both in those three
// cases was NOT, by itself, incorrect (it happened to already match bgfx bit-for-bit; the ADD case
// was already split before this change). The root-cause hypothesis as literally stated (single op
// applied to C3D_Both explains the bug) does NOT fully hold: the actual GPU state programmed here
// before this change was already correct for every reachable 2D UI preset. See this function's own
// diagnostic addition below (and the new [ggc-2dstate] log in Draw_Triangles) for what to check
// against a real run instead -- likely candidates left unruled-out: the alpha-test ref/direction,
// the texture's actual uploaded alpha channel (Ensure_Texture, out of scope for this change), or a
// blend-state bug outside this function.
//
// Ported anyway, per this task's explicit instruction to stop using C3D_Both structurally: RGB and
// Alpha are now ALWAYS decoded through their own independent C3D_RGB/C3D_Alpha calls below, even
// where the two happen to resolve to the same op+sources -- mirroring BuildTssOpsForShader's data
// flow, which always produces two independent priColorOp/priAlphaOp values from one
// Get_Primary_Gradient() read, never a single shared value. This makes the decode robust if a
// future preset (or a corrected understanding of an existing one) ever needs the two channels to
// differ, without having to restructure the whole function again.
//
// TheSuperHackers @todo githubawn 20/07/2026 Secondary/detail gradient (ShaderClass::
// Get_Secondary_Gradient() / Get_Post_Detail_Color_Func() / Get_Post_Detail_Alpha_Func()) is
// explicitly OUT OF SCOPE here, per this task -- low-detail 2D UI and the current 3D path never
// request it. If a shader ever does, this function silently falls back to the primary gradient's
// behavior alone (there is no read of those three accessors anywhere in this file), same as
// before this change. A real secondary stage would need a second citro3d TEV stage (C3D_GetTexEnv(1)
// cascading off GPU_PREVIOUS) -- see the existing @todo in Set_Texture and Draw_Triangles'
// hasDiffuse branch for why that was already deferred elsewhere in this backend.
void Citro3dBackend::Apply_Tex_Env(bool texturing_enabled)
{
    C3D_TexEnv * env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);

    // TheSuperHackers @diagnostic githubawn 19/07/2026 REVERTED the previous per-call log here --
    // it logged all 20000 of its capped entries in a single session (every Draw_Triangles call
    // hits this), and was the dominant contributor to a reported game-wide slowdown after that
    // build. Also, on reflection the data it did capture (texturing_enabled=0 with a non-null
    // m_currentTexture, repeated for the same pointer) is NOT necessarily a bug by itself:
    // ShaderClass::Uses_Texture() reflects a genuine per-draw TEXTURING_DISABLE directive from the
    // game (shader.h:275-276), and DX8Backend::Set_Texture leaves m_currentTexture holding
    // whatever was last bound regardless of the CURRENT draw's shader -- so "texture bound, shader
    // says don't use it" is completely normal for plenty of legitimate untextured draws (borders,
    // debug lines, etc), not evidence of a state-sync bug. Replaced with a much lower-volume,
    // pointer-identifiable diagnostic in Set_Texture below instead (logs the ORIGINAL
    // TextureBaseClass* before Ensure_Texture translates it to a GPU handle, so it can be
    // cross-referenced directly against W3DRadar.cpp's own logged m_terrainTexture/m_overlayTexture/
    // m_shroudTexture pointers -- Apply_Tex_Env's m_currentTexture is a translated C3D_Tex* in a
    // different address space and can't be matched against those). The new [ggc-2dstate] log added
    // to Draw_Triangles (see its own comment) is the low-volume, per-DRAW-not-per-pointer answer to
    // the same "what state did this draw actually use" question, scoped to this feature's own bug.

    if (texturing_enabled && m_currentTexture != nullptr)
    {
        C3D_TexBind(0, reinterpret_cast<C3D_Tex *>(m_currentTexture));

        // TheSuperHackers @feature githubawn 20/07/2026 Honor ShaderClass::Get_Primary_Gradient()
        // (shader.h) instead of hardwiring MODULATE for every textured draw -- GRADIENT_DISABLE
        // (texture only, D3D's "decal" blend) and GRADIENT_ADD/GRADIENT_MODULATE2X are all real
        // presets used outside the 2D UI path (water/detail blends, etc). GRADIENT_BUMPENVMAP(
        // LUMINANCE) has no fixed-function TEV equivalent and falls through to the MODULATE
        // default below, same as before this change.
        //
        // RGB and Alpha op+source are independent locals, mirroring BuildTssOpsForShader's
        // priColorOp/priAlphaOp pair (see this function's header comment) -- set together per
        // case below, then applied through separate C3D_RGB/C3D_Alpha calls, never C3D_Both.
        GPU_COMBINEFUNC rgbFunc = GPU_MODULATE;
        GPU_COMBINEFUNC alphaFunc = GPU_MODULATE;
        GPU_TEVSCALE rgbScale = GPU_TEVSCALE_1;

        switch (m_primaryGradient)
        {
            case ShaderClass::GRADIENT_DISABLE:
                // BuildTssOpsForShader: priColorOp=priAlphaOp=SELECTARG1 (fs_uber.sc's own
                // SELECTARG1 fast path, ~267-275: current=tex0, diffuse never touched at all).
                // GPU_REPLACE selects TEV source 1 (GPU_TEXTURE0, set below) unmodified for
                // both channels -- texture RGBA only, no vertex-color tint.
                rgbFunc = GPU_REPLACE;
                alphaFunc = GPU_REPLACE;
                break;
            case ShaderClass::GRADIENT_ADD:
                // BuildTssOpsForShader: priColorOp=ADD (tex.rgb + diffuse.rgb), priAlphaOp=
                // MODULATE (tex.a * diffuse.a) -- the one reachable preset where bgfx's two
                // channels genuinely differ from each other.
                rgbFunc = GPU_ADD;
                alphaFunc = GPU_MODULATE;
                break;
            case ShaderClass::GRADIENT_MODULATE2X:
                // BuildTssOpsForShader: priColorOp=MODULATE2X, priAlphaOp=MODULATE (NOT 2x --
                // fs_uber.sc never doubles alpha for this preset, only RGB, via the TevScale
                // below).
                rgbFunc = GPU_MODULATE;
                rgbScale = GPU_TEVSCALE_2;
                alphaFunc = GPU_MODULATE;
                break;
            case ShaderClass::GRADIENT_MODULATE:
            default:
                // BuildTssOpsForShader: priColorOp=priAlphaOp=MODULATE -- fs_uber.sc's own fast
                // path comment calls this "~80% of draws" (tex * diffuse, both channels).
                rgbFunc = GPU_MODULATE;
                alphaFunc = GPU_MODULATE;
                break;
        }

        // TEVOP defaults (SRC_COLOR / SRC_ALPHA, set by C3D_TexEnvInit above and never
        // overridden here) mean each source below contributes its own RGB or Alpha component
        // respectively -- no operand inversion needed for any of the four presets above.
        C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_RGB, rgbFunc);
        C3D_TexEnvScale(env, C3D_RGB, rgbScale);

        C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Alpha, alphaFunc);
    }
    else
    {
        // TheSuperHackers @feature githubawn 20/07/2026 Texturing disabled, or no texture bound:
        // vertex color (diffuse) only. BuildTssOpsForShader's Get_Texturing()==TEXTURING_DISABLE
        // branch (~1479-1493) technically decodes GRADIENT_DISABLE to priColorOp=priAlphaOp=
        // DISABLE (not SELECTARG2 like MODULATE/ADD there) -- but fs_uber.sc's general-path
        // handling of op==DISABLE (~299-303: "priColor=diffuse.rgb; priAlpha=diffuse.a") produces
        // the exact same diffuse-only result as SELECTARG2 does, so EVERY reachable
        // Get_Texturing()==DISABLE case ends up here regardless of which primary gradient was
        // requested -- there is nothing to switch on. RGB and Alpha are still routed through the
        // separate C3D_RGB/C3D_Alpha calls for consistency with the textured branch above, rather
        // than collapsed back to one C3D_Both call.
        C3D_TexEnvSrc(env, C3D_RGB, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);

        C3D_TexEnvSrc(env, C3D_Alpha, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
    }
}

// -----------------------------------------------------------------------------
// Dynamic vertex/index capture (Render2DClass's DynamicVBAccessClass /
// DynamicIBAccessClass path)
// -----------------------------------------------------------------------------
//
// TheSuperHackers @info githubawn 15/07/2026 Single-buffered: each capture
// frees the previous linearAlloc and allocates fresh. Render2DClass does one
// lock -> write -> unlock -> draw sequence per Render() call with nothing
// else capturing in between, so there is exactly one live capture at a time
// for the 2D UI path this milestone targets. A persistent ring buffer (to
// avoid the per-draw linearAlloc/linearFree churn) is a perf follow-up, not
// a correctness requirement, once real per-frame draw counts are measured.

void Citro3dBackend::Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * /*vba*/,
                                                 const void * data,
                                                 unsigned int size_bytes)
{
    if (!m_initialized || data == nullptr || size_bytes == 0)
    {
        return;
    }

    if (m_dynamicVertexData != nullptr)
    {
        // TheSuperHackers @bugfix githubawn 16/07/2026 Do not free
        // immediately -- a GPU command from an earlier draw call THIS SAME
        // frame may still reference it (see m_pendingFrees in
        // Citro3dBackend.h). Deferred to the next Begin_Scene instead.
        m_pendingFrees.push_back(m_dynamicVertexData);
    }
    m_dynamicVertexData = linearAlloc(size_bytes);
    if (m_dynamicVertexData == nullptr)
    {
        m_dynamicVertexSizeBytes = 0;
        return;
    }
    std::memcpy(m_dynamicVertexData, data, size_bytes);
    m_dynamicVertexSizeBytes = size_bytes;
}

void Citro3dBackend::Capture_Dynamic_Index_Data(const DynamicIBAccessClass * /*iba*/,
                                                const void * data,
                                                unsigned int size_bytes)
{
    if (!m_initialized || data == nullptr || size_bytes == 0)
    {
        return;
    }

    if (m_dynamicIndexData != nullptr)
    {
        // See the matching comment in Capture_Dynamic_Vertex_Data above.
        m_pendingFrees.push_back(m_dynamicIndexData);
    }
    m_dynamicIndexData = linearAlloc(size_bytes);
    if (m_dynamicIndexData == nullptr)
    {
        m_dynamicIndexSizeBytes = 0;
        return;
    }
    std::memcpy(m_dynamicIndexData, data, size_bytes);
    m_dynamicIndexSizeBytes = size_bytes;
}

void Citro3dBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    DX8Backend::Set_Vertex_Buffer(vba);
    // Data already captured via Capture_Dynamic_Vertex_Data at unlock time
    // (called before this); nothing further to bind here.

    // TheSuperHackers @feature githubawn 20/07/2026 See m_usingDynamicVertexBuffer's header
    // comment -- Draw_Triangles' sorted-batch branch needs this to tell
    // SortingRendererClass::Flush_Sorting_Pool's combined dynamic-buffer draws apart from
    // Flush()'s single-node static-buffer sorted fallback (both wrapped in the same
    // Begin/End_Sorted_Batch_Pass pair).
    m_usingDynamicVertexBuffer = true;
}

void Citro3dBackend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    DX8Backend::Set_Index_Buffer(iba, index_base_offset);
    m_indexBaseOffset = index_base_offset;
}

// -----------------------------------------------------------------------------
// 3D world draw path (Phase 3 Milestone 4): static vertex/index buffers
// -----------------------------------------------------------------------------

void Citro3dBackend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    DX8Backend::Set_Vertex_Buffer(vb, stream);

    // TheSuperHackers @feature githubawn 20/07/2026 See m_usingDynamicVertexBuffer's header
    // comment -- this static overload is the "not dynamic" half of that flag, set
    // unconditionally (even on the early-outs below) since it reflects the ENGINE'S intent for
    // this bind, not whether this backend's own capture happened to succeed.
    m_usingDynamicVertexBuffer = false;

    if (!m_initialized || vb == nullptr)
    {
        m_currentVertexData = nullptr;
        m_currentVertexStride = 0;
        m_currentVertexSizeBytes = 0;
        return;
    }

    const unsigned int expectedStride = vb->FVF_Info().Get_FVF_Size();
    const unsigned int expectedBytes = static_cast<unsigned int>(vb->Get_Vertex_Count()) * expectedStride;

    std::map<const VertexBufferClass *, GGCStaticBufferEntry>::iterator found = m_staticVBCache.find(vb);
    if (found != m_staticVBCache.end())
    {
        // TheSuperHackers @bugfix githubawn 18/07/2026 This cache is keyed by
        // raw VertexBufferClass pointer with no destroy-time invalidation
        // (see the header comment). If a VertexBufferClass is destroyed and a
        // NEW, differently-sized one is allocated at the SAME address --
        // routine heap churn during a busy load, not a rare edge case --
        // this cache hit would silently hand out a buffer sized for the OLD
        // mesh. C3D_DrawElements would then read past the end of that
        // (smaller) linearAlloc'd buffer for the new mesh's actual vertex
        // count, an out-of-bounds GPU vertex fetch that can corrupt whatever
        // linear-heap memory follows it. Treat a size mismatch as a cache
        // miss and recapture instead of trusting a stale entry.
        if (found->second.sizeBytes == expectedBytes)
        {
            m_currentVertexData = found->second.data;
            m_currentVertexStride = expectedStride;
            m_currentVertexSizeBytes = found->second.sizeBytes;
            return;
        }
        linearFree(found->second.data);
        m_staticVBCache.erase(found);
    }

    // On-demand capture: lock the stub D3D8 vertex buffer's CPU-side scratch
    // (see StubD3D8Device.cpp) and copy it into a linearAlloc'd (GPU-DMA
    // visible) buffer, once, cached by engine pointer thereafter.
    if (vb->Type() != BUFFER_TYPE_DX8)
    {
        m_currentVertexData = nullptr;
        m_currentVertexStride = 0;
        m_currentVertexSizeBytes = 0;
        return;
    }

    DX8VertexBufferClass * dx8vb = static_cast<DX8VertexBufferClass *>(const_cast<VertexBufferClass *>(vb));
    IDirect3DVertexBuffer8 * d3dvb = dx8vb->Get_DX8_Vertex_Buffer();
    if (d3dvb == nullptr)
    {
        m_currentVertexData = nullptr;
        m_currentVertexStride = 0;
        m_currentVertexSizeBytes = 0;
        return;
    }

    const unsigned int stride = expectedStride;
    const unsigned int bytes = expectedBytes;
    if (bytes == 0)
    {
        m_currentVertexData = nullptr;
        m_currentVertexStride = 0;
        m_currentVertexSizeBytes = 0;
        return;
    }

    BYTE * srcData = nullptr;
    if (FAILED(d3dvb->Lock(0, bytes, &srcData, D3DLOCK_READONLY)) || srcData == nullptr)
    {
        m_currentVertexData = nullptr;
        m_currentVertexStride = 0;
        m_currentVertexSizeBytes = 0;
        return;
    }

    void * gpuData = linearAlloc(bytes);
    if (gpuData == nullptr)
    {
        // TheSuperHackers @diagnostic githubawn 18/07/2026 Previously silent
        // -- linearAlloc (linear/GPU heap) failures have zero visibility
        // anywhere else in this codebase, unlike the general heap's
        // AllocScratch FATAL diagnostic (StubD3D8Device.cpp). Log so a
        // linear-heap exhaustion is directly diagnosable instead of just
        // "this mesh silently doesn't render."
        SDL_Log("[ggc-linalloc] FAILED vertex buffer alloc bytes=%u linearFree=%u", bytes, (unsigned)linearSpaceFree());
        d3dvb->Unlock();
        m_currentVertexData = nullptr;
        m_currentVertexStride = 0;
        m_currentVertexSizeBytes = 0;
        return;
    }
    std::memcpy(gpuData, srcData, bytes);
    d3dvb->Unlock();

    // TheSuperHackers @bugfix githubawn 17/07/2026 Without this, every static
    // mesh's vertex data is double-stored for its whole lifetime -- once in
    // the stub D3D8 buffer's own permanent scratch (general heap), once in
    // gpuData just copied above (linear heap) -- the same class of OOM bug
    // ReleaseTextureCpuScratch fixed for textures, just for geometry. Static
    // mesh buffers are write-once (unlike the 2D dynamic path), so this is
    // safe the same way the texture release was.
    ReleaseVertexBufferCpuScratch(d3dvb);

    GGCStaticBufferEntry entry = { gpuData, bytes };
    m_staticVBCache[vb] = entry;
    m_currentVertexData = gpuData;
    m_currentVertexStride = stride;
    m_currentVertexSizeBytes = bytes;
}

// TheSuperHackers @bugfix githubawn 20/07/2026 See m_indexBaseOffset's declaration:
// the engine calls this between draws that share one vertex buffer to select which
// mesh part's vertices the next draw's indices resolve against (a D3D8
// BaseVertexIndex). Citro3dBackend never overrode it, so every part after the first
// drew against the first part's vertices.
void Citro3dBackend::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
    DX8Backend::Set_Index_Buffer_Index_Offset(offset);
    m_indexBaseOffset = offset;
}

void Citro3dBackend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    DX8Backend::Set_Index_Buffer(ib, index_base_offset);
    m_indexBaseOffset = index_base_offset;

    if (!m_initialized || ib == nullptr)
    {
        m_currentIndexData = nullptr;
        m_currentIndexSizeBytes = 0;
        return;
    }

    const unsigned int expectedBytes = static_cast<unsigned int>(ib->Get_Index_Count()) * sizeof(unsigned short);

    std::map<const IndexBufferClass *, GGCStaticBufferEntry>::iterator found = m_staticIBCache.find(ib);
    if (found != m_staticIBCache.end())
    {
        // See the matching ReleaseVertexBufferCpuScratch/stale-cache comment
        // in Set_Vertex_Buffer above -- same pointer-reuse risk here.
        if (found->second.sizeBytes == expectedBytes)
        {
            m_currentIndexData = found->second.data;
            m_currentIndexSizeBytes = found->second.sizeBytes;
            return;
        }
        linearFree(found->second.data);
        m_staticIBCache.erase(found);
    }

    if (ib->Type() != BUFFER_TYPE_DX8)
    {
        m_currentIndexData = nullptr;
        m_currentIndexSizeBytes = 0;
        return;
    }

    DX8IndexBufferClass * dx8ib = static_cast<DX8IndexBufferClass *>(const_cast<IndexBufferClass *>(ib));
    IDirect3DIndexBuffer8 * d3dib = dx8ib->Get_DX8_Index_Buffer();
    if (d3dib == nullptr)
    {
        m_currentIndexData = nullptr;
        m_currentIndexSizeBytes = 0;
        return;
    }

    const unsigned int bytes = expectedBytes;
    if (bytes == 0)
    {
        m_currentIndexData = nullptr;
        m_currentIndexSizeBytes = 0;
        return;
    }

    BYTE * srcData = nullptr;
    if (FAILED(d3dib->Lock(0, bytes, &srcData, D3DLOCK_READONLY)) || srcData == nullptr)
    {
        m_currentIndexData = nullptr;
        m_currentIndexSizeBytes = 0;
        return;
    }

    void * gpuData = linearAlloc(bytes);
    if (gpuData == nullptr)
    {
        SDL_Log("[ggc-linalloc] FAILED index buffer alloc bytes=%u linearFree=%u", bytes, (unsigned)linearSpaceFree());
        d3dib->Unlock();
        m_currentIndexData = nullptr;
        m_currentIndexSizeBytes = 0;
        return;
    }
    std::memcpy(gpuData, srcData, bytes);
    d3dib->Unlock();

    // See the matching ReleaseVertexBufferCpuScratch comment above.
    ReleaseIndexBufferCpuScratch(d3dib);

    GGCStaticBufferEntry entry = { gpuData, bytes };
    m_staticIBCache[ib] = entry;
    m_currentIndexData = gpuData;
    m_currentIndexSizeBytes = bytes;
}

// TheSuperHackers @feature githubawn 19/07/2026 Destroy-time invalidation for
// m_staticVBCache, called from VertexBufferClass::~VertexBufferClass (see
// dx8vertexbuffer.cpp) before the D3D8 vertex buffer is released. Mirrors
// Release_Cached_Texture: erase the cache entry so a later VertexBufferClass
// allocated at the same address cannot alias a stale GPU-visible copy (ABA).
// The GPU may still be drawing with this frame's command list built against
// the old data, so the linearAlloc'd copy is queued on m_pendingFrees (freed
// at the next Begin_Scene, same as the dynamic VB/IB path) rather than freed
// immediately.
void Citro3dBackend::Release_Cached_Vertex_Buffer(const VertexBufferClass * vb)
{
    DX8Backend::Release_Cached_Vertex_Buffer(vb);

    if (vb == nullptr)
    {
        return;
    }

    std::map<const VertexBufferClass *, GGCStaticBufferEntry>::iterator found = m_staticVBCache.find(vb);
    if (found == m_staticVBCache.end())
    {
        return;
    }

    if (found->second.data != nullptr)
    {
        if (m_currentVertexData == found->second.data)
        {
            m_currentVertexData = nullptr;
            m_currentVertexStride = 0;
        m_currentVertexSizeBytes = 0;
        }
        m_pendingFrees.push_back(found->second.data);
    }
    m_staticVBCache.erase(found);
}

// TheSuperHackers @feature githubawn 19/07/2026 Same as
// Release_Cached_Vertex_Buffer above, but for m_staticIBCache, called from
// IndexBufferClass::~IndexBufferClass.
void Citro3dBackend::Release_Cached_Index_Buffer(const IndexBufferClass * ib)
{
    DX8Backend::Release_Cached_Index_Buffer(ib);

    if (ib == nullptr)
    {
        return;
    }

    std::map<const IndexBufferClass *, GGCStaticBufferEntry>::iterator found = m_staticIBCache.find(ib);
    if (found == m_staticIBCache.end())
    {
        return;
    }

    if (found->second.data != nullptr)
    {
        if (m_currentIndexData == found->second.data)
        {
            m_currentIndexData = nullptr;
            m_currentIndexSizeBytes = 0;
        }
        m_pendingFrees.push_back(found->second.data);
    }
    m_staticIBCache.erase(found);
}

// -----------------------------------------------------------------------------
// 3D world draw path (Phase 3 Milestone 4): transforms
// -----------------------------------------------------------------------------
//
// TheSuperHackers @bugfix githubawn 20/07/2026 Corrected convention (the
// @info below this from 17/07/2026 was wrong -- see GGC_BuildWorldViewProjC3D
// for the fix). The engine's Matrix4x4/Matrix3D (matrix3d.h/matrix4.h) are
// COLUMN-vector matrices: a point transforms as v' = M * v (v a column
// vector), with translation stored in column 3 of the row-major [row][col]
// layout. dx8wrapper.cpp transposes these before handing them to real
// Direct3D (which is what actually wants row vectors) -- this backend never
// goes through D3D, so that transpose step simply never happens here, and
// none should be added. citro3d's own vertex shader (see
// shaders_3ds/vs_2d.v.pica, reused here since its 4-attribute layout and
// per-row dp4 pattern is not specific to 2D) computes out[i] =
// dot(transform.r[i], v), i.e. out = M * v_col -- the EXACT SAME
// column-vector convention the engine's matrices are already in. So
// citro3d's "transform" uniform just needs to equal Projection*View*World,
// composed directly in the engine's own row-major layout with no transpose
// at all, plus two PICA200-specific corrections that have nothing to do with
// row/column convention: the 90-degree portrait screen tilt the 2D path gets
// for free from Mtx_OrthoTilt (there is no such call for the 3D projection,
// so it is folded in by hand), and remapping D3D's [0,1] clip-space depth
// range to PICA200's [-1,0]. Cached here as flat row-major float[16]
// (m_xMtx[row*4+col] = m[row][col], the engine's own layout) and combined
// (no transpose) once per draw in Draw_Triangles.

namespace
{
    void GGC_StoreMatrix4x4(const Matrix4x4 & m, float * out)
    {
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                out[r * 4 + c] = m[r][c];
            }
        }
    }

    void GGC_StoreMatrix3D(const Matrix3D & m, float * out)
    {
        for (int r = 0; r < 3; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                out[r * 4 + c] = m[r][c];
            }
        }
        out[12] = 0.0f; out[13] = 0.0f; out[14] = 0.0f; out[15] = 1.0f;
    }
}

void Citro3dBackend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    DX8Backend::Set_Transform(transform, m);
    switch (transform)
    {
        case RB_TRANSFORM_WORLD:      GGC_StoreMatrix4x4(m, m_worldMtx); break;
        case RB_TRANSFORM_VIEW:       GGC_StoreMatrix4x4(m, m_viewMtx); break;
        case RB_TRANSFORM_PROJECTION: GGC_StoreMatrix4x4(m, m_projMtx); break;
        default: break;
    }
}

void Citro3dBackend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    DX8Backend::Set_Transform(transform, m);
    switch (transform)
    {
        case RB_TRANSFORM_WORLD: GGC_StoreMatrix3D(m, m_worldMtx); break;
        case RB_TRANSFORM_VIEW:  GGC_StoreMatrix3D(m, m_viewMtx); break;
        default: break;
    }
}

void Citro3dBackend::Set_World_Identity()
{
    DX8Backend::Set_World_Identity();
    for (int i = 0; i < 16; ++i)
    {
        m_worldMtx[i] = ((i % 5) == 0) ? 1.0f : 0.0f;
    }
}

void Citro3dBackend::Set_View_Identity()
{
    DX8Backend::Set_View_Identity();
    for (int i = 0; i < 16; ++i)
    {
        m_viewMtx[i] = ((i % 5) == 0) ? 1.0f : 0.0f;
    }
}

void Citro3dBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar)
{
    DX8Backend::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);
    GGC_StoreMatrix4x4(matrix, m_projMtx);
}

// -----------------------------------------------------------------------------
// Sorted / translucent draw path
// -----------------------------------------------------------------------------
//
// TheSuperHackers @feature githubawn 20/07/2026 SortingRendererClass (sortingrenderer.cpp)
// drives sorted/translucent geometry -- particles, explosions, unit health bars, translucent
// decals, all present at low detail -- through TWO distinct call shapes, both of which need
// this backend's help:
//
// 1) Flush_Sorting_Pool(): pools every node whose VB/IB are BOTH sorting-type CPU arrays,
//    copies them all into ONE combined DynamicVBAccessClass/DynamicIBAccessClass (bound via
//    the existing Set_Vertex_Buffer(const DynamicVBAccessClass&)/Set_Index_Buffer(const
//    DynamicIBAccessClass&,...) overloads -- i.e. the SAME per-frame dynamic-buffer capture the
//    2D UI path already uses), globally depth-sorts the triangles, then wraps the whole flush in
//    ONE Begin_Sorted_Batch_Pass()/End_Sorted_Batch_Pass() pair and calls the ordinary 4-arg
//    g_renderBackend->Draw_Triangles(...) once per contiguous same-render-state run (see
//    Draw_Sorted_Run/Apply_Render_State there) -- NOT a separate submit function.
// 2) Flush()'s non-pooled branch: for a single node whose VB/IB are NOT both sorting-type (an
//    ordinary static mesh part that merely needs sorted draw ORDER, e.g. a translucent mesh
//    part), it binds the real static VertexBufferClass*/IndexBufferClass* (the existing 3D
//    Set_Vertex_Buffer/Set_Index_Buffer overloads) and wraps a single Begin/Capture/
//    Draw_Triangles/End sequence around just that one node.
//
// Both shapes call Apply_Render_State/set up the batch's shader/material/texture through the
// ordinary Set_Shader/Set_Material/Set_Texture overrides (already handled correctly), but BOTH
// also set the D3D world/view transform via a DIRECT DX8Wrapper::_Set_DX8_Transform call (or,
// for Flush()'s branch, no transform call at all besides Capture_Sorted_Batch_Transforms) --
// bypassing Set_Transform entirely, so m_worldMtx/m_viewMtx are never updated for either shape
// and would be stale (whatever the last ordinary opaque 3D draw left them at). This is the
// engine's OWN documented reason for these hooks (see their bgfx-side comments,
// BgfxBackend.cpp ~4433-4447): Capture_Sorted_Batch_Transforms is the dedicated side channel
// that hands the correct per-batch world/view to a backend instead.
//
// m_inSortedBatchPass distinguishes "we are inside one of the two shapes above" for
// Draw_Triangles (see its own comment), but does NOT by itself say which vertex/index buffer to
// read -- that is m_usingDynamicVertexBuffer's job (set by whichever Set_Vertex_Buffer overload
// last ran, which always immediately precedes the draw it feeds).

void Citro3dBackend::Begin_Sorted_Batch_Pass()
{
    m_inSortedBatchPass = true;
}

void Citro3dBackend::End_Sorted_Batch_Pass()
{
    m_inSortedBatchPass = false;
}

void Citro3dBackend::Capture_Sorted_Batch_Transforms(const Matrix4x4 & world, const Matrix4x4 & view)
{
    // Stored in the exact same row-major float[16] convention as m_worldMtx/m_viewMtx (see
    // GGC_StoreMatrix4x4 above) so Draw_Triangles' GGC_BuildWorldViewProjC3D call can consume
    // either pair identically -- no separate combine/transpose step needed here, unlike
    // BgfxBackend::Capture_Sorted_Batch_Transforms (BgfxBackend.cpp ~4463), which pre-multiplies
    // world*view together because bgfx's dedicated sort VIEW id carries a fixed identity view
    // matrix set once at init. This backend has no equivalent per-view state -- Draw_Triangles
    // always recomposes proj*view*world fresh per draw regardless of which world/view pair feeds
    // it -- so world and view are kept separate here, exactly like the ordinary (non-sorted) path.
    GGC_StoreMatrix4x4(world, m_sortWorldMtx);
    GGC_StoreMatrix4x4(view, m_sortViewMtx);
}

void Citro3dBackend::Capture_Sorted_Batch_Light(const RenderBackendLight & light, bool enabled)
{
    // TheSuperHackers @feature githubawn 20/07/2026 See m_sortHasDominantLight/
    // m_sortDominantLightColor's header comment -- captured for parity with
    // BgfxBackend::Capture_Sorted_Batch_Light (BgfxBackend.cpp ~4500) but not currently consumed:
    // SortingRendererClass's sorted geometry is always the 44-byte VertexFormatXYZNDUV2 dynamic
    // buffer, which already carries real per-vertex diffuse that Draw_Triangles' sorted-batch
    // branch (and the ordinary hasDiffuse 3D branch it mirrors) uses as-is, with no flat-light
    // combine. Deliberately stored in SEPARATE members from m_dominantLightColor/
    // m_hasDominantLight rather than overwriting them, so this can never leak into and corrupt
    // the ordinary opaque-3D flat-light combine for draws that follow this sort batch in the same
    // frame.
    m_sortHasDominantLight = enabled;
    if (enabled)
    {
        m_sortDominantLightColor[0] = light.diffuse[0];
        m_sortDominantLightColor[1] = light.diffuse[1];
        m_sortDominantLightColor[2] = light.diffuse[2];
    }
}

// TheSuperHackers @feature githubawn 20/07/2026 Submit_Sorted_Draw is defined further below
// (after the GGC_BuildWorldViewProjC3D/GGC_TranslateDepthCompareInverted helpers it needs,
// which are declared in the anonymous namespace right above Draw_Triangles), not here alongside
// the rest of this section's hooks -- C++ has no forward declaration for a same-file anonymous-
// namespace free function, so it must physically follow them. See the comment immediately above
// Draw_Triangles for Submit_Sorted_Draw's own documentation.

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

namespace
{
    // Row-major 4x4 multiply: out = a * b (engine/D3D convention, all three
    // in the engine's own [row][col] layout).
    void GGC_Mul4x4(const float * a, const float * b, float * out)
    {
        float r[16];
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    sum += a[row * 4 + k] * b[k * 4 + col];
                }
                r[row * 4 + col] = sum;
            }
        }
        std::memcpy(out, r, sizeof(r));
    }

    // TheSuperHackers @bugfix githubawn 20/07/2026 Rewritten -- the previous version composed
    // World*View*Projection in D3D row-vector order (v' = v*W*V*P) and then transposed the
    // combined result before handing it to citro3d. That was wrong on two counts: (1) transposing
    // a *product* of matrices does not, in general, equal recombining the same three matrices in
    // reverse order (that identity only holds value-for-value if you both reverse the order AND
    // transpose each factor: (ABC)^T = C^T*B^T*A^T, not C*B*A), so the old code did not actually
    // produce P*V*W and instead produced garbage; (2) per the column-vector explanation in the
    // block comment above, no transpose was ever needed in the first place -- the engine's
    // matrices are already in exactly the convention citro3d wants. Fixed by composing
    // Projection*View*World directly, in the engine's own row-major layout, with GGC_Mul4x4's
    // existing "out = a*b" semantics and no transpose anywhere.
    void GGC_BuildWorldViewProjC3D(const float * world, const float * view, const float * proj, C3D_Mtx * out)
    {
        float pv[16];
        float pvw[16];
        GGC_Mul4x4(proj, view, pv);
        GGC_Mul4x4(pv, world, pvw);

        // D3D -> PICA200 depth remap: Matrix4x4 projection matrices (see Set_Projection_Transform_
        // With_Z_Bias) are built for D3D's clip-space z/w range of [0,1]; PICA200 wants [-1,0].
        // Subtracting row 3 (w) from row 2 (z) maps z' = z-w, so after the perspective divide
        // z'/w = z/w - 1, taking [0,1] to [-1,0] as required. Row 3 itself is untouched by this,
        // so doing the subtraction in place (rather than into a separate local first) is safe --
        // nothing below reads the pre-subtraction row 2.
        for (int c = 0; c < 4; ++c)
        {
            pvw[2 * 4 + c] -= pvw[3 * 4 + c];
        }

        // Portrait screen tilt: the 2D UI path gets this for free from Mtx_OrthoTilt (see
        // Draw_Triangles' is2D branch), but there is no equivalent ortho-style helper for an
        // arbitrary combined 3D transform, so the same rotation is folded in here by hand.
        // citro3d's mtx_ortho tilt source rotates output x from input y and output y from
        // negated input x -- i.e. out.r[0] takes pvw's row 1, out.r[1] takes pvw's NEGATED row 0.
        // Rows 2 (depth, already remapped above) and 3 (w) pass through unrotated.
        out->r[0] = FVec4_New(pvw[1 * 4 + 0], pvw[1 * 4 + 1], pvw[1 * 4 + 2], pvw[1 * 4 + 3]);
        out->r[1] = FVec4_New(-pvw[0 * 4 + 0], -pvw[0 * 4 + 1], -pvw[0 * 4 + 2], -pvw[0 * 4 + 3]);
        out->r[2] = FVec4_New(pvw[2 * 4 + 0], pvw[2 * 4 + 1], pvw[2 * 4 + 2], pvw[2 * 4 + 3]);
        out->r[3] = FVec4_New(pvw[3 * 4 + 0], pvw[3 * 4 + 1], pvw[3 * 4 + 2], pvw[3 * 4 + 3]);
    }

    // TheSuperHackers @feature githubawn 20/07/2026 Translates ShaderClass::DepthCompareType
    // (shader.h) to citro3d's GPU_TESTFUNC for Draw_Triangles' 3D branch, INVERTING the compare
    // direction: the D3D->PICA depth remap in GGC_BuildWorldViewProjC3D above maps D3D's
    // near=0/far=1 clip range to PICA's near=-1/far=0, and citro3d's default depth map (scale
    // -1, offset 0 -- this backend never calls C3D_DepthMap) then stores -z_ndc in the depth
    // buffer: near=1, far=0. D3D's buffer ordering is the opposite (near=0, far=1, smaller is
    // nearer), so on this backend LARGER stored depth is nearer -- consistent with the existing
    // clear-to-0 + GEQUAL convention this path already used. ShaderClass presets (e.g.
    // PASS_LEQUAL, the default opaque-geometry
    // test) are written assuming D3D's ordering, so preserving their intended MEANING ("nearer or
    // equal wins") on this backend requires swapping each direction-sensitive comparison to its
    // mirror image. EQUAL/NOTEQUAL/ALWAYS/NEVER are direction-independent and pass through as-is.
    GPU_TESTFUNC GGC_TranslateDepthCompareInverted(int depthCompare)
    {
        switch (static_cast<ShaderClass::DepthCompareType>(depthCompare))
        {
            case ShaderClass::PASS_NEVER:    return GPU_NEVER;
            case ShaderClass::PASS_LESS:     return GPU_GREATER;
            case ShaderClass::PASS_EQUAL:    return GPU_EQUAL;
            case ShaderClass::PASS_LEQUAL:   return GPU_GEQUAL;
            case ShaderClass::PASS_GREATER:  return GPU_LESS;
            case ShaderClass::PASS_NOTEQUAL: return GPU_NOTEQUAL;
            case ShaderClass::PASS_GEQUAL:   return GPU_LEQUAL;
            case ShaderClass::PASS_ALWAYS:
            default:                         return GPU_ALWAYS;
        }
    }
}

// TheSuperHackers @feature githubawn 20/07/2026 DX8Wrapper::Draw_Sorting_IB_VB's direct-bind
// sorting-VB/IB path (dx8wrapper.cpp ~2138), reached from DX8Wrapper::Draw() only when a caller
// has bound BUFFER_TYPE_SORTING/DYNAMIC_SORTING vertex+index buffers directly WITHOUT going
// through SortingRendererClass::Insert_Triangles -- a different, narrower path than the
// Begin/End_Sorted_Batch_Pass draws documented in this file's "Sorted / translucent draw path"
// section above. Mirrors BgfxBackend::Submit_Sorted_Draw (BgfxBackend.cpp ~5655).
//
// TheSuperHackers @info githubawn 20/07/2026 Tracing every SortingRendererClass call site in
// this codebase (sortingrenderer.cpp's Flush()/Flush_Sorting_Pool, boxrobj.cpp, linegrp.cpp,
// dx8polygonrenderer.h's Render_Sorted) shows all of them go through Insert_Triangles ->
// Flush()/Flush_Sorting_Pool -> g_renderBackend->Draw_Triangles directly, NEVER through
// DX8Wrapper::Draw()/Draw_Sorting_IB_VB -- so this entry point appears unreached in the current
// GeneralsMD engine flow. Implemented anyway per this backend's task (and for parity with
// BgfxBackend, which also implements it) in case some other caller does bind sorting buffers
// directly, or a future engine change reintroduces one; harmless (just never invoked) if it
// truly is dead code.
//
// Unlike Draw_Triangles' sorted-batch branch below, this does not need m_usingDynamicVertexBuffer
// -- dyn_vb/dyn_ib are handed to this call fresh, immediately after Draw_Sorting_IB_VB's own
// WriteLockClass locks fired Capture_Dynamic_Vertex_Data/Capture_Dynamic_Index_Data for exactly
// these two access-class instances, with no other draw call able to interleave and overwrite
// m_dynamicVertexData/m_dynamicIndexData in between -- so the existing SINGLE-buffer capture
// (Capture_Dynamic_Vertex_Data/Capture_Dynamic_Index_Data, see their own comments) is sufficient
// here without keying transients by access-class pointer the way BgfxBackend does (its
// g_draw.pendingVB.owner == &dyn_vb check, BgfxBackend.cpp ~5669) -- that keying exists there
// because bgfx defers actual buffer consumption to end-of-frame across multiple views/batches;
// this backend consumes m_dynamicVertexData/m_dynamicIndexData synchronously, within the same
// call that captured them, every time.
void Citro3dBackend::Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                        const DynamicIBAccessClass & dyn_ib,
                                        unsigned short polygon_count,
                                        unsigned short vertex_count)
{
    if (!m_initialized || !m_shaderLoaded || polygon_count == 0)
    {
        return;
    }

    const unsigned int stride = dyn_vb.FVF_Info().Get_FVF_Size();
    const unsigned int expectedVbBytes = static_cast<unsigned int>(vertex_count) * stride;
    const unsigned int expectedIbBytes = static_cast<unsigned int>(polygon_count) * 3u * sizeof(unsigned short);
    // TheSuperHackers @feature githubawn 20/07/2026 "Claimable" here just means the last
    // Capture_Dynamic_Vertex_Data/Index_Data calls produced a buffer at least as big as this draw
    // expects -- see the .h declaration's reasoning for why the single-buffer capture (no
    // access-class-pointer keying, unlike BgfxBackend's transient-buffer-per-owner scheme) is
    // sufficient for this call shape.
    const bool claimable = (m_dynamicVertexData != nullptr) && (m_dynamicIndexData != nullptr)
        && (stride != 0) && (m_dynamicVertexSizeBytes >= expectedVbBytes)
        && (m_dynamicIndexSizeBytes >= expectedIbBytes);

    // TheSuperHackers @diagnostic githubawn 20/07/2026 [ggc-sorted] Capped at 30 per the task's
    // diagnostic-volume ceiling -- records whether this (suspected-dead, see above) entry point
    // ever actually fires in a real run, and whether its transients were claimable.
    {
        static int s_ggcSortedDirectLogCount = 0;
        if (s_ggcSortedDirectLogCount < 30)
        {
            ++s_ggcSortedDirectLogCount;
            SDL_Log("[ggc-sorted] direct #%d poly=%u vtx=%u stride=%u claimable=%d vbBytes=%u(need %u) ibBytes=%u(need %u)",
                    s_ggcSortedDirectLogCount, (unsigned)polygon_count, (unsigned)vertex_count, stride,
                    (int)claimable, m_dynamicVertexSizeBytes, expectedVbBytes,
                    m_dynamicIndexSizeBytes, expectedIbBytes);
        }
    }

    if (!claimable)
    {
        return;
    }

    C3D_BindProgram(reinterpret_cast<shaderProgram_s *>(m_program));

    // dynamic_fvf_type (dx8vertexbuffer.h) -- the only FVF a sorting-type VB/IB pair is ever
    // built with -- is exactly the 44-byte VertexFormatXYZNDUV2 diffuse layout, same as
    // Draw_Triangles' hasDiffuse 3D branch and its own sorted-batch branch.
    C3D_AttrInfo * attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);         // position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);         // normal, unused (unlit vertex-color pass)
    AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4); // diffuse, packed D3DCOLOR
    AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2);         // uv0

    C3D_BufInfo * bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    // TheSuperHackers @info githubawn 20/07/2026 No m_indexBaseOffset applied here (unlike the
    // 2D/sorted-batch branches in Draw_Triangles): Draw_Sorting_IB_VB never calls
    // Set_Index_Buffer_Index_Offset, and dyn_vb/dyn_ib are sized EXACTLY to this one draw with
    // indices already rebased to start at 0 (dx8wrapper.cpp's "index -= min_vertex_index"), so
    // there is no base-vertex offset to add.
    BufInfo_Add(bufInfo, m_dynamicVertexData, stride, 4, 0x3210);

    Apply_Tex_Env(m_texturingEnabled);

    // TheSuperHackers @info githubawn 20/07/2026 Depth: translucent/sorted geometry tests depth
    // but does not write it. This reuses m_depthCompare/m_depthWriteEnabled -- already decoded by
    // Set_Shader from whatever ShaderClass this specific sorted draw's Apply_Render_State applied
    // -- exactly as BgfxBackend::Submit_Sorted_Draw trusts its own BuildBgfxStateForShader-decoded
    // g_draw.state with no separate depth override (aside from its own out-of-scope
    // material-decal heuristic, ApplySortedMaterialDecalDepthState, not ported here -- see this
    // backend's task scope).
    const GPU_WRITEMASK depthWriteMask = m_depthWriteEnabled ? GPU_WRITE_ALL : GPU_WRITE_COLOR;
    C3D_DepthTest(true, GGC_TranslateDepthCompareInverted(m_depthCompare), depthWriteMask);
    C3D_CullFace(GPU_CULL_NONE); // see the kGgc3DCullMode @todo on Draw_Triangles' ordinary 3D branch

    // world/view: use the captured sort-batch transforms if this call happens to be wrapped in a
    // Begin/End_Sorted_Batch_Pass pair (it need not be -- Draw_Sorting_IB_VB is a separate call
    // shape from those hooks), falling back to the ordinary tracked world/view otherwise. Mirrors
    // BgfxBackend::Submit_Sorted_Draw's identical fallback ("g_views.inSortFlush ? g_frame.sortWorld
    // : g_frame.world").
    C3D_Mtx transform;
    if (m_inSortedBatchPass)
    {
        GGC_BuildWorldViewProjC3D(m_sortWorldMtx, m_sortViewMtx, m_projMtx, &transform);
    }
    else
    {
        GGC_BuildWorldViewProjC3D(m_worldMtx, m_viewMtx, m_projMtx, &transform);
    }
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_uniformTransform, &transform);

    C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(polygon_count) * 3, C3D_UNSIGNED_SHORT, m_dynamicIndexData);
}

void Citro3dBackend::Draw_Triangles(unsigned short start_index,
                                    unsigned short polygon_count,
                                    unsigned short min_vertex_index,
                                    unsigned short vertex_count)
{
    DX8Backend::Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);

    if (!m_initialized || !m_shaderLoaded || polygon_count == 0)
    {
        return;
    }

    // TheSuperHackers @feature githubawn 20/07/2026 SortingRendererClass::Flush_Sorting_Pool
    // (particles, explosions, unit health bars, translucent decals) calls this SAME 4-arg entry
    // point once per contiguous same-render-state run (see Draw_Sorted_Run/Apply_Render_State in
    // sortingrenderer.cpp) -- it does not use a separate submit function. This branch is entirely
    // additive and returns before touching any of the is2D/3D logic below, which is unchanged
    // for every other caller. See the "Sorted / translucent draw path" section above
    // Submit_Sorted_Draw for the full analysis of why this needs special-casing (Apply_Render_State
    // sets the D3D transform via a direct call that bypasses Set_Transform, and
    // Flush_Sorting_Pool's combined geometry is bound through the DYNAMIC Set_Vertex_Buffer
    // overload, not the static one the ordinary 3D branch below reads).
    //
    // m_usingDynamicVertexBuffer disambiguates this from Flush()'s single-node fallback (also
    // wrapped in Begin/End_Sorted_Batch_Pass, but binding an ordinary STATIC mesh VB/IB) -- that
    // case is left to fall through into the existing 3D branch below unchanged, which already
    // reads the right buffer; only its world/view source needs the same substitution (see
    // "worldSrc"/"viewSrc" further down).
    if (m_inSortedBatchPass && m_usingDynamicVertexBuffer)
    {
        if (m_dynamicVertexData == nullptr || m_dynamicIndexData == nullptr)
        {
            return;
        }

        // TheSuperHackers @diagnostic githubawn 20/07/2026 [ggc-sorted] Capped at 30 per the
        // task's diagnostic-volume ceiling -- proves whether translucent/sorted geometry is
        // actually reaching the GPU through this branch, and whether the dynamic buffers it
        // needs were present and big enough.
        {
            static int s_ggcSortedPoolLogCount = 0;
            if (s_ggcSortedPoolLogCount < 30)
            {
                ++s_ggcSortedPoolLogCount;
                const unsigned int idxOffsetBytes = static_cast<unsigned int>(start_index) * sizeof(unsigned short);
                const unsigned int needIbBytes = idxOffsetBytes
                    + static_cast<unsigned int>(polygon_count) * 3u * sizeof(unsigned short);
                const unsigned int needVbBytes = (m_indexBaseOffset + static_cast<unsigned int>(vertex_count)) * 44u;
                const bool claimable = (needIbBytes <= m_dynamicIndexSizeBytes) && (needVbBytes <= m_dynamicVertexSizeBytes);
                SDL_Log("[ggc-sorted] pool #%d poly=%u vtx=%u startIdx=%u vbBytes=%u ibBytes=%u claimable=%d",
                        s_ggcSortedPoolLogCount, (unsigned)polygon_count, (unsigned)vertex_count,
                        (unsigned)start_index, m_dynamicVertexSizeBytes, m_dynamicIndexSizeBytes, (int)claimable);
            }
        }

        C3D_BindProgram(reinterpret_cast<shaderProgram_s *>(m_program));

        // dynamic_fvf_type (dx8vertexbuffer.h) -- the only FVF Flush_Sorting_Pool's combined
        // buffer is ever built with -- is exactly the 44-byte VertexFormatXYZNDUV2 diffuse
        // layout, i.e. the same AttrInfo/BufInfo setup as the ordinary 3D branch's hasDiffuse
        // case below (vertex-buffer diffuse used as-is, no flat-light combine -- see that
        // branch's own comment for why).
        C3D_AttrInfo * attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);         // position
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);         // normal, unused (unlit vertex-color pass)
        AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4); // diffuse, packed D3DCOLOR
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2);         // uv0

        C3D_BufInfo * bufInfo = C3D_GetBufInfo();
        BufInfo_Init(bufInfo);
        // Same base-vertex-offset handling as the 2D branch below -- Flush_Sorting_Pool resets
        // m_indexBaseOffset to 0 via Set_Index_Buffer(dyn_ib_access, 0) before this loop starts
        // (indices are already rebased to the combined buffer during the sort step), so this is a
        // no-op in practice, applied for correctness/consistency with the other two branches.
        const unsigned int baseVertexByteOffsetSorted = m_indexBaseOffset * 44u;
        if (baseVertexByteOffsetSorted >= m_dynamicVertexSizeBytes)
        {
            return;
        }
        BufInfo_Add(bufInfo, static_cast<const unsigned char *>(m_dynamicVertexData) + baseVertexByteOffsetSorted,
                    44, 4, 0x3210);

        Apply_Tex_Env(m_texturingEnabled);

        // TheSuperHackers @info githubawn 20/07/2026 Depth: translucent/sorted geometry tests
        // depth but does not write it -- this is exactly what the shader's own decoded
        // depth-mask (m_depthWriteEnabled, from Set_Shader, called per-batch by
        // Apply_Render_State) already says for a translucent ShaderClass preset, so this reuses
        // the same decode the ordinary 3D branch uses below rather than hardcoding it.
        const GPU_WRITEMASK depthWriteMaskSorted = m_depthWriteEnabled ? GPU_WRITE_ALL : GPU_WRITE_COLOR;
        C3D_DepthTest(true, GGC_TranslateDepthCompareInverted(m_depthCompare), depthWriteMaskSorted);
        C3D_CullFace(GPU_CULL_NONE); // see the kGgc3DCullMode @todo on the ordinary 3D branch below

        C3D_Mtx transformSorted;
        GGC_BuildWorldViewProjC3D(m_sortWorldMtx, m_sortViewMtx, m_projMtx, &transformSorted);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_uniformTransform, &transformSorted);

        const unsigned int indexOffsetBytesSorted =
            static_cast<unsigned int>(start_index) * sizeof(unsigned short);
        if (indexOffsetBytesSorted >= m_dynamicIndexSizeBytes)
        {
            return;
        }
        const void * indicesSorted = static_cast<const unsigned char *>(m_dynamicIndexData) + indexOffsetBytesSorted;

        C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(polygon_count) * 3, C3D_UNSIGNED_SHORT, indicesSorted);
        return;
    }

    // Render2DClass (menus/HUD/font glyphs) always draws with identity
    // world+view (render2d.cpp), which DX8Wrapper tracks regardless of which
    // backend is active -- the same detection BgfxBackend uses to tell a UI
    // draw from real 3D world geometry. Excludes m_inSortedBatchPass (mirrors
    // BgfxBackend's SubmitEngineDraw, which similarly excludes g_views.inSortFlush from its
    // own identity-based 2D fallback check, BgfxBackend.cpp ~7076) -- a sorted draw is always
    // real 3D world geometry regardless of whatever stale world/view transform this identity
    // check would otherwise see.
    const bool is2D = !m_inSortedBatchPass
        && DX8Backend::Is_World_Identity() && DX8Backend::Is_View_Identity();

    C3D_BindProgram(reinterpret_cast<shaderProgram_s *>(m_program));

    // TheSuperHackers @info githubawn 17/07/2026 Both the 2D dynamic-buffer
    // path and the 3D static-buffer path currently target the identical
    // VertexFormatXYZNDUV2 44-byte layout (see the 3D branch's FVF check
    // below), so one AttrInfo/BufInfo setup serves both -- only the source
    // buffer pointer, stride, and transform differ per-branch.

    if (is2D)
    {
        if (m_dynamicVertexData == nullptr || m_dynamicIndexData == nullptr)
        {
            return;
        }

        // TheSuperHackers @diagnostic githubawn 16/07/2026 Root-causing "only
        // one menu button box shows, no text, no background" -- bounded so a
        // live/animating menu does not spam forever.
        {
            static int s_ggcDrawLogCount = 0;
            if (s_ggcDrawLogCount < 4000)
            {
                ++s_ggcDrawLogCount;
                unsigned diffuse0 = 0;
                const unsigned vtxOffset = static_cast<unsigned>(min_vertex_index) * 44u + 24u;
                if (vtxOffset + 4u <= m_dynamicVertexSizeBytes)
                {
                    ::memcpy(&diffuse0, static_cast<const unsigned char *>(m_dynamicVertexData) + vtxOffset, 4);
                }
                SDL_Log("[ggc-draw] #%d vtx=%u poly=%u tex=%p texturing=%d vbBytes=%u ibBytes=%u diffuse0=%08x",
                        s_ggcDrawLogCount, (unsigned)vertex_count, (unsigned)polygon_count,
                        (void*)m_currentTexture, (int)m_texturingEnabled,
                        m_dynamicVertexSizeBytes, m_dynamicIndexSizeBytes, diffuse0);
            }
        }

        // TheSuperHackers @diagnostic githubawn 20/07/2026 Targeted follow-up to the
        // Apply_Tex_Env root-cause investigation above: reports the FULL decoded 2D draw state
        // (blend factors, primary gradient, alpha-test enable+ref, texturing/texture-bound, and
        // the bound texture's GPU format) for the first 40 2D draws of a run, so a real
        // Azahar/hardware session can confirm what state the logo/progress-bar draws are
        // actually getting -- separate from [ggc-draw] above (which predates the alpha-test/
        // blend-factor member storage this reads and only logs texture/diffuse, not blend
        // state). Capped low and NOT reused past the boot/loading-screen window on purpose --
        // this is a one-shot "what happened during load" snapshot, not a standing per-frame
        // trace (see the capped-at-4000-but-still-a-lot [ggc-draw] block above for what NOT to
        // repeat here).
        {
            static int s_ggc2dStateLogCount = 0;
            if (s_ggc2dStateLogCount < 40)
            {
                ++s_ggc2dStateLogCount;
                const bool hasTexture = (m_currentTexture != nullptr);
                int gpuFormat = -1;
                if (hasTexture)
                {
                    gpuFormat = static_cast<int>(reinterpret_cast<C3D_Tex *>(m_currentTexture)->fmt);
                }
                SDL_Log("[ggc-2dstate] #%d srcBlend=%d dstBlend=%d priGradient=%d "
                        "alphaTest=%d alphaRef=%d texturing=%d texBound=%d gpuFmt=%d",
                        s_ggc2dStateLogCount, m_lastSrcBlendFactor, m_lastDstBlendFactor,
                        m_primaryGradient, (int)m_alphaTestEnabled, m_alphaTestRef,
                        (int)m_texturingEnabled, (int)hasTexture, gpuFormat);
            }
        }

        C3D_AttrInfo * attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);         // position (VertexFormatXYZNDUV2::Location)
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);         // normal, unused
        AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4); // diffuse, packed D3DCOLOR
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2);         // uv0

        C3D_BufInfo * bufInfo = C3D_GetBufInfo();
        BufInfo_Init(bufInfo);
        // Stride 44 = sizeof(VertexFormatXYZNDUV2) (dx8fvf.h): the 4 loaders
        // above only consume the leading 36 bytes; the GPU still advances a
        // full 44 bytes per vertex, correctly skipping the trailing unused uv1.
        // TheSuperHackers @bugfix githubawn 20/07/2026 m_indexBaseOffset is a base
        // VERTEX, so it shifts where the vertex buffer starts -- see the matching
        // (and far more consequential) comment in the 3D branch below. Applied here
        // too for correctness; for this 2D UI path the engine passes 0, so this is
        // a no-op in practice rather than a behavior change to a working path.
        const unsigned int baseVertexByteOffset2D = m_indexBaseOffset * 44u;
        if (baseVertexByteOffset2D >= m_dynamicVertexSizeBytes)
        {
            return;
        }
        BufInfo_Add(bufInfo, static_cast<const unsigned char *>(m_dynamicVertexData) + baseVertexByteOffset2D,
                    44, 4, 0x3210);

        Apply_Tex_Env(m_texturingEnabled);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

        C3D_Mtx transform;
        Mtx_OrthoTilt(&transform, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_uniformTransform, &transform);

        const unsigned int indexOffsetBytes =
            static_cast<unsigned int>(start_index) * sizeof(unsigned short);
        if (indexOffsetBytes >= m_dynamicIndexSizeBytes)
        {
            return;
        }
        const void * indices = static_cast<const unsigned char *>(m_dynamicIndexData) + indexOffsetBytes;

        C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(polygon_count) * 3, C3D_UNSIGNED_SHORT, indices);
        return;
    }

    // -- 3D world geometry (Phase 3 Milestone 4) -------------------------------
    //
    // TheSuperHackers @bugfix githubawn 18/07/2026 The 44-byte-only guard this used to have
    // silently skipped EVERY terrain draw (HeightMap.cpp's static tiles use DX8_FVF_XYZNUV2,
    // 40 bytes, no diffuse) -- fixed by adding a second hardcoded case for 40.
    //
    // TheSuperHackers @bugfix githubawn 19/07/2026 That was still too narrow: a live-match log
    // of every 3D draw call showed the dominant real-world format is actually 32 bytes
    // (DX8_FVF_XYZNUV1 -- pos+normal+1 UV, no diffuse either), which neither hardcoded case
    // covered -- ALL logged draws that session were silently skipped, hence "no 3D visible at
    // all" even after the terrain-specific fix. Rather than keep chasing individual FVF combos
    // one report at a time, generalized: any stride is drawable as long as it's at least big
    // enough to hold a position (12 bytes); only the one CONFIRMED diffuse-bearing format (44
    // bytes, VertexFormatXYZNDUV2) gets its own path to read real per-vertex color, everything
    // else loads position only and gets a fixed opaque-white color/normal/uv (harmless while
    // texturing/lighting are both still off -- kGgc3DTexturingEnabled below).
    //
    // TheSuperHackers @feature githubawn 20/07/2026 Now that kGgc3DTexturingEnabled is on (DXT
    // decode has landed, see Ensure_Texture), "fixed dummy uv" is no longer harmless for a
    // textured draw -- it would sample the same single texel for every vertex. Added a second
    // recognized layout (hasUvAtOffset24 below) so the two no-diffuse formats actually seen on
    // this path get real UVs too; anything else still falls back to the pre-existing fixed-dummy
    // behavior rather than guess an unverified offset.
    const bool hasDiffuse = (m_currentVertexStride == 44); // VertexFormatXYZNDUV2: pos+normal+diffuse+uv0

    // TheSuperHackers @feature githubawn 20/07/2026 DX8_FVF_XYZNUV1 (32 bytes: pos+normal+uv0)
    // and DX8_FVF_XYZNUV2 (40 bytes: pos+normal+uv0+uv1) -- verified against dx8fvf.h's
    // VertexFormatXYZNUV1/XYZNUV2 struct layouts -- both place uv0 at the SAME offset 24, right
    // after pos(12 bytes)+normal(12 bytes), with no diffuse in between. VertexFormatXYZNUV2's
    // trailing uv1 (bytes 32-39) is simply never loaded, same as how the hasDiffuse branch below
    // already leaves VertexFormatXYZNDUV2's own trailing uv1 unloaded -- BufInfo's stride still
    // advances the full per-vertex byte count either way.
    const bool hasUvAtOffset24 = (m_currentVertexStride == 32 || m_currentVertexStride == 40);

    // TheSuperHackers @diagnostic githubawn 18/07/2026 Confirming the 3D draw path is actually
    // being reached at all during a real match, and which format/skip-reason each call hits --
    // previously totally unobserved (no log existed here). Capped low since this is per-triangle-
    // batch, not per-frame.
    {
        static int s_ggc3dDrawLogCount = 0;
        if (s_ggc3dDrawLogCount < 60)
        {
            ++s_ggc3dDrawLogCount;
            // TheSuperHackers @diagnostic githubawn 20/07/2026 baseVtx/startIdx added: the
            // [ggc-ndccensus] survey proved the transform and the vertex data at the buffer START
            // are correct (2718 of 4000 batches contained on-screen geometry) while the screen
            // stayed black -- so the remaining suspect is that the GPU is not reading from where
            // the census looked. The GPU fetches from m_currentVertexData + baseVtx*stride (the
            // D3D8 BaseVertexIndex), which the census does NOT apply, so a wrong or oversized
            // baseVtx would leave the census happy while the GPU pulls garbage or out-of-bounds
            // vertices. vbBytes lets that be checked directly: baseVtx*stride must stay well
            // inside it.
            SDL_Log("[ggc-3d] #%d stride=%u vb=%p ib=%p hasDiffuse=%d vtx=%u poly=%u baseVtx=%u startIdx=%u vbBytes=%u ibBytes=%u",
                    s_ggc3dDrawLogCount, m_currentVertexStride, m_currentVertexData, m_currentIndexData,
                    (int)hasDiffuse, (unsigned)vertex_count, (unsigned)polygon_count,
                    m_indexBaseOffset, (unsigned)start_index,
                    m_currentVertexSizeBytes, m_currentIndexSizeBytes);
        }
    }

    if (m_currentVertexData == nullptr || m_currentIndexData == nullptr
        || m_currentVertexStride < 12)
    {
        return;
    }

    // TheSuperHackers @feature githubawn 17/07/2026 Per user direction: land
    // 3D geometry visibility first, then view an untextured match while DXT
    // decode (see Ensure_Texture) is still unported -- kGgc3DTexturingEnabled
    // is a single switch to flip once that lands. 2D UI/fonts are unaffected
    // (still driven by m_texturingEnabled in the is2D branch above).
    Apply_Tex_Env(kGgc3DTexturingEnabled);

    // TheSuperHackers @bugfix githubawn 20/07/2026 Replaced the hardcoded GPU_GEQUAL/GPU_WRITE_ALL
    // with the actual ShaderClass-decoded depth state from Set_Shader (m_depthCompare/
    // m_depthWriteEnabled -- see GGC_TranslateDepthCompareInverted above for why the compare
    // direction is inverted). Depth test is unconditionally enabled for every 3D draw: a shader
    // whose preset is PASS_ALWAYS just decodes to GPU_ALWAYS, which is already the correct
    // "never actually reject" no-op, so there is no separate enable flag to track.
    const GPU_WRITEMASK depthWriteMask = m_depthWriteEnabled ? GPU_WRITE_ALL : GPU_WRITE_COLOR;
    C3D_DepthTest(true, GGC_TranslateDepthCompareInverted(m_depthCompare), depthWriteMask);

    // TheSuperHackers @todo githubawn 20/07/2026 kGgc3DCullMode is left at GPU_CULL_NONE
    // regardless of m_cullEnabled (decoded from ShaderClass::Get_Cull_Mode() in Set_Shader but
    // otherwise unused for now): the PICA200 top-screen framebuffer is rotated 90 degrees relative
    // to D3D's screen space (see the portrait tilt in GGC_BuildWorldViewProjC3D), which likely also
    // flips the effective front-face winding order, but whether the correct enabled mode is
    // GPU_CULL_BACK_CCW or GPU_CULL_FRONT_CCW is not yet known without live geometry to check
    // against -- guessing wrong would cull ALL visible faces instead of none, which is strictly
    // worse than the current "never cull" fallback. Flip this to the correct constant (and start
    // honoring m_cullEnabled) once real 3D geometry is visible enough to tell front from back.
    static const GPU_CULLMODE kGgc3DCullMode = GPU_CULL_NONE;
    C3D_CullFace(kGgc3DCullMode);

    C3D_AttrInfo * attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // position

    C3D_BufInfo * bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);

    // TheSuperHackers @bugfix githubawn 20/07/2026 THE fix for "3D geometry draws but nothing is
    // visible". m_indexBaseOffset is a D3D8 BaseVertexIndex: D3D adds it to every index to pick
    // the vertex (effective_vertex = VB[IB[start+i] + base]). This code instead added it to the
    // index-array start, which shifts WHICH INDICES ARE READ rather than which vertex they
    // resolve to -- so any mesh with a non-zero base vertex (i.e. essentially all real world
    // geometry, which shares one vertex buffer across many parts via
    // Set_Index_Buffer_Index_Offset) read indices belonging to a different part and produced
    // garbage or degenerate triangles. BgfxBackend hit and fixed this identical bug; see its
    // "Offset semantics" comment in BgfxBackend.cpp (SubmitEngineDraw), which records the same
    // wrong-then-right reasoning. bgfx expresses the fix as setVertexBuffer's _startVertex; the
    // PICA equivalent is to advance the vertex buffer BASE POINTER by base*stride here, and to
    // leave the index offset as start_index alone (below).
    const unsigned int baseVertexByteOffset = m_indexBaseOffset * m_currentVertexStride;
    const unsigned char * vertexBase =
        static_cast<const unsigned char *>(m_currentVertexData) + baseVertexByteOffset;

    if (hasDiffuse)
    {
        // TheSuperHackers @todo githubawn 20/07/2026 This 44-byte layout's vertex color comes
        // straight from the buffer (the diffuse loader below), so Compute_Flat_Light_Color's
        // constant-attribute trick used in the other two branches does not apply here -- the
        // task that added scene lighting considered folding the light color in via a SECOND TEV
        // stage instead (C3D_TexEnv* env1 = C3D_GetTexEnv(1); GPU_MODULATE of GPU_PREVIOUS by a
        // GPU_CONSTANT set through C3D_TexEnvColor, cascading after Apply_Tex_Env's stage-0
        // combine) -- mechanically straightforward, mirroring the primary-gradient stage this
        // file already builds in Apply_Tex_Env. DELIBERATELY NOT DONE: C3D_TexEnvColor's u32
        // packing order (which byte is R vs. A) is not documented in the locally available
        // libctru headers (c3d/texenv.h just says "u32 color"), and this codebase's own existing
        // GPU-facing color packing is NOT self-consistent enough to infer it safely -- Clear()
        // above builds its C3D_RenderTargetClear color as r<<24|g<<16|b<<8|a (RRGGBBAA), which is
        // a DIFFERENT register than a TEV constant and not good evidence for this one. Getting a
        // TEV constant's byte order wrong does not fail loudly -- it produces a subtly
        // wrong-channel tint on every 44-byte-diffuse 3D draw (units, some props) that would only
        // be caught by eyeballing real hardware/emulator output, which this environment cannot
        // do. Left unlit (vertex-buffer diffuse only, unchanged from before this feature) rather
        // than risk that. To add this safely: confirm GPU_TEVn_COLOR's byte order against
        // 3dbrew's GPU register docs or a known-working citro3d sample that sets a TEV constant
        // color and visually verify the result on hardware/Azahar before relying on it.
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);         // normal, unused (unlit vertex-color pass for now)
        AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4); // diffuse, packed D3DCOLOR
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2);         // uv0, unused while texturing is disabled
        BufInfo_Add(bufInfo, vertexBase, 44, 4, 0x3210);
    }
    else if (hasUvAtOffset24)
    {
        // TheSuperHackers @feature githubawn 20/07/2026 Real position+normal+uv0 load, for actual
        // 3D texturing now that kGgc3DTexturingEnabled is on. AttrInfo_AddLoader reads
        // CONTIGUOUSLY from the buffer start, in CALL order -- pos(3 floats=12B, added just above
        // this branch) + normal(3 floats=12B) + uv0(2 floats=8B) is exactly the leading 32 bytes
        // of BOTH the 32-byte XYZNUV1 and 40-byte XYZNUV2 layouts, with no diffuse gap to skip
        // (unlike the 44-byte hasDiffuse branch above) -- so all three load straight through with
        // no byte-level reordering needed.
        //
        // The REGISTER-level mapping is the actual crux here: vs_2d.v.pica (shared with the 2D
        // path, so it cannot be edited/reordered for this branch) expects v0=pos, v1=normal,
        // v2=color, v3=uv, but this vertex layout has no diffuse -- v2 is skipped entirely and
        // supplied as a C3D_FixedAttribSet constant instead, same as the hasDiffuse branch's
        // now-decorative dummy-normal pattern used to do for ALL of 1/2/3. So uv0 is added as
        // this buffer's THIRD load (load-index 2, right after pos/normal) but tagged for
        // REGISTER 3 -- AttrInfo_AddLoader's first argument (register) and its call order (load
        // index, i.e. which bytes come from the buffer in which sequence) are independent knobs.
        // BufInfo_Add's permutation nibble then says "pull load-indices 0,1,2 in that exact
        // order from this buffer" (0x210, one hex digit per load-index, same digit-per-index
        // convention as the hasDiffuse branch's 0x3210 for 4 loaders) -- i.e. pos, then normal,
        // then uv0, contiguously, regardless of which register each one ends up tagged for.
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // normal -- loaded for contiguity with uv0
                                                        // below; still unused by the shader itself
                                                        // (unlit vertex-color pass for now)
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2); // uv0 -- real texture coordinates
        // TheSuperHackers @bugfix githubawn 20/07/2026 A fixed attribute must be DECLARED in the
        // AttrInfo with AttrInfo_AddFixed before C3D_FixedAttribSet's value is delivered to the
        // shader -- see devkitPro's own citro3d example ($DEVKITPRO/examples/3ds/graphics/gpu/
        // simple_tri), which pairs "AttrInfo_AddFixed(attrInfo, 1)" with "C3D_FixedAttribSet(1,
        // ...)" for exactly this reason. Calling only C3D_FixedAttribSet (as this code did)
        // leaves the register undefined -- in practice zero -- so every untextured 3D triangle
        // shaded to black, which is precisely the "entire 3D world is fully black" symptom (and
        // why it was black rather than the white an unbound texture stage would otherwise give).
        // Declared AFTER the three loaders above so it takes attribute index 3 and leaves load
        // indices 0,1,2 -- the ones BufInfo_Add's 0x210 permutation refers to -- untouched.
        AttrInfo_AddFixed(attrInfo, 2); // color: not present in this vertex layout
        // TheSuperHackers @feature githubawn 20/07/2026 Was a hardcoded opaque-white constant
        // (255,255,255,255) -- now Compute_Flat_Light_Color's crude ambient+dominant-light CPU
        // combine (see the "Basic scene lighting" block above Ensure_Texture, and the @todo there
        // about why this is flat/per-draw rather than real per-vertex N.L). Same 255.0f-not-1.0f
        // pre-multiply reasoning as before: vs_2d.v.pica divides inclr by 255 to normalize a real
        // GPU_UNSIGNED_BYTE diffuse attribute, so a fixed constant feeding that same register must
        // pre-multiply by 255 too.
        {
            unsigned char lr, lg, lb;
            Compute_Flat_Light_Color(lr, lg, lb);
            C3D_FixedAttribSet(2, static_cast<float>(lr), static_cast<float>(lg), static_cast<float>(lb), 255.0f);
        }
        BufInfo_Add(bufInfo, vertexBase, m_currentVertexStride, 3, 0x210);
    }
    else // Layout not verified against a known UV offset (see hasUvAtOffset24 above) -- conservative
         // fallback, unchanged from before this feature: position only from the buffer, uv fixed at
         // (0,0) so a bound texture (if texturing is enabled and one is actually set for this draw)
         // still samples a single defined texel rather than undefined GPU register state, instead of
         // guessing an offset that might not hold for this stride.
    {
        // TheSuperHackers @bugfix githubawn 20/07/2026 Declare all three as fixed attributes
        // before setting their values -- see the AttrInfo_AddFixed explanation in the
        // hasUvAtOffset24 branch above; without this the values never reach the shader at all.
        AttrInfo_AddFixed(attrInfo, 1); // normal
        AttrInfo_AddFixed(attrInfo, 2); // color
        AttrInfo_AddFixed(attrInfo, 3); // uv0
        C3D_FixedAttribSet(1, 0.0f, 0.0f, 1.0f, 0.0f); // dummy normal, unused (unlit pass)
        // TheSuperHackers @bugfix githubawn 20/07/2026 vs_2d.v.pica now multiplies inclr by 1/255
        // (see that file's colscale constant) to turn the real per-vertex GPU_UNSIGNED_BYTE diffuse
        // attribute -- which arrives unnormalized as 0..255, not 0..1 -- into a usable 0..1 color.
        // This fixed-attribute path feeds attr 2 directly (bypassing the vertex buffer entirely),
        // so it must pre-multiply by the SAME 255 the shader will divide by, or "opaque white" would
        // become ~0.004 (near-black) post-shader instead. Attrs 1/3 above/below stay 0..1 as before
        // -- they are dummy normal/uv values the shader never scales.
        //
        // TheSuperHackers @feature githubawn 20/07/2026 Was a hardcoded opaque-white constant --
        // now Compute_Flat_Light_Color's crude combine, same as the hasUvAtOffset24 branch above
        // (see its matching comment and the "Basic scene lighting" block earlier in this file).
        {
            unsigned char lr, lg, lb;
            Compute_Flat_Light_Color(lr, lg, lb);
            C3D_FixedAttribSet(2, static_cast<float>(lr), static_cast<float>(lg), static_cast<float>(lb), 255.0f);
        }
        C3D_FixedAttribSet(3, 0.0f, 0.0f, 0.0f, 0.0f); // dummy uv
        BufInfo_Add(bufInfo, vertexBase, m_currentVertexStride, 1, 0x0);
    }

    C3D_Mtx transform;
    // TheSuperHackers @feature githubawn 20/07/2026 Flush()'s single-node sorted fallback
    // (sortingrenderer.cpp) reaches this ordinary 3D branch with the right STATIC geometry
    // already bound (m_usingDynamicVertexBuffer is false for it, see the sorted-batch branch
    // above this function), but it sets the D3D transform via a direct call that bypasses
    // Set_Transform, so m_worldMtx/m_viewMtx would be stale here -- substitute the
    // Capture_Sorted_Batch_Transforms-captured pair instead, same as the sorted-batch branch
    // above. Every other caller of this function has m_inSortedBatchPass == false and gets
    // byte-for-byte the same m_worldMtx/m_viewMtx this always used.
    const float * worldSrc = m_inSortedBatchPass ? m_sortWorldMtx : m_worldMtx;
    const float * viewSrc = m_inSortedBatchPass ? m_sortViewMtx : m_viewMtx;
    GGC_BuildWorldViewProjC3D(worldSrc, viewSrc, m_projMtx, &transform);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_uniformTransform, &transform);

    // TheSuperHackers @diagnostic githubawn 20/07/2026 The 3D world is still entirely black even
    // though draws reach C3D_DrawElements with plausible matrices and vertex data, so the open
    // question is now specifically WHERE this geometry lands. Run the exact same transform the
    // vertex shader will run, on a spread of vertices across this batch (not just vertex 0 --
    // a terrain tile's first vertex sits at a tile corner and being off-screen proves nothing),
    // and report how many of them survive the frustum. This separates the two remaining
    // hypotheses in one run: "geometry is off-screen / degenerate" (transform still wrong) will
    // show 0 inside with wild NDC values, whereas "geometry is on-screen but invisible" (a
    // fragment-stage problem: depth test, alpha test, or tex env) will show a healthy count
    // inside with sane NDC. Capped hard and sampled sparsely so it cannot flood the log.
    // TheSuperHackers @diagnostic githubawn 20/07/2026 REWORKED: the first version of this logged
    // only the first 6 draws and reported inside=0 for all of them, which looked damning but
    // proved nothing. Reconstructing the camera from the logged view matrix showed those six are
    // map-corner terrain tiles that genuinely sit outside the view frustum -- exactly what a
    // CORRECT transform should produce for them. A per-batch sample of the first few draws
    // therefore cannot distinguish "transform is wrong" from "these particular tiles are simply
    // off-screen". What settles it is a census: scan a large number of batches and report how
    // many contained ANY on-screen vertex. If a whole match's worth of geometry yields zero
    // on-screen batches, the transform is wrong; if a healthy fraction are on-screen while the
    // display stays black, the defect is downstream in the fragment stage instead.
    {
        static unsigned s_ggcNdcBatches = 0;         // batches examined
        static unsigned s_ggcNdcBatchesWithInside = 0; // batches with >=1 vertex inside the frustum
        static unsigned s_ggcNdcTotalInside = 0;
        if (s_ggcNdcBatches < 4000 && m_currentVertexStride >= 12 && vertex_count > 0)
        {
            const unsigned kCensusSamples = 8;
            const unsigned censusStep = (vertex_count > kCensusSamples) ? (vertex_count / kCensusSamples) : 1;
            unsigned censusInside = 0, censusSampled = 0;
            for (unsigned i = 0; i < vertex_count && censusSampled < kCensusSamples; i += censusStep)
            {
                float p[3];
                std::memcpy(p, static_cast<const unsigned char *>(m_currentVertexData)
                            + static_cast<size_t>(i) * m_currentVertexStride, sizeof(p));
                float c[4];
                for (int r = 0; r < 4; ++r)
                {
                    const C3D_FVec & row = transform.r[r];
                    c[r] = row.x * p[0] + row.y * p[1] + row.z * p[2] + row.w;
                }
                ++censusSampled;
                if (c[3] <= 0.0f) { continue; }
                const float nx = c[0] / c[3], ny = c[1] / c[3], nz = c[2] / c[3];
                if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f && nz >= -1.0f && nz <= 0.0f)
                {
                    ++censusInside;
                }
            }
            ++s_ggcNdcBatches;
            s_ggcNdcTotalInside += censusInside;
            if (censusInside > 0)
            {
                ++s_ggcNdcBatchesWithInside;
            }
            // Report periodically rather than per batch, so this stays a handful of lines.
            if ((s_ggcNdcBatches % 250u) == 0u)
            {
                SDL_Log("[ggc-ndccensus] batches=%u withOnscreenVerts=%u totalOnscreenSamples=%u",
                        s_ggcNdcBatches, s_ggcNdcBatchesWithInside, s_ggcNdcTotalInside);
            }
        }
    }

    {
        static int s_ggcNdcLogCount = 0;
        if (s_ggcNdcLogCount < 6 && m_currentVertexStride >= 12 && vertex_count > 0)
        {
            ++s_ggcNdcLogCount;
            const unsigned kSamples = 16;
            const unsigned step = (vertex_count > kSamples) ? (vertex_count / kSamples) : 1;
            unsigned inside = 0, behind = 0, sampled = 0;
            float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f, minZ = 1e30f, maxZ = -1e30f;
            for (unsigned i = 0; i < vertex_count && sampled < kSamples; i += step)
            {
                float p[3];
                std::memcpy(p, static_cast<const unsigned char *>(m_currentVertexData)
                            + static_cast<size_t>(i) * m_currentVertexStride, sizeof(p));
                // Same math as the shader: out[r] = dot(transform.r[r], (p,1)).
                float c[4];
                for (int r = 0; r < 4; ++r)
                {
                    const C3D_FVec & row = transform.r[r];
                    c[r] = row.x * p[0] + row.y * p[1] + row.z * p[2] + row.w;
                }
                ++sampled;
                if (c[3] <= 0.0f) { ++behind; continue; }
                const float nx = c[0] / c[3], ny = c[1] / c[3], nz = c[2] / c[3];
                if (nx < minX) minX = nx; if (nx > maxX) maxX = nx;
                if (ny < minY) minY = ny; if (ny > maxY) maxY = ny;
                if (nz < minZ) minZ = nz; if (nz > maxZ) maxZ = nz;
                // PICA clip volume: x,y in [-1,1], z in [-1,0].
                if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f && nz >= -1.0f && nz <= 0.0f)
                {
                    ++inside;
                }
            }
            SDL_Log("[ggc-ndc] #%d sampled=%u inside=%u behindW=%u x=[%f..%f] y=[%f..%f] z=[%f..%f]",
                    s_ggcNdcLogCount, sampled, inside, behind, minX, maxX, minY, maxY, minZ, maxZ);
        }
    }

    // TheSuperHackers @diagnostic githubawn 19/07/2026 Draws now correctly reach this point for
    // every vertex format (confirmed via the [ggc-3d] log above), yet the user still reports no
    // visible 3D geometry -- ruling out the format-skip bug means the next most likely causes are
    // the transform math (garbage/degenerate world/view/proj producing off-screen or
    // zero-area geometry) or garbage vertex position data. Logs both raw matrices and the first
    // vertex's actual position for just the first 3 draws (each entry is large, keep this cap low).
    {
        static int s_ggcXformLogCount = 0;
        if (s_ggcXformLogCount < 3)
        {
            ++s_ggcXformLogCount;
            float firstPos[3] = { 0.0f, 0.0f, 0.0f };
            std::memcpy(firstPos, m_currentVertexData, sizeof(firstPos));
            SDL_Log("[ggc-xform] #%d firstVertexPos=(%f,%f,%f)", s_ggcXformLogCount,
                    firstPos[0], firstPos[1], firstPos[2]);
            SDL_Log("[ggc-xform] #%d world=[%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f]",
                    s_ggcXformLogCount,
                    m_worldMtx[0], m_worldMtx[1], m_worldMtx[2], m_worldMtx[3],
                    m_worldMtx[4], m_worldMtx[5], m_worldMtx[6], m_worldMtx[7],
                    m_worldMtx[8], m_worldMtx[9], m_worldMtx[10], m_worldMtx[11],
                    m_worldMtx[12], m_worldMtx[13], m_worldMtx[14], m_worldMtx[15]);
            SDL_Log("[ggc-xform] #%d view=[%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f]",
                    s_ggcXformLogCount,
                    m_viewMtx[0], m_viewMtx[1], m_viewMtx[2], m_viewMtx[3],
                    m_viewMtx[4], m_viewMtx[5], m_viewMtx[6], m_viewMtx[7],
                    m_viewMtx[8], m_viewMtx[9], m_viewMtx[10], m_viewMtx[11],
                    m_viewMtx[12], m_viewMtx[13], m_viewMtx[14], m_viewMtx[15]);
            SDL_Log("[ggc-xform] #%d proj=[%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f]",
                    s_ggcXformLogCount,
                    m_projMtx[0], m_projMtx[1], m_projMtx[2], m_projMtx[3],
                    m_projMtx[4], m_projMtx[5], m_projMtx[6], m_projMtx[7],
                    m_projMtx[8], m_projMtx[9], m_projMtx[10], m_projMtx[11],
                    m_projMtx[12], m_projMtx[13], m_projMtx[14], m_projMtx[15]);
        }
    }

    // TheSuperHackers @bugfix githubawn 20/07/2026 start_index ONLY -- m_indexBaseOffset was
    // applied to the vertex buffer base above, which is where a D3D8 BaseVertexIndex belongs.
    // Adding it here as well (as this line used to) double-counted it AND applied it in the
    // wrong dimension; see the full explanation at vertexBase's definition.
    const unsigned int indexOffsetBytes =
        static_cast<unsigned int>(start_index) * sizeof(unsigned short);
    if (indexOffsetBytes >= m_currentIndexSizeBytes)
    {
        return;
    }
    const void * indices = static_cast<const unsigned char *>(m_currentIndexData) + indexOffsetBytes;

    C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(polygon_count) * 3, C3D_UNSIGNED_SHORT, indices);
}

// -----------------------------------------------------------------------------
// Draw_Strip
// -----------------------------------------------------------------------------
//
// TheSuperHackers @feature githubawn 20/07/2026 Triangle-strip draw path -- was never overridden
// here, so it fell through to DX8Backend's DX8Wrapper::Draw_Strip forward (D3D8 stub only, draws
// nothing). Mirrors BgfxBackend::Draw_Strip (BgfxBackend.cpp ~8170), which itself is a thin
// wrapper around the same SubmitEngineDraw helper Draw_Triangles uses, passing an extra
// triangle_strip=true flag that (a) switches bgfx's primitive type and (b) computes
// indexCount = polygon_count+2 instead of polygon_count*3 (BgfxBackend.cpp ~7392 -- a strip's
// total index count is triangleCount+2, not triangleCount*3; despite the IRenderBackend.h
// parameter being NAMED "index_count", every caller -- dx8polygonrenderer.h's
// "index_count-2"/W3DWater.cpp's "m_numIndices-2" -- actually passes the STRIP'S TRIANGLE COUNT,
// exactly like DX8Wrapper::Draw_Strip's own "polygon_count" parameter of the same underlying
// D3DPT_TRIANGLESTRIP DrawIndexedPrimitive call this whole path traces back to).
//
// This backend has no equivalent shared submit helper -- Draw_Triangles' logic is inlined, not
// factored out, and is explicitly off-limits to restructure into one without a build to verify
// the refactor didn't regress it (see this file's top-of-task restrictions). So this duplicates
// Draw_Triangles' is2D/3D setup verbatim (same AttrInfo/BufInfo layouts, same
// hasDiffuse/hasUvAtOffset24/fallback vertex-format detection, same base-vertex-offset and
// depth/cull decode) rather than sharing code with it, changing only: the primitive type
// (GPU_TRIANGLE_STRIP, not GPU_TRIANGLES) and the index-count formula (index_count+2, not
// polygon_count*3). The only known reachable caller (W3DWater.cpp's water grid) is out of scope
// per this backend's low-detail rendering restrictions (water effects excluded) and is not
// itself part of a SortingRendererClass sorted-batch pass, so -- unlike Draw_Triangles -- this
// does not need the m_inSortedBatchPass/m_usingDynamicVertexBuffer handling added there; it
// always reads m_worldMtx/m_viewMtx and whichever of the static/dynamic buffers is currently
// bound, exactly like Draw_Triangles did before this task's sorted-batch work.
void Citro3dBackend::Draw_Strip(unsigned short start_index,
                                unsigned short index_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    DX8Backend::Draw_Strip(start_index, index_count, min_vertex_index, vertex_count);

    if (!m_initialized || !m_shaderLoaded || index_count == 0)
    {
        return;
    }

    const bool is2D = !m_inSortedBatchPass
        && DX8Backend::Is_World_Identity() && DX8Backend::Is_View_Identity();
    const unsigned int indexCount = static_cast<unsigned int>(index_count) + 2u;

    C3D_BindProgram(reinterpret_cast<shaderProgram_s *>(m_program));

    // TheSuperHackers @diagnostic githubawn 20/07/2026 [ggc-strip] Confirms strip draws actually
    // reach this backend at all (previously silently dropped, see this function's own comment).
    // Capped at 30 -- this is a rare draw type (water grid is the only known caller).
    {
        static int s_ggcStripLogCount = 0;
        if (s_ggcStripLogCount < 30)
        {
            ++s_ggcStripLogCount;
            SDL_Log("[ggc-strip] #%d is2D=%d triCount=%u vtx=%u startIdx=%u baseVtx=%u stride=%u",
                    s_ggcStripLogCount, (int)is2D, (unsigned)index_count, (unsigned)vertex_count,
                    (unsigned)start_index, m_indexBaseOffset,
                    is2D ? 44u : m_currentVertexStride);
        }
    }

    if (is2D)
    {
        if (m_dynamicVertexData == nullptr || m_dynamicIndexData == nullptr)
        {
            return;
        }

        C3D_AttrInfo * attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4);
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2);

        C3D_BufInfo * bufInfo = C3D_GetBufInfo();
        BufInfo_Init(bufInfo);
        const unsigned int baseVertexByteOffset2D = m_indexBaseOffset * 44u;
        if (baseVertexByteOffset2D >= m_dynamicVertexSizeBytes)
        {
            return;
        }
        BufInfo_Add(bufInfo, static_cast<const unsigned char *>(m_dynamicVertexData) + baseVertexByteOffset2D,
                    44, 4, 0x3210);

        Apply_Tex_Env(m_texturingEnabled);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

        C3D_Mtx transform;
        Mtx_OrthoTilt(&transform, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_uniformTransform, &transform);

        const unsigned int indexOffsetBytes =
            static_cast<unsigned int>(start_index) * sizeof(unsigned short);
        if (indexOffsetBytes >= m_dynamicIndexSizeBytes)
        {
            return;
        }
        const void * indices = static_cast<const unsigned char *>(m_dynamicIndexData) + indexOffsetBytes;

        C3D_DrawElements(GPU_TRIANGLE_STRIP, static_cast<int>(indexCount), C3D_UNSIGNED_SHORT, indices);
        return;
    }

    // -- 3D world geometry -- mirrors Draw_Triangles' 3D branch (see its own comments for the
    // hasDiffuse/hasUvAtOffset24/fallback reasoning and the base-vertex-offset fix).
    const bool hasDiffuse = (m_currentVertexStride == 44);
    const bool hasUvAtOffset24 = (m_currentVertexStride == 32 || m_currentVertexStride == 40);

    if (m_currentVertexData == nullptr || m_currentIndexData == nullptr
        || m_currentVertexStride < 12)
    {
        return;
    }

    Apply_Tex_Env(kGgc3DTexturingEnabled);

    const GPU_WRITEMASK depthWriteMask = m_depthWriteEnabled ? GPU_WRITE_ALL : GPU_WRITE_COLOR;
    C3D_DepthTest(true, GGC_TranslateDepthCompareInverted(m_depthCompare), depthWriteMask);
    C3D_CullFace(GPU_CULL_NONE); // see the kGgc3DCullMode @todo on Draw_Triangles' 3D branch

    C3D_AttrInfo * attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);

    C3D_BufInfo * bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);

    const unsigned int baseVertexByteOffset = m_indexBaseOffset * m_currentVertexStride;
    const unsigned char * vertexBase =
        static_cast<const unsigned char *>(m_currentVertexData) + baseVertexByteOffset;

    if (hasDiffuse)
    {
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4);
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2);
        BufInfo_Add(bufInfo, vertexBase, 44, 4, 0x3210);
    }
    else if (hasUvAtOffset24)
    {
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2);
        AttrInfo_AddFixed(attrInfo, 2);
        {
            unsigned char lr, lg, lb;
            Compute_Flat_Light_Color(lr, lg, lb);
            C3D_FixedAttribSet(2, static_cast<float>(lr), static_cast<float>(lg), static_cast<float>(lb), 255.0f);
        }
        BufInfo_Add(bufInfo, vertexBase, m_currentVertexStride, 3, 0x210);
    }
    else
    {
        AttrInfo_AddFixed(attrInfo, 1);
        AttrInfo_AddFixed(attrInfo, 2);
        AttrInfo_AddFixed(attrInfo, 3);
        C3D_FixedAttribSet(1, 0.0f, 0.0f, 1.0f, 0.0f);
        {
            unsigned char lr, lg, lb;
            Compute_Flat_Light_Color(lr, lg, lb);
            C3D_FixedAttribSet(2, static_cast<float>(lr), static_cast<float>(lg), static_cast<float>(lb), 255.0f);
        }
        C3D_FixedAttribSet(3, 0.0f, 0.0f, 0.0f, 0.0f);
        BufInfo_Add(bufInfo, vertexBase, m_currentVertexStride, 1, 0x0);
    }

    C3D_Mtx transform;
    GGC_BuildWorldViewProjC3D(m_worldMtx, m_viewMtx, m_projMtx, &transform);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_uniformTransform, &transform);

    const unsigned int indexOffsetBytes =
        static_cast<unsigned int>(start_index) * sizeof(unsigned short);
    if (indexOffsetBytes >= m_currentIndexSizeBytes)
    {
        return;
    }
    const void * indices = static_cast<const unsigned char *>(m_currentIndexData) + indexOffsetBytes;

    C3D_DrawElements(GPU_TRIANGLE_STRIP, static_cast<int>(indexCount), C3D_UNSIGNED_SHORT, indices);
}

// -----------------------------------------------------------------------------
// Static vertex/index sub-range capture (rigid-mesh shared-container append)
// -----------------------------------------------------------------------------
//
// TheSuperHackers @feature githubawn 20/07/2026 VertexBufferClass::AppendLockClass /
// IndexBufferClass::AppendLockClass (dx8vertexbuffer.cpp/dx8indexbuffer.cpp) fire these when a
// rigid-mesh FVF-category shared container writes one mesh's worth of data into its own
// sub-range of a larger, pre-sized shared VB/IB -- e.g. a container built for capacity N gets
// mesh A appended at [0, k) when A first loads, then mesh B appended at [k, k+m) later when B
// loads, both into the SAME VertexBufferClass* / IndexBufferClass*, without the container's
// Get_Vertex_Count()/Get_Index_Count() (its fixed capacity) ever changing.
//
// Set_Vertex_Buffer(const VertexBufferClass*, stream)/Set_Index_Buffer(const IndexBufferClass*,
// offset) already capture this container's FULL current content lazily on FIRST bind, and
// already treat a Get_Vertex_Count()/Get_Index_Count() SIZE MISMATCH against the cached entry as
// a stale cache and fully re-capture (see their own comments) -- so a container that GROWS is
// already handled with no help from here. What is NOT already handled: a sub-range written
// AFTER this backend already cached+bound the container once, where the container's total size
// is unchanged (the common "fixed-capacity container, mesh added into a previously-unused or
// freed-and-reused slot" case) -- the cached linearAlloc'd copy would otherwise keep serving
// whatever was in that slot at the time of the original capture forever, exactly the same class
// of staleness Invalidate_Cached_Texture fixed for the font glyph atlas.
//
// Since the cached copy is a plain CPU-visible linearAlloc'd buffer (not a GPU-driver-managed
// buffer object), a precise in-place partial update -- memcpy just the written sub-range at its
// matching byte offset -- is both correct and cheap, mirroring BgfxBackend::Capture_Vertex_Sub_Range/
// Capture_Index_Sub_Range's own precise bgfx::update(handle, start, mem) partial GPU-buffer
// update (BgfxBackend.cpp ~4353/4399), rather than a full re-upload. A full re-upload would also
// be an acceptable (if less precise) fallback per this task's own guidance; used here only for
// the genuine-resize case below, which should not occur given the size-mismatch handling in
// Set_Vertex_Buffer/Set_Index_Buffer already described, but is handled defensively anyway.
//
// Containers whose VB/IB is BUFFER_TYPE_SORTING (AppendLockClass also fires for those, per its
// own dtor comment) are not handled here: Set_Vertex_Buffer(const VertexBufferClass*, stream)
// only ever caches BUFFER_TYPE_DX8 buffers (see its own Type() check), so m_staticVBCache/
// m_staticIBCache never contain an entry for a sorting-type container -- the find() below
// naturally no-ops for those, which is correct: actual sorted rendering reads from the
// per-frame DYNAMIC combined buffer instead (see Draw_Triangles' sorted-batch branch), not this
// static cache.

void Citro3dBackend::Capture_Vertex_Sub_Range(const VertexBufferClass * vb,
                                              const void * data,
                                              unsigned int start_vertex,
                                              unsigned int size_bytes)
{
    if (!m_initialized || vb == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }

    std::map<const VertexBufferClass *, GGCStaticBufferEntry>::iterator found = m_staticVBCache.find(vb);
    if (found == m_staticVBCache.end())
    {
        // Not cached yet (first bind hasn't happened, or this is a BUFFER_TYPE_SORTING
        // container -- see this section's own comment) -- Set_Vertex_Buffer's own first-bind
        // capture reads the D3D8 buffer's CURRENT full content, which already includes this
        // just-written sub-range, so there is nothing to do here.
        return;
    }

    const unsigned int stride = vb->FVF_Info().Get_FVF_Size();
    if (stride == 0)
    {
        return;
    }
    const unsigned int byteOffset = start_vertex * stride;
    const unsigned long long endByte = static_cast<unsigned long long>(byteOffset) + size_bytes;
    if (endByte > found->second.sizeBytes)
    {
        // TheSuperHackers @bugfix githubawn 20/07/2026 The sub-range no longer fits inside the
        // already-cached copy -- the container must have grown since that capture (should not
        // happen given Set_Vertex_Buffer's own size-mismatch staleness check, see this section's
        // top comment, but handled defensively). Evict rather than write out of bounds; the next
        // Set_Vertex_Buffer bind will see the size mismatch and fully re-capture from the D3D8
        // buffer's current (bigger) content.
        void * staleData = found->second.data;
        const bool wasCurrent = (m_currentVertexData == staleData);
        m_pendingFrees.push_back(staleData);
        m_staticVBCache.erase(found);
        if (wasCurrent)
        {
            m_currentVertexData = nullptr;
            m_currentVertexStride = 0;
            m_currentVertexSizeBytes = 0;
        }
        return;
    }

    std::memcpy(static_cast<unsigned char *>(found->second.data) + byteOffset, data, size_bytes);
}

void Citro3dBackend::Capture_Index_Sub_Range(const IndexBufferClass * ib,
                                             const void * data,
                                             unsigned int start_index,
                                             unsigned int size_bytes)
{
    if (!m_initialized || ib == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }

    std::map<const IndexBufferClass *, GGCStaticBufferEntry>::iterator found = m_staticIBCache.find(ib);
    if (found == m_staticIBCache.end())
    {
        // See the matching comment in Capture_Vertex_Sub_Range above.
        return;
    }

    const unsigned int byteOffset = start_index * static_cast<unsigned int>(sizeof(unsigned short));
    const unsigned long long endByte = static_cast<unsigned long long>(byteOffset) + size_bytes;
    if (endByte > found->second.sizeBytes)
    {
        // See the matching genuine-resize comment in Capture_Vertex_Sub_Range above.
        void * staleData = found->second.data;
        const bool wasCurrent = (m_currentIndexData == staleData);
        m_pendingFrees.push_back(staleData);
        m_staticIBCache.erase(found);
        if (wasCurrent)
        {
            m_currentIndexData = nullptr;
            m_currentIndexSizeBytes = 0;
        }
        return;
    }

    std::memcpy(static_cast<unsigned char *>(found->second.data) + byteOffset, data, size_bytes);
}
