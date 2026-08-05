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

/***********************************************************************************************
 * TheSuperHackers @feature Full clone of dx8indexbuffer.h targeting D3D9Ex, adapted from
 * the proven origin/dx9-test-clean port. Mutually exclusive with dx8indexbuffer.h/.cpp at
 * the CMake level -- only ever compiled when GGC_RENDER_BACKEND is dx9ex, so duplicating
 * DynamicIBAccessClass/SortingIndexBufferClass here (rather than sharing dx8indexbuffer.h's
 * copies) is intentional.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "WWDebug/wwdebug.h"
#include "WWMath/sphere.h"
#include "indexbufferclass.h"
#include "bufferusagetype.h"

class SortingRendererClass;
struct IDirect3DIndexBuffer9;
class DX9IndexBufferClass;
class SortingIndexBufferClass;

// HY 2/14/01
// Created
class DynamicIBAccessClass
{
	W3DMPO_CODE(DynamicIBAccessClass)

	friend SortingRendererClass;

	unsigned Type;
	unsigned short IndexCount;
	unsigned short IndexBufferOffset;
	IndexBufferClass* IndexBuffer;

	void Allocate_Sorting_Dynamic_Buffer();
	void Allocate_DX9EX_Dynamic_Buffer();

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
** DX9IndexBufferClass
** This class wraps a D3D9Ex index buffer. D3D9Ex forbids D3DPOOL_MANAGED
** entirely, so this always creates with D3DPOOL_DEFAULT regardless of the
** usage flag.
*/
class DX9IndexBufferClass : public IndexBufferClass
{
	W3DMPO_CODE(DX9IndexBufferClass)

	friend IndexBufferClass::WriteLockClass;
	friend IndexBufferClass::AppendLockClass;
public:
	enum UsageType {
		USAGE_DEFAULT=0,
		USAGE_DYNAMIC=1,
		USAGE_SOFTWAREPROCESSING=2,
		USAGE_NPATCHES=4
	};

	DX9IndexBufferClass(unsigned short index_count,UsageType usage=USAGE_DEFAULT);
	virtual ~DX9IndexBufferClass() override;

	void Copy(unsigned int* indices,unsigned start_index,unsigned index_count);
	void Copy(unsigned short* indices,unsigned start_index,unsigned index_count);

	IDirect3DIndexBuffer9* Get_DX9_Index_Buffer()	{ return index_buffer; }

private:
	IDirect3DIndexBuffer9*	index_buffer;		// actual D3D9Ex index buffer
};



class SortingIndexBufferClass : public IndexBufferClass
{
	W3DMPO_CODE(SortingIndexBufferClass)

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
