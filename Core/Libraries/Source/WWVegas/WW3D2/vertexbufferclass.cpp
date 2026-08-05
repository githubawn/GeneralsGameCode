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

// TheSuperHackers @refactor VertexBufferClass's bookkeeping (ctor/dtor,
// counters) split out of dx8vertexbuffer.cpp so it keeps existing even when
// that file is excluded from a non-DX8 build. This file must stay
// D3D-header-free: WriteLockClass/AppendLockClass's constructors are NOT
// here, because their bodies switch on Type() and cast to the concrete
// DX8-specific class -- that dispatch logic is backend-specific and belongs
// duplicated in each backend's own file (see dx8vertexbuffer.cpp), not here.

#include "vertexbufferclass.h"
#include "fvfinfoclass.h"

static int _VertexBufferCount;
static int _VertexBufferTotalVertices;
static int _VertexBufferTotalSize;

VertexBufferClass::VertexBufferClass(unsigned type_, unsigned FVF, unsigned short vertex_count_)
	:
	VertexCount(vertex_count_),
	type(type_),
	engine_refs(0)
{
	WWASSERT(VertexCount);
	WWASSERT(FVF != 0);
	fvf_info=W3DNEW FVFInfoClass(FVF);

	_VertexBufferCount++;
	_VertexBufferTotalVertices+=VertexCount;
	_VertexBufferTotalSize+=VertexCount*fvf_info->Get_FVF_Size();
}

VertexBufferClass::~VertexBufferClass()
{
	_VertexBufferCount--;
	_VertexBufferTotalVertices-=VertexCount;
	_VertexBufferTotalSize-=VertexCount*fvf_info->Get_FVF_Size();

	delete fvf_info;
}

unsigned VertexBufferClass::Get_Total_Buffer_Count()
{
	return _VertexBufferCount;
}

unsigned VertexBufferClass::Get_Total_Allocated_Vertices()
{
	return _VertexBufferTotalVertices;
}

unsigned VertexBufferClass::Get_Total_Allocated_Memory()
{
	return _VertexBufferTotalSize;
}

void VertexBufferClass::Add_Engine_Ref() const
{
	engine_refs++;
}

void VertexBufferClass::Release_Engine_Ref() const
{
	engine_refs--;
	WWASSERT(engine_refs>=0);
}
