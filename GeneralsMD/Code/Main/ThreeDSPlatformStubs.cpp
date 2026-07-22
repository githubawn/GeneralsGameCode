// ThreeDSPlatformStubs.cpp
// TheSuperHackers @build githubawn 14/07/2026
// New Nintendo 3DS platform helpers.

#if defined(__3DS__)
#include <cstdio>
// TheSuperHackers @feature githubawn 19/07/2026 SDL_Log is the sink used for
// the __wrap_abort fatal logging below (see that function).
#include <SDL3/SDL.h>
// TheSuperHackers @bugfix githubawn 15/07/2026 The root cause of every "boot
// works for ~8s then jumps to PC=0 / runaway unmapped reads" crash seen so
// far (NOT a stack-overflow from the RSF's exheader StackSize field, and NOT
// GetModuleFileName's buffer -- both were red herrings that happened to not
// change the outcome). Disassembly of devkitARM's own linked-in
// libctru/system/stack_adjust.c (initSystem(), called from __libctru_init
// right after __system_allocateHeaps runs) proves the *actual* C-runtime
// stack pointer is computed as:
//     sp = fake_heap_start + __stacksize__ (8-aligned)
//     fake_heap_start = sp   // bumps the heap floor past the reserved stack
// i.e. the kernel/exheader-provided 2MB "Locked" stack region set up per
// GeneralsMD/Code/Main/3ds.rsf's StackSize is NEVER used by the compiled
// binary's SP at all -- it is a completely separate, unused allocation.
// The real stack lives at the bottom of our custom heap (fake_heap_start,
// i.e. OS_HEAP_AREA_BEGIN = 0x08000000) and is sized by the weak symbol
// __stacksize__, which libctru leaves effectively unset if the application
// doesn't define its own. That left only ~30KB of real stack -- enough for
// ~8s of engine init, but GlobalData::generateExeCRC() alone needs a 64KB+
// local buffer (GlobalData.cpp's `unsigned char crcBlock[65536]`), which
// walked the SP straight past fake_heap_start (0x08000000) into totally
// unmapped memory below it. Overriding this weak symbol with a real value is
// the documented devkitARM/libctru mechanism for sizing the app's stack (the
// RSF's exheader StackSize is honored by real 3DS hardware/interactive
// menus for other purposes, but not by this crt0 startup path). Carved out
// of our 24MB custom heap below, well within its budget.
extern "C" unsigned int __stacksize__ = 4 * 1024 * 1024; // 4MB

