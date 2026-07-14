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

// TheSuperHackers @build githubawn 10/07/2026 PS2 render backend Tier 0
// stub -- see PS2Backend.h and docs/ps2-port-plan.md.
//
// TheSuperHackers @build githubawn 11/07/2026 gsKit init + Clear() bring-up.
// Call sequence follows gsKit's own examples/basic/basic.c (github.com/
// ps2dev/gsKit) as ground truth rather than reconstructing it from headers
// alone. bgfx was considered and ruled out for this seam: its entire
// renderer abstraction is built around compiling vertex/fragment shaders
// (via shaderc) to each target API's bytecode, and the GS has no
// programmable shader units at all -- it's a fixed-function rasterizer
// driven by GIF register writes (blend mode, texture combiner stage,
// etc. as state, not code). There is no shader concept for bgfx to
// target, official or unofficial, on this hardware.

#include "PS2Backend.h"
#include "vector3.h"
#include "dx8fvf.h"
#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"
#include "texture.h"
#include "DXTUtils.h"
#include "dx8wrapper.h"

#include <gsKit.h>
#include <dmaKit.h>
#include <gsTexManager.h>
#include <cstring>
#include <algorithm>
#if defined(__PS2__)
#include <malloc.h>  // mallinfo() for the mid-match memory snapshot below
#endif

// TheSuperHackers @build githubawn 12/07/2026 TEMP diagnostic -- counts and
// logs real per-frame call activity to host:ps2_render_diag.txt so we can
// tell whether Clear/Begin_Scene/End_Scene are actually reached at runtime
// and what flip_frame is, without relying on screenshots. Remove once the
// black-screen/"No Image" issue is root-caused.
#include <cstdio>
namespace
{
    int s_beginSceneCount = 0;
    int s_endSceneCount = 0;
    int s_endSceneFlipTrueCount = 0;
    int s_clearCount = 0;
    bool s_clearColorFlagEverTrue = false;
    bool s_clearColorFlagEverFalse = false;
    float s_lastClearR = -1.0f;
    float s_lastClearG = -1.0f;
    float s_lastClearB = -1.0f;
    float s_maxClearR = 0.0f;
    float s_maxClearG = 0.0f;
    float s_maxClearB = 0.0f;

    // TheSuperHackers @build githubawn 12/07/2026 TEMP diagnostic: which
    // geometry submission path(s) does the engine actually use? The engine
    // has at least three distinct routes (static Set_Vertex_Buffer+
    // Draw_Triangles, DynamicVBAccessClass overload, Submit_Sorted_Draw)
    // and it's not yet known which the shell/menu screen exercises.
    int s_captureVertexDataCount = 0;
    int s_captureIndexDataCount = 0;
    int s_captureDynamicVertexDataCount = 0;
    int s_captureDynamicIndexDataCount = 0;
    int s_setVertexBufferStaticCount = 0;
    int s_setVertexBufferDynamicCount = 0;
    int s_setIndexBufferStaticCount = 0;
    int s_setIndexBufferDynamicCount = 0;
    int s_drawTrianglesCount = 0;
    int s_drawTrianglesSkippedNullBind = 0;
    int s_drawTrianglesSkippedCacheMiss = 0;
    int s_drawTrianglesActualTriDrawn = 0;
    int s_drawTrianglesSkippedBadCoord = 0;
    int s_submitSortedDrawCount = 0;
    float s_lastGoodSX0 = 0.0f, s_lastGoodSY0 = 0.0f;
    float s_lastBadSX0 = 0.0f, s_lastBadSY0 = 0.0f;
    bool s_hadBadCoord = false;
    int s_lastDynVbCacheSize = 0, s_lastDynIbCacheSize = 0;
    int s_lastVbCacheSize = 0, s_lastIbCacheSize = 0;
    int s_maxDynVbCacheSize = 0, s_maxVbCacheSize = 0;

    // TheSuperHackers @build githubawn 12/07/2026 TEMP: unconditional
    // per-triangle detail of the LAST attempted triangle, so whatever is on
    // disk at the moment PCSX2 itself segfaults reflects the actual fatal
    // triangle's raw data. The crash reproduced at the exact same triangle
    // count (63024) across two runs with materially different code (buffer
    // size, cache eviction), ruling those out -- this is data-dependent.
    int s_lastAttemptFvf = 0;
    bool s_lastAttemptPretransformed = false;
    float s_lastAttemptSX[3] = {0,0,0}, s_lastAttemptSY[3] = {0,0,0};
    unsigned int s_lastAttemptColor[3] = {0,0,0};

    // TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic: user
    // hypothesis worth checking directly rather than assuming -- is the
    // screen black because terrain draws with color 0, or because a 2D UI
    // quad (e.g. the shell's full-screen "BlankWindow.wnd" background)
    // draws AFTER terrain in the same frame and covers it, unrelated to
    // terrain's own color? Track per-frame draw order: last 3D (terrain)
    // draw index, and any large near-fullscreen 2D (pretransformed) quad's
    // index + bbox + color. Dumped once per frame, first 3 frames only.
    int s_frameDrawIndex = 0;
    int s_lastTerrainDrawIndex = -1;
    int s_largeUIQuadDrawIndex = -1;
    float s_largeUIQuadMinX = 0, s_largeUIQuadMinY = 0, s_largeUIQuadMaxX = 0, s_largeUIQuadMaxY = 0;
    unsigned int s_largeUIQuadColor = 0;
    int s_framesDumped = 0;

    void LogPS2RenderDiag()
    {
        FILE * fp = fopen("host:ps2_render_diag.txt", "w");
        if (fp != nullptr) {
            fprintf(fp, "Begin_Scene calls: %d\n", s_beginSceneCount);
            fprintf(fp, "End_Scene calls: %d (flip_frame=true: %d)\n", s_endSceneCount, s_endSceneFlipTrueCount);
            fprintf(fp, "Clear calls: %d (clear_color ever true: %d, ever false: %d)\n",
                    s_clearCount, s_clearColorFlagEverTrue, s_clearColorFlagEverFalse);
            fprintf(fp, "Last clear color: R=%f G=%f B=%f\n", s_lastClearR, s_lastClearG, s_lastClearB);
            fprintf(fp, "Max clear color seen: R=%f G=%f B=%f\n", s_maxClearR, s_maxClearG, s_maxClearB);
            fprintf(fp, "Capture_Vertex_Data: %d, Capture_Index_Data: %d\n",
                    s_captureVertexDataCount, s_captureIndexDataCount);
            fprintf(fp, "Capture_Dynamic_Vertex_Data: %d, Capture_Dynamic_Index_Data: %d\n",
                    s_captureDynamicVertexDataCount, s_captureDynamicIndexDataCount);
            fprintf(fp, "Set_Vertex_Buffer(static): %d, Set_Vertex_Buffer(dynamic): %d\n",
                    s_setVertexBufferStaticCount, s_setVertexBufferDynamicCount);
            fprintf(fp, "Set_Index_Buffer(static): %d, Set_Index_Buffer(dynamic): %d\n",
                    s_setIndexBufferStaticCount, s_setIndexBufferDynamicCount);
            fprintf(fp, "Draw_Triangles calls: %d (skipped null bind: %d, skipped cache miss: %d, skipped bad coord: %d, actual tris drawn: %d)\n",
                    s_drawTrianglesCount, s_drawTrianglesSkippedNullBind, s_drawTrianglesSkippedCacheMiss, s_drawTrianglesSkippedBadCoord, s_drawTrianglesActualTriDrawn);
            fprintf(fp, "Last good tri v0 screen coord: (%f, %f)\n", s_lastGoodSX0, s_lastGoodSY0);
            fprintf(fp, "Had bad coord: %d, last bad tri v0 screen coord: (%f, %f)\n", s_hadBadCoord, s_lastBadSX0, s_lastBadSY0);
            fprintf(fp, "Submit_Sorted_Draw calls: %d\n", s_submitSortedDrawCount);
            fprintf(fp, "Cache sizes just before clear -- dynVB: %d (max %d), dynIB: %d, static VB: %d (max %d), static IB: %d\n",
                    s_lastDynVbCacheSize, s_maxDynVbCacheSize, s_lastDynIbCacheSize,
                    s_lastVbCacheSize, s_maxVbCacheSize, s_lastIbCacheSize);
            fprintf(fp, "Last attempted triangle: fvf=0x%X pretransformed=%d\n",
                    s_lastAttemptFvf, s_lastAttemptPretransformed);
            fprintf(fp, "  v0: (%f, %f) color=0x%08X\n", s_lastAttemptSX[0], s_lastAttemptSY[0], s_lastAttemptColor[0]);
            fprintf(fp, "  v1: (%f, %f) color=0x%08X\n", s_lastAttemptSX[1], s_lastAttemptSY[1], s_lastAttemptColor[1]);
            fprintf(fp, "  v2: (%f, %f) color=0x%08X\n", s_lastAttemptSX[2], s_lastAttemptSY[2], s_lastAttemptColor[2]);
            fclose(fp);
        }
    }
}

