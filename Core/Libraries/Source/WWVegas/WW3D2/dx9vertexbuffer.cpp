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
 * TheSuperHackers @feature Full clone of dx8vertexbuffer.cpp targeting D3D9Ex, adapted
 * from the proven origin/dx9-test-clean port (which targeted plain D3D9). Differences from
 * that reference, both required by D3D9Ex specifically:
 *   - D3D9Ex forbids D3DPOOL_MANAGED entirely -- always D3DPOOL_DEFAULT, never conditional.
 *   - Device access goes through DX9ExBackend::Get_Device(), not a DX9Wrapper facade.
 *   - Support_TnL()/Support_NPatches() caps checks are dropped: DX9ExBackend::Initialize()
 *     already forces D3DCREATE_HARDWARE_VERTEXPROCESSING unconditionally, so software vertex
 *     processing fallback doesn't apply here.
 * This file must never see D3D8 headers -- it opts out of the shared PCH (see
 * CMakeLists.txt SKIP_PRECOMPILE_HEADERS) the same way Backend/DX9ExBackend.cpp does.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

//#define VERTEX_BUFFER_LOG

#include "Utility/CppMacros.h" // Must be first (see the PCH's own ordering)
#include "dx9vertexbuffer.h"

#include <d3d9.h>

#include "Backend/DX9ExBackend.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WWMath/vector2.h"
#include "WWMath/vector3.h"
#include "WWMath/vector4.h"
#include "WWLib/wwstring.h"
#include "WWDebug/wwmemlog.h"

namespace
{
	// Mirrors DX8Wrapper::Convert_Color's math (dx8wrapper.h) -- pure Vector4-
	// to-packed-ARGB conversion, no D3D dependency, duplicated here rather
	// than shared to keep this file free of any DX8Wrapper reference.
	unsigned Convert_Color(const Vector4& color)
	{
		unsigned r = (unsigned)(color.X*255.0f);
		unsigned g = (unsigned)(color.Y*255.0f);
		unsigned b = (unsigned)(color.Z*255.0f);
		unsigned a = (unsigned)(color.W*255.0f);
		return (a<<24) | (r<<16) | (g<<8) | b;
	}
}

#define DEFAULT_VB_SIZE 5000

static bool _DynamicSortingVertexArrayInUse=false;
static SortingVertexBufferClass* _DynamicSortingVertexArray=nullptr;
static unsigned short _DynamicSortingVertexArraySize=0;
static unsigned short _DynamicSortingVertexArrayOffset=0;

static bool _DynamicDX9EXVertexBufferInUse=false;
static DX9VertexBufferClass* _DynamicDX9EXVertexBuffer=nullptr;
static unsigned short _DynamicDX9EXVertexBufferSize=DEFAULT_VB_SIZE;
static unsigned short _DynamicDX9EXVertexBufferOffset=0;

static const FVFInfoClass _DynamicFVFInfo(dynamic_fvf_type);

// ----------------------------------------------------------------------------
//
// VertexBufferClass::WriteLockClass/AppendLockClass -- duplicated here rather
// than shared with dx8vertexbuffer.cpp, because their bodies dispatch on
// Type() and cast to the concrete backend-specific class.
//
// ----------------------------------------------------------------------------

VertexBufferClass::WriteLockClass::WriteLockClass(VertexBufferClass* VertexBuffer, int flags)
	:
	VertexBufferLockClass(VertexBuffer)
{
	WWASSERT(VertexBuffer);
	WWASSERT(!VertexBuffer->Engine_Refs());
	VertexBuffer->Add_Ref();
	switch (VertexBuffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9VertexBufferClass*>(VertexBuffer)->Get_DX9_Vertex_Buffer()->Lock(
			0,
			0,
			(void**)&Vertices,
			flags)));
		break;
	case BUFFER_TYPE_SORTING:
		Vertices=static_cast<SortingVertexBufferClass*>(VertexBuffer)->VertexBuffer;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

VertexBufferClass::WriteLockClass::~WriteLockClass()
{
	switch (VertexBuffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9VertexBufferClass*>(VertexBuffer)->Get_DX9_Vertex_Buffer()->Unlock()));
		break;
	case BUFFER_TYPE_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	VertexBuffer->Release_Ref();
}

