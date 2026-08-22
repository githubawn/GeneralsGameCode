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

// TheSuperHackers @refactor IndexBufferClass split out of dx8indexbuffer.h,
// mirroring vertexbufferclass.h's split out of dx8vertexbuffer.h. The base
// class itself holds no D3D pointer; this header keeps it that way at the
// #include level too, so a non-DX8 backend can see IndexBufferClass without
// seeing any D3D8 declarations.

#include "WWLib/always.h"
#include "WWLib/refcount.h"
#include "WWDebug/wwdebug.h"
#include "bufferusagetype.h"

class IndexBufferClass;
class SortingRendererClass;
class DX8Wrapper;
class DX9ExBackend;

// TheSuperHackers @refactor Backend-neutral construction, mirroring
// Create_Vertex_Buffer() in vertexbufferclass.h -- see that comment.
IndexBufferClass* Create_Index_Buffer(unsigned short index_count, BufferUsageType usage=WW3D_USAGE_DEFAULT);

/**
** IndexBufferClass
** Backend-agnostic base class for an index buffer. Holds no D3D pointer;
** concrete backends (DX8IndexBufferClass, SortingIndexBufferClass, ...)
** hold whatever GPU resource they need. Use the lock objects to modify or
** append to the index buffer.
*/
class IndexBufferClass : public RefCountClass
{
protected:
	virtual ~IndexBufferClass() override;
public:
	IndexBufferClass(unsigned type, unsigned short index_count);

	void Copy(unsigned int* indices,unsigned start_index,unsigned index_count);
	void Copy(unsigned short* indices,unsigned start_index,unsigned index_count);

	unsigned short Get_Index_Count() const { return index_count; }

	unsigned Type() const { return type; }

	void Add_Engine_Ref() const;
	void Release_Engine_Ref() const;
	unsigned Engine_Refs() const { return engine_refs; }

	class WriteLockClass
	{
		IndexBufferClass* index_buffer;
		unsigned short* indices;
	public:
		WriteLockClass(IndexBufferClass* index_buffer, int flags=0);
		~WriteLockClass();

		unsigned short* Get_Index_Array() { return indices; }
	};

	class AppendLockClass
	{
		IndexBufferClass* index_buffer;
		unsigned short* indices;
	public:
		AppendLockClass(IndexBufferClass* index_buffer,unsigned start_index, unsigned index_range);
		~AppendLockClass();

		unsigned short* Get_Index_Array() { return indices; }
	};

	static unsigned Get_Total_Buffer_Count();
	static unsigned Get_Total_Allocated_Indices();
	static unsigned Get_Total_Allocated_Memory();

protected:
	mutable int					engine_refs;
	unsigned short				index_count;		// number of indices
	unsigned						type;
};

/**
** DynamicIBAccessClass
** Backend-agnostic dynamic index buffer access, mirroring DynamicVBAccessClass
** in vertexbufferclass.h -- see that class's comment for the split rationale.
*/
class DynamicIBAccessClass
{
	W3DMPO_CODE(DynamicIBAccessClass)

	friend DX8Wrapper;
	friend DX9ExBackend;
	friend SortingRendererClass;

	unsigned Type;
	unsigned short IndexCount;
	unsigned short IndexBufferOffset;
	IndexBufferClass* IndexBuffer;

	void Allocate_Sorting_Dynamic_Buffer();
	void Allocate_Backend_Dynamic_Buffer();

public:
	DynamicIBAccessClass(unsigned short type, unsigned short index_count);
	~DynamicIBAccessClass();

	unsigned Get_Type() const { return Type; }
	unsigned short Get_Index_Count() const { return IndexCount; }

	// Call at the end of the execution, or at whatever time you wish to release
	// the recycled dynamic index buffer.
	static void _Deinit();
	static void _Reset(bool frame_changed);
	static unsigned short Get_Default_Index_Count();	///<current size of dynamic index buffer

	// To lock the index buffer, create instance of this write class locally.
	// The buffer is automatically unlocked when you exit the scope.
	class WriteLockClass
	{
		DynamicIBAccessClass* DynamicIBAccess;
		unsigned short* Indices;
	public:
		WriteLockClass(DynamicIBAccessClass* ib_access);
		~WriteLockClass();
		unsigned short* Get_Index_Array() { return Indices; }
	};

	friend WriteLockClass;
};

/**
** SortingIndexBufferClass
** Backend-agnostic: identical in both DX8 and DX9Ex (a plain unsigned short[]
** array, no D3D pointer at all) -- declared once here, but still implemented
** separately (identically) in dx8indexbuffer.cpp/dx9indexbuffer.cpp per the
** no-shared-bodies rule (see DynamicIBAccessClass above).
*/
class SortingIndexBufferClass : public IndexBufferClass
{
	W3DMPO_CODE(SortingIndexBufferClass)

	friend DX8Wrapper;
	friend DX9ExBackend;
	friend SortingRendererClass;
	friend IndexBufferClass::WriteLockClass;
	friend IndexBufferClass::AppendLockClass;
	friend DynamicIBAccessClass::WriteLockClass;
public:
	SortingIndexBufferClass(unsigned short index_count);
	virtual ~SortingIndexBufferClass() override;

protected:
	unsigned short* index_buffer;
};