PS2Backend::PS2Backend()
    : m_gsGlobal(nullptr)
    , m_bindMode(BIND_NONE)
    , m_boundVB(nullptr)
    , m_boundIB(nullptr)
    , m_boundDynVB(nullptr)
    , m_boundDynIB(nullptr)
    , m_indexBaseOffset(0)
    , m_boundTexture(nullptr)
    // TheSuperHackers @build githubawn 13/07/2026 Was false (disabling GS-
    // side texture sampling for any draw not flagged pretransformed 2D UI),
    // added as a memory-saving experiment earlier this session -- confirmed
    // by direct measurement to save zero memory (StubD3D8Device already
    // allocates a texture's native scratch at CreateTexture/load time,
    // before this draw-time flag is ever consulted), so there was never a
    // real tradeoff to begin with. Also found (docs/ps2-port-plan.md,
    // memory-budget section) to be silently breaking real UI content: the
    // main menu logo submits as non-pretransformed geometry (isPretransformed
    // false, confirmed via host: diagnostics), so with this off its texture
    // was never sampled -- rendering as a flat white untextured quad.
    , m_enable3DTextures(true)
    , m_textureCacheVramBytes(0)
    , m_currentFrameIndex(0)
    , m_world(true)
    , m_view(true)
    , m_projection(true)
{
    WWDEBUG_SAY(("[PS2Backend] Backend constructed."));
}

PS2Backend::~PS2Backend()
{
}

void PS2Backend::Initialize(void * hwnd, int width, int height)
{
    // TheSuperHackers @bugfix githubawn 13/07/2026 The oneshot drawbuffer
    // (Os_AllocSize) is host EE RAM used to accumulate GIF packets for the
    // CURRENT frame's primitives -- a completely separate resource from the
    // GS's own 4MB VRAM. gsKit does NOT bounds-check writes into this pool:
    // once a single frame's primitive count exceeds it, gsKit_prim_triangle_*
    // / TexManager_bind keep writing past the end of the allocation, silently
    // corrupting whatever heap memory comes next (confirmed via a host:
    // diagnostic logging pool_max[dbuf] - pool_cur directly, which went
    // negative and grew steadily more negative ~144 bytes/triangle before the
    // process visibly "hung"). A real skirmish frame needs far more
    // primitives than the shell map's decorative background ever did.
    // Rather than size this for the worst-case frame (10MB alone pushed us
    // OOM at the 128MB dev budget; the buffer is DOUBLE-buffered internally,
    // so Os_AllocSize is charged twice), keep it SMALL and flush mid-frame
    // when it approaches full (see MaybeFlushOneshot(), called from
    // DrawCapturedTriangle). gsKit's oneshot pool is double-buffered
    // (pool[2]/dbuf) precisely so gsKit_queue_exec can kick one buffer's DMA
    // while the next fills -- flushing mid-frame is the standard PS2 pattern
    // for scenes larger than one buffer, and makes the required size
    // independent of scene complexity. 1MB per buffer (2MB total) is ample
    // between flushes. Per_AllocSize (persistent pool) is unused on this path.
    GSGLOBAL * gsGlobal = gsKit_init_global_custom(1 * 1024 * 1024, 256 * 1024);
    // TheSuperHackers @build githubawn 12/07/2026 TEMP diagnostic: real
    // triangles now submit successfully with valid on-screen coordinates and
    // solid white (0xFFFFFFFF) color, yet nothing appears -- disabling alpha
    // blending entirely rules out a blend-equation misunderstanding (e.g.
    // GS alpha=0x80 not actually meaning "fully opaque" the way assumed)
    // as the reason opaque triangles could still resolve to invisible.
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);

    // TheSuperHackers @build githubawn 13/07/2026 gsKit_init_screen() picks
    // the real output size from the PS2's own TV-standard video mode (NTSC
    // non-interlaced = 640x448, not the engine's PC-oriented 640x480
    // DEFAULT_RESOLUTION_WIDTH/HEIGHT passed in via the width/height params
    // above, which this function has never actually consulted -- gsKit
    // doesn't take a requested size here, it reports what the hardware
    // supports). Every other subsystem that cares about screen size (2D UI
    // layout, mouse-to-screen mapping, the projection matrix's aspect
    // ratio) reads back DX8Wrapper::ResolutionWidth/ResolutionHeight, which
    // stayed stuck at the 640x480 construction-time default with no PS2
    // code ever correcting it. Write the real detected size back so the
    // rest of the engine agrees with what the GS is actually displaying,
    // the same handshake a real width/height negotiation would do.
    DX8Wrapper::ResolutionWidth = gsGlobal->Width;
    DX8Wrapper::ResolutionHeight = gsGlobal->Height;

    // TheSuperHackers @build githubawn 12/07/2026 CORRECTED: GS_PERSISTENT is
    // gsKit's "set it and forget it" pool -- primitives stay queued until
    // manually cleared, they do NOT get freed by gsKit_queue_exec(). It is
    // meant for static geometry set up once, not continuously-changing
    // per-frame content. GS_ONESHOT is the double-buffered pool that swaps
    // (and implicitly clears the just-executed half) on every
    // gsKit_queue_exec() -- exactly what a real per-frame game loop needs.
    // Using PERSISTENT here meant every triangle ever drawn since boot
    // stayed queued forever; this was misdiagnosed as the earlier "No Image"
    // display bug's fix (that was actually fixed by gsKit_init_screen()
    // being called correctly at all), and only surfaced once real geometry
    // submission began: PCSX2 itself (any GS renderer, Vulkan or Software)
    // segfaulted once cumulative queued primitives overflowed the drawbuffer
    // -- explaining why enlarging the buffer only delayed, not fixed, the
    // crash, and why the crash point scaled with total triangles drawn
    // since boot rather than any one frame's content.
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    // TheSuperHackers @build githubawn 12/07/2026 Tier 1 geometry bring-up:
    // no depth buffer management exists yet on this path (single overdraw
    // layer, no Z read/write), so disable Z-test entirely rather than pass
    // an arbitrary, meaningless Z value that could occasionally clip
    // triangles against uninitialized VRAM depth contents.
    gsKit_set_test(gsGlobal, GS_ZTEST_OFF);

    // TheSuperHackers @build githubawn 12/07/2026 Texture bring-up: gsKit's
    // texture manager handles VRAM allocation/upload/caching for us -- see
    // EnsurePS2Texture()/DrawCapturedTriangle() below.
    gsKit_TexManager_init(gsGlobal);

    m_gsGlobal = reinterpret_cast<struct gsGlobal *>(gsGlobal);

    WWDEBUG_SAY(("[PS2Backend] gsKit initialized: %dx%d", gsGlobal->Width, gsGlobal->Height));
}

void PS2Backend::Shutdown()
{
    m_gsGlobal = nullptr;
}

void PS2Backend::Begin_Scene()
{
    ++s_beginSceneCount;
    ++m_currentFrameIndex;
    // TheSuperHackers @build githubawn 12/07/2026 Dynamic VB/IB content is
    // only ever valid for the frame it was written in (the same ring-buffer
    // memory gets reused/overwritten every frame by design), and
    // DynamicVBAccessClass/DynamicIBAccessClass instances are typically
    // stack-allocated per draw call site -- caching them keyed by pointer
    // with no eviction meant the caches grew for every new call site the
    // menu exercised over time, unbounded. That is suspected to be why
    // PCSX2 itself (not the emulated game) segfaulted after growing numbers
    // of frames once real triangle submission started: increasing gsKit's
    // persistent buffer 16x only pushed the crash back proportionally
    // rather than fixing it, consistent with a real leak rather than a
    // fixed-size overflow. Record sizes right before clearing so the diag
    // file shows how large they grew over the previous frame(s).
    s_lastDynVbCacheSize = static_cast<int>(m_dynVbCache.size());
    s_lastDynIbCacheSize = static_cast<int>(m_dynIbCache.size());
    s_lastVbCacheSize = static_cast<int>(m_vbCache.size());
    s_lastIbCacheSize = static_cast<int>(m_ibCache.size());
    if (s_lastDynVbCacheSize > s_maxDynVbCacheSize) s_maxDynVbCacheSize = s_lastDynVbCacheSize;
    if (s_lastVbCacheSize > s_maxVbCacheSize) s_maxVbCacheSize = s_lastVbCacheSize;

    m_dynVbCache.clear();
    m_dynIbCache.clear();
    m_boundDynVB = nullptr;
    m_boundDynIB = nullptr;
    if (m_bindMode == BIND_DYNAMIC) {
        m_bindMode = BIND_NONE;
    }
    if (m_gsGlobal != nullptr) {
        gsKit_TexManager_nextFrame(reinterpret_cast<GSGLOBAL *>(m_gsGlobal));
    }
    if (s_beginSceneCount <= 5 || (s_beginSceneCount % 60) == 0) {
        LogPS2RenderDiag();
    }
    // TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic: reset
    // per-frame draw-order tracking (see s_frameDrawIndex declaration).
    s_frameDrawIndex = 0;
    s_lastTerrainDrawIndex = -1;
    s_largeUIQuadDrawIndex = -1;
}

