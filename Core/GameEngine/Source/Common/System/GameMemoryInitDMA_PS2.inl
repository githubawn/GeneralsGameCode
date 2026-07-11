/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// TheSuperHackers @build githubawn 11/07/2026 PS2-only DMA pool sizing (see
// docs/ps2-port-plan.md). GameMemoryInitDMA_GeneralsMD.inl's initialCount
// values reserve one contiguous blob per pool IMMEDIATELY at
// initMemoryManager() time (MemoryPool::init() -> createBlob(initial)),
// before any GlobalData/INI/map loading -- confirmed by reading the code,
// not assumed. Summed across all 7 pools that table commits ~44 MB upfront,
// tuned for a PC with hundreds of MB free. initialAllocationCount here is
// cut to roughly 1/8th; overflowAllocationCount (the growth increment used
// only if a pool actually runs out, not pre-allocated) is left unchanged
// as a safety net. This is pure capacity tuning, not a behavior change --
// pools still grow via overflow blobs if the lower initial count isn't
// enough, at the cost of some extra allocation-time fragmentation, which is
// an acceptable trade during bring-up while we measure real requirements.
static const PoolInitRec DefaultDMA[] =
{
	//          name, allocSize, initialCount, overflowCount
	{   "dmaPool_16",        16,        16250,         10000 },
	{   "dmaPool_32",        32,        31250,         10000 },
	{   "dmaPool_64",        64,        12500,         10000 },
	{  "dmaPool_128",       128,        10000,         10000 },
	{  "dmaPool_256",       256,         2500,          5000 },
	{  "dmaPool_512",       512,         2000,          5000 },
	{ "dmaPool_1024",      1024,          750,          1024 }
};
