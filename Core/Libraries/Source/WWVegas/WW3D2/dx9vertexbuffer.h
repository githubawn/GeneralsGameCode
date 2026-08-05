/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#pragma once

// TheSuperHackers @feature DX9Ex sibling of DX8VertexBufferClass. This
// header must never see D3D8 or D3D9 SDK headers (see CMakeLists.txt
// SKIP_PRECOMPILE_HEADERS / dx9vertexbuffer.cpp) -- only vertexbufferclass.h
// and bufferusagetype.h, both D3D-header-free.

#include "vertexbufferclass.h"
#include "bufferusagetype.h"

struct IDirect3DVertexBuffer9;

/**
** DX9VertexBufferClass
** D3D9Ex forbids D3DPOOL_MANAGED entirely, so unlike DX8VertexBufferClass
** (which uses D3DPOOL_MANAGED for non-dynamic buffers), this class always
** creates with D3DPOOL_DEFAULT regardless of the usage flag. Device-lost/
** reset recreation is not implemented yet (see
** BACKEND_AGNOSTIC_RESOURCES_PLAN.md correctness trap #6) -- this is scoped
** to get real geometry on screen, not full device-reset robustness.
*/
class DX9VertexBufferClass : public VertexBufferClass
{
public:
	enum UsageType {
		USAGE_DEFAULT=0,
		USAGE_DYNAMIC=1,
	};

	DX9VertexBufferClass(unsigned FVF, unsigned short VertexCount, UsageType usage=USAGE_DEFAULT);
	virtual ~DX9VertexBufferClass() override;

	IDirect3DVertexBuffer9 * Get_DX9_Vertex_Buffer() { return VertexBuffer; }

	// Opaque lock/unlock so dx8vertexbuffer.cpp's shared WriteLockClass/
	// AppendLockClass dispatch can drive this buffer without ever needing
	// <d3d9.h> itself. `flags` is passed straight through to
	// IDirect3DVertexBuffer9::Lock -- D3DLOCK_DISCARD/NOOVERWRITE/NOSYSLOCK
	// share the same bit values between D3D8 and D3D9.
	void * Lock_Raw(unsigned offset_bytes, unsigned size_bytes, unsigned flags);
	void Unlock_Raw();

private:
	IDirect3DVertexBuffer9 * VertexBuffer;

	void Create_Vertex_Buffer(UsageType usage);
};
