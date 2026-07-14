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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: MemoryInit.cpp
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: MemoryInit.cpp
//
// Created:   Steven Johnson, August 2001
//
// Desc:      Memory manager
//
// ----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

// SYSTEM INCLUDES

// USER INCLUDES
#include "Lib/BaseType.h"
#include "Common/GameMemory.h"

struct PoolSizeRec
{
	const char* name;
	Int initial;
	Int overflow;
};

// TheSuperHackers @build githubawn 11/07/2026 PS2 gets its own DMA pool
// sizing (see GameMemoryInitDMA_PS2.inl and docs/ps2-port-plan.md) -- the
// stock tables reserve ~44 MB upfront, unconditionally, before any asset
// loads. The per-type object Pools_* table is left as-is per game (it's
// small object-count tuning, not raw DMA byte budgets).
#if defined(__PS2__)
#include "GameMemoryInitDMA_PS2.inl"
#if RTS_GENERALS
#include "GameMemoryInitPools_Generals.inl"
#elif RTS_ZEROHOUR
#include "GameMemoryInitPools_GeneralsMD.inl"
#endif
#elif RTS_GENERALS
#include "GameMemoryInitDMA_Generals.inl"
#include "GameMemoryInitPools_Generals.inl"
#elif RTS_ZEROHOUR
#include "GameMemoryInitDMA_GeneralsMD.inl"
#include "GameMemoryInitPools_GeneralsMD.inl"
#endif

//-----------------------------------------------------------------------------
void userMemoryManagerGetDmaParms(Int *numSubPools, const PoolInitRec **pParms)
{
	*numSubPools = ARRAY_SIZE(DefaultDMA);
	*pParms = DefaultDMA;
}

#if defined(__PS2__)
// TheSuperHackers @build githubawn 13/07/2026 PS2-only right-sizing for a
// handful of named per-class pools whose PC-era `PoolSizes[]` initial
// counts reserve a single upfront blob far larger than what's ever used --
// found by a linker --wrap malloc/calloc live-tracker (PS2MallocWrap.cpp)
// that caught a 10.5MB single allocation (MeshClass's blob) invisible to
// mallinfo-vs-pool-live-bytes accounting, since a pool's own "live bytes"
// report (MemoryPoolFactory::dumpAllPoolsPS2) is used*allocSize, not the
// blob's actual reserved size -- an over-provisioned blob is real,
// resident memory mallinfo() sees but no per-pool report caught until this
// was traced. At the shell-map screen: MeshClass reserves 14000 slots for
// 1037 used (740B each, ~9.6MB wasted alone); MeshMatDescClass/
// MeshModelClass/VertexMaterialClass/MaterialInfoClass/DynD3DMATERIAL8
// reserve 6000-8192 for single-digit usage; MotionChannelClass/
// ShareBufferClass reserve 16384/32768 for 16/54 used. Summed across every
// pool (not just these), total reserved-but-unused waste measured at
// ~27.8MB -- see docs/ps2-port-plan.md. Deliberately a small override
// list layered on top of the shared PoolSizes[] table (not a forked copy
// of the whole multi-hundred-entry table, which would drift out of sync
// with balance changes made for other platforms) -- overflowAllocation-
// Count is untouched, so a real match needing more than this still grows
// via cheap overflow blobs exactly like before, just without the huge
// unconditional upfront reservation. Sized with real headroom (not bare
// shell-map usage) since actual matches will reference more distinct
// meshes/materials than the menu shell map does.
struct PS2PoolSizeOverride
{
	const char* name;
	Int initial;
};
static const PS2PoolSizeOverride PS2PoolOverrides[] =
{
	{ "MeshClass",              1500 },
	{ "MeshMatDescClass",         80 },
	{ "MeshModelClass",           80 },
	{ "VertexMaterialClass",     150 },
	{ "MotionChannelClass",      250 },
	{ "ShareBufferClass",        250 },
	{ "DynD3DMATERIAL8",         150 },
	{ "W3DDisplayString",        350 },
	{ "HLodClass",               500 },
	{ "FontCharsBuffer",          12 },
	{ "MaterialInfoClass",        80 },
	{ "W3DGameWindow",           300 },
	{ nullptr, 0 },
};
#endif

//-----------------------------------------------------------------------------
void userMemoryAdjustPoolSize(const char *poolName, Int& initialAllocationCount, Int& overflowAllocationCount)
{
	if (initialAllocationCount > 0)
		return;

	for (const PoolSizeRec* p = PoolSizes; p->name != nullptr; ++p)
	{
		if (strcmp(p->name, poolName) == 0)
		{
			initialAllocationCount = p->initial;
			overflowAllocationCount = p->overflow;
#if defined(__PS2__)
			for (const PS2PoolSizeOverride* o = PS2PoolOverrides; o->name != nullptr; ++o)
			{
				if (strcmp(o->name, poolName) == 0)
				{
					initialAllocationCount = o->initial;
					break;
				}
			}
#endif
			return;
		}
	}

	DEBUG_CRASH(("Initial size for pool %s not found -- you should add it to MemoryInit.cpp",poolName));
}

//-----------------------------------------------------------------------------
static Int roundUpMemBound(Int i)
{
	const int MEM_BOUND_ALIGNMENT = 4;

	if (i < MEM_BOUND_ALIGNMENT)
		return MEM_BOUND_ALIGNMENT;
	else
		return (i + (MEM_BOUND_ALIGNMENT-1)) & ~(MEM_BOUND_ALIGNMENT-1);
}

//-----------------------------------------------------------------------------
void userMemoryManagerInitPools()
{
	// note that we MUST use stdio stuff here, and not the normal game file system
	// (with bigfile support, etc), because that relies on memory pools, which
	// aren't yet initialized properly! so rely ONLY on straight stdio stuff here.
	// (not even AsciiString. thanks.)

	// since we're called prior to main, the cur dir might not be what
	// we expect. so do it the hard way.
	char buf[_MAX_PATH];
	::GetModuleFileName(nullptr, buf, sizeof(buf));
	if (char* pEnd = strrchr(buf, '\\'))
	{
		*pEnd = 0;
	}
	strlcat(buf, "\\Data\\INI\\MemoryPools.ini", ARRAY_SIZE(buf));

	FILE* fp = fopen(buf, "r");
	if (fp)
	{
		char poolName[256];
		int initial, overflow;
		while (fgets(buf, _MAX_PATH, fp))
		{
			if (buf[0] == ';')
				continue;
			if (sscanf(buf, "%s %d %d", poolName, &initial, &overflow ) == 3)
			{
				for (PoolSizeRec* p = PoolSizes; p->name != nullptr; ++p)
				{
					if (stricmp(p->name, poolName) == 0)
					{
						// currently, these must be multiples of 4. so round up.
						p->initial = roundUpMemBound(initial);
						p->overflow = roundUpMemBound(overflow);
						break;	// from for-p
					}
				}
			}
		}
		fclose(fp);
	}
}

