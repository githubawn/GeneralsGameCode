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

class FVFInfoClass;
class VertexBufferClass;

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
