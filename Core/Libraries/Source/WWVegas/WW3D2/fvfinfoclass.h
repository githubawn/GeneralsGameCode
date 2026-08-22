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

// TheSuperHackers @refactor FVFInfoClass split out of dx8fvf.h so it can be
// included (by VertexBufferClass-consuming code, including a non-DX8
// backend) without pulling in <d3d8.h>. WW3D_DP_MAXTEXCOORD mirrors
// D3DDP_MAXTEXCOORD's value (8) -- identical in both D3D8 and D3D9.

#include "WWLib/always.h"
#ifdef WWDEBUG
#include "WWDebug/wwdebug.h"
#endif

class StringClass;

enum { WW3D_DP_MAXTEXCOORD = 8 };

// TheSuperHackers @refactor Backend-neutral mirror of dx8fvf.h's/dx9fvf.h's
// DX8_FVF_*/DX9_FVF_* enums. Both are the same D3DFVF_* bit combinations --
// the D3DFVF_ macro values are identical between the D3D8 and D3D9 SDKs --
// so callers that only need to pass an FVF to Create_Vertex_Buffer() (see
// vertexbufferclass.h) can use these literal values instead of pulling in
// dx8fvf.h/<d3d8.h> or dx9fvf.h/<d3d9.h> just to name one.
enum {
	WW3D_FVF_XYZ        = 0x002,                         // D3DFVF_XYZ
	WW3D_FVF_XYZRHW     = 0x004,                         // D3DFVF_XYZRHW
	WW3D_FVF_NORMAL     = 0x010,                         // D3DFVF_NORMAL
	WW3D_FVF_DIFFUSE    = 0x040,                         // D3DFVF_DIFFUSE
	WW3D_FVF_TEX1       = 0x100,                         // D3DFVF_TEX1
	WW3D_FVF_TEX2       = 0x200,                         // D3DFVF_TEX2
	WW3D_FVF_XYZNUV1    = 0x002|0x010|0x100,              // D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1
	WW3D_FVF_XYZNUV2    = 0x002|0x010|0x200,              // D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2
	WW3D_FVF_XYZNDUV1   = 0x002|0x010|0x100|0x040,        // D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1|D3DFVF_DIFFUSE
	WW3D_FVF_XYZNDUV2   = 0x002|0x010|0x200|0x040,        // D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2|D3DFVF_DIFFUSE
	WW3D_FVF_XYZDUV1    = 0x002|0x100|0x040,              // D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_DIFFUSE
	WW3D_FVF_XYZDUV2    = 0x002|0x200|0x040               // D3DFVF_XYZ|D3DFVF_TEX2|D3DFVF_DIFFUSE
};

// FVF info class can be created for any legal FVF. It constructs information
// of offsets to various elements in the vertex buffer.

class FVFInfoClass
{
	W3DMPO_CODE(FVFInfoClass)

	mutable unsigned						FVF;
	mutable unsigned						fvf_size;

	unsigned							location_offset;
	unsigned							normal_offset;
	unsigned							blend_offset;
	unsigned							texcoord_offset[WW3D_DP_MAXTEXCOORD];
	unsigned							diffuse_offset;
	unsigned							specular_offset;
public:
	FVFInfoClass(unsigned FVF);

	unsigned Get_Location_Offset() const { return location_offset; }
	unsigned Get_Normal_Offset() const { return normal_offset; }
#ifdef WWDEBUG
	inline unsigned Get_Tex_Offset(unsigned int n) const { WWASSERT(n<WW3D_DP_MAXTEXCOORD); return texcoord_offset[n]; }
#else
	unsigned Get_Tex_Offset(unsigned int n) const { return texcoord_offset[n]; }
#endif

	unsigned Get_Diffuse_Offset() const { return diffuse_offset; }
	unsigned Get_Specular_Offset() const { return specular_offset; }
	unsigned Get_FVF() const { return FVF; }
	unsigned Get_FVF_Size() const { return fvf_size; }

	void Get_FVF_Name(StringClass& fvfname) const;	// For debug purposes

	// for enabling vertex shaders
	void Set_FVF(unsigned fvf) const { FVF=fvf; }
	void Set_FVF_Size(unsigned size) const { fvf_size=size; }
};
