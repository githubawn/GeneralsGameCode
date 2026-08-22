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

// TheSuperHackers @refactor VertexBufferClass split out of dx8vertexbuffer.h
// so it can be included without pulling in dx8fvf.h -> <d3d8.h>. The base
// class itself holds no D3D pointer (see BACKEND_AGNOSTIC_RESOURCES_PLAN.md);
// this header keeps it that way at the #include level too, so a non-DX8
// backend can see VertexBufferClass without seeing any D3D8 declarations.

#include "WWLib/always.h"
#include "WWLib/refcount.h"
#include "WWDebug/wwdebug.h"
#include "bufferusagetype.h"
#include "fvfinfoclass.h"

class FVFInfoClass;
class VertexBufferClass;
class SortingRendererClass;
class DX8Wrapper;
class DX9ExBackend;
struct VertexFormatXYZNDUV2;

// TheSuperHackers @refactor D3DFVF_ bit values are identical between the
// D3D8 and D3D9 SDKs (see fvfinfoclass.h), so this constant -- previously
// duplicated verbatim in dx8vertexbuffer.h/dx9vertexbuffer.h -- has one
// definition here instead.
const unsigned dynamic_fvf_type = WW3D_FVF_XYZ|WW3D_FVF_NORMAL|WW3D_FVF_TEX2|WW3D_FVF_DIFFUSE;

// TheSuperHackers @refactor Backend-neutral construction for callers that
// only ever need a plain (non-sorting) vertex buffer and don't otherwise
// touch any backend-specific type (DX8VertexBufferClass, DX9VertexBufferClass,
// ...). Defined once per backend in dx8vertexbuffer.cpp/dx9vertexbuffer.cpp,
// mutually exclusive at the CMake level like the classes themselves -- see
// BACKEND_AGNOSTIC_RESOURCES_PLAN.md. fvf must be one of the WW3D_FVF_*
// constants in fvfinfoclass.h (or an equivalent raw D3DFVF_* bit combination).
VertexBufferClass* Create_Vertex_Buffer(unsigned fvf, unsigned short vertex_count, BufferUsageType usage=WW3D_USAGE_DEFAULT);

class VertexBufferLockClass
{
protected:
	VertexBufferClass* VertexBuffer;
	void* Vertices;

	// This class can't be used directly, so constructor as to be protected
	VertexBufferLockClass(VertexBufferClass* vertex_buffer_) : VertexBuffer(vertex_buffer_) {}
public:
	void* Get_Vertex_Array() { return Vertices; }
};

/**
** VertexBufferClass
** Backend-agnostic base class for a vertex buffer. Holds no D3D pointer;
** concrete backends (DX8VertexBufferClass, SortingVertexBufferClass, ...)
** hold whatever GPU resource they need. Use the lock objects to modify or
** append to the vertex buffer.
*/
class VertexBufferClass : public RefCountClass
{
protected:
	VertexBufferClass(unsigned type, unsigned FVF, unsigned short VertexCount);
	virtual ~VertexBufferClass() override;
public:

	const FVFInfoClass& FVF_Info() const { return *fvf_info; }
	unsigned short Get_Vertex_Count() const { return VertexCount; }
	unsigned Type() const { return type; }

	void Add_Engine_Ref() const;
	void Release_Engine_Ref() const;
	unsigned Engine_Refs() const { return engine_refs; }

	class WriteLockClass : public VertexBufferLockClass
	{
	public:
		WriteLockClass(VertexBufferClass* vertex_buffer, int flags=0);
		~WriteLockClass();
	};

	class AppendLockClass : public VertexBufferLockClass
	{
	public:
		AppendLockClass(VertexBufferClass* vertex_buffer,unsigned start_index, unsigned index_range);
		~AppendLockClass();
	};

	static unsigned Get_Total_Buffer_Count();
	static unsigned Get_Total_Allocated_Vertices();
	static unsigned Get_Total_Allocated_Memory();

protected:
	unsigned							type;
	unsigned short					VertexCount;
	mutable int						engine_refs;
	FVFInfoClass*					fvf_info;
};

/**
** DynamicVBAccessClass
** Dynamic vertex buffer access is a wrapper to a single cycled dynamic
** vertex buffer. Backend-agnostic: identical layout/public interface in
** both DX8 and DX9Ex, differing only in the private buffer-allocation
** implementation (see Allocate_Backend_Dynamic_Buffer()'s definition,
** duplicated per backend in dx8vertexbuffer.cpp/dx9vertexbuffer.cpp -- not
** shared, same rule as VertexBufferClass::WriteLockClass/AppendLockClass).
**
** Type parameter can be either BUFFER_TYPE_DYNAMIC_SORTING or the active
** backend's BUFFER_TYPE_DYNAMIC_* tag -- use Get_Default_Dynamic_Buffer_Type()
** (bufferusagetype.h) rather than hardcoding one.
**
** NOTE: Dynamic vertex buffer accessors should only be used locally!
*/
class DynamicVBAccessClass
{
	friend DX8Wrapper;
	friend DX9ExBackend;
	friend SortingRendererClass;

	const FVFInfoClass& FVFInfo;
	unsigned Type;
	unsigned short VertexCount;
	unsigned short VertexBufferOffset;
	VertexBufferClass* VertexBuffer;

	void Allocate_Sorting_Dynamic_Buffer();
	void Allocate_Backend_Dynamic_Buffer();
public:
	// Note: Even though the constructor takes fvf as a parameter, currently the
	// only acceptable parameter is "dynamic_fvf_type". Any other type will
	// result to an assert.
	DynamicVBAccessClass(unsigned type,unsigned fvf,unsigned short vertex_count);
	~DynamicVBAccessClass();

	const FVFInfoClass& FVF_Info() const { return FVFInfo; }
	unsigned Get_Type() const { return Type; }
	unsigned short Get_Vertex_Count() const { return VertexCount; }

	// Call at the end of the execution, or at whatever time you wish to release
	// the recycled dynamic vertex buffer.
	static void _Deinit();
	static void _Reset(bool frame_changed);
	static unsigned short Get_Default_Vertex_Count();	///<current size of dynamic vertex buffer

	// To lock the vertex buffer, create instance of this write class locally.
	// The buffer is automatically unlocked when you exit the scope.
	class WriteLockClass
	{
		DynamicVBAccessClass* DynamicVBAccess;
		VertexFormatXYZNDUV2 * Vertices;
	public:
		WriteLockClass(DynamicVBAccessClass* vb_access);
		~WriteLockClass();

		VertexFormatXYZNDUV2 * Get_Formatted_Vertex_Array();
	};
	friend WriteLockClass;
};

inline VertexFormatXYZNDUV2 * DynamicVBAccessClass::WriteLockClass::Get_Formatted_Vertex_Array()
{
	// assert that the format of the dynamic vertex buffer is still what we think it is.
	WWASSERT(DynamicVBAccess->VertexBuffer->FVF_Info().Get_FVF() == dynamic_fvf_type);
	return Vertices;
}