void PS2Backend::End_Scene(bool flip_frame)
{
    ++s_endSceneCount;
    if (flip_frame) {
        ++s_endSceneFlipTrueCount;
    }
    if (s_endSceneCount <= 5 || (s_endSceneCount % 60) == 0) {
        LogPS2RenderDiag();
    }
    // TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic: dump
    // this frame's draw-order summary (first 3 frames only) -- checking the
    // user's hypothesis that a large 2D UI quad drawn after terrain, not
    // terrain's own color, is what's covering the screen.
    if (s_framesDumped < 3) {
        ++s_framesDumped;
        FILE * fp = fopen("host:ps2_draworder_diag.txt", "a");
        if (fp != nullptr) {
            fprintf(fp, "frame %d: totalDraws=%d lastTerrainDrawIdx=%d largeUIQuadDrawIdx=%d",
                s_framesDumped, s_frameDrawIndex, s_lastTerrainDrawIndex, s_largeUIQuadDrawIndex);
            if (s_largeUIQuadDrawIndex >= 0) {
                fprintf(fp, " uiQuadBBox=(%f,%f)-(%f,%f) uiQuadColor=0x%08X drawnAfterTerrain=%d",
                    s_largeUIQuadMinX, s_largeUIQuadMinY, s_largeUIQuadMaxX, s_largeUIQuadMaxY,
                    s_largeUIQuadColor, (int)(s_largeUIQuadDrawIndex > s_lastTerrainDrawIndex));
            }
            fprintf(fp, "\n");
            fclose(fp);
        }
    }

    if (m_gsGlobal == nullptr) {
        return;
    }
    GSGLOBAL * gsGlobal = reinterpret_cast<GSGLOBAL *>(m_gsGlobal);

    gsKit_queue_exec(gsGlobal);
    if (flip_frame) {
        gsKit_sync_flip(gsGlobal);
    }

#if defined(__PS2__)
    // TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic: one-shot
    // mid-match total-heap snapshot at a frame we reliably reach in a running
    // skirmish. Since the game no longer OOMs (which is what used to trigger
    // the full memory census in GameMemory.cpp), this is the only way to see
    // where live memory actually settles now, to quantify the RAM cuts and
    // decide whether further ones are needed. arena is the newlib heap's total
    // from the OS (covers everything: GameMemory pools, texture scratch, gsKit
    // buffers, raw blocks).
    if (s_endSceneCount == 60) {
        struct mallinfo mi = mallinfo();
        FILE * fp = fopen("host:ps2_memsnapshot.txt", "w");
        if (fp != nullptr) {
            fprintf(fp, "at End_Scene #60: arena(total from OS)=%u uordblks(in-use)=%u fordblks(free)=%u\n",
                (unsigned)mi.arena, (unsigned)mi.uordblks, (unsigned)mi.fordblks);
            extern size_t GetPS2ScratchLiveBytes();
            fprintf(fp, "StubD3D8 scratch LIVE = %u bytes\n", (unsigned)GetPS2ScratchLiveBytes());
            fclose(fp);
        }
    }
#endif
}

void PS2Backend::Flip_To_Primary()
{
}

void PS2Backend::Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil)
{
    ++s_clearCount;
    if (clear_color) {
        s_clearColorFlagEverTrue = true;
        s_lastClearR = color.X;
        s_lastClearG = color.Y;
        s_lastClearB = color.Z;
        if (color.X > s_maxClearR) s_maxClearR = color.X;
        if (color.Y > s_maxClearG) s_maxClearG = color.Y;
        if (color.Z > s_maxClearB) s_maxClearB = color.Z;
    } else {
        s_clearColorFlagEverFalse = true;
    }
    if (s_clearCount <= 5 || (s_clearCount % 60) == 0) {
        LogPS2RenderDiag();
    }

    if (m_gsGlobal == nullptr || !clear_color) {
        return;
    }
    GSGLOBAL * gsGlobal = reinterpret_cast<GSGLOBAL *>(m_gsGlobal);

    unsigned char r = static_cast<unsigned char>(color.X * 255.0f);
    unsigned char g = static_cast<unsigned char>(color.Y * 255.0f);
    unsigned char b = static_cast<unsigned char>(color.Z * 255.0f);

    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(r, g, b, 0x00, 0x00));
}

// -- Tier 1: minimal fixed-function software T&L ----------------------------
//
// TheSuperHackers @build githubawn 12/07/2026 See PS2Backend.h for the
// overall design rationale. Capture_Vertex_Data/Capture_Index_Data are
// generic IRenderBackend hooks called from dx8vertexbuffer.cpp/
// dx8indexbuffer.cpp for every backend (not bgfx-specific), so real geometry
// bytes reach us here regardless of the D3D8 stub device doing nothing.

void PS2Backend::Capture_Vertex_Data(const VertexBufferClass * vb,
                                     const void * data, unsigned int size_bytes)
{
    ++s_captureVertexDataCount;
    if (vb == nullptr || data == nullptr || size_bytes == 0) {
        return;
    }
    CapturedVertexBuffer & entry = m_vbCache[vb];
    entry.bytes.resize(size_bytes);
    memcpy(entry.bytes.data(), data, size_bytes);
    entry.fvf = vb->FVF_Info().Get_FVF();
    entry.stride = vb->FVF_Info().Get_FVF_Size();
}

void PS2Backend::Capture_Index_Data(const IndexBufferClass * ib,
                                    const void * data, unsigned int size_bytes)
{
    ++s_captureIndexDataCount;
    if (ib == nullptr || data == nullptr || size_bytes == 0) {
        return;
    }
    CapturedIndexBuffer & entry = m_ibCache[ib];
    entry.indices.resize(size_bytes / sizeof(unsigned short));
    memcpy(entry.indices.data(), data, size_bytes);
}

void PS2Backend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    ++s_setVertexBufferStaticCount;
    DX8Backend::Set_Vertex_Buffer(vb, stream);
    m_boundVB = vb;
    m_bindMode = (vb != nullptr) ? BIND_STATIC : BIND_NONE;
}

void PS2Backend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    ++s_setIndexBufferStaticCount;
    DX8Backend::Set_Index_Buffer(ib, index_base_offset);
    m_boundIB = ib;
    m_indexBaseOffset = index_base_offset;
}

void PS2Backend::Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                             const void * data, unsigned int size_bytes)
{
    ++s_captureDynamicVertexDataCount;
    if (vba == nullptr || data == nullptr || size_bytes == 0) {
        return;
    }
    CapturedVertexBuffer & entry = m_dynVbCache[vba];
    entry.bytes.resize(size_bytes);
    memcpy(entry.bytes.data(), data, size_bytes);
    entry.fvf = vba->FVF_Info().Get_FVF();
    entry.stride = vba->FVF_Info().Get_FVF_Size();
}

void PS2Backend::Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                            const void * data, unsigned int size_bytes)
{
    ++s_captureDynamicIndexDataCount;
    if (iba == nullptr || data == nullptr || size_bytes == 0) {
        return;
    }
    CapturedIndexBuffer & entry = m_dynIbCache[iba];
    entry.indices.resize(size_bytes / sizeof(unsigned short));
    memcpy(entry.indices.data(), data, size_bytes);
}

void PS2Backend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    ++s_setVertexBufferDynamicCount;
    DX8Backend::Set_Vertex_Buffer(vba);
    m_boundDynVB = &vba;
    m_bindMode = BIND_DYNAMIC;
}

void PS2Backend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    ++s_setIndexBufferDynamicCount;
    DX8Backend::Set_Index_Buffer(iba, index_base_offset);
    m_boundDynIB = &iba;
    m_indexBaseOffset = index_base_offset;
    m_bindMode = BIND_DYNAMIC;
}

void PS2Backend::Submit_Sorted_Draw(const DynamicVBAccessClass & /*dyn_vb*/,
                                    const DynamicIBAccessClass & /*dyn_ib*/,
                                    unsigned short /*polygon_count*/,
                                    unsigned short /*vertex_count*/)
{
    ++s_submitSortedDrawCount;
}

RenderResource PS2Backend::Register_Loaded_Texture(TextureBaseClass * tex)
{
    // Real work happens lazily in EnsurePS2Texture() the first time the
    // texture is actually bound (mirrors BgfxBackend's EnsureBgfxTexture
    // pattern) -- Set_Texture receives the TextureBaseClass* pointer
    // directly, so this handle only needs to be non-invalid, never looked
    // up again.
    RenderResource rr;
    rr.id = reinterpret_cast<std::uint64_t>(tex);
    return rr;
}

void PS2Backend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Backend::Set_Texture(stage, texture);
    if (stage == 0) {
        m_boundTexture = texture;
    }
}

namespace
{
    // TheSuperHackers @build githubawn 12/07/2026 BC1/BC2/BC3 (DXT1/3/5)
    // software block decompression. The GS has no native block-compressed
    // texture format (its only space-saving mechanism is palettized
    // PSMT8/PSMT4 via a CLUT, not block compression), so unlike a modern
    // GPU we can't just upload the compressed bytes -- decode to RGBA8 on
    // the EE once per distinct texture (not a hot path) same as every other
    // format here. DXT_SurfacePitch/DXT_SurfaceRows (DXTUtils.h) are already
    // proven correct by StubD3D8Device/BgfxBackendTextures against this same
    // stub texture pipeline -- reused rather than re-derived. DXT2 is
    // treated as DXT3 and DXT4 as DXT5 (premultiplied-alpha variants have an
    // identical block layout; the distinction doesn't matter for UI art).
    inline void Decode565(unsigned short p, unsigned char & r, unsigned char & g, unsigned char & b)
    {
        r = static_cast<unsigned char>(((p >> 11) & 0x1F) * 255 / 31);
        g = static_cast<unsigned char>(((p >> 5) & 0x3F) * 255 / 63);
        b = static_cast<unsigned char>((p & 0x1F) * 255 / 31);
    }

