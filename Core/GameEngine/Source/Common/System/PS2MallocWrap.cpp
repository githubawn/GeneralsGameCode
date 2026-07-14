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

// TheSuperHackers @build githubawn 13/07/2026 PS2-only TEMP diagnostic.
// GameMemory.cpp and StubD3D8Device.cpp both live-track their own
// allocations, but only see what goes through their own specific call
// point (operator new / AllocScratch's std::calloc) -- both of those
// funnel to malloc/calloc/realloc under the hood anyway (GlobalAlloc is a
// thin shim over malloc, see win32_shims/windows.h), and this measured
// ~44MB of live mallinfo() usage that neither of them could see, meaning
// something else -- third-party code (SDL3, zlib, pthread-embedded) or
// engine code that calls malloc directly (e.g. WWLib/TARGA.cpp's TGA
// image buffers) -- is allocating outside both of those choke points.
// This file wraps the actual bottom-of-the-stack allocator via the
// linker's --wrap feature (see cmake/toolchains/ps2.cmake) to catch
// literally everything in one net, including allocations GameMemory.cpp
// and StubD3D8Device.cpp already account for -- the live total here is
// expected to roughly match mallinfo()'s total, not just the unexplained
// remainder; the per-size breakdown is what actually identifies the gap
// (cross-reference against the already-known DMA pool bucket sizes and
// texture scratch sizes to see what's left over).
#if defined(__PS2__)

#include <cstdio>
#include <cstdlib>

extern "C" void* __real_malloc(size_t size);
extern "C" void* __real_calloc(size_t nmemb, size_t size);
extern "C" void* __real_realloc(void* ptr, size_t size);
extern "C" void  __real_free(void* ptr);

namespace {

// Plain fixed-size arrays, not a container -- a container's own first-use
// storage allocation would call malloc itself, recursing straight back
// into __wrap_malloc (the exact reentrancy trap GameMemory.cpp's raw-block
// live tracking and StubD3D8Device.cpp's scratch live tracking both
// already hit and fixed the same way).
const int PS2_MALLOCWRAP_LIVE_CAPACITY = 32768;
void * s_livePtrs[PS2_MALLOCWRAP_LIVE_CAPACITY];
size_t s_liveSizes[PS2_MALLOCWRAP_LIVE_CAPACITY];
int s_liveCount = 0;
size_t s_liveBytes = 0;
size_t s_everBytes = 0;

// TheSuperHackers @build githubawn 13/07/2026 TEMP diagnostic: same poor-
// man's stack scan technique already used in GameMemory.cpp for the
// 349,680-byte raw-block investigation (that one turned out to be transient
// DDS-buffer churn, not the live culprit -- __builtin_return_address(N>0)
// is unusable here too, same MIPS/R5900 limitation). Targeting the two
// most-repeated/most-suspicious sizes from the live breakdown: 32768 (a
// round power-of-2, smells like a fixed buffer size e.g. a pthread stack
// or I/O buffer, not image/object data) and 2097152 (exactly 2MB, same
// suspicion). Logged once per matching allocation to
// host:ps2_mallocwrap_stackscan.txt; addr2line the .text-range hits
// against the unstripped ELF afterward.
void StackScanIfInteresting(size_t bytes)
{
	// TheSuperHackers @build githubawn 13/07/2026 First pass (32768/2097152)
	// resolved to StubD3D8Device::CreateTexture/AllocScratch (already-known
	// texture scratch). Second pass (72096/17420) resolved to
	// StubD3D8Device::CreateVertexBuffer/AllocScratch (also already-known
	// scratch) and INI::initFromINIMulti's ModelConditionInfo vector growth
	// via DynamicMemoryAllocator (confirmed: matches the already-known
	// 17408-byte raw-block live entry exactly, the 12-byte difference is
	// just MemoryPoolSingleBlock::calcRawBlockSize's header overhead). Both
	// rounds were dead ends already counted in the scratch/raw-block
	// totals, not new. Retargeting at the single largest unexplained live
	// allocation: 10,528,000 bytes, count=1.
	if (bytes != 10528000)
	{
		return;
	}
	register unsigned long spReg asm("sp");
	unsigned long * sp = (unsigned long *)spReg;
	FILE * fp = fopen("host:ps2_mallocwrap_stackscan.txt", "a");
	if (fp != nullptr)
	{
		fprintf(fp, "--- stack scan for %u-byte alloc (sp=%p) ---\n", (unsigned)bytes, (void*)sp);
		for (int i = 0; i < 512; i++)
		{
			unsigned long val = sp[i];
			if (val >= 0x00100000UL && val <= 0x00b30000UL)
			{
				fprintf(fp, "  sp[%d] = 0x%08lx\n", i, val);
			}
		}
		fclose(fp);
	}
}

void Track(void * ptr, size_t bytes)
{
	if (ptr == nullptr)
	{
		return;
	}
	StackScanIfInteresting(bytes);
	s_everBytes += bytes;
	for (int i = 0; i < s_liveCount; i++)
	{
		if (s_livePtrs[i] == nullptr)
		{
			s_livePtrs[i] = ptr;
			s_liveSizes[i] = bytes;
			s_liveBytes += bytes;
			return;
		}
	}
	if (s_liveCount < PS2_MALLOCWRAP_LIVE_CAPACITY)
	{
		s_livePtrs[s_liveCount] = ptr;
		s_liveSizes[s_liveCount] = bytes;
		s_liveCount++;
		s_liveBytes += bytes;
	}
	// else: capacity exceeded, silently skip (diagnostic-only).
}

void Untrack(void * ptr)
{
	if (ptr == nullptr)
	{
		return;
	}
	for (int i = 0; i < s_liveCount; i++)
	{
		if (s_livePtrs[i] == ptr)
		{
			s_liveBytes -= s_liveSizes[i];
			s_livePtrs[i] = nullptr;
			return;
		}
	}
}

} // namespace

