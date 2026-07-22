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

// TheSuperHackers @refactor bobtista 22/04/2026 Stage 1
// No-op implementations of the Direct3D 8 COM interfaces. These stubs allow
// DX8Wrapper to be driven against a synthetic Direct3D 8 device without
// loading d3d8.dll at runtime. Every method returns S_OK / D3D_OK and does
// nothing; resource creation returns ref-counted stub objects; Lock methods
// hand out heap-allocated scratch buffers. Compiled only under the
// GGC_BGFX_STANDALONE build configuration used by the bgfx-only renderer.

#include "StubD3D8Device.h"

#if defined(GGC_BGFX_STANDALONE)

// TheSuperHackers @build bobtista 13/06/2026 The bundled d3d8.h only declares the
// IID_IDirect3D* GUIDs on Windows (dxguid.lib). The stub device's QueryInterface
// compares against them, so pull in the non-Windows GUID definitions.
#ifndef _WIN32
#include "d3d8_iids.h"
#endif

#include "DXTUtils.h"
#include "wwdebug.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#if defined(__3DS__)
// TheSuperHackers @diagnostic githubawn 16/07/2026 stderr is not reliably
// captured anywhere readable on 3DS; SDL_Log is (sdmc:/3ds/SDL_Log.txt),
// already relied on throughout the 3DS port for exactly this reason.
#include <SDL3/SDL.h>
#endif
#include <unordered_map>