    void DecodeDXTBlock(const unsigned char * block, WW3DFormat format,
                        unsigned char outBlock[16][4])
    {
        const unsigned char * alphaBlock = nullptr;
        const unsigned char * colorBlock = block;
        const bool hasExplicitAlpha4 = (format == WW3D_FORMAT_DXT2 || format == WW3D_FORMAT_DXT3);
        const bool hasInterpolatedAlpha = (format == WW3D_FORMAT_DXT4 || format == WW3D_FORMAT_DXT5);
        if (hasExplicitAlpha4 || hasInterpolatedAlpha) {
            alphaBlock = block;
            colorBlock = block + 8;
        }

        unsigned short c0, c1;
        memcpy(&c0, colorBlock + 0, 2);
        memcpy(&c1, colorBlock + 2, 2);
        unsigned int indices;
        memcpy(&indices, colorBlock + 4, 4);

        unsigned char r0, g0, b0, r1, g1, b1;
        Decode565(c0, r0, g0, b0);
        Decode565(c1, r1, g1, b1);

        // DXT1 with c0<=c1 means "3-color + transparent black" mode; DXT3/5
        // always use 4-color opaque mode (their alpha comes from the
        // separate alpha block instead).
        const bool fourColorOpaque = hasExplicitAlpha4 || hasInterpolatedAlpha || (c0 > c1);

        unsigned char palette[4][3];
        palette[0][0] = r0; palette[0][1] = g0; palette[0][2] = b0;
        palette[1][0] = r1; palette[1][1] = g1; palette[1][2] = b1;
        if (fourColorOpaque) {
            palette[2][0] = static_cast<unsigned char>((2 * r0 + r1) / 3);
            palette[2][1] = static_cast<unsigned char>((2 * g0 + g1) / 3);
            palette[2][2] = static_cast<unsigned char>((2 * b0 + b1) / 3);
            palette[3][0] = static_cast<unsigned char>((r0 + 2 * r1) / 3);
            palette[3][1] = static_cast<unsigned char>((g0 + 2 * g1) / 3);
            palette[3][2] = static_cast<unsigned char>((b0 + 2 * b1) / 3);
        } else {
            palette[2][0] = static_cast<unsigned char>((r0 + r1) / 2);
            palette[2][1] = static_cast<unsigned char>((g0 + g1) / 2);
            palette[2][2] = static_cast<unsigned char>((b0 + b1) / 2);
            palette[3][0] = 0; palette[3][1] = 0; palette[3][2] = 0; // transparent black
        }

        unsigned char alpha[16];
        if (hasExplicitAlpha4) {
            for (int i = 0; i < 16; ++i) {
                const unsigned char nibble = (alphaBlock[i / 2] >> ((i % 2) * 4)) & 0xF;
                alpha[i] = static_cast<unsigned char>(nibble * 17);
            }
        } else if (hasInterpolatedAlpha) {
            const unsigned char a0 = alphaBlock[0];
            const unsigned char a1 = alphaBlock[1];
            unsigned long long alphaIdx = 0;
            memcpy(&alphaIdx, alphaBlock + 2, 6); // 48 bits, 3 bits/texel
            unsigned char aPalette[8];
            aPalette[0] = a0; aPalette[1] = a1;
            if (a0 > a1) {
                for (int i = 1; i <= 6; ++i) {
                    aPalette[1 + i] = static_cast<unsigned char>(((7 - i) * a0 + i * a1) / 7);
                }
            } else {
                for (int i = 1; i <= 4; ++i) {
                    aPalette[1 + i] = static_cast<unsigned char>(((5 - i) * a0 + i * a1) / 5);
                }
                aPalette[6] = 0;
                aPalette[7] = 255;
            }
            for (int i = 0; i < 16; ++i) {
                const unsigned idx = static_cast<unsigned>((alphaIdx >> (i * 3)) & 0x7);
                alpha[i] = aPalette[idx];
            }
        } else {
            for (int i = 0; i < 16; ++i) {
                alpha[i] = (fourColorOpaque || ((indices >> (2 * i)) & 3) != 3) ? 0xFF : 0;
            }
        }

        for (int i = 0; i < 16; ++i) {
            const unsigned idx = (indices >> (2 * i)) & 3;
            outBlock[i][0] = palette[idx][0];
            outBlock[i][1] = palette[idx][1];
            outBlock[i][2] = palette[idx][2];
            outBlock[i][3] = alpha[i];
        }
    }

    bool DecodeDXT(const D3DLOCKED_RECT & locked, WW3DFormat format,
                   unsigned width, unsigned height,
                   std::vector<unsigned char> & outRgba8)
    {
        const unsigned blockBytes = (format == WW3D_FORMAT_DXT1) ? 8 : 16;
        const unsigned pitch = DXT_SurfacePitch(width, blockBytes);
        const unsigned rows = DXT_SurfaceRows(height);
        const unsigned char * base = static_cast<const unsigned char *>(locked.pBits);

        outRgba8.resize(width * height * 4);

        for (unsigned by = 0; by < rows; ++by) {
            const unsigned char * rowPtr = base + by * pitch;
            const unsigned blocksPerRow = (width + 3) / 4;
            for (unsigned bx = 0; bx < blocksPerRow; ++bx) {
                unsigned char outBlock[16][4];
                DecodeDXTBlock(rowPtr + bx * blockBytes, format, outBlock);

                for (int py = 0; py < 4; ++py) {
                    const unsigned destY = by * 4 + py;
                    if (destY >= height) break;
                    for (int px = 0; px < 4; ++px) {
                        const unsigned destX = bx * 4 + px;
                        if (destX >= width) break;
                        unsigned char * dst = outRgba8.data() + (destY * width + destX) * 4;
                        const unsigned char * src = outBlock[py * 4 + px];
                        dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
                    }
                }
            }
        }
        return true;
    }

    // D3D8's 32-bit RGB formats are stored in memory as B,G,R,A (little
    // endian view of 0xAARRGGBB); GS_PSM_CT32 expects R,G,B,A. Packed
    // 16-bit formats are unpacked to 8-bit-per-channel RGBA the same way
    // BgfxBackendTextures.cpp's Expand*ToBGRA8 helpers do (verified against
    // that already-working code rather than re-deriving bit layouts).
    bool ConvertToRGBA8(const D3DLOCKED_RECT & locked, WW3DFormat format,
                        unsigned width, unsigned height,
                        std::vector<unsigned char> & outRgba8)
    {
        outRgba8.resize(width * height * 4);
        const unsigned char * srcBase = static_cast<const unsigned char *>(locked.pBits);

        switch (format) {
            case WW3D_FORMAT_A8R8G8B8:
            case WW3D_FORMAT_X8R8G8B8:
                for (unsigned y = 0; y < height; ++y) {
                    const unsigned char * src = srcBase + y * locked.Pitch;
                    unsigned char * dst = outRgba8.data() + y * width * 4;
                    for (unsigned x = 0; x < width; ++x) {
                        dst[0] = src[2]; // R
                        dst[1] = src[1]; // G
                        dst[2] = src[0]; // B
                        dst[3] = (format == WW3D_FORMAT_A8R8G8B8) ? src[3] : 0xFF;
                        src += 4; dst += 4;
                    }
                }
                return true;
            case WW3D_FORMAT_R8G8B8:
                for (unsigned y = 0; y < height; ++y) {
                    const unsigned char * src = srcBase + y * locked.Pitch;
                    unsigned char * dst = outRgba8.data() + y * width * 4;
                    for (unsigned x = 0; x < width; ++x) {
                        dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = 0xFF;
                        src += 3; dst += 4;
                    }
                }
                return true;
            case WW3D_FORMAT_R5G6B5:
                for (unsigned y = 0; y < height; ++y) {
                    const unsigned short * src = reinterpret_cast<const unsigned short *>(srcBase + y * locked.Pitch);
                    unsigned char * dst = outRgba8.data() + y * width * 4;
                    for (unsigned x = 0; x < width; ++x) {
                        unsigned short p = src[x];
                        dst[0] = static_cast<unsigned char>(((p >> 11) & 0x1F) * 255 / 31);
                        dst[1] = static_cast<unsigned char>(((p >> 5) & 0x3F) * 255 / 63);
                        dst[2] = static_cast<unsigned char>((p & 0x1F) * 255 / 31);
                        dst[3] = 0xFF;
                        dst += 4;
                    }
                }
                return true;
            case WW3D_FORMAT_A1R5G5B5:
            case WW3D_FORMAT_X1R5G5B5:
                for (unsigned y = 0; y < height; ++y) {
                    const unsigned short * src = reinterpret_cast<const unsigned short *>(srcBase + y * locked.Pitch);
                    unsigned char * dst = outRgba8.data() + y * width * 4;
                    for (unsigned x = 0; x < width; ++x) {
                        unsigned short p = src[x];
                        dst[0] = static_cast<unsigned char>(((p >> 10) & 0x1F) * 255 / 31);
                        dst[1] = static_cast<unsigned char>(((p >> 5) & 0x1F) * 255 / 31);
                        dst[2] = static_cast<unsigned char>((p & 0x1F) * 255 / 31);
                        dst[3] = (format == WW3D_FORMAT_A1R5G5B5) ? ((p & 0x8000) ? 0xFF : 0) : 0xFF;
                        dst += 4;
                    }
                }
                return true;
            case WW3D_FORMAT_A4R4G4B4:
            case WW3D_FORMAT_X4R4G4B4:
                for (unsigned y = 0; y < height; ++y) {
                    const unsigned short * src = reinterpret_cast<const unsigned short *>(srcBase + y * locked.Pitch);
                    unsigned char * dst = outRgba8.data() + y * width * 4;
                    for (unsigned x = 0; x < width; ++x) {
                        unsigned short p = src[x];
                        dst[0] = static_cast<unsigned char>(((p >> 8) & 0xF) * 17);
                        dst[1] = static_cast<unsigned char>(((p >> 4) & 0xF) * 17);
                        dst[2] = static_cast<unsigned char>((p & 0xF) * 17);
                        dst[3] = (format == WW3D_FORMAT_A4R4G4B4) ? static_cast<unsigned char>(((p >> 12) & 0xF) * 17) : 0xFF;
                        dst += 4;
                    }
                }
                return true;
            case WW3D_FORMAT_DXT1:
            case WW3D_FORMAT_DXT2:
            case WW3D_FORMAT_DXT3:
            case WW3D_FORMAT_DXT4:
            case WW3D_FORMAT_DXT5:
                return DecodeDXT(locked, format, width, height, outRgba8);
            default:
                return false; // Palettized formats not yet supported.
        }
    }
}

