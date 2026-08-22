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

#pragma once

// TheSuperHackers @refactor Moved out of dx8wrapper.h (which is not
// D3D8-header-free) so DX9ExBackend and DX9VertexBufferClass/
// DX9IndexBufferClass can see VertexBufferClass::Type()/IndexBufferClass::Type()
// tags without pulling in <d3d8.h>.
enum {
	BUFFER_TYPE_DX8,
	BUFFER_TYPE_SORTING,
	BUFFER_TYPE_DYNAMIC_DX8,
	BUFFER_TYPE_DYNAMIC_SORTING,
	BUFFER_TYPE_DX9EX,
	BUFFER_TYPE_DYNAMIC_DX9EX,
	BUFFER_TYPE_INVALID
};

// TheSuperHackers @refactor Backend-neutral mirror of
// DX8VertexBufferClass::UsageType/DX8IndexBufferClass::UsageType/
// DX9VertexBufferClass::UsageType/DX9IndexBufferClass::UsageType, which are
// all identical bit values today. Lets callers that only construct buffers
// through Create_Vertex_Buffer()/Create_Index_Buffer() (see
// vertexbufferclass.h/indexbufferclass.h) avoid naming either concrete
// backend class just to pass a usage flag.
enum BufferUsageType {
	WW3D_USAGE_DEFAULT=0,
	WW3D_USAGE_DYNAMIC=1,
	WW3D_USAGE_SOFTWAREPROCESSING=2,
	WW3D_USAGE_NPATCHES=4
};

// TheSuperHackers @refactor Backend-neutral mirror of D3DLOCK_DISCARD/
// D3DLOCK_NOOVERWRITE -- identical bit values in both the D3D8 and D3D9
// SDKs -- for callers passing lock flags to VertexBufferClass::WriteLockClass/
// IndexBufferClass::WriteLockClass without naming either D3D header.
enum {
	WW3D_LOCK_DISCARD     = 0x00002000L,
	WW3D_LOCK_NOOVERWRITE = 0x00001000L
};

// TheSuperHackers @refactor Callers constructing a DynamicVBAccessClass/
// DynamicIBAccessClass need to pass a BUFFER_TYPE_DYNAMIC_* tag matching the
// active backend. This picks it via the same GGC_RENDER_BACKEND_DX9EX
// compile definition RenderBackend.cpp already uses to select the backend
// class itself (see Backend/RenderBackend.cpp) -- no D3D types are involved
// here, just an enum selection, so this is not the dx8/dx9 "mixing" pattern
// that's banned elsewhere (see BACKEND_AGNOSTIC_RESOURCES_PLAN.md).
inline unsigned Get_Default_Dynamic_Buffer_Type()
{
#if defined(GGC_RENDER_BACKEND_DX9EX)
	return BUFFER_TYPE_DYNAMIC_DX9EX;
#else
	return BUFFER_TYPE_DYNAMIC_DX8;
#endif
}