VertexBufferClass::AppendLockClass::AppendLockClass(VertexBufferClass* VertexBuffer,unsigned start_index, unsigned index_range)
	:
	VertexBufferLockClass(VertexBuffer)
{
	WWASSERT(VertexBuffer);
	WWASSERT(!VertexBuffer->Engine_Refs());
	WWASSERT(start_index+index_range<=VertexBuffer->Get_Vertex_Count());
	VertexBuffer->Add_Ref();
	switch (VertexBuffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9VertexBufferClass*>(VertexBuffer)->Get_DX9_Vertex_Buffer()->Lock(
			start_index*VertexBuffer->FVF_Info().Get_FVF_Size(),
			index_range*VertexBuffer->FVF_Info().Get_FVF_Size(),
			(void**)&Vertices,
			0)));	// Default (no) flags
		break;
	case BUFFER_TYPE_SORTING:
		Vertices=static_cast<SortingVertexBufferClass*>(VertexBuffer)->VertexBuffer+start_index;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

VertexBufferClass::AppendLockClass::~AppendLockClass()
{
	switch (VertexBuffer->Type()) {
	case BUFFER_TYPE_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9VertexBufferClass*>(VertexBuffer)->Get_DX9_Vertex_Buffer()->Unlock()));
		break;
	case BUFFER_TYPE_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	VertexBuffer->Release_Ref();
}

// ----------------------------------------------------------------------------

SortingVertexBufferClass::SortingVertexBufferClass(unsigned short VertexCount)
	:
	VertexBufferClass(BUFFER_TYPE_SORTING, dynamic_fvf_type, VertexCount)
{
	WWMEMLOG(MEM_RENDERER);
	VertexBuffer=W3DNEWARRAY VertexFormatXYZNDUV2[VertexCount];
}

SortingVertexBufferClass::~SortingVertexBufferClass()
{
	delete[] VertexBuffer;
}

// ----------------------------------------------------------------------------

DX9VertexBufferClass::DX9VertexBufferClass(unsigned FVF, unsigned short vertex_count_, UsageType usage)
	:
	VertexBufferClass(BUFFER_TYPE_DX9EX, FVF, vertex_count_),
	VertexBuffer(nullptr)
{
	Create_Vertex_Buffer(usage);
}

DX9VertexBufferClass::DX9VertexBufferClass(
	const Vector3* vertices,
	const Vector3* normals,
	const Vector2* tex_coords,
	unsigned short VertexCount,
	UsageType usage)
	:
	VertexBufferClass(BUFFER_TYPE_DX9EX, D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_NORMAL, VertexCount),
	VertexBuffer(nullptr)
{
	WWASSERT(vertices);
	WWASSERT(normals);
	WWASSERT(tex_coords);

	Create_Vertex_Buffer(usage);
	Copy(vertices,normals,tex_coords,0,VertexCount);
}

DX9VertexBufferClass::DX9VertexBufferClass(
	const Vector3* vertices,
	const Vector3* normals,
	const Vector4* diffuse,
	const Vector2* tex_coords,
	unsigned short VertexCount,
	UsageType usage)
	:
	VertexBufferClass(BUFFER_TYPE_DX9EX, D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_NORMAL|D3DFVF_DIFFUSE, VertexCount),
	VertexBuffer(nullptr)
{
	WWASSERT(vertices);
	WWASSERT(normals);
	WWASSERT(tex_coords);
	WWASSERT(diffuse);

	Create_Vertex_Buffer(usage);
	Copy(vertices,normals,tex_coords,diffuse,0,VertexCount);
}

DX9VertexBufferClass::DX9VertexBufferClass(
	const Vector3* vertices,
	const Vector4* diffuse,
	const Vector2* tex_coords,
	unsigned short VertexCount,
	UsageType usage)
	:
	VertexBufferClass(BUFFER_TYPE_DX9EX, D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_DIFFUSE, VertexCount),
	VertexBuffer(nullptr)
{
	WWASSERT(vertices);
	WWASSERT(tex_coords);
	WWASSERT(diffuse);

	Create_Vertex_Buffer(usage);
	Copy(vertices,tex_coords,diffuse,0,VertexCount);
}

DX9VertexBufferClass::DX9VertexBufferClass(
	const Vector3* vertices,
	const Vector2* tex_coords,
	unsigned short VertexCount,
	UsageType usage)
	:
	VertexBufferClass(BUFFER_TYPE_DX9EX, D3DFVF_XYZ|D3DFVF_TEX1, VertexCount),
	VertexBuffer(nullptr)
{
	WWASSERT(vertices);
	WWASSERT(tex_coords);

	Create_Vertex_Buffer(usage);
	Copy(vertices,tex_coords,0,VertexCount);
}