namespace
{
    // TheSuperHackers @build githubawn 12/07/2026 TEMP diagnostic: the
    // first texture bring-up attempt hung PCSX2 (no crash, no progress,
    // diag counters frozen) shortly after the first few triangles drew.
    // Unconditional since this only fires once per distinct texture (not a
    // hot path) -- logs each step so whatever's on disk when it hangs shows
    // exactly which call got stuck or what dimensions were involved.
    void LogTexStep(const char * step, int a = 0, int b = 0, int c = 0)
    {
        FILE * fp = fopen("host:ps2_texture_diag.txt", "a");
        if (fp != nullptr) {
            fprintf(fp, "%s a=%d b=%d c=%d\n", step, a, b, c);
            fclose(fp);
        }
    }

    // TheSuperHackers @build githubawn 12/07/2026 Halve an RGBA8 image via a
    // 2x2 box filter (one mip-level step). GS VRAM (4MB total, shared with
    // the framebuffer and every other bound texture) is a real hardware
    // limit, not a PCSX2 quirk -- PCSX2 emulates the actual GS's VRAM size,
    // so real background/menu art (512x512, 1024x1024 DXT-compressed
    // source) genuinely cannot fit at native resolution as decoded RGBA8.
    // Repeated halving trades resolution for VRAM fit; this is a bring-up
    // stopgap; the real PS2-appropriate answer is native palettized
    // PSMT8/PSMT4 upload instead of always expanding to RGBA8 (tracked in
    // docs/ps2-port-plan.md).
    void HalveRGBA8(unsigned srcW, unsigned srcH, const std::vector<unsigned char> & src,
                    unsigned & outW, unsigned & outH, std::vector<unsigned char> & dst)
    {
        outW = (std::max)(1u, srcW / 2);
        outH = (std::max)(1u, srcH / 2);
        dst.resize(outW * outH * 4);
        for (unsigned y = 0; y < outH; ++y) {
            const unsigned sy0 = (std::min)(y * 2, srcH - 1);
            const unsigned sy1 = (std::min)(y * 2 + 1, srcH - 1);
            for (unsigned x = 0; x < outW; ++x) {
                const unsigned sx0 = (std::min)(x * 2, srcW - 1);
                const unsigned sx1 = (std::min)(x * 2 + 1, srcW - 1);
                const unsigned char * p00 = src.data() + (sy0 * srcW + sx0) * 4;
                const unsigned char * p01 = src.data() + (sy0 * srcW + sx1) * 4;
                const unsigned char * p10 = src.data() + (sy1 * srcW + sx0) * 4;
                const unsigned char * p11 = src.data() + (sy1 * srcW + sx1) * 4;
                unsigned char * d = dst.data() + (y * outW + x) * 4;
                for (int c = 0; c < 4; ++c) {
                    d[c] = static_cast<unsigned char>((p00[c] + p01[c] + p10[c] + p11[c]) / 4);
                }
            }
        }
    }
}

GSTEXTURE * PS2Backend::EnsurePS2Texture(TextureBaseClass * tex)
{
    if (tex == nullptr) {
        return nullptr;
    }

    auto it = m_textureCache.find(tex);
    if (it != m_textureCache.end()) {
        it->second.lastUsedFrame = m_currentFrameIndex;
        return it->second.valid ? &it->second.gsTex : nullptr;
    }

    LogTexStep("enter", reinterpret_cast<int>(tex));
    CapturedTexture & entry = m_textureCache[tex];
    entry.valid = false;
    entry.vramBytes = 0;
    entry.lastUsedFrame = m_currentFrameIndex;

    TextureClass * tex2d = tex->As_TextureClass();
    IDirect3DTexture8 * d3dTex = (tex2d != nullptr) ? tex->Peek_D3D_Texture() : nullptr;
    if (tex2d == nullptr || d3dTex == nullptr) {
        LogTexStep("no_d3d_mirror");
        return nullptr;
    }

    LogTexStep("before_getleveldesc");
    D3DSURFACE_DESC desc;
    if (FAILED(d3dTex->GetLevelDesc(0, &desc))) {
        LogTexStep("getleveldesc_failed");
        return nullptr;
    }
    LogTexStep("after_getleveldesc", desc.Width, desc.Height, static_cast<int>(desc.Format));

    // TheSuperHackers @build githubawn 12/07/2026 Sanity cap on the NATIVE
    // decode size only (separate from the VRAM-fit cap below) -- guards
    // against ever allocating a runaway scratch buffer if desc ever
    // contained garbage dimensions. Real game textures are well under this.
    const unsigned maxNativeDim = 2048;
    if (desc.Width == 0 || desc.Height == 0 ||
        desc.Width > maxNativeDim || desc.Height > maxNativeDim) {
        LogTexStep("rejected_dimensions", desc.Width, desc.Height);
        return nullptr;
    }

    LogTexStep("before_lockrect");
    D3DLOCKED_RECT locked;
    if (FAILED(d3dTex->LockRect(0, &locked, nullptr, D3DLOCK_READONLY))) {
        LogTexStep("lockrect_failed");
        return nullptr;
    }
    LogTexStep("after_lockrect", static_cast<int>(locked.Pitch));

    std::vector<unsigned char> nativeRgba8;
    const bool converted = ConvertToRGBA8(locked, tex2d->Get_Texture_Format(), desc.Width, desc.Height, nativeRgba8);
    LogTexStep("after_convert", converted ? 1 : 0);
    d3dTex->UnlockRect(0);
    if (!converted) {
        return nullptr;
    }

    // TheSuperHackers @build githubawn 12/07/2026 Real GS VRAM is only 4MB
    // total on real hardware (and PCSX2 enforces the same budget) -- shared
    // with the framebuffer, Z-buffer, and every other bound texture. A
    // 1024x1024 RGBA32 texture is, by itself, exactly 4MB and hung
    // gsKit_TexManager_bind's VRAM allocator outright rather than failing
    // gracefully (confirmed via per-step host: diagnostics: the hang was
    // pinpointed to that exact call for that exact texture; every smaller
    // 64x64 texture before it bound and drew fine). This is precisely why
    // the original PC assets are DXT-compressed -- uncompressed RGBA8 was
    // never meant to fit this hardware's video memory at real texture
    // sizes. Rather than reject anything above budget outright (which meant
    // no background/menu art rendered at all), repeatedly halve via a 2x2
    // box filter until it fits -- a real PS2 VRAM constraint, not just a
    // PCSX2 one, so this tradeoff would be needed on real hardware too.
    // 128x128 (64KB) is a conservative per-texture bring-up budget; the
    // real Tier 1 answer (native palettized PSMT8/PSMT4 upload, no
    // resolution loss) is tracked separately in docs/ps2-port-plan.md.
    const unsigned maxDim = 128;
    unsigned curW = desc.Width, curH = desc.Height;
    std::vector<unsigned char> * curBuf = &nativeRgba8;
    std::vector<unsigned char> scratchA, scratchB;
    bool useA = true;
    while (curW > maxDim || curH > maxDim) {
        std::vector<unsigned char> & dst = useA ? scratchA : scratchB;
        unsigned nextW, nextH;
        HalveRGBA8(curW, curH, *curBuf, nextW, nextH, dst);
        curW = nextW; curH = nextH;
        curBuf = &dst;
        useA = !useA;
    }
    entry.rgba8 = *curBuf;
    LogTexStep("after_downsample", curW, curH);

    // TheSuperHackers @build githubawn 13/07/2026 Our own downsampled copy
    // above (entry.rgba8) is now cached and is what every future call for
    // this texture returns via the m_textureCache hit at function entry --
    // d3dTex is never read again after this point (this whole decode block
    // only runs once per texture, on a cache miss). Release its
    // native-resolution D3D8-level scratch now instead of leaving it
    // resident for the texture's whole lifetime; measured at ~27MB total
    // live across all textures at the shell-map OOM point
    // (docs/ps2-port-plan.md). No-ops safely for anything not D3DPOOL_
    // MANAGED or already surfaced -- see ReleaseNativeScratchIfManagedPS2's
    // own comment for why those are excluded.
    {
        extern int ReleaseNativeScratchIfManagedPS2(IDirect3DTexture8 * tex);
        ReleaseNativeScratchIfManagedPS2(d3dTex);
    }

    // TheSuperHackers @bugfix githubawn 13/07/2026 Proactively evict the
    // least-recently-used cached texture(s) before this one would push us
    // over budget (see m_textureCacheVramBytes's declaration in
    // PS2Backend.h): gsKit_TexManager_bind's own internal VRAM allocator
    // hangs outright rather than evicting when it runs out of room mid-
    // frame, and gsKit_TexManager_nextFrame() (our only other eviction
    // trigger) runs once per Begin_Scene -- too coarse when a single frame
    // needs to bind more distinct textures than fit in VRAM at once
    // (confirmed: the hang point moved between the shell map and a
    // skirmish match, tracking cumulative distinct-texture VRAM pressure
    // rather than any fixed triangle/draw count). 1.5MB leaves real
    // headroom under the GS's 4MB total for the double-buffered 640x448x4
    // framebuffer (~1.15MB) plus gsKit's own bookkeeping -- conservative on
    // purpose, since real hardware has no more room to give than PCSX2
    // does here.
    const unsigned kTextureVramBudget = 1536u * 1024u;
    const unsigned newBytes = curW * curH * 4u;
    while (m_textureCacheVramBytes + newBytes > kTextureVramBudget && !m_textureCache.empty()) {
        auto oldestIt = m_textureCache.end();
        unsigned oldestFrame = 0;
        bool haveCandidate = false;
        for (auto mapIt = m_textureCache.begin(); mapIt != m_textureCache.end(); ++mapIt) {
            if (!mapIt->second.valid) {
                continue; // skip our own in-progress entry (not valid yet) and any failed entries
            }
            if (!haveCandidate || mapIt->second.lastUsedFrame < oldestFrame) {
                oldestFrame = mapIt->second.lastUsedFrame;
                oldestIt = mapIt;
                haveCandidate = true;
            }
        }
        if (!haveCandidate) {
            break; // nothing evictable left (only our own in-progress entry remains)
        }
        if (m_gsGlobal != nullptr) {
            gsKit_TexManager_free(reinterpret_cast<GSGLOBAL *>(m_gsGlobal), &oldestIt->second.gsTex);
        }
        m_textureCacheVramBytes -= oldestIt->second.vramBytes;
        m_textureCache.erase(oldestIt);
    }
    m_textureCacheVramBytes += newBytes;
    entry.vramBytes = newBytes;

    memset(&entry.gsTex, 0, sizeof(entry.gsTex));
    entry.gsTex.Width = curW;
    entry.gsTex.Height = curH;
    entry.gsTex.PSM = GS_PSM_CT32;
    entry.gsTex.Filter = GS_FILTER_LINEAR;
    entry.gsTex.Delayed = 0;
    entry.gsTex.Mem = reinterpret_cast<u32 *>(entry.rgba8.data());
    LogTexStep("before_setup_tbw");
    gsKit_setup_tbw(&entry.gsTex);
    LogTexStep("done");
    entry.valid = true;

    return &entry.gsTex;
}

