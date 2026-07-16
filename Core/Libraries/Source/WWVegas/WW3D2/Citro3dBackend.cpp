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
#include "shader.h"
#include "texture.h"
#include "StubD3D8Device.h"

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
    , m_currentTexture(nullptr)
    , m_dynamicVertexData(nullptr)
    , m_dynamicVertexSizeBytes(0)
    , m_dynamicIndexData(nullptr)
    , m_dynamicIndexSizeBytes(0)
    , m_indexBaseOffset(0)
{
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

    Ensure_Shader_Loaded();
}

void Citro3dBackend::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    for (std::map<TextureBaseClass *, C3D_Tex_tag *>::iterator it = m_textureCache.begin();
         it != m_textureCache.end(); ++it)
    {
        C3D_Tex * tex = reinterpret_cast<C3D_Tex *>(it->second);
        C3D_TexDelete(tex);
        delete tex;
    }
    m_textureCache.clear();
    m_currentTexture = nullptr;

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
// for this whole draw path, so there's nothing to decode). Real 3D shader
// state (Milestones 4-5) is unhandled.

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

    std::map<TextureBaseClass *, C3D_Tex_tag *>::iterator found = m_textureCache.find(texture);
    if (found != m_textureCache.end())
    {
        return found->second;
    }

    TextureClass * tex2d = texture->As_TextureClass();
    IDirect3DTexture8 * d3dTex = texture->Peek_D3D_Texture();
    if (tex2d == nullptr || d3dTex == nullptr)
    {
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
            SDL_Log("[ggc-tex] #%d tex=%p fmt=%d w=%d h=%d supported=%d",
                    s_ggcTexLogCount, (void*)texture, (int)fmt,
                    tex2d->Get_Width(), tex2d->Get_Height(),
                    (int)(fmt == WW3D_FORMAT_A8R8G8B8 || fmt == WW3D_FORMAT_X8R8G8B8
                          || fmt == WW3D_FORMAT_R5G6B5 || fmt == WW3D_FORMAT_A4R4G4B4
                          || fmt == WW3D_FORMAT_A1R5G5B5));
        }
    }

    if (fmt != WW3D_FORMAT_A8R8G8B8 && fmt != WW3D_FORMAT_X8R8G8B8
        && fmt != WW3D_FORMAT_R5G6B5 && fmt != WW3D_FORMAT_A4R4G4B4
        && fmt != WW3D_FORMAT_A1R5G5B5)
    {
        // TheSuperHackers @todo githubawn 15/07/2026 DXT decompression is
        // not ported yet. Cache a null entry so this texture is not retried
        // every single Set_Texture call.
        m_textureCache[texture] = nullptr;
        return nullptr;
    }

    D3DLOCKED_RECT locked = { 0 };
    if (FAILED(d3dTex->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)) || locked.pBits == nullptr)
    {
        return nullptr;
    }

    const unsigned width = static_cast<unsigned>(tex2d->Get_Width());
    const unsigned height = static_cast<unsigned>(tex2d->Get_Height());

    // TheSuperHackers @tweak githubawn 16/07/2026 Upload at the matching
    // 2-byte-per-pixel PICA200 format for the three 16-bit source formats
    // instead of always expanding to 4-byte GPU_RGBA8 -- halves GPU/linear-
    // heap texture memory for these with no quality loss (lossless repack,
    // see GGC_SwizzleR5G6B5ToPicaRGB565 and neighbors). A8R8G8B8/X8R8G8B8
    // still upload as GPU_RGBA8 (down-converting a 32bpp source is a real,
    // separately-auditable quality tradeoff, not a free win).
    const GPU_TEXCOLOR gpuFormat =
        (fmt == WW3D_FORMAT_R5G6B5) ? GPU_RGB565 :
        (fmt == WW3D_FORMAT_A4R4G4B4) ? GPU_RGBA4 :
        (fmt == WW3D_FORMAT_A1R5G5B5) ? GPU_RGBA5551 :
        GPU_RGBA8;

    C3D_Tex * tex = new C3D_Tex;
    if (!C3D_TexInit(tex, static_cast<u16>(width), static_cast<u16>(height), gpuFormat))
    {
        d3dTex->UnlockRect(0);
        delete tex;
        return nullptr;
    }

    const unsigned char * srcBits = static_cast<const unsigned char *>(locked.pBits);
    const unsigned srcPitch = static_cast<unsigned>(locked.Pitch);
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
    else
    {
        GGC_SwizzleBGRA8ToPicaRGBA8(srcBits, srcPitch, dstBits, width, height);
    }
    C3D_TexFlush(tex);
    // TheSuperHackers @tweak githubawn 16/07/2026 GPU_LINEAR blurred small
    // UI text (font glyph atlas) and other pixel-scale UI art on the 3DS's
    // low-res screen. GPU_NEAREST keeps everything crisp -- appropriate for
    // this 2D UI draw path in general, not just text.
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

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
    m_textureCache[texture] = result;
    return result;
}