DX9VertexBufferClass::~DX9VertexBufferClass()
{
	VertexBuffer->Release();
}

void DX9VertexBufferClass::Create_Vertex_Buffer(UsageType usage)
{
	WWASSERT(!VertexBuffer);

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
	HRESULT ret=device->CreateVertexBuffer(
		FVF_Info().Get_FVF_Size()*VertexCount,
		usage_flags,
		FVF_Info().Get_FVF(),
		D3DPOOL_DEFAULT,
		&VertexBuffer,
		nullptr);

	// If ran out of video memory, try releasing least used textures and mesh cache, then retry.
	if (ret==D3DERR_OUTOFVIDEOMEMORY) {
		TextureClass::Invalidate_Old_Unused_Textures(5000);
		WW3D::_Invalidate_Mesh_Cache();

		ret=device->CreateVertexBuffer(
			FVF_Info().Get_FVF_Size()*VertexCount,
			usage_flags,
			FVF_Info().Get_FVF(),
			D3DPOOL_DEFAULT,
			&VertexBuffer,
			nullptr);
	}

	WWASSERT(SUCCEEDED(ret));
}

void DX9VertexBufferClass::Copy(const Vector3* loc, const Vector3* norm, const Vector2* uv, unsigned first_vertex,unsigned count)
{
	WWASSERT(loc);
	WWASSERT(norm);
	WWASSERT(uv);
	WWASSERT(count<=VertexCount);
	WWASSERT(FVF_Info().Get_FVF()==DX9_FVF_XYZNUV1);

	if (first_vertex) {
		VertexBufferClass::AppendLockClass l(this,first_vertex,count);
		VertexFormatXYZNUV1* verts=(VertexFormatXYZNUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].nx=(*norm)[0];
			verts[v].ny=(*norm)[1];
			verts[v].nz=(*norm++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
		}
	}
	else {
		VertexBufferClass::WriteLockClass l(this);
		VertexFormatXYZNUV1* verts=(VertexFormatXYZNUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].nx=(*norm)[0];
			verts[v].ny=(*norm)[1];
			verts[v].nz=(*norm++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
		}
	}
}

void DX9VertexBufferClass::Copy(const Vector3* loc, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(count<=VertexCount);
	WWASSERT(FVF_Info().Get_FVF()==DX9_FVF_XYZ);

	if (first_vertex) {
		VertexBufferClass::AppendLockClass l(this,first_vertex,count);
		VertexFormatXYZ* verts=(VertexFormatXYZ*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
		}
	}
	else {
		VertexBufferClass::WriteLockClass l(this);
		VertexFormatXYZ* verts=(VertexFormatXYZ*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
		}
	}
}

void DX9VertexBufferClass::Copy(const Vector3* loc, const Vector2* uv, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(uv);
	WWASSERT(count<=VertexCount);
	WWASSERT(FVF_Info().Get_FVF()==DX9_FVF_XYZUV1);

	if (first_vertex) {
		VertexBufferClass::AppendLockClass l(this,first_vertex,count);
		VertexFormatXYZUV1* verts=(VertexFormatXYZUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
		}
	}
	else {
		VertexBufferClass::WriteLockClass l(this);
		VertexFormatXYZUV1* verts=(VertexFormatXYZUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
		}
	}
}

void DX9VertexBufferClass::Copy(const Vector3* loc, const Vector3* norm, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(norm);
	WWASSERT(count<=VertexCount);
	WWASSERT(FVF_Info().Get_FVF()==DX9_FVF_XYZN);

	if (first_vertex) {
		VertexBufferClass::AppendLockClass l(this,first_vertex,count);
		VertexFormatXYZN* verts=(VertexFormatXYZN*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].nx=(*norm)[0];
			verts[v].ny=(*norm)[1];
			verts[v].nz=(*norm++)[2];
		}
	}
	else {
		VertexBufferClass::WriteLockClass l(this);
		VertexFormatXYZN* verts=(VertexFormatXYZN*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].nx=(*norm)[0];
			verts[v].ny=(*norm)[1];
			verts[v].nz=(*norm++)[2];
		}
	}
}

