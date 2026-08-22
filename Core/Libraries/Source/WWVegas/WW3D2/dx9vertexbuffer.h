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
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *              TheSuperHackers @feature Full clone of dx8vertexbuffer.h targeting             *
 *              D3D9Ex, adapted from the proven origin/dx9-test-clean port. Mutually            *
 *              exclusive with dx8vertexbuffer.h/.cpp at the CMake level -- this file           *
 *              is only ever compiled when GGC_RENDER_BACKEND is dx9ex, so duplicating          *
 *              DynamicVBAccessClass/SortingVertexBufferClass here (rather than sharing         *
 *              dx8vertexbuffer.h's copies) is intentional, not an oversight.                   *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "WWDebug/wwdebug.h"
#include "dx9fvf.h"
#include "vertexbufferclass.h"
#include "bufferusagetype.h"

// TheSuperHackers @refactor dynamic_fvf_type and DynamicVBAccessClass moved
// to vertexbufferclass.h (included above) -- backend-agnostic, shared with
// dx8vertexbuffer.h rather than duplicated. Only Allocate_Backend_Dynamic_Buffer()'s
// definition (in dx9vertexbuffer.cpp) is DX9Ex-specific.

class SortingRendererClass;
class Vector2;
class Vector3;
class Vector4;
class StringClass;
class DX9VertexBufferClass;
struct IDirect3DVertexBuffer9;

/**
** DX9VertexBufferClass
** This class wraps a D3D9Ex vertex buffer. Use the lock objects to modify or
** append to the vertex buffer. D3D9Ex forbids D3DPOOL_MANAGED entirely, so
** unlike DX8VertexBufferClass this always creates with D3DPOOL_DEFAULT
** regardless of the usage flag.
*/
class DX9VertexBufferClass : public VertexBufferClass
{
	W3DMPO_CODE(DX9VertexBufferClass)
protected:
	virtual ~DX9VertexBufferClass() override;
public:
	enum UsageType {
		USAGE_DEFAULT=0,
		USAGE_DYNAMIC=1,
		USAGE_SOFTWAREPROCESSING=2,
		USAGE_NPATCHES=4
	};

	DX9VertexBufferClass(unsigned FVF, unsigned short VertexCount, UsageType usage=USAGE_DEFAULT);
	DX9VertexBufferClass(const Vector3* vertices, const Vector3* normals, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);
	DX9VertexBufferClass(const Vector3* vertices, const Vector3* normals, const Vector4* diffuse, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);
	DX9VertexBufferClass(const Vector3* vertices, const Vector4* diffuse, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);
	DX9VertexBufferClass(const Vector3* vertices, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);

	IDirect3DVertexBuffer9* Get_DX9_Vertex_Buffer() { return VertexBuffer; }

	void Copy(const Vector3* loc, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector2* uv, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector3* norm, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector3* norm, const Vector2* uv, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector3* norm, const Vector2* uv, const Vector4* diffuse, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector2* uv, const Vector4* diffuse, unsigned first_vertex, unsigned count);

protected:
	IDirect3DVertexBuffer9*		VertexBuffer;

	void Create_Vertex_Buffer(UsageType usage);
};


/**
** SortingVertexBufferClass
** This class acts as a vertex buffer for the vertices that need to be passed to alpha renderer.
*/
class SortingVertexBufferClass : public VertexBufferClass
{
	W3DMPO_CODE(SortingVertexBufferClass)

	friend SortingRendererClass;
	friend VertexBufferClass::WriteLockClass;
	friend VertexBufferClass::AppendLockClass;
	friend DynamicVBAccessClass::WriteLockClass;

	VertexFormatXYZNDUV2* VertexBuffer;

protected:
	virtual ~SortingVertexBufferClass() override;
public:
	SortingVertexBufferClass(unsigned short VertexCount);
};