namespace
{

// TheSuperHackers @refactor bobtista 22/04/2026 Scratch buffers
// must bypass the game's overridden global operator new[] (see
// GameMemory.cpp: new[] routes through TheDynamicMemoryAllocator). Stub
// surfaces can be tens of MB (e.g. a 4096x4096 shadow map = 64 MB) and
// allocating them through the pool corrupts pool metadata. std::calloc is
// not overridden when RTS_MEMORYPOOL_OVERRIDE_MALLOC=OFF (the default).

#if defined(__3DS__)
// TheSuperHackers @bugfix githubawn 16/07/2026 The scratch-sharing pool
// experiment below (every AllocScratch request under 2MB reusing one of a
// handful of round-robin slots instead of getting its own allocation) went
// through three real correctness bugs in a row: a single shared buffer
// silently aliased textures needing distinct simultaneous data (blank white
// UI panels); a non-atomic slot counter raced between the main thread and
// the background texture-loader thread (LoaderThreadClass), corrupting
// whichever two textures happened to land on the same slot at once
// (visible as stuttering); and even after making the pool thread-safe, the
// font glyph atlas -- which is incrementally rebuilt in place across many
// re-locks, not write-once -- kept losing previously-rendered glyphs
// whenever its scratch was reused for something else in between. Each fix
// was real but the pattern of "another texture usage pattern this doesn't
// handle" kept recurring, and reliably enumerating every current and
// future case that needs simultaneous or persistent scratch (this file is
// shared by every GGC_BGFX_STANDALONE platform, not just 3DS) is not
// something to keep discovering by shipping and finding the next visual
// bug. Disabled; reverted to real per-allocation scratch (matching every
// other platform). The genuinely safe, already-verified memory win is
// Citro3dBackend::Ensure_Texture releasing a write-once texture's CPU-side
// copy after its one-time GPU upload (see ReleaseTextureCpuScratch below
// and its call site) -- that stays in effect and needs no pool.
#define GGC_3DS_SHARE_SCRATCH_POOL 0
#endif

#if defined(__3DS__) && GGC_3DS_SHARE_SCRATCH_POOL
// TheSuperHackers @bugfix githubawn 16/07/2026 A single global round-robin
// pool (one shared slot array + one shared "next slot" counter) caused
// visible texture stuttering/corruption: this engine loads textures from a
// background thread (LoaderThreadClass, see textureloader.cpp), so the main
// thread and loader thread could call AllocScratch around the same time.
// The "next slot" counter was a plain non-atomic int -- two threads could
// both read the same index before either incremented it, land on the same
// slot, and stomp each other's pixel data mid-write. Give each thread its
// own independent slot array via thread_local: no shared mutable state
// between threads means no race, full stop, at the cost of each thread that
// touches this path getting its own pool (worst case ~2 threads here: main
// + loader).
//
// One remaining subtlety: COM-style ref-counted D3D8 objects don't
// guarantee a texture is destroyed on the same thread that created it, so
// the *deleter* (StubAllocDeleter::operator(), below) cannot rely on
// thread_local storage to recognize "is this pointer one of my pool slots"
// -- it might be checking from a different thread than the one that
// allocated it. Track all pool pointers, across every thread's array, in
// one small mutex-guarded global set for that check specifically. Writes to
// it are rare (at most kGgc3dsScratchPoolSlots per thread, lazily, once
// each, ever); reads happen on every scratch buffer destruction, but the
// set stays tiny so a lock is cheap.
static const size_t kGgc3dsScratchSlotSize = 2 * 1024 * 1024;  // 2MB per slot
static const int kGgc3dsScratchPoolSlots = 8;                  // per thread

static std::mutex& Ggc3dsScratchRegistryMutex()
{
	static std::mutex m;
	return m;
}
static std::vector<uint8_t*>& Ggc3dsScratchRegistry()
{
	static std::vector<uint8_t*> v;
	return v;
}

static uint8_t* Ggc3dsScratchPoolSlot(int index)
{
	static thread_local uint8_t* slots[kGgc3dsScratchPoolSlots] = {};
	if (slots[index] == nullptr)
	{
		slots[index] = static_cast<uint8_t*>(std::calloc(kGgc3dsScratchSlotSize, 1));
		std::lock_guard<std::mutex> lock(Ggc3dsScratchRegistryMutex());
		Ggc3dsScratchRegistry().push_back(slots[index]);
	}
	return slots[index];
}

static uint8_t* Ggc3dsNextScratchPoolSlot()
{
	static thread_local int s_nextSlot = 0;
	uint8_t* slot = Ggc3dsScratchPoolSlot(s_nextSlot);
	s_nextSlot = (s_nextSlot + 1) % kGgc3dsScratchPoolSlots;
	return slot;
}

static bool Ggc3dsIsScratchPoolPointer(uint8_t* p)
{
	std::lock_guard<std::mutex> lock(Ggc3dsScratchRegistryMutex());
	const std::vector<uint8_t*>& reg = Ggc3dsScratchRegistry();
	for (size_t i = 0; i < reg.size(); ++i)
	{
		if (reg[i] == p)
		{
			return true;
		}
	}
	return false;
}
#endif

struct StubAllocDeleter
{
	void operator()(uint8_t* p) const noexcept
	{
#if defined(__3DS__) && GGC_3DS_SHARE_SCRATCH_POOL
		if (Ggc3dsIsScratchPoolPointer(p))
		{
			return; // pool slots are never individually freed
		}
#endif
		std::free(p);
	}
};
using StubScratch = std::unique_ptr<uint8_t[], StubAllocDeleter>;

// TheSuperHackers @bugfix githubawn 16/07/2026 A failed calloc() here used to
// be silently swallowed: every LockRect() implementation in this file (see
// e.g. StubD3D8Surface::LockRect below) unconditionally returns D3D_OK with
// pBits set to whatever this returned, including null. DX8_ErrorCode (the
// wrapper nearly every call site uses) only logs a failed HRESULT and
// returns -- it does not throw or halt -- so a null-because-OOM scratch
// buffer was invisible at the call site and callers write through it
// unchecked. On 3DS this surfaced as writes/reads through near-null
// addresses tens of seconds later, in unrelated code (DX8TextureCategoryClass
// ::Add_Mesh writing index data, W3DTreeTextureClass::update writing pixel
// data) with no indication OOM was the actual cause. A handful of call sites
// have since been given explicit null checks (W3DTreeBuffer.cpp), but that is
// whack-a-mole against however many more exist across the engine assuming
// (correctly, on a real D3D8 driver) that Lock never really fails. Make the
// failure loud and immediate at the one place it actually happens instead:
// every platform sharing this stub device benefits, not just 3DS (elsewhere
// this path simply never triggers if there is enough memory, so there is no
// behavior change for a platform that isn't already hitting silent
// corruption from this).
// TheSuperHackers @diagnostic githubawn 17/07/2026 tag identifies the call
// site (see callers below) so a FATAL log line says WHICH resource type and
// construction-vs-relock path produced an implausible/failing size, instead
// of just the raw byte count. Also flags implausibly large requests (bigger
// than the entire 3DS memory budget) BEFORE calling calloc, since those are
// necessarily a corrupted/garbage size value, not genuine memory pressure --
// calloc failing on a merely-large-but-plausible request is the real OOM
// case this was originally built to catch; a multi-gigabyte request on a
// ~178MB platform is a different bug class entirely.
static StubScratch AllocScratch(size_t bytes, const char* tag = "unknown")
{
	if (bytes == 0)
	{
		bytes = 4;
	}
#if defined(__3DS__)
	if (bytes > 200u * 1024u * 1024u)
	{
		std::fprintf(stderr, "[StubD3D8Device] FATAL: implausible size %zu requested by '%s' (corrupted size, not real OOM)\n", bytes, tag);
		std::fflush(stderr);
		SDL_Log("[StubD3D8Device] FATAL: implausible size %zu requested by '%s' (corrupted size, not real OOM)", bytes, tag);
		std::abort();
	}
#endif
#if defined(__3DS__) && GGC_3DS_SHARE_SCRATCH_POOL
	if (bytes <= kGgc3dsScratchSlotSize)
	{
		return StubScratch(Ggc3dsNextScratchPoolSlot());
	}
#endif
	void* p = std::calloc(bytes, 1);
	if (p == nullptr)
	{
		std::fprintf(stderr, "[StubD3D8Device] FATAL: calloc(%zu) failed for '%s' (out of memory)\n", bytes, tag);
		std::fflush(stderr);
#if defined(__3DS__)
		SDL_Log("[StubD3D8Device] FATAL: calloc(%zu) failed for '%s' (out of memory)", bytes, tag);
#endif
		std::abort();
	}
	return StubScratch(static_cast<uint8_t*>(p));
}

// TheSuperHackers @bugfix bobtista 22/04/2026 Stub Lock/Desc
// previously reported pitch = width*4 regardless of format. Terrain uses
// D3DFMT_A1R5G5B5 (2 bytes/pixel); upload paths (EnsureBgfxTexture,
// D3DXFilterTexture) trust the reported pitch as source stride, so a
// bogus 4bpp pitch caused them to read every other row and run off the
// end of the written data — visible as dark horizontal/diagonal bands
// on terrain in standalone. Resolve per-format bytes per pixel for the
// formats the engine actually creates; anything else falls back to 4.
static UINT BytesPerPixel(D3DFORMAT fmt)
{
	switch (fmt)
	{
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_G16R16:
	case D3DFMT_X8L8V8U8:
	case D3DFMT_Q8W8V8U8:
	case D3DFMT_V16U16:
	case D3DFMT_W11V11U10:
	case D3DFMT_A2W10V10U10:
	case D3DFMT_D32:
	case D3DFMT_D24S8:
	case D3DFMT_D24X8:
	case D3DFMT_D24X4S4:
		return 4;
	case D3DFMT_R8G8B8:
		return 3;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_X4R4G4B4:
	case D3DFMT_A8L8:
	case D3DFMT_A8P8:
	case D3DFMT_A8R3G3B2:
	case D3DFMT_V8U8:
	case D3DFMT_L6V5U5:
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
	case D3DFMT_D16:
	case D3DFMT_D16_LOCKABLE:
	case D3DFMT_D15S1:
	case D3DFMT_INDEX16:
		return 2;
	case D3DFMT_R3G3B2:
	case D3DFMT_A8:
	case D3DFMT_L8:
	case D3DFMT_P8:
	case D3DFMT_A4L4:
		return 1;
	case D3DFMT_INDEX32:
		return 4;
	default:
		return 4;
	}
}

// TheSuperHackers @bugfix bobtista 27/04/2026 Compressed D3D textures lock
// by block rows, not pixel rows. Reporting them as width*4 made bgfx upload
// skip chunks of DXT atlas data and fill the gaps with black.
static bool IsCompressedFormat(D3DFORMAT fmt)
{
	switch (fmt)
	{
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return true;
	default:
		return false;
	}
}

static UINT BlockBytes(D3DFORMAT fmt)
{
	return (fmt == D3DFMT_DXT1) ? 8 : 16;
}

static UINT SurfacePitch(D3DFORMAT fmt, UINT width)
{
	if (IsCompressedFormat(fmt))
	{
		return DXT_SurfacePitch(width, BlockBytes(fmt));
	}
	return width * BytesPerPixel(fmt);
}

static UINT SurfaceRows(D3DFORMAT fmt, UINT height)
{
	if (IsCompressedFormat(fmt))
	{
		return DXT_SurfaceRows(height);
	}
	return height;
}

static UINT SurfaceStorageSize(D3DFORMAT fmt, UINT width, UINT height)
{
	if (IsCompressedFormat(fmt))
	{
		return DXT_SurfaceStorageSize(width, height, BlockBytes(fmt));
	}
	return SurfacePitch(fmt, width) * SurfaceRows(fmt, height);
}

class StubD3D8Device;

static void FillCaps(D3DCAPS8& caps)
{
	std::memset(&caps, 0, sizeof(caps));
	caps.DeviceType = D3DDEVTYPE_HAL;
	caps.AdapterOrdinal = 0;
	caps.Caps = 0;
	caps.Caps2 = D3DCAPS2_CANRENDERWINDOWED | D3DCAPS2_DYNAMICTEXTURES | D3DCAPS2_FULLSCREENGAMMA | D3DCAPS2_CANCALIBRATEGAMMA;
	caps.Caps3 = 0;
	caps.PresentationIntervals = D3DPRESENT_INTERVAL_DEFAULT | D3DPRESENT_INTERVAL_IMMEDIATE | D3DPRESENT_INTERVAL_ONE;
	caps.CursorCaps = D3DCURSORCAPS_COLOR | D3DCURSORCAPS_LOWRES;
	caps.DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE | D3DDEVCAPS_DRAWPRIMTLVERTEX
		| D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY
		| D3DDEVCAPS_TLVERTEXSYSTEMMEMORY | D3DDEVCAPS_TLVERTEXVIDEOMEMORY
		| D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY
		| D3DDEVCAPS_CANRENDERAFTERFLIP | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
		| D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX
		| D3DDEVCAPS_HWRASTERIZATION;
	caps.PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_LINEPATTERNREP
		| D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW
		| D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_CLIPTLVERTS
		| D3DPMISCCAPS_TSSARGTEMP | D3DPMISCCAPS_BLENDOP;
	caps.RasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST
		| D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_FOGTABLE
		| D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZBIAS
		| D3DPRASTERCAPS_ANISOTROPY | D3DPRASTERCAPS_WFOG | D3DPRASTERCAPS_ZFOG
		| D3DPRASTERCAPS_COLORPERSPECTIVE;
	caps.ZCmpCaps = D3DPCMPCAPS_NEVER | D3DPCMPCAPS_LESS | D3DPCMPCAPS_EQUAL
		| D3DPCMPCAPS_LESSEQUAL | D3DPCMPCAPS_GREATER | D3DPCMPCAPS_NOTEQUAL
		| D3DPCMPCAPS_GREATEREQUAL | D3DPCMPCAPS_ALWAYS;
	caps.SrcBlendCaps = D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE
		| D3DPBLENDCAPS_SRCCOLOR | D3DPBLENDCAPS_INVSRCCOLOR
		| D3DPBLENDCAPS_SRCALPHA | D3DPBLENDCAPS_INVSRCALPHA
		| D3DPBLENDCAPS_DESTALPHA | D3DPBLENDCAPS_INVDESTALPHA
		| D3DPBLENDCAPS_DESTCOLOR | D3DPBLENDCAPS_INVDESTCOLOR
		| D3DPBLENDCAPS_SRCALPHASAT | D3DPBLENDCAPS_BOTHSRCALPHA
		| D3DPBLENDCAPS_BOTHINVSRCALPHA;
	caps.DestBlendCaps = caps.SrcBlendCaps;
	caps.AlphaCmpCaps = caps.ZCmpCaps;
	caps.ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_SPECULARGOURAUDRGB
		| D3DPSHADECAPS_ALPHAGOURAUDBLEND | D3DPSHADECAPS_FOGGOURAUD;
	// TheSuperHackers @bugfix bobtista 22/04/2026 DO NOT set
	// D3DPTEXTURECAPS_POW2 or D3DPTEXTURECAPS_SQUAREONLY here; those are
	// RESTRICTIONS, not features. D3DXCreateTexture reads these and rounds
	// rectangular textures down to the largest legal square (e.g. 2048x1
	// collapsed to 1x1) — which wedged the terrain alpha-edge texture.
	caps.TextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA
		| D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE | D3DPTEXTURECAPS_ALPHAPALETTE
		| D3DPTEXTURECAPS_PROJECTED | D3DPTEXTURECAPS_CUBEMAP
		| D3DPTEXTURECAPS_VOLUMEMAP | D3DPTEXTURECAPS_MIPMAP
		| D3DPTEXTURECAPS_MIPVOLUMEMAP | D3DPTEXTURECAPS_MIPCUBEMAP;
	caps.TextureFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR
		| D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MIPFPOINT
		| D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MAGFPOINT
		| D3DPTFILTERCAPS_MAGFLINEAR | D3DPTFILTERCAPS_MAGFANISOTROPIC;
	caps.CubeTextureFilterCaps = caps.TextureFilterCaps;
	caps.VolumeTextureFilterCaps = caps.TextureFilterCaps;
	caps.TextureAddressCaps = D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_MIRROR
		| D3DPTADDRESSCAPS_CLAMP | D3DPTADDRESSCAPS_BORDER
		| D3DPTADDRESSCAPS_INDEPENDENTUV | D3DPTADDRESSCAPS_MIRRORONCE;
	caps.VolumeTextureAddressCaps = caps.TextureAddressCaps;
	caps.LineCaps = D3DLINECAPS_TEXTURE | D3DLINECAPS_ZTEST | D3DLINECAPS_BLEND | D3DLINECAPS_ALPHACMP | D3DLINECAPS_FOG;
#if defined(__3DS__)
	// TheSuperHackers @tweak githubawn 20/07/2026 PICA200's hard texture-size
	// limit is 1024x1024 (citro3d's C3D_TexInit rejects anything larger).
	// The generic 4096 below let Validate_Texture_Size (which clamps against
	// these caps) hand this backend sizes it can never actually upload,
	// pushing the failure downstream to Ensure_Texture's C3D_TexInit path
	// instead. Guarded to 3DS only -- every other GGC_BGFX_STANDALONE
	// platform (win32-bgfx-standalone, mac, android, wasm) keeps 4096.
	caps.MaxTextureWidth = 1024;
	caps.MaxTextureHeight = 1024;
#else
	caps.MaxTextureWidth = 4096;
	caps.MaxTextureHeight = 4096;
#endif
	caps.MaxVolumeExtent = 256;
	caps.MaxTextureRepeat = 8192;
	caps.MaxTextureAspectRatio = 0;
	caps.MaxAnisotropy = 16;
	caps.MaxVertexW = 1e10f;
	caps.GuardBandLeft = -32768.0f;
	caps.GuardBandTop = -32768.0f;
	caps.GuardBandRight = 32768.0f;
	caps.GuardBandBottom = 32768.0f;
	caps.ExtentsAdjust = 0.0f;
	caps.StencilCaps = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_ZERO | D3DSTENCILCAPS_REPLACE
		| D3DSTENCILCAPS_INCRSAT | D3DSTENCILCAPS_DECRSAT | D3DSTENCILCAPS_INVERT
		| D3DSTENCILCAPS_INCR | D3DSTENCILCAPS_DECR;
	caps.FVFCaps = 8 | D3DFVFCAPS_PSIZE;
	caps.TextureOpCaps = D3DTEXOPCAPS_DISABLE | D3DTEXOPCAPS_SELECTARG1 | D3DTEXOPCAPS_SELECTARG2
		| D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_MODULATE2X | D3DTEXOPCAPS_MODULATE4X
		| D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_ADDSIGNED | D3DTEXOPCAPS_ADDSIGNED2X
		| D3DTEXOPCAPS_SUBTRACT | D3DTEXOPCAPS_ADDSMOOTH
		| D3DTEXOPCAPS_BLENDDIFFUSEALPHA | D3DTEXOPCAPS_BLENDTEXTUREALPHA
		| D3DTEXOPCAPS_BLENDFACTORALPHA | D3DTEXOPCAPS_BLENDTEXTUREALPHAPM
		| D3DTEXOPCAPS_BLENDCURRENTALPHA | D3DTEXOPCAPS_PREMODULATE
		| D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR | D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA
		| D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR | D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA
		| D3DTEXOPCAPS_BUMPENVMAP | D3DTEXOPCAPS_BUMPENVMAPLUMINANCE
		| D3DTEXOPCAPS_DOTPRODUCT3 | D3DTEXOPCAPS_MULTIPLYADD | D3DTEXOPCAPS_LERP;
	caps.MaxTextureBlendStages = 8;
	caps.MaxSimultaneousTextures = 4;
	caps.VertexProcessingCaps = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7
		| D3DVTXPCAPS_DIRECTIONALLIGHTS | D3DVTXPCAPS_POSITIONALLIGHTS
		| D3DVTXPCAPS_LOCALVIEWER | D3DVTXPCAPS_TWEENING;
	caps.MaxActiveLights = 8;
	caps.MaxUserClipPlanes = 6;
	caps.MaxVertexBlendMatrices = 4;
	caps.MaxVertexBlendMatrixIndex = 0;
	caps.MaxPointSize = 256.0f;
	caps.MaxPrimitiveCount = 65535;
	caps.MaxVertexIndex = 65535;
	caps.MaxStreams = 8;
	caps.MaxStreamStride = 256;
	caps.VertexShaderVersion = D3DVS_VERSION(1, 1);
	caps.MaxVertexShaderConst = 256;
	caps.PixelShaderVersion = D3DPS_VERSION(1, 1);
	caps.MaxPixelShaderValue = 1.0f;
}

// ---------------------------------------------------------------------------
// Surface
// ---------------------------------------------------------------------------
class StubD3D8Surface final : public IDirect3DSurface8
{
public:
	// Owning ctor — allocates its own scratch buffer (for render targets,
	// depth stencil surfaces, image surfaces).
	StubD3D8Surface(IDirect3DDevice8* device, IUnknown* container, UINT width, UINT height, D3DFORMAT format, const char* tag = "Surface")
		: m_refCount(1), m_device(device), m_container(container), m_width(width), m_height(height), m_format(format),
		  m_ownedScratch(AllocScratch(SurfaceStorageSize(format, width, height), tag)),
		  m_scratchPtr(m_ownedScratch.get()),
		  m_lastLockFlags(0), m_versionCounterPtr(nullptr)
	{
	}
	// Borrowing ctor — reuses the container's scratch buffer. Used when a
	// texture's GetSurfaceLevel(0) hands back a surface view; the game's
	// writes through the surface must land in the same memory that the
	// bgfx backend later reads via the texture's LockRect. Real D3D8
	// aliases level 0 this way; the owning/borrowing split mirrors that.
	//
	// TheSuperHackers @feature githubawn 20/07/2026 versionCounterPtr, when
	// non-null, points at the owning StubD3D8Texture's m_contentVersion (see
	// GetSurfaceLevel below). A pointer to the counter is stored rather than
	// a StubD3D8Texture* so this surface does not need that class's full
	// definition (StubD3D8Texture is declared later in this file) just to
	// bump it from UnlockRect. Left null for every other borrowing-ctor call
	// site (e.g. cube texture faces) -- those do not feed Citro3dBackend's
	// texture cache, so there is nothing to invalidate.
	StubD3D8Surface(IDirect3DDevice8* device, IUnknown* container, UINT width, UINT height, D3DFORMAT format, uint8_t* borrowedScratch, unsigned* versionCounterPtr = nullptr)
		: m_refCount(1), m_device(device), m_container(container), m_width(width), m_height(height), m_format(format),
		  m_ownedScratch(),
		  m_scratchPtr(borrowedScratch),
		  m_lastLockFlags(0), m_versionCounterPtr(versionCounterPtr)
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DSurface8 || riid == IID_IUnknown) { *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr)
		{
			return E_POINTER;
		}
		*ppDevice = m_device;
		if (m_device)
		{
			m_device->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD(GetContainer)(REFIID, void** ppContainer) override
	{
		if (ppContainer == nullptr)
		{
			return E_POINTER;
		}
		*ppContainer = m_container;
		if (m_container)
		{
			m_container->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(GetDesc)(D3DSURFACE_DESC* pDesc) override
	{
		if (pDesc == nullptr)
		{
			return E_POINTER;
		}
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = SurfaceStorageSize(m_format, m_width, m_height);
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = m_width;
		pDesc->Height = m_height;
		return D3D_OK;
	}
	STDMETHOD(LockRect)(D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD Flags) override
	{
		if (pLockedRect == nullptr)
		{
			return E_POINTER;
		}
		// TheSuperHackers @feature githubawn 20/07/2026 Remembered so
		// UnlockRect below knows whether this lock is allowed to bump the
		// owning texture's content version -- see m_versionCounterPtr.
		m_lastLockFlags = Flags;
		pLockedRect->Pitch = static_cast<INT>(SurfacePitch(m_format, m_width));
		pLockedRect->pBits = m_scratchPtr;
		return D3D_OK;
	}
	STDMETHOD(UnlockRect)() override
	{
		// TheSuperHackers @feature githubawn 20/07/2026 A non-read-only lock
		// through a GetSurfaceLevel() surface writes into the same scratch
		// the owning texture's own LockRect(level) would -- bump its content
		// version here too, so Citro3dBackend::Ensure_Texture's cache
		// notices writes made via this path (see StubD3D8Texture::
		// GetSurfaceLevel) and not just ones made via Texture::LockRect
		// directly.
		if (m_versionCounterPtr != nullptr && !(m_lastLockFlags & D3DLOCK_READONLY))
		{
			++(*m_versionCounterPtr);
		}
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	IUnknown* m_container;
	UINT m_width;
	UINT m_height;
	D3DFORMAT m_format;
	StubScratch m_ownedScratch;
	uint8_t* m_scratchPtr;
	DWORD m_lastLockFlags;
	unsigned* m_versionCounterPtr;
};

// ---------------------------------------------------------------------------
// Vertex Buffer
// ---------------------------------------------------------------------------
class StubD3D8VertexBuffer final : public IDirect3DVertexBuffer8
{
public:
	StubD3D8VertexBuffer(IDirect3DDevice8* device, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_length(length), m_usage(usage), m_fvf(fvf), m_pool(pool),
		  m_scratch(AllocScratch(length, "VertexBuffer-ctor"))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DVertexBuffer8 || riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr)
		{
			return E_POINTER;
		}
		*ppDevice = m_device;
		if (m_device)
		{
			m_device->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_VERTEXBUFFER; }
	STDMETHOD(Lock)(UINT OffsetToLock, UINT, BYTE** ppbData, DWORD Flags) override
	{
		if (ppbData == nullptr)
		{
			return E_POINTER;
		}
		if (!m_scratch)
		{
			// Lazily reallocate if ReleaseCpuScratch freed this -- see its
			// comment below, same pattern as StubD3D8Texture.
			m_scratch = AllocScratch(m_length, "VertexBuffer-relock");
		}
		*ppbData = m_scratch.get() + OffsetToLock;
#if defined(__APPLE__)
		// TheSuperHackers @diagnostic ggc-vblock: remember this lock so Unlock can
		// report the post-write buffer state. Strip when done.
		if (m_length >= 100000u)
		{
			m_lastLockFlags = Flags;
			m_lastLockOff = OffsetToLock;
			if ((Flags & 0x00000010u) == 0u) { ++m_writeLockCount; } // not READONLY
		}
#endif
		return D3D_OK;
	}
	STDMETHOD(Unlock)() override
	{
#if defined(__APPLE__)
		// TheSuperHackers @diagnostic ggc-vblock: scan the WHOLE buffer AFTER the
		// engine has written, to tell "engine writes zeros" vs "data lost in capture".
		if (m_length >= 100000u)
		{
			static unsigned s_lines = 0;
			if (s_lines < 400u)
			{
				++s_lines;
				const unsigned char * p = m_scratch.get();
				unsigned nz = 0, firstNz = 0xFFFFFFFFu, lastNz = 0;
				for (unsigned i = 0; i < m_length; ++i)
				{
					if (p[i] != 0) { ++nz; if (firstNz == 0xFFFFFFFFu) firstNz = i; lastNz = i; }
				}
				FILE * lf = fopen("/tmp/ggc_vblock.log", "a");
				if (lf != nullptr)
				{
					fprintf(lf, "UNLOCK VB=%p len=%u lastOff=%u lastFlags=0x%x writeLocks=%u nzTotal=%u firstNz=%d lastNz=%u\n",
							(void*)this, m_length, m_lastLockOff, (unsigned)m_lastLockFlags,
							m_writeLockCount, nz, (firstNz==0xFFFFFFFFu)?-1:(int)firstNz, lastNz);
					fclose(lf);
				}
			}
		}
#endif
		return D3D_OK;
	}
	STDMETHOD(GetDesc)(D3DVERTEXBUFFER_DESC* pDesc) override
	{
		if (pDesc == nullptr)
		{
			return E_POINTER;
		}
		pDesc->Format = D3DFMT_VERTEXDATA;
		pDesc->Type = D3DRTYPE_VERTEXBUFFER;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = m_length;
		pDesc->FVF = m_fvf;
		return D3D_OK;
	}

	// See ReleaseVertexBufferCpuScratch's comment (StubD3D8Device.h) for why
	// this exists -- same double-storage problem as StubD3D8Texture::
	// ReleaseCpuScratch, no per-level/surface-aliasing concern here since a
	// vertex buffer is one flat allocation.
	void ReleaseCpuScratch()
	{
		m_scratch.reset();
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_length;
	DWORD m_usage;
	DWORD m_fvf;
	D3DPOOL m_pool;
	StubScratch m_scratch;
	unsigned m_writeLockCount = 0; // TheSuperHackers @diagnostic ggc-vblock (strip when done)
	DWORD m_lastLockFlags = 0;     // TheSuperHackers @diagnostic ggc-vblock (strip when done)
	UINT m_lastLockOff = 0;        // TheSuperHackers @diagnostic ggc-vblock (strip when done)
};

// ---------------------------------------------------------------------------
// Index Buffer
// ---------------------------------------------------------------------------
class StubD3D8IndexBuffer final : public IDirect3DIndexBuffer8
{
public:
	StubD3D8IndexBuffer(IDirect3DDevice8* device, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_length(length), m_usage(usage), m_format(format), m_pool(pool),
		  m_scratch(AllocScratch(length, "IndexBuffer-ctor"))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DIndexBuffer8 || riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr)
		{
			return E_POINTER;
		}
		*ppDevice = m_device;
		if (m_device)
		{
			m_device->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_INDEXBUFFER; }
	STDMETHOD(Lock)(UINT OffsetToLock, UINT, BYTE** ppbData, DWORD) override
	{
		if (ppbData == nullptr)
		{
			return E_POINTER;
		}
		if (!m_scratch)
		{
			// Lazily reallocate if ReleaseCpuScratch freed this -- see its
			// comment below, same pattern as StubD3D8Texture.
			m_scratch = AllocScratch(m_length, "IndexBuffer-relock");
		}
		*ppbData = m_scratch.get() + OffsetToLock;
		return D3D_OK;
	}
	STDMETHOD(Unlock)() override { return D3D_OK; }
	// See ReleaseIndexBufferCpuScratch's comment (StubD3D8Device.h).
	void ReleaseCpuScratch()
	{
		m_scratch.reset();
	}
	STDMETHOD(GetDesc)(D3DINDEXBUFFER_DESC* pDesc) override
	{
		if (pDesc == nullptr)
		{
			return E_POINTER;
		}
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_INDEXBUFFER;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = m_length;
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_length;
	DWORD m_usage;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	StubScratch m_scratch;
};

// ---------------------------------------------------------------------------
// Texture (2D)
// ---------------------------------------------------------------------------
static DWORD ComputeFullMipLevels(UINT w, UINT h)
{
	DWORD levels = 1;
	UINT m = w > h ? w : h;
	while (m > 1) { m >>= 1; ++levels; }
	return levels;
}

class StubD3D8Texture final : public IDirect3DTexture8
{
public:
	StubD3D8Texture(IDirect3DDevice8* device, UINT width, UINT height, UINT requestedLevels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_width(width), m_height(height), m_usage(usage), m_format(format), m_pool(pool),
		  m_contentVersion(0)
	{
		// D3D8 convention: Levels == 0 means "all mips down to 1x1".
		m_levels = requestedLevels == 0 ? ComputeFullMipLevels(width, height) : requestedLevels;
		if (m_levels > 16)
		{
			m_levels = 16;
		}
		m_levelScratch.reset(new StubScratch[m_levels]);
		m_surfaces.reset(new IDirect3DSurface8*[m_levels]);
		// TheSuperHackers @feature githubawn 20/07/2026 Per-level lock flags,
		// value-initialized to 0 (i.e. not D3DLOCK_READONLY) -- see LockRect/
		// UnlockRect below.
		m_levelLockFlags.reset(new DWORD[m_levels]());
		for (DWORD i = 0; i < m_levels; ++i)
		{
			UINT lw = width  >> i; if (lw == 0) lw = 1;
			UINT lh = height >> i; if (lh == 0) lh = 1;
			m_levelScratch[i] = AllocScratch(SurfaceStorageSize(format, lw, lh), "Texture-ctor");
			m_surfaces[i] = nullptr;
		}
	}
	~StubD3D8Texture()
	{
		for (DWORD i = 0; i < m_levels; ++i)
		{
			if (m_surfaces[i])
			{
				m_surfaces[i]->Release();
			}
		}
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DTexture8 || riid == IID_IDirect3DBaseTexture8 ||
			riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr)
		{
			return E_POINTER;
		}
		*ppDevice = m_device;
		if (m_device)
		{
			m_device->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_TEXTURE; }
	STDMETHOD_(DWORD, SetLOD)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetLOD)() override { return 0; }
	STDMETHOD_(DWORD, GetLevelCount)() override { return m_levels; }
	STDMETHOD(GetLevelDesc)(UINT level, D3DSURFACE_DESC* pDesc) override
	{
		if (pDesc == nullptr)
		{
			return E_POINTER;
		}
		if (level >= m_levels)
		{
			level = m_levels - 1;
		}
		UINT lw = m_width  >> level; if (lw == 0) lw = 1;
		UINT lh = m_height >> level; if (lh == 0) lh = 1;
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = SurfaceStorageSize(m_format, lw, lh);
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = lw;
		pDesc->Height = lh;
		return D3D_OK;
	}
	STDMETHOD(GetSurfaceLevel)(UINT level, IDirect3DSurface8** ppSurfaceLevel) override
	{
		if (ppSurfaceLevel == nullptr)
		{
			return E_POINTER;
		}
		if (level >= m_levels)
		{
			level = m_levels - 1;
		}
		if (m_surfaces[level] == nullptr)
		{
			// Real D3D8 aliases surface level N with the texture's level-N
			// storage. Writes via the surface must be visible through a
			// subsequent texture-level LockRect. EnsureBgfxTexture relies
			// on this aliasing when it uploads texture pixels to bgfx.
			UINT lw = m_width  >> level; if (lw == 0) lw = 1;
			UINT lh = m_height >> level; if (lh == 0) lh = 1;
			m_surfaces[level] = new StubD3D8Surface(m_device, static_cast<IDirect3DTexture8*>(this), lw, lh, m_format, m_levelScratch[level].get(), &m_contentVersion);
		}
		m_surfaces[level]->AddRef();
		*ppSurfaceLevel = m_surfaces[level];
		return D3D_OK;
	}
	STDMETHOD(LockRect)(UINT level, D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD Flags) override
	{
		if (pLockedRect == nullptr)
		{
			return E_POINTER;
		}
		if (level >= m_levels)
		{
			level = m_levels - 1;
		}
		UINT lw = m_width >> level; if (lw == 0) lw = 1;
		UINT lh = m_height >> level; if (lh == 0) lh = 1;
		// TheSuperHackers @feature githubawn 20/07/2026 Remembered so
		// UnlockRect below knows whether this lock is allowed to bump
		// m_contentVersion -- see its declaration.
		m_levelLockFlags[level] = Flags;
		if (!m_levelScratch[level])
		{
			// TheSuperHackers @feature githubawn 16/07/2026 Lazily reallocate if
			// ReleaseCpuScratch freed this level -- see its comment below. A
			// texture that gets locked again after being released (e.g. a
			// rebuilt font atlas via Invalidate_Cached_Texture) just gets a
			// fresh zeroed buffer here, same as first use.
			m_levelScratch[level] = AllocScratch(SurfaceStorageSize(m_format, lw, lh), "Texture-relock");
			// TheSuperHackers @bugfix githubawn 20/07/2026 Deliberately do NOT
			// bump m_contentVersion here, and do not be tempted to "because the
			// bytes changed". This buffer is ZEROED -- it is the absence of the
			// pixels, not new pixels. Bumping here told
			// Citro3dBackend::Ensure_Texture that the source had fresh content
			// and made it re-upload all-zeros over a perfectly good GPU texture,
			// which is what made large UI images (menu backdrop, load screen,
			// score screen) appear for one frame and then vanish, the radar
			// upload blank, and 3D geometry go black. Worse, it self-sustained:
			// the re-upload's own read-only lock hit this same path again, so
			// every single bind re-uploaded zeros (visible in the [ggc-tex] log
			// as the same texture pointer re-uploading on consecutive lines).
			// The GPU copy already uploaded is strictly better than anything
			// this buffer can now provide, so leave the version alone and let
			// the cache keep serving it. A caller that genuinely rewrites the
			// texture still unlocks non-read-only below, which bumps correctly.
		}
		pLockedRect->Pitch = static_cast<INT>(SurfacePitch(m_format, lw));
		pLockedRect->pBits = m_levelScratch[level].get();
		return D3D_OK;
	}
	STDMETHOD(UnlockRect)(UINT level) override
	{
		if (level >= m_levels)
		{
			level = m_levels - 1;
		}
		// TheSuperHackers @feature githubawn 20/07/2026 See
		// GGC_GetTextureContentVersion in the header: a read-only lock (e.g.
		// Citro3dBackend::Ensure_Texture's own upload-time LockRect) must not
		// bump this, or every upload would invalidate its own cache entry.
		if (!(m_levelLockFlags[level] & D3DLOCK_READONLY))
		{
			++m_contentVersion;
		}
		return D3D_OK;
	}
	STDMETHOD(AddDirtyRect)(CONST RECT*) override { return D3D_OK; }

	// TheSuperHackers @feature githubawn 20/07/2026 See
	// GGC_GetTextureContentVersion in the header.
	unsigned GetContentVersion() const { return m_contentVersion; }

	// TheSuperHackers @bugfix githubawn 20/07/2026 See GGC_TextureHasCpuScratch below. A level
	// whose scratch was released still reports a valid pointer from LockRect (it reallocates
	// zeroed on demand), so callers cannot detect the difference from the lock alone.
	bool HasCpuScratch(UINT level) const
	{
		if (level >= m_levels)
		{
			return false;
		}
		return static_cast<bool>(m_levelScratch[level]);
	}

	// TheSuperHackers @feature githubawn 16/07/2026 Root fix for the 3DS
	// match-load OOM (StubD3D8Device's calloc'd CPU-side scratch buffer
	// competing with the GPU-side upload -- e.g. citro3d's C3D_Tex via
	// linearAlloc -- for memory, permanently double-storing every texture's
	// pixel data for its whole lifetime). Called by a render backend
	// (Citro3dBackend::Ensure_Texture) once it has finished reading a level's
	// pixels via LockRect and uploaded them to its own GPU-resident copy.
	// Only frees a level if GetSurfaceLevel was never called for it: a
	// StubD3D8Surface returned from GetSurfaceLevel aliases this scratch
	// pointer directly (see its "borrowing ctor"), and freeing out from under
	// an already-handed-out surface would leave it dangling. Levels with an
	// existing surface are left alone -- a smaller, safe win rather than a
	// correctness risk.
	void ReleaseCpuScratch()
	{
		for (DWORD i = 0; i < m_levels; ++i)
		{
			if (m_surfaces[i] == nullptr)
			{
				m_levelScratch[i].reset();
			}
		}
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_width;
	UINT m_height;
	DWORD m_usage;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	// TheSuperHackers @feature githubawn 20/07/2026 See GGC_GetTextureContentVersion
	// in the header for what bumps this and why.
	unsigned m_contentVersion;
	DWORD m_levels;
	std::unique_ptr<StubScratch[]> m_levelScratch;
	std::unique_ptr<DWORD[]> m_levelLockFlags;
	std::unique_ptr<IDirect3DSurface8*[]> m_surfaces;
};

} // namespace

// TheSuperHackers @feature githubawn 16/07/2026 Public entry point for
// ReleaseCpuScratch above -- StubD3D8Texture itself is anonymous-namespace
// private to this translation unit, so a render backend holding only the
// public IDirect3DTexture8* handle needs this free function to reach it.
// StubD3D8Texture is the sole concrete IDirect3DTexture8 implementation
// under GGC_BGFX_STANDALONE (see StubD3D8Device::CreateTexture), so the
// static_cast is safe for any texture obtained through this stub device.
void ReleaseTextureCpuScratch(IDirect3DTexture8* texture)
{
	if (texture != nullptr)
	{
		static_cast<StubD3D8Texture*>(texture)->ReleaseCpuScratch();
	}
}

// TheSuperHackers @feature githubawn 20/07/2026 Public entry point for
// StubD3D8Texture::GetContentVersion -- same "free function next to the COM
// interface" pattern as ReleaseTextureCpuScratch above, for the same reason
// (StubD3D8Texture is anonymous-namespace private, callers only hold the
// public IDirect3DTexture8* handle). Returns 0 for a null texture, matching
// a freshly-constructed texture's initial version so a caller comparing
// against a previously-stored version of 0 does not misread a null pointer
// as "unchanged" versus "never uploaded".
unsigned GGC_GetTextureContentVersion(IDirect3DTexture8* texture)
{
	if (texture == nullptr)
	{
		return 0;
	}
	return static_cast<StubD3D8Texture*>(texture)->GetContentVersion();
}

// TheSuperHackers @bugfix githubawn 20/07/2026 Reports whether level 0 still holds real CPU-side
// pixels, i.e. whether re-reading this texture would yield its actual image or just the zeroed
// buffer LockRect lazily reallocates after ReleaseTextureCpuScratch freed it. Citro3dBackend needs
// this before throwing away an already-uploaded GPU texture: doing so is only safe if the pixels
// can actually be read back. Without the check, any invalidation of a scratch-released texture
// replaced a correct GPU image with all-zeros, which is exactly the reported "large image appears
// for a moment and then disappears".
bool GGC_TextureHasCpuScratch(IDirect3DTexture8* texture)
{
	if (texture == nullptr)
	{
		return false;
	}
	return static_cast<StubD3D8Texture*>(texture)->HasCpuScratch(0);
}

// TheSuperHackers @bugfix githubawn 17/07/2026 Same rationale as
// ReleaseTextureCpuScratch above: StubD3D8VertexBuffer/StubD3D8IndexBuffer
// are the sole concrete implementations of these D3D8 interfaces under
// GGC_BGFX_STANDALONE, so the static_cast is safe for any buffer obtained
// through this stub device.
void ReleaseVertexBufferCpuScratch(IDirect3DVertexBuffer8* vb)
{
	if (vb != nullptr)
	{
		static_cast<StubD3D8VertexBuffer*>(vb)->ReleaseCpuScratch();
	}
}

void ReleaseIndexBufferCpuScratch(IDirect3DIndexBuffer8* ib)
{
	if (ib != nullptr)
	{
		static_cast<StubD3D8IndexBuffer*>(ib)->ReleaseCpuScratch();
	}
}

namespace
{

// ---------------------------------------------------------------------------
// Cube Texture
// ---------------------------------------------------------------------------
class StubD3D8CubeTexture final : public IDirect3DCubeTexture8
{
public:
	StubD3D8CubeTexture(IDirect3DDevice8* device, UINT edge, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_edge(edge), m_format(format),
		  m_scratch(AllocScratch(SurfaceStorageSize(format, edge, edge), "CubeTexture-ctor"))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DCubeTexture8 || riid == IID_IDirect3DBaseTexture8 ||
			riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr)
		{
			return E_POINTER;
		}
		*ppDevice = m_device;
		if (m_device)
		{
			m_device->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_CUBETEXTURE; }
	STDMETHOD_(DWORD, SetLOD)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetLOD)() override { return 0; }
	STDMETHOD_(DWORD, GetLevelCount)() override { return 1; }
	STDMETHOD(GetLevelDesc)(UINT, D3DSURFACE_DESC* pDesc) override
	{
		if (pDesc == nullptr)
		{
			return E_POINTER;
		}
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = SurfaceStorageSize(m_format, m_edge, m_edge);
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = m_edge;
		pDesc->Height = m_edge;
		return D3D_OK;
	}
	STDMETHOD(GetCubeMapSurface)(D3DCUBEMAP_FACES, UINT, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr)
		{
			return E_POINTER;
		}
		*ppSurface = new StubD3D8Surface(m_device, static_cast<IDirect3DCubeTexture8*>(this), m_edge, m_edge, m_format, m_scratch.get());
		return D3D_OK;
	}
	STDMETHOD(LockRect)(D3DCUBEMAP_FACES, UINT, D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD) override
	{
		if (pLockedRect == nullptr)
		{
			return E_POINTER;
		}
		pLockedRect->Pitch = static_cast<INT>(SurfacePitch(m_format, m_edge));
		pLockedRect->pBits = m_scratch.get();
		return D3D_OK;
	}
	STDMETHOD(UnlockRect)(D3DCUBEMAP_FACES, UINT) override { return D3D_OK; }
	STDMETHOD(AddDirtyRect)(D3DCUBEMAP_FACES, CONST RECT*) override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_edge;
	D3DFORMAT m_format;
	StubScratch m_scratch;
};

// ---------------------------------------------------------------------------
// Volume Texture
// ---------------------------------------------------------------------------
class StubD3D8VolumeTexture final : public IDirect3DVolumeTexture8
{
public:
	StubD3D8VolumeTexture(IDirect3DDevice8* device, UINT width, UINT height, UINT depth, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_width(width), m_height(height), m_depth(depth), m_format(format),
		  m_scratch(AllocScratch(static_cast<size_t>(width) * height * depth * BytesPerPixel(format), "VolumeTexture-ctor"))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DVolumeTexture8 || riid == IID_IDirect3DBaseTexture8 ||
			riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr)
		{
			return E_POINTER;
		}
		*ppDevice = m_device;
		if (m_device)
		{
			m_device->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_VOLUMETEXTURE; }
	STDMETHOD_(DWORD, SetLOD)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetLOD)() override { return 0; }
	STDMETHOD_(DWORD, GetLevelCount)() override { return 1; }
	STDMETHOD(GetLevelDesc)(UINT, D3DVOLUME_DESC* pDesc) override
	{
		if (pDesc == nullptr)
		{
			return E_POINTER;
		}
		const UINT bpp = BytesPerPixel(m_format);
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_VOLUME;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = m_width * m_height * m_depth * bpp;
		pDesc->Width = m_width;
		pDesc->Height = m_height;
		pDesc->Depth = m_depth;
		return D3D_OK;
	}
	STDMETHOD(GetVolumeLevel)(UINT, IDirect3DVolume8** ppVolumeLevel) override
	{
		if (ppVolumeLevel == nullptr)
		{
			return E_POINTER;
		}
		*ppVolumeLevel = nullptr;
		return D3D_OK;
	}
	STDMETHOD(LockBox)(UINT, D3DLOCKED_BOX* pLockedVolume, CONST D3DBOX*, DWORD) override
	{
		if (pLockedVolume == nullptr)
		{
			return E_POINTER;
		}
		const UINT bpp = BytesPerPixel(m_format);
		pLockedVolume->RowPitch = static_cast<INT>(m_width * bpp);
		pLockedVolume->SlicePitch = static_cast<INT>(m_width * m_height * bpp);
		pLockedVolume->pBits = m_scratch.get();
		return D3D_OK;
	}
	STDMETHOD(UnlockBox)(UINT) override { return D3D_OK; }
	STDMETHOD(AddDirtyBox)(CONST D3DBOX*) override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_width;
	UINT m_height;
	UINT m_depth;
	D3DFORMAT m_format;
	StubScratch m_scratch;
};

// ---------------------------------------------------------------------------
// Swap Chain
// ---------------------------------------------------------------------------
class StubD3D8SwapChain final : public IDirect3DSwapChain8
{
public:
	StubD3D8SwapChain(IDirect3DDevice8* device, IDirect3DSurface8* backBuffer)
		: m_refCount(1), m_device(device), m_backBuffer(backBuffer)
	{
		if (m_backBuffer)
		{
			m_backBuffer->AddRef();
		}
	}
	~StubD3D8SwapChain()
	{
		if (m_backBuffer)
		{
			m_backBuffer->Release();
		}
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DSwapChain8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}
	STDMETHOD(Present)(CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) override { return D3D_OK; }
	STDMETHOD(GetBackBuffer)(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8** ppBackBuffer) override
	{
		if (ppBackBuffer == nullptr)
		{
			return E_POINTER;
		}
		*ppBackBuffer = m_backBuffer;
		if (m_backBuffer)
		{
			m_backBuffer->AddRef();
		}
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	IDirect3DSurface8* m_backBuffer;
};

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------
class StubD3D8Device final : public IDirect3DDevice8
{
	struct StageStateKey
	{
		DWORD stage;
		DWORD type;

		bool operator==(const StageStateKey & rhs) const
		{
			return stage == rhs.stage && type == rhs.type;
		}
	};
	struct StageStateKeyHash
	{
		size_t operator()(const StageStateKey & key) const
		{
			return (static_cast<size_t>(key.stage) << 8) ^ static_cast<size_t>(key.type);
		}
	};

public:
	StubD3D8Device(IDirect3D8* parent, HWND focusWindow, UINT width, UINT height)
		: m_refCount(1), m_parent(parent), m_focusWindow(focusWindow), m_width(width), m_height(height),
		  m_backBuffer(nullptr), m_depthStencil(nullptr)
	{
		if (m_parent)
		{
			m_parent->AddRef();
		}
		m_backBuffer = new StubD3D8Surface(this, nullptr, width, height, D3DFMT_A8R8G8B8);
		m_depthStencil = new StubD3D8Surface(this, nullptr, width, height, D3DFMT_D24S8);
	}
	~StubD3D8Device()
	{
		if (m_depthStencil)
		{
			m_depthStencil->Release();
		}
		if (m_backBuffer)
		{
			m_backBuffer->Release();
		}
		if (m_parent)
		{
			m_parent->Release();
		}
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DDevice8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}

	STDMETHOD(TestCooperativeLevel)() override { return D3D_OK; }
	STDMETHOD_(UINT, GetAvailableTextureMem)() override { return 512u * 1024u * 1024u; }
	STDMETHOD(ResourceManagerDiscardBytes)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetDirect3D)(IDirect3D8** ppD3D8) override
	{
		if (ppD3D8 == nullptr)
		{
			return E_POINTER;
		}
		*ppD3D8 = m_parent;
		if (m_parent)
		{
			m_parent->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(GetDeviceCaps)(D3DCAPS8* pCaps) override
	{
		if (pCaps == nullptr)
		{
			return E_POINTER;
		}
		FillCaps(*pCaps);
		return D3D_OK;
	}
	STDMETHOD(GetDisplayMode)(D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr)
		{
			return E_POINTER;
		}
		pMode->Width = m_width;
		pMode->Height = m_height;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS* pParameters) override
	{
		if (pParameters == nullptr)
		{
			return E_POINTER;
		}
		pParameters->AdapterOrdinal = 0;
		pParameters->DeviceType = D3DDEVTYPE_HAL;
		pParameters->hFocusWindow = m_focusWindow;
		pParameters->BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		return D3D_OK;
	}
	STDMETHOD(SetCursorProperties)(UINT, UINT, IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD_(void, SetCursorPosition)(UINT, UINT, DWORD) override {}
	STDMETHOD_(BOOL, ShowCursor)(BOOL) override { return TRUE; }
	STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain8** pSwapChain) override
	{
		if (pSwapChain == nullptr)
		{
			return E_POINTER;
		}
		*pSwapChain = new StubD3D8SwapChain(this, m_backBuffer);
		return D3D_OK;
	}
	STDMETHOD(Reset)(D3DPRESENT_PARAMETERS*) override { return D3D_OK; }
	STDMETHOD(Present)(CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) override { return D3D_OK; }
	STDMETHOD(GetBackBuffer)(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8** ppBackBuffer) override
	{
		if (ppBackBuffer == nullptr)
		{
			return E_POINTER;
		}
		*ppBackBuffer = m_backBuffer;
		if (m_backBuffer)
		{
			m_backBuffer->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(GetRasterStatus)(D3DRASTER_STATUS* pRasterStatus) override
	{
		if (pRasterStatus) { pRasterStatus->InVBlank = FALSE; pRasterStatus->ScanLine = 0; }
		return D3D_OK;
	}
	STDMETHOD_(void, SetGammaRamp)(DWORD, CONST D3DGAMMARAMP*) override {}
	STDMETHOD_(void, GetGammaRamp)(D3DGAMMARAMP*) override {}

	STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) override
	{
		if (ppTexture == nullptr)
		{
			return E_POINTER;
		}
		*ppTexture = new StubD3D8Texture(this, Width, Height, Levels, Usage, Format, Pool);
		return D3D_OK;
	}
	STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DVolumeTexture8** ppVolumeTexture) override
	{
		if (ppVolumeTexture == nullptr)
		{
			return E_POINTER;
		}
		*ppVolumeTexture = new StubD3D8VolumeTexture(this, Width, Height, Depth, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DCubeTexture8** ppCubeTexture) override
	{
		if (ppCubeTexture == nullptr)
		{
			return E_POINTER;
		}
		*ppCubeTexture = new StubD3D8CubeTexture(this, EdgeLength, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer) override
	{
		if (ppVertexBuffer == nullptr)
		{
			return E_POINTER;
		}
		*ppVertexBuffer = new StubD3D8VertexBuffer(this, Length, Usage, FVF, Pool);
		return D3D_OK;
	}
	STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer) override
	{
		if (ppIndexBuffer == nullptr)
		{
			return E_POINTER;
		}
		*ppIndexBuffer = new StubD3D8IndexBuffer(this, Length, Usage, Format, Pool);
		return D3D_OK;
	}
	STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, BOOL, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr)
		{
			return E_POINTER;
		}
#if defined(__3DS__)
		{
			char tag[96];
			std::snprintf(tag, sizeof(tag), "RenderTarget w=%u h=%u fmt=%d", Width, Height, static_cast<int>(Format));
			*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format, tag);
			return D3D_OK;
		}
#endif
		*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr)
		{
			return E_POINTER;
		}
#if defined(__3DS__)
		{
			char tag[96];
			std::snprintf(tag, sizeof(tag), "DepthStencilSurface w=%u h=%u fmt=%d", Width, Height, static_cast<int>(Format));
			*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format, tag);
			return D3D_OK;
		}
#endif
		*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateImageSurface)(UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr)
		{
			return E_POINTER;
		}
#if defined(__3DS__)
		{
			char tag[96];
			std::snprintf(tag, sizeof(tag), "ImageSurface w=%u h=%u fmt=%d", Width, Height, static_cast<int>(Format));
			*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format, tag);
			return D3D_OK;
		}
#endif
		*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format);
		return D3D_OK;
	}

	// TheSuperHackers @bugfix bobtista 22/04/2026 D3DX8 loads
	// texture files via Create (MANAGED) + Create (SYSTEMMEM scratch) +
	// UpdateTexture(scratch → managed). A no-op UpdateTexture leaves
	// the managed texture empty, which is why infantry / fonts / HUD
	// textures were rendering black. Lock both sides and memcpy.
	STDMETHOD(CopyRects)(IDirect3DSurface8* src, CONST RECT* srcRects, UINT count, IDirect3DSurface8* dst, CONST POINT* dstPts) override
	{
		if (src == nullptr || dst == nullptr)
		{
			return D3D_OK;
		}
		D3DSURFACE_DESC sd, dd;
		if (FAILED(src->GetDesc(&sd)) || FAILED(dst->GetDesc(&dd)))
		{
			return D3D_OK;
		}
		D3DLOCKED_RECT sl = {}, dl = {};
		if (FAILED(src->LockRect(&sl, nullptr, 0)))
		{
			return D3D_OK;
		}
		if (FAILED(dst->LockRect(&dl, nullptr, 0))) { src->UnlockRect(); return D3D_OK; }
		if (count == 0 || srcRects == nullptr)
		{
			const UINT h = SurfaceRows(sd.Format, sd.Height) < SurfaceRows(dd.Format, dd.Height)
				? SurfaceRows(sd.Format, sd.Height) : SurfaceRows(dd.Format, dd.Height);
			const UINT rowBytes = SurfacePitch(sd.Format, sd.Width) < SurfacePitch(dd.Format, dd.Width)
				? SurfacePitch(sd.Format, sd.Width) : SurfacePitch(dd.Format, dd.Width);
			for (UINT y = 0; y < h; ++y)
			{
				std::memcpy(
					static_cast<uint8_t*>(dl.pBits) + y * dl.Pitch,
					static_cast<const uint8_t*>(sl.pBits) + y * sl.Pitch,
					rowBytes);
			}
		}
		else
		{
			if (IsCompressedFormat(sd.Format) || IsCompressedFormat(dd.Format))
			{
				if (sd.Format != dd.Format || !IsCompressedFormat(sd.Format) || !IsCompressedFormat(dd.Format))
				{
					dst->UnlockRect();
					src->UnlockRect();
					return D3DERR_INVALIDCALL;
				}
				// TheSuperHackers @bugfix bobtista 28/04/2026 Support
				// block-aligned DXT rect copies. Terrain atlases are populated
				// through CopyRects from compressed tiles; skipping those rects
				// leaves black atlas cells that bgfx samples as large dark
				// patches in standalone.
				const UINT blockBytes = BlockBytes(sd.Format);
				for (UINT i = 0; i < count; ++i)
				{
					const RECT& r = srcRects[i];
					const LONG dx = dstPts ? dstPts[i].x : r.left;
					const LONG dy = dstPts ? dstPts[i].y : r.top;
					const UINT srcBlockLeft = static_cast<UINT>(r.left) / 4;
					const UINT srcBlockTop = static_cast<UINT>(r.top) / 4;
					const UINT srcBlockRight = (static_cast<UINT>(r.right) + 3) / 4;
					const UINT srcBlockBottom = (static_cast<UINT>(r.bottom) + 3) / 4;
					const UINT dstBlockX = static_cast<UINT>(dx) / 4;
					const UINT dstBlockY = static_cast<UINT>(dy) / 4;
					const UINT blockRows = srcBlockBottom - srcBlockTop;
					const UINT rowBytes = (srcBlockRight - srcBlockLeft) * blockBytes;
					for (UINT y = 0; y < blockRows; ++y)
					{
						std::memcpy(
							static_cast<uint8_t*>(dl.pBits) + (dstBlockY + y) * dl.Pitch + dstBlockX * blockBytes,
							static_cast<const uint8_t*>(sl.pBits) + (srcBlockTop + y) * sl.Pitch + srcBlockLeft * blockBytes,
							rowBytes);
					}
				}
			}
			else
			{
				const UINT bpp = BytesPerPixel(sd.Format);
				for (UINT i = 0; i < count; ++i)
				{
					const RECT& r = srcRects[i];
					const LONG dx = dstPts ? dstPts[i].x : r.left;
					const LONG dy = dstPts ? dstPts[i].y : r.top;
					const LONG rw = r.right - r.left;
					const LONG rh = r.bottom - r.top;
					for (LONG y = 0; y < rh; ++y)
					{
						std::memcpy(
							static_cast<uint8_t*>(dl.pBits) + (dy + y) * dl.Pitch + dx * bpp,
							static_cast<const uint8_t*>(sl.pBits) + (r.top + y) * sl.Pitch + r.left * bpp,
							rw * bpp);
					}
				}
			}
		}
		dst->UnlockRect();
		src->UnlockRect();
		return D3D_OK;
	}
	STDMETHOD(UpdateTexture)(IDirect3DBaseTexture8* src, IDirect3DBaseTexture8* dst) override
	{
		if (src == nullptr || dst == nullptr)
		{
			return D3D_OK;
		}
		if (src->GetType() != D3DRTYPE_TEXTURE || dst->GetType() != D3DRTYPE_TEXTURE)
		{
			return D3D_OK;
		}
		IDirect3DTexture8* st = static_cast<IDirect3DTexture8*>(src);
		IDirect3DTexture8* dt = static_cast<IDirect3DTexture8*>(dst);
		DWORD srcLevels = st->GetLevelCount();
		DWORD dstLevels = dt->GetLevelCount();
		DWORD levels = srcLevels < dstLevels ? srcLevels : dstLevels;
		for (DWORD i = 0; i < levels; ++i)
		{
			D3DSURFACE_DESC sd, dd;
			if (FAILED(st->GetLevelDesc(i, &sd)) || FAILED(dt->GetLevelDesc(i, &dd)))
			{
				break;
			}
			D3DLOCKED_RECT sl = {}, dl = {};
			if (FAILED(st->LockRect(i, &sl, nullptr, 0)))
			{
				break;
			}
			if (FAILED(dt->LockRect(i, &dl, nullptr, 0))) { st->UnlockRect(i); break; }
			const UINT h = SurfaceRows(sd.Format, sd.Height) < SurfaceRows(dd.Format, dd.Height)
				? SurfaceRows(sd.Format, sd.Height) : SurfaceRows(dd.Format, dd.Height);
			const UINT rowBytes = SurfacePitch(sd.Format, sd.Width) < SurfacePitch(dd.Format, dd.Width)
				? SurfacePitch(sd.Format, sd.Width) : SurfacePitch(dd.Format, dd.Width);
			for (UINT y = 0; y < h; ++y)
			{
				std::memcpy(
					static_cast<uint8_t*>(dl.pBits) + y * dl.Pitch,
					static_cast<const uint8_t*>(sl.pBits) + y * sl.Pitch,
					rowBytes);
			}
			dt->UnlockRect(i);
			st->UnlockRect(i);
		}
		return D3D_OK;
	}
	STDMETHOD(GetFrontBuffer)(IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD(SetRenderTarget)(IDirect3DSurface8*, IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD(GetRenderTarget)(IDirect3DSurface8** ppRenderTarget) override
	{
		if (ppRenderTarget == nullptr)
		{
			return E_POINTER;
		}
		*ppRenderTarget = m_backBuffer;
		if (m_backBuffer)
		{
			m_backBuffer->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface8** ppZStencilSurface) override
	{
		if (ppZStencilSurface == nullptr)
		{
			return E_POINTER;
		}
		*ppZStencilSurface = m_depthStencil;
		if (m_depthStencil)
		{
			m_depthStencil->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(BeginScene)() override { return D3D_OK; }
	STDMETHOD(EndScene)() override { return D3D_OK; }
	STDMETHOD(Clear)(DWORD, CONST D3DRECT*, DWORD, D3DCOLOR, float, DWORD) override { return D3D_OK; }
	STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* m) override
	{
		// TheSuperHackers @bugfix bobtista 22/04/2026 W3DWater and
		// W3DTreeBuffer read back the current view/world
		// transform via DX8Wrapper::_Get_DX8_Transform, compute inverses
		// and feed shader constants. A GetTransform that returns zero
		// yields a singular matrix (det = 0), producing NaN shader
		// inputs and visible banding artifacts on terrain water/tree
		// passes. Record what the game sets so GetTransform can return
		// the real matrix.
		if (m)
		{
			m_transforms[static_cast<DWORD>(state)] = *m;
		}
		return D3D_OK;
	}
	STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE state, D3DMATRIX* pMatrix) override
	{
		if (pMatrix == nullptr)
		{
			return E_POINTER;
		}
		auto it = m_transforms.find(static_cast<DWORD>(state));
		if (it != m_transforms.end())
		{
			*pMatrix = it->second;
		}
		else
		{
			// Identity default.
			std::memset(pMatrix, 0, sizeof(*pMatrix));
			pMatrix->_11 = pMatrix->_22 = pMatrix->_33 = pMatrix->_44 = 1.0f;
		}
		return D3D_OK;
	}
	STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override { return D3D_OK; }
	STDMETHOD(SetViewport)(CONST D3DVIEWPORT8*) override { return D3D_OK; }
	STDMETHOD(GetViewport)(D3DVIEWPORT8* pViewport) override
	{
		if (pViewport)
		{
			pViewport->X = 0; pViewport->Y = 0;
			pViewport->Width = m_width; pViewport->Height = m_height;
			pViewport->MinZ = 0.0f; pViewport->MaxZ = 1.0f;
		}
		return D3D_OK;
	}
	STDMETHOD(SetMaterial)(CONST D3DMATERIAL8*) override { return D3D_OK; }
	STDMETHOD(GetMaterial)(D3DMATERIAL8* pMaterial) override
	{
		if (pMaterial)
		{
			std::memset(pMaterial, 0, sizeof(*pMaterial));
		}
		return D3D_OK;
	}
	STDMETHOD(SetLight)(DWORD, CONST D3DLIGHT8*) override { return D3D_OK; }
	STDMETHOD(GetLight)(DWORD, D3DLIGHT8* pLight) override
	{
		if (pLight)
		{
			std::memset(pLight, 0, sizeof(*pLight));
		}
		return D3D_OK;
	}
	STDMETHOD(LightEnable)(DWORD, BOOL) override { return D3D_OK; }
	STDMETHOD(GetLightEnable)(DWORD, BOOL* pEnable) override
	{
		if (pEnable)
		{
			*pEnable = FALSE;
		}
		return D3D_OK;
	}
	STDMETHOD(SetClipPlane)(DWORD, CONST float*) override { return D3D_OK; }
	STDMETHOD(GetClipPlane)(DWORD, float* pPlane) override
	{
		if (pPlane)
		{
			std::memset(pPlane, 0, 4 * sizeof(float));
		}
		return D3D_OK;
	}
	STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE state, DWORD value) override
	{
		m_renderStates[static_cast<DWORD>(state)] = value;
		return D3D_OK;
	}
	STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE state, DWORD* pValue) override
	{
		if (pValue)
		{
			auto it = m_renderStates.find(static_cast<DWORD>(state));
			*pValue = (it != m_renderStates.end()) ? it->second : 0;
		}
		return D3D_OK;
	}
	STDMETHOD(BeginStateBlock)() override { return D3D_OK; }
	STDMETHOD(EndStateBlock)(DWORD* pToken) override
	{
		if (pToken)
		{
			*pToken = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(ApplyStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(CaptureStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(DeleteStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE, DWORD* pToken) override
	{
		if (pToken)
		{
			*pToken = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(SetClipStatus)(CONST D3DCLIPSTATUS8*) override { return D3D_OK; }
	STDMETHOD(GetClipStatus)(D3DCLIPSTATUS8* pClipStatus) override
	{
		if (pClipStatus)
		{
			std::memset(pClipStatus, 0, sizeof(*pClipStatus));
		}
		return D3D_OK;
	}
	STDMETHOD(GetTexture)(DWORD, IDirect3DBaseTexture8** ppTexture) override
	{
		if (ppTexture)
		{
			*ppTexture = nullptr;
		}
		return D3D_OK;
	}
	STDMETHOD(SetTexture)(DWORD, IDirect3DBaseTexture8*) override { return D3D_OK; }
	STDMETHOD(GetTextureStageState)(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* pValue) override
	{
		if (pValue == nullptr)
		{
			return E_POINTER;
		}
		const StageStateKey key = { stage, static_cast<DWORD>(type) };
		std::unordered_map<StageStateKey, DWORD, StageStateKeyHash>::const_iterator it = m_textureStageStates.find(key);
		*pValue = (it != m_textureStageStates.end()) ? it->second : 0;
		return D3D_OK;
	}
	STDMETHOD(SetTextureStageState)(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
	{
		const StageStateKey key = { stage, static_cast<DWORD>(type) };
		m_textureStageStates[key] = value;
		return D3D_OK;
	}
	STDMETHOD(ValidateDevice)(DWORD* pNumPasses) override
	{
		if (pNumPasses)
		{
			*pNumPasses = 1;
		}
		return D3D_OK;
	}
	STDMETHOD(GetInfo)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(SetPaletteEntries)(UINT, CONST PALETTEENTRY*) override { return D3D_OK; }
	STDMETHOD(GetPaletteEntries)(UINT, PALETTEENTRY*) override { return D3D_OK; }
	STDMETHOD(SetCurrentTexturePalette)(UINT) override { return D3D_OK; }
	STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber) override
	{
		if (PaletteNumber)
		{
			*PaletteNumber = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE, UINT, UINT) override { return D3D_OK; }
	STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT) override { return D3D_OK; }
	STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE, UINT, CONST void*, UINT) override { return D3D_OK; }
	STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE, UINT, UINT, UINT, CONST void*, D3DFORMAT, CONST void*, UINT) override { return D3D_OK; }
	STDMETHOD(ProcessVertices)(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) override { return D3D_OK; }
	STDMETHOD(CreateVertexShader)(CONST DWORD*, CONST DWORD*, DWORD* pHandle, DWORD) override
	{
		if (pHandle)
		{
			*pHandle = 1;
		}
		return D3D_OK;
	}
	STDMETHOD(SetVertexShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShader)(DWORD* pHandle) override
	{
		if (pHandle)
		{
			*pHandle = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(DeleteVertexShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(SetVertexShaderConstant)(DWORD, CONST void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShaderConstant)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShaderDeclaration)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData)
		{
			*pSizeOfData = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(GetVertexShaderFunction)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData)
		{
			*pSizeOfData = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(SetStreamSource)(UINT, IDirect3DVertexBuffer8*, UINT) override { return D3D_OK; }
	STDMETHOD(GetStreamSource)(UINT, IDirect3DVertexBuffer8** ppStreamData, UINT* pStride) override
	{
		if (ppStreamData)
		{
			*ppStreamData = nullptr;
		}
		if (pStride)
		{
			*pStride = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(SetIndices)(IDirect3DIndexBuffer8*, UINT) override { return D3D_OK; }
	STDMETHOD(GetIndices)(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex) override
	{
		if (ppIndexData)
		{
			*ppIndexData = nullptr;
		}
		if (pBaseVertexIndex)
		{
			*pBaseVertexIndex = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(CreatePixelShader)(CONST DWORD*, DWORD* pHandle) override
	{
		if (pHandle)
		{
			*pHandle = 1;
		}
		return D3D_OK;
	}
	STDMETHOD(SetPixelShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShader)(DWORD* pHandle) override
	{
		if (pHandle)
		{
			*pHandle = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(DeletePixelShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(SetPixelShaderConstant)(DWORD, CONST void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShaderConstant)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShaderFunction)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData)
		{
			*pSizeOfData = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(DrawRectPatch)(UINT, CONST float*, CONST D3DRECTPATCH_INFO*) override { return D3D_OK; }
	STDMETHOD(DrawTriPatch)(UINT, CONST float*, CONST D3DTRIPATCH_INFO*) override { return D3D_OK; }
	STDMETHOD(DeletePatch)(UINT) override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3D8* m_parent;
	HWND m_focusWindow;
	UINT m_width;
	UINT m_height;
	IDirect3DSurface8* m_backBuffer;
	IDirect3DSurface8* m_depthStencil;
	std::unordered_map<DWORD, D3DMATRIX> m_transforms;
	std::unordered_map<DWORD, DWORD> m_renderStates;
	std::unordered_map<StageStateKey, DWORD, StageStateKeyHash> m_textureStageStates;
};

// ---------------------------------------------------------------------------
// Direct3D 8 factory interface
// ---------------------------------------------------------------------------
class StubD3D8Interface final : public IDirect3D8
{
public:
	StubD3D8Interface() : m_refCount(1) {}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3D8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}

	STDMETHOD(RegisterSoftwareDevice)(void*) override { return D3D_OK; }
	STDMETHOD_(UINT, GetAdapterCount)() override { return 1; }
	STDMETHOD(GetAdapterIdentifier)(UINT, DWORD, D3DADAPTER_IDENTIFIER8* pIdentifier) override
	{
		if (pIdentifier == nullptr)
		{
			return E_POINTER;
		}
		std::memset(pIdentifier, 0, sizeof(*pIdentifier));
		std::strncpy(pIdentifier->Driver, "StubD3D8", sizeof(pIdentifier->Driver) - 1);
		std::strncpy(pIdentifier->Description, "Generals bgfx standalone stub", sizeof(pIdentifier->Description) - 1);
		// TheSuperHackers @build bobtista 13/06/2026 min-dx8-sdk fork's adapter
		// identifier has no DriverVersion field on non-Windows.
#if defined(_WIN32)
		pIdentifier->DriverVersion.QuadPart = 0;
#endif
		pIdentifier->VendorId = 0;
		pIdentifier->DeviceId = 0;
		pIdentifier->SubSysId = 0;
		pIdentifier->Revision = 0;
		pIdentifier->WHQLLevel = 0;
		return D3D_OK;
	}
	STDMETHOD_(UINT, GetAdapterModeCount)(UINT) override { return 1; }
	STDMETHOD(EnumAdapterModes)(UINT, UINT, D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr)
		{
			return E_POINTER;
		}
		pMode->Width = 1920;
		pMode->Height = 1080;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(GetAdapterDisplayMode)(UINT, D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr)
		{
			return E_POINTER;
		}
		pMode->Width = 1920;
		pMode->Height = 1080;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(CheckDeviceType)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) override { return S_OK; }
	STDMETHOD(CheckDeviceFormat)(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) override { return S_OK; }
	STDMETHOD(CheckDeviceMultiSampleType)(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE) override { return S_OK; }
	STDMETHOD(CheckDepthStencilMatch)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) override { return S_OK; }
	STDMETHOD(GetDeviceCaps)(UINT, D3DDEVTYPE, D3DCAPS8* pCaps) override
	{
		if (pCaps == nullptr)
		{
			return E_POINTER;
		}
		FillCaps(*pCaps);
		return D3D_OK;
	}
	STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT) override { return NULL; }
	STDMETHOD(CreateDevice)(UINT, D3DDEVTYPE, HWND hFocusWindow, DWORD, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) override
	{
		if (ppReturnedDeviceInterface == nullptr)
		{
			return E_POINTER;
		}
		UINT width = 1920;
		UINT height = 1080;
		if (pPresentationParameters)
		{
			if (pPresentationParameters->BackBufferWidth)
			{
				width = pPresentationParameters->BackBufferWidth;
			}
			if (pPresentationParameters->BackBufferHeight)
			{
				height = pPresentationParameters->BackBufferHeight;
			}
		}
		*ppReturnedDeviceInterface = new StubD3D8Device(this, hFocusWindow, width, height);
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
};

} // namespace

IDirect3D8* CreateStubD3D8Interface()
{
	return new StubD3D8Interface();
}

#endif // GGC_BGFX_STANDALONE