// TheSuperHackers @build githubawn 14/07/2026 libctru's svc.h/mappable.h have
// no __cplusplus/extern "C" guards, so including them from a .cpp file gives
// their declarations C++ mangled linkage by default, which then fails to
// link against the C-linkage symbols actually compiled into libctru.a.
// Wrap the includes themselves in extern "C" to force C linkage.
extern "C"
{
#include <3ds/svc.h>
#include <3ds/allocator/mappable.h>
#include <3ds/env.h>
#include <3ds/os.h>
#include <3ds/result.h>

    extern char* fake_heap_start;
    extern char* fake_heap_end;

    // TheSuperHackers @bugfix githubawn 14/07/2026 __ctru_heap /
    // __ctru_linear_heap (the actual base addresses, as opposed to the
    // _size tunables) are non-weak globals DEFINED in libctru's own
    // allocateHeaps.o, which is still linked in (only the weak
    // __system_allocateHeaps function is overridden here, below). Other
    // libctru code reads __ctru_linear_heap/__ctru_linear_heap_size
    // directly -- most importantly libctru/source/allocator/linear.cpp,
    // which backs linearAlloc()/linearFree() (used by citro3d for GPU
    // vertex/texture buffers). An earlier version of this override used
    // local shadow variables instead of writing back to these globals,
    // leaving them at their default zero/uninitialized state: any
    // linearAlloc() call then computed addresses off a bogus zero-based
    // arena, producing near-null physical addresses. That is what was
    // crashing Azahar's JIT shortly after boot (visible in the log as
    // "Unknown GetPhysMemRegionInfo @ 0x000000", then a native access
    // violation in JIT-compiled code with no guest-side panic/break logged
    // at all). Assign the real allocation results into these globals so
    // every libctru consumer sees the correct heap layout, not just our
    // own fake_heap_start/end.
    extern u32 __ctru_heap;
    extern u32 __ctru_linear_heap;
    extern u32 __ctru_heap_size;
    extern u32 __ctru_linear_heap_size;

    // TheSuperHackers @bugfix githubawn 14/07/2026 Boot panic: libctru's
    // __system_allocateHeaps (libctru/source/system/allocateHeaps.c) is
    // supposed to be tunable via the weak __ctru_heap_size /
    // __ctru_linear_heap_size globals, but overriding just those two
    // globals from this translation unit did not take effect at the actual
    // svcControlMemory call site: the app-heap request still came out as
    // (remaining - linear_size) rather than our fixed override, even though
    // the override's value was verified correct in the linked ELF's .data
    // section via objdump. This looks like a weak-symbol/static-archive
    // linkage quirk between our object and libctru's precompiled
    // allocateHeaps.o, not a logic bug in our override itself.
    //
    // __system_allocateHeaps() is ALSO weak, so override the whole function
    // instead of just its tunables -- this removes any cross-object internal
    // variable reference and guarantees our fixed sizes are what actually
    // reaches svcControlMemory.
    //
    // TheSuperHackers @bugfix githubawn 16/07/2026 The original 24MB/32MB
    // split here (libctru's own conservative Legacy/64MB-mode caps, kept
    // deliberately small while root-causing the heap-allocation mechanism
    // itself) turned out to be genuinely too small once boot got far enough
    // to reach it: TheThingFactory's Object.ini parse (the largest single
    // INI dataset in the game -- every unit/building template) throws
    // ERROR_OUT_OF_MEMORY (confirmed via boot-trace instrumentation, caught
    // as the raw ErrorCode value 0xdead0002 = ERROR_BASE+1 in Errors.h).
    // GeneralsMD/Code/Main/3ds.rsf requests New3DS SystemModeExt=178MB;
    // raise both caps to actually use that budget instead of the old
    // Legacy-mode-sized defaults, leaving a ~30MB margin below 178MB for
    // kernel/TLS/other reservations this override doesn't control.
    // TheSuperHackers @bugfix githubawn 16/07/2026 Both osGetMemRegionFree()
    // (a single query, tried with several margins) and a svcControlMemory
    // step-down probe loop (which additionally turned out to have two real
    // bugs of its own: a u32 multiply overflow in the starting guess, and a
    // missing hard-stop after giving up) failed identically at this exact
    // point in boot. Researching how libctru's OWN default
    // __system_allocateHeaps (allocateHeaps.c, which this whole function
    // overrides) sizes these heaps turned up the actual answer: it does not
    // use osGetMemRegionFree or trial-and-error at all. It queries the
    // process's resource limit object for RESLIMIT_COMMIT, which the
    // libctru source comments note "for APPLICATION this is equal to
    // APPMEMALLOC at all times" -- i.e. it is a direct, authoritative read of
    // the SystemModeExt-based budget (3ds.rsf's 178MB), not a snapshot that
    // can be stale or unpopulated this early. This is the same mechanism
    // every 3DS homebrew app relies on for correct sizing on both real
    // hardware and emulators; the earlier osGetMemRegionFree/probe-loop
    // approaches were reinventing this with strictly worse tools.
    void __system_allocateHeaps(void)
    {
        Result rc;
        Handle reslimit = 0;
        rc = svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE);
        if (R_FAILED(rc))
            svcBreak(USERBREAK_PANIC);

        s64 maxCommit = 0, currentCommit = 0;
        ResourceLimitType reslimitType = RESLIMIT_COMMIT;
        svcGetResourceLimitLimitValues(&maxCommit, reslimit, &reslimitType, 1);
        svcGetResourceLimitCurrentValues(&currentCommit, reslimit, &reslimitType, 1);
        svcCloseHandle(reslimit);

        const u32 remaining = (u32)(maxCommit - currentCommit) & ~0xFFF; // page-aligned

        // TheSuperHackers @bugfix githubawn 16/07/2026 Tried shifting this to
        // 80/20 heap/linear-heap, reasoning that AllocScratch (the thing
        // actually observed failing via its OOM diagnostic) draws only from
        // the general heap while linear "only" holds citro3d's GPU-side
        // texture uploads and small per-frame vertex/index buffers. That
        // broke boot entirely -- crashed within ~40s, before even this
        // process's own file-based boot trace could start, i.e. before
        // GameEngine::init() -- meaning something early (citro3d's own
        // internal GPU command-list buffers at C3D_Init, or the menu's own
        // texture/font-atlas loads) needs more of the linear heap than 20%
        // leaves.
        //
        // TheSuperHackers @bugfix githubawn 18/07/2026 Tried a fixed 24MB
        // linear heap (~4x the ~6.3MB peak usage measured mid-match via
        // GameLogic.cpp's SP-MARGIN diagnostic, general heap was at 112.7MB/
        // 119.1MB used -- only ~150KB free -- at that same point, so the
        // rebalance reasoning was sound for STEADY-STATE match usage). That
        // broke boot entirely, the same failure mode as the 80/20 (~35MB
        // linear) attempt above.
        //
        // TheSuperHackers @bugfix githubawn 18/07/2026 Added a real boot-time
        // linear-heap trace (Citro3dBackend.cpp's Initialize baseline +
        // Ensure_Texture's per-texture log) instead of guessing again. It
        // does NOT support "usage volume" as the failure cause: right after
        // C3D_Init, linear heap had 53.6MB free (of the then-56MB/32% pool);
        // by the time all ~200 early menu textures had loaded, only ~11.9MB
        // had been used total, well under even the failed 24MB attempt.
        //
        // TheSuperHackers @bugfix githubawn 18/07/2026 ROOT CAUSE FOUND: the
        // 24MB/40MB linear-heap attempts were never actually a linear-heap
        // problem at all. Azahar's own log showed the GENERAL heap's own
        // svcControlMemory call (below) failing with "Trying to allocate
        // already allocated memory" at ~127MB (the 40MB-linear split's
        // resulting general heap size) -- reproducible every time, survives
        // a full emulator restart, so not stale state. The 68/32 split's
        // ~119MB general heap succeeds; pushing it to ~127MB does not: there
        // appears to be a real ceiling on a single general-heap allocation
        // at OS_HEAP_AREA_BEGIN somewhere in [119MB, 127MB), independent of
        // the linear heap's own size entirely. Compounding this: svcBreak
        // (USERBREAK_PANIC) does NOT halt execution on Azahar (the same
        // emulator quirk that bit the old ProbeAlloc code, see the removed
        // step-down-probe-loop history above) -- so the failed call was
        // silently falling through to use __ctru_heap in whatever
        // half-set state svcControlMemory left it in, which is what actually
        // produced the PC=0 corruption crash chased for most of this
        // session, not heap exhaustion from usage. Reverted to the
        // known-working 68/32 split (~119MB general heap, under the
        // apparent ceiling) and added a hard stop after every svcBreak in
        // this function so a future allocation failure halts for real
        // instead of silently continuing on Azahar. Growing the general
        // heap further will need a smaller test step from 119MB (e.g.
        // +2-4MB at a time) to find the actual ceiling, not another
        // large jump.
        // TheSuperHackers @diagnostic githubawn 18/07/2026 Binary-searching the general-heap
        // ceiling found above. Known bounds so far: 119MB succeeds ("will run"), 127MB fails
        // ("Trying to allocate already allocated memory", confirmed reproducible). Each round:
        // test the midpoint of the current [low-that-works, high-that-fails] range; if it boots,
        // that becomes the new low; if it fails the same way, that becomes the new high.
        //   Round 1: midpoint of [119, 127] = 123MB <- CONFIRMED WORKING (20/07/2026). A full
        //   skirmish load ran to ~660 placed objects on this size; GameLogic.cpp's SP-MARGIN
        //   trace recorded generalHeap total=128974848 (123MB) with peak used=101.7MB, i.e.
        //   ~27MB headroom -- a large improvement on the 119MB split's ~4.6MB, helped by the
        //   texture/VB/IB CPU-scratch release fixes landed since. Bounds are now [123, 127).
        // Total commit requested from svcControlMemory is unchanged from the 68/32 split (only
        // the heap/linear split point moves), so a failure here can only be the ceiling itself,
        // not a different cause.
        //
        // TheSuperHackers @info githubawn 20/07/2026 Deliberately NOT continuing the search to
        // 125MB for now: at 123MB the general heap already has ~27MB of headroom at match peak,
        // so the binding constraint has moved elsewhere. The linear heap (~44MB, of which the
        // same trace showed only ~6MB in use) is the pool with real slack, and it is the one
        // that will absorb the DXT-decoded 3D textures once texturing is enabled -- so leave
        // that slack in place rather than converting it into general heap that nothing needs.
        // Resume the [123, 127) search only if general-heap OOM reappears.
        constexpr u32 kGgcGeneralHeapTestBytes = 123u * 1024u * 1024u;
        __ctru_heap_size = kGgcGeneralHeapTestBytes;

        // TheSuperHackers @tweak githubawn 20/07/2026 Per user request, shrink the linear heap
        // by 5MB while investigating the match-exit crash. Note what this does NOT do: the 5MB
        // is left UNCOMMITTED rather than handed to the general heap. Rolling it into the
        // general heap would take that allocation to 128MB, and the binary search recorded
        // above has 127MB as a CONFIRMED-FAILING size ("Trying to allocate already allocated
        // memory", reproducible across emulator restarts) -- so that version of this change
        // would not shrink anything, it would just fail to boot. Leaving the 5MB unallocated
        // keeps the general heap at its known-good 123MB and still removes the memory from
        // play, which is what a linear-heap-pressure or address-reuse theory needs to be
        // tested. The trace measured only ~6MB of ~44MB linear actually in use, so this is
        // well clear of real demand even with DXT-decoded textures now landing there.
        constexpr u32 kGgcLinearHeapReductionBytes = 5u * 1024u * 1024u;
        __ctru_linear_heap_size = remaining - __ctru_heap_size - kGgcLinearHeapReductionBytes;

        rc = svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, __ctru_heap_size,
                               MEMOP_ALLOC, static_cast<MemPerm>(MEMPERM_READ | MEMPERM_WRITE));
        if (R_FAILED(rc))
        {
            svcBreak(USERBREAK_PANIC);
            for (;;) {} // Azahar's svcBreak does not actually halt execution; force it.
        }

        rc = svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, __ctru_linear_heap_size,
                               MEMOP_ALLOC_LINEAR, static_cast<MemPerm>(MEMPERM_READ | MEMPERM_WRITE));
        if (R_FAILED(rc))
        {
            svcBreak(USERBREAK_PANIC);
            for (;;) {} // Azahar's svcBreak does not actually halt execution; force it.
        }

        mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

        fake_heap_start = reinterpret_cast<char*>(__ctru_heap);
        fake_heap_end = fake_heap_start + __ctru_heap_size;
    }
}