void PS2Backend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    DX8Backend::Set_Transform(transform, m);
    switch (transform)
    {
        case RB_TRANSFORM_WORLD:      m_world = m; break;
        case RB_TRANSFORM_VIEW:       m_view = m; break;
        case RB_TRANSFORM_PROJECTION: m_projection = m; break;
        default: break;
    }
}

void PS2Backend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    DX8Backend::Set_Transform(transform, m);
    Matrix4x4 m4(m);
    switch (transform)
    {
        case RB_TRANSFORM_WORLD: m_world = m4; break;
        case RB_TRANSFORM_VIEW:  m_view = m4; break;
        default: break;
    }
}

namespace
{
    inline unsigned int PackVertexColor(unsigned fvf, unsigned diffuseOffset, const unsigned char * v)
    {
        if ((fvf & D3DFVF_DIFFUSE) == 0) {
            return 0xFFFFFFFFu; // FVF has no diffuse channel: D3D8 fixed-function default is white.
        }
        unsigned int packed = 0;
        memcpy(&packed, v + diffuseOffset, sizeof(packed));
        return packed;
    }

    // TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic: which
    // textures run in 2D UI vs 3D world draws, and their approximate native
    // memory footprint, independent of whether m_enable3DTextures is on --
    // this must observe every texture regardless of the sampling toggle, so
    // it queries GetLevelDesc directly (cheap, no lock/copy) rather than
    // going through EnsurePS2Texture's full decode path. Approximates the
    // native scratch size the same way StubD3D8Texture computes it
    // (width*height*bytesPerPixel, +33% for the 3-mip-level chain that
    // TerrainTextureClass specifically requests -- see the earlier research
    // that traced this) since the exact StubD3D8Device-side number isn't
    // exposed per-texture, only as a running total (GetPS2ScratchTotalBytes).
    std::unordered_map<const TextureBaseClass *, bool> & GetSeenTextureMap()
    {
        static std::unordered_map<const TextureBaseClass *, bool> s_seen;
        return s_seen;
    }

    unsigned int & Get2DTextureBytes() { static unsigned int v = 0; return v; }
    unsigned int & Get3DTextureBytes() { static unsigned int v = 0; return v; }

    void TrackTextureUsage(TextureBaseClass * tex, bool isPretransformed)
    {
        if (tex == nullptr) {
            return;
        }
        auto & seen = GetSeenTextureMap();
        if (seen.find(tex) != seen.end()) {
            return; // Already counted -- avoid double-counting across many draw calls.
        }
        TextureClass * tex2d = tex->As_TextureClass();
        IDirect3DTexture8 * d3dTex = (tex2d != nullptr) ? tex->Peek_D3D_Texture() : nullptr;
        if (d3dTex == nullptr) {
            return;
        }
        D3DSURFACE_DESC desc;
        if (FAILED(d3dTex->GetLevelDesc(0, &desc))) {
            return;
        }
        seen[tex] = true;

        unsigned bpp = 4;
        switch (tex2d->Get_Texture_Format()) {
            case WW3D_FORMAT_R5G6B5:
            case WW3D_FORMAT_A1R5G5B5:
            case WW3D_FORMAT_X1R5G5B5:
            case WW3D_FORMAT_A4R4G4B4:
            case WW3D_FORMAT_X4R4G4B4:
                bpp = 2;
                break;
            case WW3D_FORMAT_DXT1: case WW3D_FORMAT_DXT2: case WW3D_FORMAT_DXT3:
            case WW3D_FORMAT_DXT4: case WW3D_FORMAT_DXT5:
                bpp = 1; // Compressed source; native scratch is still decompressed-ish sized in this stub, treat as rough lower bound.
                break;
            default:
                bpp = 4;
                break;
        }
        const unsigned approxNativeBytes = static_cast<unsigned>(
            (static_cast<unsigned long long>(desc.Width) * desc.Height * bpp * 4) / 3); // +33% for mips

        FILE * fp = fopen("host:ps2_texture_breakdown.txt", "a");
        if (fp != nullptr) {
            fprintf(fp, "%s %ux%u fmt=%d approxNativeBytes=%u\n",
                isPretransformed ? "2D" : "3D", desc.Width, desc.Height,
                static_cast<int>(tex2d->Get_Texture_Format()), approxNativeBytes);
            fclose(fp);
        }
        if (isPretransformed) {
            Get2DTextureBytes() += approxNativeBytes;
        } else {
            Get3DTextureBytes() += approxNativeBytes;
        }
    }
}

