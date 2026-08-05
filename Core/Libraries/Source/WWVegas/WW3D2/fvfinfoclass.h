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
