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

// TheSuperHackers @refactor bobtista 22/04/2026 Phase 5.2 Stage 1
// No-op implementations of the Direct3D 8 COM interfaces. These stubs allow
// DX8Wrapper to be driven against a synthetic Direct3D 8 device without
// loading d3d8.dll at runtime. Every method returns S_OK / D3D_OK and does
// nothing; resource creation returns ref-counted stub objects; Lock methods
// hand out heap-allocated scratch buffers. Compiled only under the
// GGC_BGFX_STANDALONE build configuration used by the bgfx-only renderer.

#pragma once

#if defined(GGC_BGFX_STANDALONE)

#include <d3d8.h>

// Entry point. Returns a newly AddRef'd IDirect3D8 stub. Caller releases.
IDirect3D8* CreateStubD3D8Interface();

// TheSuperHackers @feature githubawn 16/07/2026 Frees a texture's CPU-side
// scratch buffer(s) once a render backend has finished reading them (e.g.
// after uploading to its own GPU-resident copy), so the pixel data isn't
// permanently double-stored (once here, once in the backend's own texture
// memory) for the whole lifetime of every texture. Safe to call repeatedly;
// a subsequent LockRect lazily reallocates. See StubD3D8Texture::
// ReleaseCpuScratch's comment in the .cpp for the one case it deliberately
// leaves alone (a level already aliased by a GetSurfaceLevel() surface).
void ReleaseTextureCpuScratch(IDirect3DTexture8* texture);

// TheSuperHackers @bugfix githubawn 17/07/2026 Same double-storage problem as
// ReleaseTextureCpuScratch above, for static mesh vertex/index buffers: a
// render backend that copies a static VertexBufferClass/IndexBufferClass's
// data into its own GPU-visible buffer (e.g. Citro3dBackend's static-mesh
// cache) should release this stub's permanent scratch afterward, or every
// mesh's geometry is double-stored (once here in general heap, once in the
// backend's own GPU-resident copy) for its whole lifetime -- the same class
// of bug the texture fix above addresses, just for geometry. Safe to call
// repeatedly; a subsequent Lock lazily reallocates.
void ReleaseVertexBufferCpuScratch(IDirect3DVertexBuffer8* vb);
void ReleaseIndexBufferCpuScratch(IDirect3DIndexBuffer8* ib);

#endif // GGC_BGFX_STANDALONE