void PS2Backend::MaybeFlushOneshot()
{
    if (m_gsGlobal == nullptr) {
        return;
    }
    GSGLOBAL * gsGlobal = reinterpret_cast<GSGLOBAL *>(m_gsGlobal);
    if (gsGlobal->CurQueue == nullptr) {
        return;
    }

    // Bytes still free in the current oneshot draw buffer (see the
    // pool_max/pool_cur layout in gsInit.h's struct gsQueue). When this drops
    // below a safety threshold, kick the accumulated packets to the GS now and
    // let the double-buffered pool reset for the next batch, rather than let a
    // later primitive write past the end (which corrupts heap memory -- the
    // bug this guards against; see Initialize()). The threshold is deliberately
    // generous: a textured triangle plus its TexManager bind packet is on the
    // order of a couple hundred bytes, so 128KB leaves headroom for hundreds
    // more primitives even if several are queued before the next check.
    const long remaining = (long)((char*)gsGlobal->CurQueue->pool_max[gsGlobal->CurQueue->dbuf]
        - (char*)gsGlobal->CurQueue->pool_cur);
    const long kFlushThreshold = 128 * 1024;
    if (remaining < kFlushThreshold) {
        // gsKit_queue_exec kicks the oneshot (and persistent) queue's DMA and
        // resets the oneshot pool. We do NOT sync_flip here -- the display
        // framebuffer only swaps at End_Scene; this mid-frame kick just draws
        // the accumulated primitives to the same back framebuffer and frees
        // the pool to keep filling.
        gsKit_queue_exec(gsGlobal);
    }
}

