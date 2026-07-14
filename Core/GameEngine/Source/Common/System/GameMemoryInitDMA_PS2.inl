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
//
// TheSuperHackers @build githubawn 13/07/2026 Added the 5 buckets below
// (1088/2176/4064/4352/8704) after measurement, not guessing: a host:
// diagnostic logging every DynamicMemoryAllocator::allocateBytesDoNotZero-
// Implementation() call that fell through to MemoryPoolSingleBlock::
// rawAllocateSingleBlock() (findPoolForSize() returns null for anything
// bigger than the previous max bucket, dmaPool_1024) found 6,154 individual
// "raw block" allocations totalling ~126MB ever-allocated -- more than
// textures (~28MB) and every named pool combined (~28MB) put together, and
// the dominant contributor to shell-map loading hitting ERROR_OUT_OF_MEMORY
// at ~113MB of the 128MB dev-console budget. 82% of those 6,154 allocations
// (5,065) were exactly one of these 5 sizes -- each just barely over the
// old 1024-byte ceiling, so every one paid for an individual calloc() call
// (and its heap-fragmentation cost) instead of a cheap pooled blob slice.
// findPoolForSize() walks this table in order and returns the first bucket
// whose allocSize >= the request, so these MUST stay sorted ascending.
// Modest initial counts (blobs are still cheap to grow via overflow, which
// itself does one calloc per *batch* of blocks, not per allocation) --
// this is about eliminating one-off raw allocations, not pre-committing
// the full peak count upfront.
// TheSuperHackers @build githubawn 13/07/2026 Second tuning pass, again
// from direct measurement, not guessing: dumpAllPoolsPS2's per-pool
// used/total breakdown at the shell-map screen showed several of these
// buckets had already grown past their initialCount via one or more
// overflow blobs, and the overflow blob(s) themselves were mostly empty
// (e.g. dmaPool_128: initial 10000 + one 10000-block overflow = 20000
// total, but only 12331 actually used -- ~960KB wasted in that one
// overflow blob alone; dmaPool_512 similarly: 2000+5000+5000=12000 total
// for 8438 used, ~1.8MB wasted). Raised initialCount enough to cover
// measured usage without needing any overflow growth at the shell-map
// screen, and shrank overflowCount so a real match that does need more
// doesn't over-commit a huge extra blob the way the old 1:1 initial/
// overflow pairing did. Not a guess -- see docs/ps2-port-plan.md for the
// full ~27.8MB total reserved-but-unused figure this and the
// GameMemoryInit.cpp PS2PoolOverrides fix are both chasing.
static const PoolInitRec DefaultDMA[] =
{
	//          name, allocSize, initialCount, overflowCount
	{   "dmaPool_16",        16,        16250,         10000 },
	{   "dmaPool_32",        32,        31250,         10000 },
	{   "dmaPool_64",        64,        12500,         10000 },
	{  "dmaPool_128",       128,        16000,          4000 },
	{  "dmaPool_256",       256,         2500,          5000 },
	{  "dmaPool_512",       512,        11000,          3000 },
	{ "dmaPool_1024",      1024,         1600,           500 },
	{ "dmaPool_1088",      1088,          600,           400 },
	{ "dmaPool_2176",      2176,         1000,           400 },
	{ "dmaPool_4064",      4064,          950,           300 },
	{ "dmaPool_4352",      4352,          450,           250 },
	{ "dmaPool_8704",      8704,          420,           200 }
};
