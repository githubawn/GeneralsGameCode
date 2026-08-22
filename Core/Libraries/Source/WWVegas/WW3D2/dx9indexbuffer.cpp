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
 * TheSuperHackers @feature Full clone of dx8indexbuffer.cpp targeting D3D9Ex, adapted
 * from the proven origin/dx9-test-clean port -- see dx9vertexbuffer.cpp's header comment
 * for the D3D9Ex-specific deviations (D3DPOOL_DEFAULT always, DX9ExBackend::Get_Device(),
 * no Support_TnL()/Support_NPatches() caps checks). This file must never see D3D8 headers.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

//#define INDEX_BUFFER_LOG

#include "Utility/CppMacros.h" // Must be first (see the PCH's own ordering)
#include "dx9indexbuffer.h"

#include <d3d9.h>

#include "Backend/DX9ExBackend.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WWDebug/wwmemlog.h"

#define DEFAULT_IB_SIZE 5000

static bool _DynamicSortingIndexArrayInUse=false;
static SortingIndexBufferClass* _DynamicSortingIndexArray;
static unsigned short _DynamicSortingIndexArraySize=0;
static unsigned short _DynamicSortingIndexArrayOffset=0;

static bool _DynamicDX9EXIndexBufferInUse=false;
static DX9IndexBufferClass* _DynamicDX9EXIndexBuffer=nullptr;
static unsigned short _DynamicDX9EXIndexBufferSize=DEFAULT_IB_SIZE;
static unsigned short _DynamicDX9EXIndexBufferOffset=0;

// ----------------------------------------------------------------------------
//
// IndexBufferClass::WriteLockClass/AppendLockClass -- duplicated here rather
// than shared with dx8indexbuffer.cpp; see dx9vertexbuffer.cpp's equivalent
// note for why.
//
// ----------------------------------------------------------------------------

IndexBufferClass::WriteLockClass::WriteLockClass(IndexBufferClass* index_buffer_, int flags) : index_buffer(index_buffer_)
{
	WWASSERT(index_buffer);
	WWASSERT(!index_buffer->Engine_Refs());
	index_buffer->Add_Ref();
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9IndexBufferClass*>(index_buffer)->Get_DX9_Index_Buffer()->Lock(
			0,
			index_buffer->Get_Index_Count()*sizeof(WORD),
			(void**)&indices,
			flags)));
		break;
	case BUFFER_TYPE_SORTING:
		indices=static_cast<SortingIndexBufferClass*>(index_buffer)->index_buffer;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

IndexBufferClass::WriteLockClass::~WriteLockClass()
{
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9IndexBufferClass*>(index_buffer)->Get_DX9_Index_Buffer()->Unlock()));
		break;
	case BUFFER_TYPE_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	index_buffer->Release_Ref();
}

IndexBufferClass::AppendLockClass::AppendLockClass(IndexBufferClass* index_buffer_,unsigned start_index, unsigned index_range)
	:
	index_buffer(index_buffer_)
{
	WWASSERT(start_index+index_range<=index_buffer->Get_Index_Count());
	WWASSERT(index_buffer);
	WWASSERT(!index_buffer->Engine_Refs());
	index_buffer->Add_Ref();
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9IndexBufferClass*>(index_buffer)->Get_DX9_Index_Buffer()->Lock(
			start_index*sizeof(unsigned short),
			index_range*sizeof(unsigned short),
			(void**)&indices,
			0)));
		break;
	case BUFFER_TYPE_SORTING:
		indices=static_cast<SortingIndexBufferClass*>(index_buffer)->index_buffer+start_index;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

IndexBufferClass::AppendLockClass::~AppendLockClass()
{
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9IndexBufferClass*>(index_buffer)->Get_DX9_Index_Buffer()->Unlock()));
		break;
	case BUFFER_TYPE_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	index_buffer->Release_Ref();
}

// ----------------------------------------------------------------------------