void PS2Backend::DrawCapturedTriangle(const unsigned char * v0,
                                      const unsigned char * v1,
                                      const unsigned char * v2,
                                      unsigned fvf,
                                      unsigned locationOffset,
                                      unsigned diffuseOffset,
                                      unsigned texOffset,
                                      bool hasTexCoord)
{
    if (m_gsGlobal == nullptr) {
        return;
    }
    GSGLOBAL * gsGlobal = reinterpret_cast<GSGLOBAL *>(m_gsGlobal);

    // Flush the oneshot buffer if it is getting full BEFORE we add this
    // triangle's primitive (and its TexManager bind packet) -- checking here,
    // between whole primitives, guarantees we never split a GIF packet.
    MaybeFlushOneshot();

    const bool isPretransformed = (fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW;
    const unsigned char * verts[3] = { v0, v1, v2 };
    float sx[3], sy[3];

    if (isPretransformed) {
        // D3DFVF_XYZRHW vertices are already in viewport pixel space (this
        // is how the engine draws 2D UI/menu quads under fixed-function
        // D3D8) -- use x/y directly, no matrix transform needed.
        for (int i = 0; i < 3; ++i) {
            float pos[3];
            memcpy(pos, verts[i] + locationOffset, sizeof(pos));
            sx[i] = pos[0];
            sy[i] = pos[1];
        }
    } else {
        Matrix4x4 combined = m_projection * m_view * m_world;
        for (int i = 0; i < 3; ++i) {
            float pos[3];
            memcpy(pos, verts[i] + locationOffset, sizeof(pos));
            Vector4 clip;
            // TheSuperHackers @bugfix githubawn 13/07/2026 The Vector3-input
            // overload of Transform_Vector hardcodes out->W = 1.0f instead
            // of computing it from the matrix's row 3 (see Matrix4.h) -- it
            // is only correct for pure affine matrices (bottom row
            // [0,0,0,1]), not a real perspective projection matrix, whose
            // row 3 is what actually produces a usable W for the
            // perspective divide below. combined = projection*view*world
            // always carries a real projection matrix here, so using the
            // Vector3 overload silently threw away the perspective divide
            // entirely (clip.W was always exactly 1.0 regardless of depth),
            // producing wildly wrong screen coordinates for any vertex not
            // at the origin -- this is why terrain never appeared even
            // though every triangle "successfully" submitted. Use the
            // Vector4 overload (with an explicit w=1 input) which computes
            // all four output components from the matrix, W included.
            Matrix4x4::Transform_Vector(combined, Vector4(pos[0], pos[1], pos[2], 1.0f), &clip);
            if (clip.W < 0.0001f) {
                return; // Behind the eye or degenerate -- skip (no clipping yet).
            }
            const float ndcX = clip.X / clip.W;
            const float ndcY = clip.Y / clip.W;
            sx[i] = (ndcX * 0.5f + 0.5f) * gsGlobal->Width;
            sy[i] = (1.0f - (ndcY * 0.5f + 0.5f)) * gsGlobal->Height;
        }
    }

    // TheSuperHackers @build githubawn 12/07/2026 A PCSX2 crash (wild-pointer
    // TLB miss store) was observed shortly after this code path went live.
    // Suspected cause: unsanitized transform output (NaN, or a huge value
    // from a near-zero perspective divide) reaching gsKit's internal
    // fixed-point conversion. Reject anything not finite or wildly outside
    // the visible screen range rather than ever handing gsKit a bad value.
    for (int i = 0; i < 3; ++i) {
        const bool finite = (sx[i] == sx[i]) && (sy[i] == sy[i]) // NaN != itself
            && (sx[i] > -1.0e6f) && (sx[i] < 1.0e6f)
            && (sy[i] > -1.0e6f) && (sy[i] < 1.0e6f);
        if (!finite) {
            ++s_drawTrianglesSkippedBadCoord;
            s_hadBadCoord = true;
            s_lastBadSX0 = sx[0];
            s_lastBadSY0 = sy[0];
            return;
        }
    }
    s_lastGoodSX0 = sx[0];
    s_lastGoodSY0 = sy[0];

    unsigned int c0 = PackVertexColor(fvf, diffuseOffset, v0);
    unsigned int c1 = PackVertexColor(fvf, diffuseOffset, v1);
    unsigned int c2 = PackVertexColor(fvf, diffuseOffset, v2);

    s_lastAttemptFvf = static_cast<int>(fvf);
    s_lastAttemptPretransformed = isPretransformed;
    s_lastAttemptSX[0] = sx[0]; s_lastAttemptSY[0] = sy[0]; s_lastAttemptColor[0] = c0;
    s_lastAttemptSX[1] = sx[1]; s_lastAttemptSY[1] = sy[1]; s_lastAttemptColor[1] = c1;
    s_lastAttemptSX[2] = sx[2]; s_lastAttemptSY[2] = sy[2]; s_lastAttemptColor[2] = c2;

    // TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic (see
    // s_frameDrawIndex declaration above): track draw order to check
    // whether a large 2D UI quad draws AFTER terrain and covers it.
    ++s_frameDrawIndex;
    if (!isPretransformed) {
        s_lastTerrainDrawIndex = s_frameDrawIndex;
    } else if (m_gsGlobal != nullptr) {
        GSGLOBAL * gg = reinterpret_cast<GSGLOBAL *>(m_gsGlobal);
        float minX = sx[0], maxX = sx[0], minY = sy[0], maxY = sy[0];
        for (int i = 1; i < 3; ++i) {
            minX = (std::min)(minX, sx[i]); maxX = (std::max)(maxX, sx[i]);
            minY = (std::min)(minY, sy[i]); maxY = (std::max)(maxY, sy[i]);
        }
        const float coveredFracX = (maxX - minX) / (float)gg->Width;
        const float coveredFracY = (maxY - minY) / (float)gg->Height;
        if (coveredFracX > 0.5f && coveredFracY > 0.5f) {
            s_largeUIQuadDrawIndex = s_frameDrawIndex;
            s_largeUIQuadMinX = minX; s_largeUIQuadMinY = minY;
            s_largeUIQuadMaxX = maxX; s_largeUIQuadMaxY = maxY;
            s_largeUIQuadColor = c0;
        }
    }
    // TheSuperHackers @build githubawn 12/07/2026 Was previously logged
    // unconditionally (every triangle) to catch a since-fixed crash (see
    // GS_ONESHOT fix in Initialize()). That was a host: file I/O round-trip
    // per triangle -- with thousands of triangles/frame it tanked real FPS
    // to ~4. No longer needed; state is still recorded above for the
    // throttled logging call sites (Begin_Scene/End_Scene/Clear/Draw_Triangles).

    auto ToGSColor = [](unsigned int argb) -> u64 {
        const unsigned char r = (argb >> 16) & 0xFF;
        const unsigned char g = (argb >> 8) & 0xFF;
        const unsigned char b = argb & 0xFF;
        return GS_SETREG_RGBAQ(r, g, b, 0x80, 0x00);
    };

    // TheSuperHackers @build githubawn 12/07/2026 Textured path: only taken
    // when a texture is actually bound AND the FVF has a texture coordinate
    // to read (some draws, e.g. debug wireframes, have neither). gsKit's
    // texture functions want texel-space UV (0..Width/0..Height), not the
    // normalized [0,1] UV the engine stores -- confirmed via gsKit's own
    // gsKit_float_to_int_u/v inline helpers (gsInline.h), not assumed.
    // TheSuperHackers @build githubawn 13/07/2026 3D world draws (terrain,
    // units -- anything not pretransformed 2D UI) skip texture sampling
    // entirely when m_enable3DTextures is off, falling through to the
    // flat/gouraud vertex-colored path below. This does NOT reduce the
    // texture's native scratch memory (StubD3D8Device already allocated
    // that at texture-load time, before this draw call ever runs) -- it
    // only avoids capturing/uploading a GS-side copy for 3D-only textures,
    // saving m_textureCache/GS VRAM. See docs/ps2-port-plan.md for the
    // ongoing investigation into the actual 2D-vs-3D memory split.
    if (hasTexCoord && m_boundTexture != nullptr) {
        TrackTextureUsage(m_boundTexture, isPretransformed);
    }
    const bool wantTexture = hasTexCoord && m_boundTexture != nullptr &&
        (isPretransformed || m_enable3DTextures);
    GSTEXTURE * gsTex = wantTexture ? EnsurePS2Texture(m_boundTexture) : nullptr;
    if (gsTex != nullptr) {
        float u[3], vcoord[3];
        for (int i = 0; i < 3; ++i) {
            float uv[2];
            memcpy(uv, verts[i] + texOffset, sizeof(uv));
            u[i] = uv[0] * gsTex->Width;
            vcoord[i] = uv[1] * gsTex->Height;
        }
        // TheSuperHackers @build githubawn 12/07/2026 Per-triangle logging
        // here (previously before_bind/after_bind/after_prim on every
        // textured draw) was removed -- it tanked FPS from 30 to 6, the same
        // mistake as the earlier per-triangle Draw_Triangles logging. It did
        // its job finding the gsKit_TexManager_bind hang; EnsurePS2Texture's
        // once-per-distinct-texture logging is enough going forward.
        // TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic,
        // re-added and throttled (first 20 calls to THIS non-pretransformed
        // i.e. 3D-world branch only, not the already-proven-fine 2D UI
        // branch): terrain draws are a brand-new caller of this exact
        // gsKit_TexManager_bind call site (m_enable3DTextures was false
        // for all of this session until the menu-logo fix flipped it on),
        // and the engine hangs shortly after terrain textures start
        // binding. Confirming whether this specific call is where it dies,
        // and which texture (pointer/dims) it dies on.
#if defined(__PS2__)
        // TheSuperHackers @build githubawn 13/07/2026 First 20 calls all
        // succeeded cleanly (see docs/ps2-port-plan.md) -- the hang is
        // further in. Switched to a sparse unbounded heartbeat (every 200
        // calls past the first 20) so we can see the last call number that
        // got logged before the process froze, without the per-triangle
        // logging FPS cost the comment above warns about.
        static int s_ggc3DTexCallNum = 0;
        bool ggcShouldLog = false;
        if (!isPretransformed) {
            ++s_ggc3DTexCallNum;
            // TheSuperHackers @build githubawn 13/07/2026 For the skirmish
            // scenario the freeze point is consistently right after call
            // 94000, reproducible byte-for-byte even after adding proactive
            // VRAM eviction (which had zero effect on the freeze point --
            // ruling out gsKit VRAM exhaustion as the cause). Narrowing the
            // dense window to bracket the true last-completing call.
            ggcShouldLog = (s_ggc3DTexCallNum <= 20) || (s_ggc3DTexCallNum % 200) == 0
                || (s_ggc3DTexCallNum >= 94600 && s_ggc3DTexCallNum <= 95500);
            if (ggcShouldLog) {
                FILE * fp = fopen("host:ps2_3dtexbind_diag.txt", "a");
                if (fp != nullptr) {
                    // TheSuperHackers @build githubawn 13/07/2026 Testing the
                    // GS_ONESHOT drawbuffer-exhaustion hypothesis: this frame
                    // (67) submits far more primitives than the shell map
                    // ever did, and the buffer can only be freed on flip
                    // (End_Scene), which can't happen mid-frame -- a genuine
                    // deadlock if a single frame's GIF-packet data exceeds
                    // Os_AllocSize. Logging remaining oneshot pool space.
                    long remaining = -1;
                    if (gsGlobal->CurQueue != nullptr) {
                        remaining = (long)((char*)gsGlobal->CurQueue->pool_max[gsGlobal->CurQueue->dbuf]
                            - (char*)gsGlobal->CurQueue->pool_cur);
                    }
                    fprintf(fp, "call=%d before bind: gsTex=%p Width=%d Height=%d boundTexture=%p oneshotRemaining=%ld\n",
                        s_ggc3DTexCallNum, (void*)gsTex, gsTex->Width, gsTex->Height, (void*)m_boundTexture, remaining);
                    fclose(fp);
                }
            }
        }
#endif
        gsKit_TexManager_bind(gsGlobal, gsTex);
#if defined(__PS2__)
        if (!isPretransformed && ggcShouldLog) {
            FILE * fp = fopen("host:ps2_3dtexbind_diag.txt", "a");
            if (fp != nullptr) { fprintf(fp, "call=%d bind RETURNED\n", s_ggc3DTexCallNum); fclose(fp); }
        }
#endif
        gsKit_prim_triangle_goraud_texture_3d(gsGlobal, gsTex,
            sx[0], sy[0], 0, u[0], vcoord[0],
            sx[1], sy[1], 0, u[1], vcoord[1],
            sx[2], sy[2], 0, u[2], vcoord[2],
            ToGSColor(c0), ToGSColor(c1), ToGSColor(c2));
#if defined(__PS2__)
        if (!isPretransformed && ggcShouldLog) {
            FILE * fp = fopen("host:ps2_3dtexbind_diag.txt", "a");
            if (fp != nullptr) { fprintf(fp, "call=%d prim_triangle RETURNED\n", s_ggc3DTexCallNum); fclose(fp); }
        }
#endif
        return;
    }

    gsKit_prim_triangle_gouraud_3d(gsGlobal,
        sx[0], sy[0], 0,
        sx[1], sy[1], 0,
        sx[2], sy[2], 0,
        ToGSColor(c0), ToGSColor(c1), ToGSColor(c2));
}

void PS2Backend::Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    ++s_drawTrianglesCount;
    if (s_drawTrianglesCount <= 30 || (s_drawTrianglesCount % 60) == 0) {
        LogPS2RenderDiag();
    }

    const CapturedVertexBuffer * vbEntryPtr = nullptr;
    const CapturedIndexBuffer * ibEntryPtr = nullptr;

    if (m_bindMode == BIND_STATIC) {
        if (m_boundVB == nullptr || m_boundIB == nullptr) {
            ++s_drawTrianglesSkippedNullBind;
            return;
        }
        auto vbIt = m_vbCache.find(m_boundVB);
        auto ibIt = m_ibCache.find(m_boundIB);
        if (vbIt == m_vbCache.end() || ibIt == m_ibCache.end()) {
            ++s_drawTrianglesSkippedCacheMiss;
            return;
        }
        vbEntryPtr = &vbIt->second;
        ibEntryPtr = &ibIt->second;
    } else if (m_bindMode == BIND_DYNAMIC) {
        if (m_boundDynVB == nullptr || m_boundDynIB == nullptr) {
            ++s_drawTrianglesSkippedNullBind;
            return;
        }
        auto vbIt = m_dynVbCache.find(m_boundDynVB);
        auto ibIt = m_dynIbCache.find(m_boundDynIB);
        if (vbIt == m_dynVbCache.end() || ibIt == m_dynIbCache.end()) {
            ++s_drawTrianglesSkippedCacheMiss;
            return;
        }
        vbEntryPtr = &vbIt->second;
        ibEntryPtr = &ibIt->second;
    } else {
        ++s_drawTrianglesSkippedNullBind;
        return;
    }

    const CapturedVertexBuffer & vbEntry = *vbEntryPtr;
    const CapturedIndexBuffer & ibEntry = *ibEntryPtr;
    if (vbEntry.stride == 0) {
        return;
    }

    FVFInfoClass fvfInfo(vbEntry.fvf);
    const unsigned locationOffset = fvfInfo.Get_Location_Offset();
    const unsigned diffuseOffset = fvfInfo.Get_Diffuse_Offset();
    const bool hasTexCoord = (vbEntry.fvf & D3DFVF_TEXCOUNT_MASK) != 0;
    const unsigned texOffset = hasTexCoord ? fvfInfo.Get_Tex_Offset(0) : 0;

    const unsigned base = static_cast<unsigned>(start_index) + m_indexBaseOffset;
    for (unsigned short tri = 0; tri < polygon_count; ++tri) {
        const unsigned i0 = base + tri * 3u + 0u;
        const unsigned i1 = base + tri * 3u + 1u;
        const unsigned i2 = base + tri * 3u + 2u;
        if (i2 >= ibEntry.indices.size()) {
            break; // Would read past the captured index buffer -- stop rather than fault.
        }
        const unsigned idx0 = ibEntry.indices[i0];
        const unsigned idx1 = ibEntry.indices[i1];
        const unsigned idx2 = ibEntry.indices[i2];
        const unsigned maxByteOffset = (static_cast<unsigned>(vbEntry.stride) *
            (std::max)((std::max)(idx0, idx1), idx2)) + vbEntry.stride;
        if (maxByteOffset > vbEntry.bytes.size()) {
            continue; // Would read past the captured vertex buffer -- skip this triangle.
        }

        const unsigned char * v0 = vbEntry.bytes.data() + idx0 * vbEntry.stride;
        const unsigned char * v1 = vbEntry.bytes.data() + idx1 * vbEntry.stride;
        const unsigned char * v2 = vbEntry.bytes.data() + idx2 * vbEntry.stride;

        DrawCapturedTriangle(v0, v1, v2, vbEntry.fvf, locationOffset, diffuseOffset, texOffset, hasTexCoord);
        ++s_drawTrianglesActualTriDrawn;
        if (s_drawTrianglesActualTriDrawn <= 30) {
            LogPS2RenderDiag();
        }
    }
}
