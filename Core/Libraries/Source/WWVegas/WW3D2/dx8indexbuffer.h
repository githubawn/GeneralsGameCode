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
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8indexbuffer.h                       $*
 *                                                                                             *
 *              Original Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                      $Author:: Jani_p                                                      $*
 *                                                                                             *
 *                     $Modtime:: 7/10/01 12:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 12                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "WWDebug/wwdebug.h"
#include "WWMath/sphere.h"
#include "indexbufferclass.h"

class DX8Wrapper;
class SortingRendererClass;
struct IDirect3DIndexBuffer8;
class DX8IndexBufferClass;
class SortingIndexBufferClass;

// TheSuperHackers @refactor DynamicIBAccessClass moved to indexbufferclass.h
// (included above) -- backend-agnostic, shared with dx9indexbuffer.h rather
// than duplicated. Only Allocate_Backend_Dynamic_Buffer()'s definition (in
// dx8indexbuffer.cpp) is DX8-specific.

/**
** DX8IndexBufferClass
** This class wraps a DX8 index buffer.
*/
class DX8IndexBufferClass : public IndexBufferClass
{
	W3DMPO_CODE(DX8IndexBufferClass)

	friend IndexBufferClass::WriteLockClass;
	friend IndexBufferClass::AppendLockClass;
public:
	enum UsageType {
		USAGE_DEFAULT=0,
		USAGE_DYNAMIC=1,
		USAGE_SOFTWAREPROCESSING=2,
		USAGE_NPATCHES=4
	};

	DX8IndexBufferClass(unsigned short index_count,UsageType usage=USAGE_DEFAULT);
	virtual ~DX8IndexBufferClass() override;

	void Copy(unsigned int* indices,unsigned start_index,unsigned index_count);
	void Copy(unsigned short* indices,unsigned start_index,unsigned index_count);

	IDirect3DIndexBuffer8* Get_DX8_Index_Buffer()	{ return index_buffer; }

private:
	IDirect3DIndexBuffer8*	index_buffer;		// actual dx8 index buffer
};



// TheSuperHackers @refactor SortingIndexBufferClass moved to
// indexbufferclass.h (included above) -- backend-agnostic, shared with
// dx9indexbuffer.h rather than duplicated (only its .cpp implementation
// stays duplicated, per the no-shared-bodies rule).