void DX9VertexBufferClass::Copy(const Vector3* loc, const Vector3* norm, const Vector2* uv, const Vector4* diffuse, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(norm);
	WWASSERT(uv);
	WWASSERT(diffuse);
	WWASSERT(count<=VertexCount);
	WWASSERT(FVF_Info().Get_FVF()==DX9_FVF_XYZNDUV1);

	if (first_vertex) {
		VertexBufferClass::AppendLockClass l(this,first_vertex,count);
		VertexFormatXYZNDUV1* verts=(VertexFormatXYZNDUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].nx=(*norm)[0];
			verts[v].ny=(*norm)[1];
			verts[v].nz=(*norm++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
			verts[v].diffuse=Convert_Color(diffuse[v]);
		}
	}
	else {
		VertexBufferClass::WriteLockClass l(this);
		VertexFormatXYZNDUV1* verts=(VertexFormatXYZNDUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].nx=(*norm)[0];
			verts[v].ny=(*norm)[1];
			verts[v].nz=(*norm++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
			verts[v].diffuse=Convert_Color(diffuse[v]);
		}
	}
}

void DX9VertexBufferClass::Copy(const Vector3* loc, const Vector2* uv, const Vector4* diffuse, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(uv);
	WWASSERT(diffuse);
	WWASSERT(count<=VertexCount);
	WWASSERT(FVF_Info().Get_FVF()==DX9_FVF_XYZDUV1);

	if (first_vertex) {
		VertexBufferClass::AppendLockClass l(this,first_vertex,count);
		VertexFormatXYZDUV1* verts=(VertexFormatXYZDUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
			verts[v].diffuse=Convert_Color(diffuse[v]);
		}
	}
	else {
		VertexBufferClass::WriteLockClass l(this);
		VertexFormatXYZDUV1* verts=(VertexFormatXYZDUV1*)l.Get_Vertex_Array();
		for (unsigned v=0;v<count;++v) {
			verts[v].x=(*loc)[0];
			verts[v].y=(*loc)[1];
			verts[v].z=(*loc++)[2];
			verts[v].u1=(*uv)[0];
			verts[v].v1=(*uv++)[1];
			verts[v].diffuse=Convert_Color(diffuse[v]);
		}
	}
}

// ----------------------------------------------------------------------------

DynamicVBAccessClass::DynamicVBAccessClass(unsigned t,unsigned fvf,unsigned short vertex_count_)
	:
	Type(t),
	FVFInfo(_DynamicFVFInfo),
	VertexCount(vertex_count_),
	VertexBuffer(nullptr)
{
	WWASSERT(fvf==dynamic_fvf_type);
	WWASSERT(Type==BUFFER_TYPE_DYNAMIC_DX9EX || Type==BUFFER_TYPE_DYNAMIC_SORTING);

	if (Type==BUFFER_TYPE_DYNAMIC_DX9EX) {
		Allocate_DX9EX_Dynamic_Buffer();
	}
	else {
		Allocate_Sorting_Dynamic_Buffer();
	}
}

DynamicVBAccessClass::~DynamicVBAccessClass()
{
	if (Type==BUFFER_TYPE_DYNAMIC_DX9EX) {
		_DynamicDX9EXVertexBufferInUse=false;
		_DynamicDX9EXVertexBufferOffset+=(unsigned) VertexCount;
	}
	else {
		_DynamicSortingVertexArrayInUse=false;
		_DynamicSortingVertexArrayOffset+=VertexCount;
	}

	REF_PTR_RELEASE (VertexBuffer);
}

void DynamicVBAccessClass::_Deinit()
{
	WWASSERT ((_DynamicDX9EXVertexBuffer == nullptr) || (_DynamicDX9EXVertexBuffer->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicDX9EXVertexBuffer);
	_DynamicDX9EXVertexBufferInUse=false;
	_DynamicDX9EXVertexBufferSize=DEFAULT_VB_SIZE;
	_DynamicDX9EXVertexBufferOffset=0;

	WWASSERT ((_DynamicSortingVertexArray == nullptr) || (_DynamicSortingVertexArray->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicSortingVertexArray);
	WWASSERT(!_DynamicSortingVertexArrayInUse);
	_DynamicSortingVertexArrayInUse=false;
	_DynamicSortingVertexArraySize=0;
	_DynamicSortingVertexArrayOffset=0;
}

void DynamicVBAccessClass::Allocate_DX9EX_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicDX9EXVertexBufferInUse);
	_DynamicDX9EXVertexBufferInUse=true;

	// If requesting more vertices than dynamic vertex buffer can fit, delete the vb
	// and adjust the size to the new count.
	if (VertexCount>_DynamicDX9EXVertexBufferSize) {
		REF_PTR_RELEASE(_DynamicDX9EXVertexBuffer);
		_DynamicDX9EXVertexBufferSize=VertexCount;
		if (_DynamicDX9EXVertexBufferSize<DEFAULT_VB_SIZE) _DynamicDX9EXVertexBufferSize=DEFAULT_VB_SIZE;
	}

	// Create a new vb if one doesn't exist currently
	if (!_DynamicDX9EXVertexBuffer) {
		_DynamicDX9EXVertexBuffer=NEW_REF(DX9VertexBufferClass,(
			dynamic_fvf_type,
			_DynamicDX9EXVertexBufferSize,
			DX9VertexBufferClass::USAGE_DYNAMIC));
		_DynamicDX9EXVertexBufferOffset=0;
	}

	// Any room at the end of the buffer?
	if (((unsigned)VertexCount+_DynamicDX9EXVertexBufferOffset)>_DynamicDX9EXVertexBufferSize) {
		_DynamicDX9EXVertexBufferOffset=0;
	}

	REF_PTR_SET(VertexBuffer,_DynamicDX9EXVertexBuffer);
	VertexBufferOffset=_DynamicDX9EXVertexBufferOffset;
}