// TheSuperHackers @feature githubawn 19/07/2026 Match-exit crash groundwork:
// -Wl,--wrap=abort (cmake/toolchains/nintendo-3ds.cmake) redirects every call
// to abort() in this binary -- including the ones assert()/WWASSERT-style
// fatal paths and an uncaught std::terminate ultimately fall through to (see
// SDL3Main.cpp's std::set_terminate handler) -- to __wrap_abort below instead
// of libctru/newlib's own abort(). libctru has no POSIX signal handlers to
// catch this after the fact the way the Android build's SIGABRT handler
// (SDL3Main.cpp's ggc_install_crash_handler) does, so without this an
// abort() on 3DS just vanishes with no trace at all. SDL_Log opens/appends/
// closes sdmc:/3ds/SDL_Log.txt per call (see docs' 3DS debugging notes), so
// there is no flush concern even though the process dies immediately after.
//
// __real_abort is the actual libc abort() that --wrap=abort renames calls
// to; declared here only to satisfy the --wrap contract, NOT called below --
// see the reasoning in the function body.
extern "C" void __real_abort(void) __attribute__((noreturn));

extern "C" void __wrap_abort(void)
{
    // Recursion guard: if anything on this path (SDL_Log, etc.) itself
    // triggers another abort(), log only the first occurrence instead of
    // re-entering indefinitely.
    static bool s_ggcAborting = false;
    if (!s_ggcAborting)
    {
        s_ggcAborting = true;
        SDL_Log("[ggc-fatal] abort() called");
    }

    // TheSuperHackers @bugfix githubawn 19/07/2026 Deliberately do NOT call
    // __real_abort() here. __system_allocateHeaps above already documents
    // that Azahar's svcBreak does not actually halt execution; __real_abort's
    // own behavior on this toolchain (whatever combination of raise(SIGABRT)/
    // svcBreak/_exit newlib resolves to) is equally unproven. Use the same
    // "svcBreak then force a hang with an infinite loop" pattern already
    // relied on elsewhere in this file so the process reliably stops instead
    // of falling through to undefined or continued execution.
    svcBreak(USERBREAK_PANIC);
    for (;;) {}
}
#endif