DX9IndexBufferClass::DX9IndexBufferClass(unsigned short index_count_,UsageType usage)
	:
	IndexBufferClass(BUFFER_TYPE_DX9EX,index_count_)
{
	WWASSERT(index_count);

	IDirect3DDevice9Ex * device = DX9ExBackend::Get_Device();
	WWASSERT(device);
	if (device == nullptr)
	{
		return;
	}

	unsigned usage_flags=
		D3DUSAGE_WRITEONLY|
		((usage&USAGE_DYNAMIC) ? D3DUSAGE_DYNAMIC : 0)|
		((usage&USAGE_NPATCHES) ? D3DUSAGE_NPATCHES : 0)|
		((usage&USAGE_SOFTWAREPROCESSING) ? D3DUSAGE_SOFTWAREPROCESSING : 0);

	// D3D9Ex forbids D3DPOOL_MANAGED -- always D3DPOOL_DEFAULT, unlike the
	// DX8/plain-D3D9 path which uses D3DPOOL_MANAGED for non-dynamic buffers.
	HRESULT ret=device->CreateIndexBuffer(
		sizeof(WORD)*index_count,
		usage_flags,
		D3DFMT_INDEX16,
		D3DPOOL_DEFAULT,
		&index_buffer,
		nullptr);

	if (ret==D3DERR_OUTOFVIDEOMEMORY) {
		TextureClass::Invalidate_Old_Unused_Textures(5000);
		WW3D::_Invalidate_Mesh_Cache();

		ret=device->CreateIndexBuffer(
			sizeof(WORD)*index_count,
			usage_flags,
			D3DFMT_INDEX16,
			D3DPOOL_DEFAULT,
			&index_buffer,
			nullptr);
	}

	WWASSERT(SUCCEEDED(ret));
}

DX9IndexBufferClass::~DX9IndexBufferClass()
{
	index_buffer->Release();
}

// ----------------------------------------------------------------------------

SortingIndexBufferClass::SortingIndexBufferClass(unsigned short index_count_)
	:
	IndexBufferClass(BUFFER_TYPE_SORTING,index_count_)
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(index_count);

	index_buffer=W3DNEWARRAY unsigned short[index_count];
}

SortingIndexBufferClass::~SortingIndexBufferClass()
{
	delete[] index_buffer;
}

// ----------------------------------------------------------------------------

DynamicIBAccessClass::DynamicIBAccessClass(unsigned short type_, unsigned short index_count_)
	:
	IndexCount(index_count_),
	IndexBuffer(nullptr),
	Type(type_)
{
	WWASSERT(Type==BUFFER_TYPE_DYNAMIC_DX9EX || Type==BUFFER_TYPE_DYNAMIC_SORTING);
	if (Type==BUFFER_TYPE_DYNAMIC_DX9EX) {
		Allocate_Backend_Dynamic_Buffer();
	}
	else {
		Allocate_Sorting_Dynamic_Buffer();
	}
}

DynamicIBAccessClass::~DynamicIBAccessClass()
{
	REF_PTR_RELEASE(IndexBuffer);
	if (Type==BUFFER_TYPE_DYNAMIC_DX9EX) {
		_DynamicDX9EXIndexBufferInUse=false;
		_DynamicDX9EXIndexBufferOffset+=IndexCount;
	}
	else {
		_DynamicSortingIndexArrayInUse=false;
		_DynamicSortingIndexArrayOffset+=IndexCount;
	}
}