void Citro3dBackend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Backend::Set_Texture(stage, texture);

    if (!m_initialized || stage != 0)
    {
        return;
    }

    m_currentTexture = Ensure_Texture(texture);
}

void Citro3dBackend::Release_Cached_Texture(TextureBaseClass * texture)
{
    DX8Backend::Release_Cached_Texture(texture);

    if (texture == nullptr)
    {
        return;
    }

    std::map<TextureBaseClass *, C3D_Tex_tag *>::iterator found = m_textureCache.find(texture);
    if (found == m_textureCache.end())
    {
        return;
    }

    if (found->second != nullptr)
    {
        C3D_Tex * tex = reinterpret_cast<C3D_Tex *>(found->second);
        if (m_currentTexture == found->second)
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

    std::map<TextureBaseClass *, C3D_Tex_tag *>::iterator found = m_textureCache.find(texture);
    if (found == m_textureCache.end())
    {
        return;
    }

    if (found->second != nullptr)
    {
        C3D_Tex * tex = reinterpret_cast<C3D_Tex *>(found->second);
        if (m_currentTexture == found->second)
        {
            m_currentTexture = nullptr;
        }
        C3D_TexDelete(tex);
        delete tex;
    }
    m_textureCache.erase(found);
}

void Citro3dBackend::Apply_Tex_Env(bool texturing_enabled)
{
    C3D_TexEnv * env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);

    if (texturing_enabled && m_currentTexture != nullptr)
    {
        C3D_TexBind(0, reinterpret_cast<C3D_Tex *>(m_currentTexture));
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    }
    else
    {
        C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
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
}

void Citro3dBackend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    DX8Backend::Set_Index_Buffer(iba, index_base_offset);
    m_indexBaseOffset = index_base_offset;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void Citro3dBackend::Draw_Triangles(unsigned short start_index,
                                    unsigned short polygon_count,
                                    unsigned short min_vertex_index,
                                    unsigned short vertex_count)
{
    DX8Backend::Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);

    if (!m_initialized || !m_shaderLoaded)
    {
        return;
    }

    // 2D UI detection: Render2DClass always draws with identity world+view
    // (render2d.cpp), which DX8Wrapper already tracks regardless of which
    // backend is active. Real 3D world geometry (Milestones 4-5) is not
    // translated yet, so skip anything that is not a 2D UI draw.
    if (!DX8Backend::Is_World_Identity() || !DX8Backend::Is_View_Identity())
    {
        return;
    }

    if (m_dynamicVertexData == nullptr || m_dynamicIndexData == nullptr || polygon_count == 0)
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

    C3D_BindProgram(reinterpret_cast<shaderProgram_s *>(m_program));

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
    BufInfo_Add(bufInfo, m_dynamicVertexData, 44, 4, 0x3210);

    Apply_Tex_Env(m_texturingEnabled);

    C3D_Mtx transform;
    Mtx_OrthoTilt(&transform, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_uniformTransform, &transform);

    const unsigned int indexOffsetBytes =
        (static_cast<unsigned int>(start_index) + m_indexBaseOffset) * sizeof(unsigned short);
    if (indexOffsetBytes >= m_dynamicIndexSizeBytes)
    {
        return;
    }
    const void * indices = static_cast<const unsigned char *>(m_dynamicIndexData) + indexOffsetBytes;

    C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(polygon_count) * 3, C3D_UNSIGNED_SHORT, indices);
}