void DynamicVBAccessClass::Allocate_Sorting_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicSortingVertexArrayInUse);
	_DynamicSortingVertexArrayInUse=true;

	unsigned new_vertex_count=_DynamicSortingVertexArrayOffset+VertexCount;
	WWASSERT(new_vertex_count<65536);
	if (new_vertex_count>_DynamicSortingVertexArraySize) {
		REF_PTR_RELEASE(_DynamicSortingVertexArray);
		_DynamicSortingVertexArraySize=new_vertex_count;
		if (_DynamicSortingVertexArraySize<DEFAULT_VB_SIZE) _DynamicSortingVertexArraySize=DEFAULT_VB_SIZE;
	}

	if (!_DynamicSortingVertexArray) {
		_DynamicSortingVertexArray=NEW_REF(SortingVertexBufferClass,(_DynamicSortingVertexArraySize));
		_DynamicSortingVertexArrayOffset=0;
	}

	REF_PTR_SET(VertexBuffer,_DynamicSortingVertexArray);
	VertexBufferOffset=_DynamicSortingVertexArrayOffset;
}

// ----------------------------------------------------------------------------

DynamicVBAccessClass::WriteLockClass::WriteLockClass(DynamicVBAccessClass* dynamic_vb_access_)
	:
	DynamicVBAccess(dynamic_vb_access_)
{
	switch (DynamicVBAccess->Get_Type()) {
	case BUFFER_TYPE_DYNAMIC_DX9EX:
		WWASSERT(_DynamicDX9EXVertexBuffer);
		// Lock with discard contents if the buffer offset is zero
		WWASSERT(SUCCEEDED(static_cast<DX9VertexBufferClass*>(DynamicVBAccess->VertexBuffer)->Get_DX9_Vertex_Buffer()->Lock(
			DynamicVBAccess->VertexBufferOffset*_DynamicDX9EXVertexBuffer->FVF_Info().Get_FVF_Size(),
			DynamicVBAccess->Get_Vertex_Count()*DynamicVBAccess->VertexBuffer->FVF_Info().Get_FVF_Size(),
			(void**)&Vertices,
			D3DLOCK_NOSYSLOCK | (!DynamicVBAccess->VertexBufferOffset ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE))));
		break;
	case BUFFER_TYPE_DYNAMIC_SORTING:
		Vertices=static_cast<SortingVertexBufferClass*>(DynamicVBAccess->VertexBuffer)->VertexBuffer;
		Vertices+=DynamicVBAccess->VertexBufferOffset;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

DynamicVBAccessClass::WriteLockClass::~WriteLockClass()
{
	switch (DynamicVBAccess->Get_Type()) {
	case BUFFER_TYPE_DYNAMIC_DX9EX:
		WWASSERT(SUCCEEDED(static_cast<DX9VertexBufferClass*>(DynamicVBAccess->VertexBuffer)->Get_DX9_Vertex_Buffer()->Unlock()));
		break;
	case BUFFER_TYPE_DYNAMIC_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
}

void DynamicVBAccessClass::_Reset(bool frame_changed)
{
	_DynamicSortingVertexArrayOffset=0;
	if (frame_changed) _DynamicDX9EXVertexBufferOffset=0;
}

unsigned short DynamicVBAccessClass::Get_Default_Vertex_Count()
{
	return _DynamicDX9EXVertexBufferSize;
}