void DynamicIBAccessClass::_Deinit()
{
	WWASSERT ((_DynamicDX9EXIndexBuffer == nullptr) || (_DynamicDX9EXIndexBuffer->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicDX9EXIndexBuffer);
	_DynamicDX9EXIndexBufferInUse=false;
	_DynamicDX9EXIndexBufferSize=DEFAULT_IB_SIZE;
	_DynamicDX9EXIndexBufferOffset=0;

	WWASSERT ((_DynamicSortingIndexArray == nullptr) || (_DynamicSortingIndexArray->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicSortingIndexArray);
	_DynamicSortingIndexArrayInUse=false;
	_DynamicSortingIndexArraySize=0;
	_DynamicSortingIndexArrayOffset=0;
}

// ----------------------------------------------------------------------------

DynamicIBAccessClass::WriteLockClass::WriteLockClass(DynamicIBAccessClass* ib_access_)
	:
	DynamicIBAccess(ib_access_)
{
	DynamicIBAccess->IndexBuffer->Add_Ref();
	switch (DynamicIBAccess->Get_Type()) {
	case BUFFER_TYPE_DYNAMIC_DX9EX:
		WWASSERT(DynamicIBAccess);
		WWASSERT(SUCCEEDED(
			static_cast<DX9IndexBufferClass*>(DynamicIBAccess->IndexBuffer)->Get_DX9_Index_Buffer()->Lock(
			DynamicIBAccess->IndexBufferOffset*sizeof(WORD),
			DynamicIBAccess->Get_Index_Count()*sizeof(WORD),
			(void**)&Indices,
			!DynamicIBAccess->IndexBufferOffset ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE)));
		break;
	case BUFFER_TYPE_DYNAMIC_SORTING:
		Indices=static_cast<SortingIndexBufferClass*>(DynamicIBAccess->IndexBuffer)->index_buffer;
		Indices+=DynamicIBAccess->IndexBufferOffset;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

DynamicIBAccessClass::WriteLockClass::~WriteLockClass()
{
	switch (DynamicIBAccess->Get_Type()) {
	case BUFFER_TYPE_DYNAMIC_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9IndexBufferClass*>(DynamicIBAccess->IndexBuffer)->Get_DX9_Index_Buffer()->Unlock()));
		break;
	case BUFFER_TYPE_DYNAMIC_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	DynamicIBAccess->IndexBuffer->Release_Ref();
}

// ----------------------------------------------------------------------------

void DynamicIBAccessClass::Allocate_Backend_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicDX9EXIndexBufferInUse);
	_DynamicDX9EXIndexBufferInUse=true;

	// If requesting more indices than dynamic index buffer can fit, delete the ib
	// and adjust the size to the new count.
	if (IndexCount>_DynamicDX9EXIndexBufferSize) {
		REF_PTR_RELEASE(_DynamicDX9EXIndexBuffer);
		_DynamicDX9EXIndexBufferSize=IndexCount;
		if (_DynamicDX9EXIndexBufferSize<DEFAULT_IB_SIZE) _DynamicDX9EXIndexBufferSize=DEFAULT_IB_SIZE;
	}

	// Create a new ib if one doesn't exist currently
	if (!_DynamicDX9EXIndexBuffer) {
		_DynamicDX9EXIndexBuffer=NEW_REF(DX9IndexBufferClass,(
			_DynamicDX9EXIndexBufferSize,
			DX9IndexBufferClass::USAGE_DYNAMIC));
		_DynamicDX9EXIndexBufferOffset=0;
	}

	// Any room at the end of the buffer?
	if (((unsigned)IndexCount+_DynamicDX9EXIndexBufferOffset)>_DynamicDX9EXIndexBufferSize) {
		_DynamicDX9EXIndexBufferOffset=0;
	}

	REF_PTR_SET(IndexBuffer,_DynamicDX9EXIndexBuffer);
	IndexBufferOffset=_DynamicDX9EXIndexBufferOffset;
}

void DynamicIBAccessClass::Allocate_Sorting_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicSortingIndexArrayInUse);
	_DynamicSortingIndexArrayInUse=true;

	unsigned new_index_count=_DynamicSortingIndexArrayOffset+IndexCount;
	WWASSERT(new_index_count<65536);
	if (new_index_count>_DynamicSortingIndexArraySize) {
		REF_PTR_RELEASE(_DynamicSortingIndexArray);
		_DynamicSortingIndexArraySize=new_index_count;
		if (_DynamicSortingIndexArraySize<DEFAULT_IB_SIZE) _DynamicSortingIndexArraySize=DEFAULT_IB_SIZE;
	}

	if (!_DynamicSortingIndexArray) {
		_DynamicSortingIndexArray=NEW_REF(SortingIndexBufferClass,(_DynamicSortingIndexArraySize));
		_DynamicSortingIndexArrayOffset=0;
	}

	REF_PTR_SET(IndexBuffer,_DynamicSortingIndexArray);
	IndexBufferOffset=_DynamicSortingIndexArrayOffset;
}

void DynamicIBAccessClass::_Reset(bool frame_changed)
{
	_DynamicSortingIndexArrayOffset=0;
	if (frame_changed) _DynamicDX9EXIndexBufferOffset=0;
}

unsigned short DynamicIBAccessClass::Get_Default_Index_Count()
{
	return _DynamicDX9EXIndexBufferSize;
}

// ----------------------------------------------------------------------------

IndexBufferClass* Create_Index_Buffer(unsigned short index_count, BufferUsageType usage)
{
	return NEW_REF(DX9IndexBufferClass,(index_count, static_cast<DX9IndexBufferClass::UsageType>(usage)));
}
