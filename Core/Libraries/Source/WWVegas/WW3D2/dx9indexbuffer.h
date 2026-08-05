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

// TheSuperHackers @feature DX9Ex sibling of DX8IndexBufferClass.
// dx8indexbuffer.h is already D3D8-header-free (unlike dx8vertexbuffer.h,
// which needed splitting -- see vertexbufferclass.h), so it's safe to
// include directly here.

#include "dx8indexbuffer.h"
#include "bufferusagetype.h"

struct IDirect3DIndexBuffer9;

/**
** DX9IndexBufferClass
** D3D9Ex forbids D3DPOOL_MANAGED entirely, so this always creates with
** D3DPOOL_DEFAULT regardless of the usage flag (see DX9VertexBufferClass).
*/
class DX9IndexBufferClass : public IndexBufferClass
{
public:
	enum UsageType {
		USAGE_DEFAULT=0,
		USAGE_DYNAMIC=1,
	};

	DX9IndexBufferClass(unsigned short index_count, UsageType usage=USAGE_DEFAULT);
	virtual ~DX9IndexBufferClass() override;

	IDirect3DIndexBuffer9 * Get_DX9_Index_Buffer() { return index_buffer; }

	// Opaque lock/unlock so dx8indexbuffer.cpp's shared WriteLockClass/
	// AppendLockClass dispatch never needs <d3d9.h> itself.
	void * Lock_Raw(unsigned offset_bytes, unsigned size_bytes, unsigned flags);
	void Unlock_Raw();

private:
	IDirect3DIndexBuffer9 * index_buffer;
};