extern "C" void* __wrap_malloc(size_t size)
{
	void* p = __real_malloc(size);
	Track(p, size);
	return p;
}

extern "C" void* __wrap_calloc(size_t nmemb, size_t size)
{
	void* p = __real_calloc(nmemb, size);
	Track(p, nmemb * size);
	return p;
}

extern "C" void* __wrap_realloc(void* ptr, size_t size)
{
	Untrack(ptr);
	void* p = __real_realloc(ptr, size);
	Track(p, size);
	return p;
}

extern "C" void __wrap_free(void* ptr)
{
	Untrack(ptr);
	__real_free(ptr);
}

size_t GetPS2MallocWrapLiveBytes()
{
	return s_liveBytes;
}

size_t GetPS2MallocWrapEverBytes()
{
	return s_everBytes;
}

void DumpPS2MallocWrapLiveBreakdown(FILE * fp)
{
	static const int MAX_DISTINCT = 4096;
	static size_t distinctSizes[MAX_DISTINCT];
	static int distinctCounts[MAX_DISTINCT];
	int numDistinct = 0;
	for (int i = 0; i < s_liveCount; i++)
	{
		if (s_livePtrs[i] == nullptr)
		{
			continue;
		}
		size_t sz = s_liveSizes[i];
		int j = 0;
		for (; j < numDistinct; j++)
		{
			if (distinctSizes[j] == sz)
			{
				distinctCounts[j]++;
				break;
			}
		}
		if (j == numDistinct && numDistinct < MAX_DISTINCT)
		{
			distinctSizes[numDistinct] = sz;
			distinctCounts[numDistinct] = 1;
			numDistinct++;
		}
	}
	for (int j = 0; j < numDistinct; j++)
	{
		fprintf(fp, "live size=%u count=%d totalBytes=%u\n",
			(unsigned)distinctSizes[j], distinctCounts[j], (unsigned)(distinctSizes[j] * distinctCounts[j]));
	}
}

#endif // __PS2__
