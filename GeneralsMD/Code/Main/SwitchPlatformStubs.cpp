// SwitchPlatformStubs.cpp
// TheSuperHackers @build githubawn 29/06/2026
// Nintendo Switch AArch64 platform helpers.
//
// The build now uses standard libnx hardware TLS (see nintendo-switch.cmake:
// no -mtp=soft), so thread_local accesses read TPIDRRO_EL0 inline and nothing
// calls __aarch64_read_tp. A correct minimal definition is kept below purely as
// insurance for any stray reference from a statically-linked object; the earlier
// version had a bogus "dummy TLS" fallback that returned a bad thread pointer and
// crashed libnx's fsdev the moment it touched thread-local state.

#if defined(__SWITCH__) && defined(__aarch64__)
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <cstdio>
#include <switch/arm/thread_context.h>

extern "C" unsigned int svcOutputDebugString(const char *str, unsigned long size);

// TheSuperHackers @diagnostic githubawn 06/07/2026 CPU exception handler. On strict
// emulators (Eden/yuzu-lineage) a wild-pointer access faults instead of being silently
// absorbed like Ryujinx. libnx calls this on a guest CPU exception; dump the faulting
// PC + fault address + registers to the SD trace file so the PC can be addr2line'd
// against the (symbol-rich) ELF to get the exact source line.
extern "C" void __libnx_exception_handler(ThreadExceptionDump *ctx)
{
    FILE *f = std::fopen("ggc_boot.txt", "a");
    if (f)
    {
        std::fprintf(f, "[ggc] EXCEPTION pc=0x%016llx far=0x%016llx lr=0x%016llx sp=0x%016llx err=0x%x\n",
            (unsigned long long)ctx->pc.x, (unsigned long long)ctx->far.x,
            (unsigned long long)ctx->lr.x, (unsigned long long)ctx->sp.x,
            (unsigned)ctx->error_desc);
        // Runtime address of this handler -> compute the PIE load slide vs the ELF:
        //   slide = handlerRuntime - <elf addr of __libnx_exception_handler>
        //   elf_pc_for_addr2line = pc - slide
        std::fprintf(f, "[ggc] slideRef handlerRuntime=0x%016llx\n",
            (unsigned long long)(unsigned long)(void*)&__libnx_exception_handler);
        for (int i = 0; i < 29; ++i)
            std::fprintf(f, "[ggc]  x%02d=0x%016llx\n", i, (unsigned long long)ctx->cpu_gprs[i].x);
        std::fflush(f);
        std::fclose(f);
    }
    char b[128];
    int n = std::snprintf(b, sizeof(b), "[ggc] EXCEPTION pc=0x%016llx far=0x%016llx\n",
        (unsigned long long)ctx->pc.x, (unsigned long long)ctx->far.x);
    if (n > 0) svcOutputDebugString(b, (unsigned)n);
    // spin so the dump is flushed and the crash state is inspectable
    for (;;) { }
}

extern "C"
{
    unsigned int svcOutputDebugString(const char *str, unsigned long size);

    // TheSuperHackers @bugfix githubawn 03/07/2026 Initialize the MAIN thread's C++
    // TLS block and set tpidr_el0. In this build libnx's crt0 left tpidr_el0 == 0 for
    // the main thread (custom link rules + --allow-multiple-definition), so every
    // thread_local access computes tpidr_el0(0)+offset -> near-null -> crash (hit in
    // OpenAL ALCcontext::getThreadContext and bgfx s_threadIndex). switch.ld reserves
    // .main.tls and exposes the template symbols below; replicate what crt0 should do:
    // allocate a block whose first 16 bytes are the TCB, copy the .tdata init image at
    // +16 (offset 0x10, matching the compiler's TPREL), zero the rest, set tpidr_el0.
    // Runs as the earliest constructor so all later ctors + main see working TLS.
    // Switch-only; cannot affect any other platform.
    extern char __tdata_lma[];
    extern char __tdata_lma_end[];
    extern char __tls_start[];
    extern char __tls_end[];

    __attribute__((constructor(101))) static void ggc_switch_setup_main_tls(void)
    {
        unsigned long tp = 0;
        __asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
        if (tp != 0)
            return; // libnx already set it up; leave it alone

        const unsigned long tdata_size = (unsigned long)(__tdata_lma_end - __tdata_lma);
        const unsigned long tls_size   = (unsigned long)(__tls_end - __tls_start);
        const unsigned long total      = 16 + tls_size + 64; // 16-byte TCB + TLS + pad
        unsigned char *block = (unsigned char *)memalign(64, total);
        if (block == 0)
            return;
        for (unsigned long i = 0; i < total; ++i)
            block[i] = 0;
        for (unsigned long i = 0; i < tdata_size; ++i)
            block[16 + i] = __tdata_lma[i];
        __asm__ volatile("msr tpidr_el0, %0" ::"r"(block));
    }

    // Early boot probe (runs just after the TLS setup above).
    __attribute__((constructor(102))) static void ggc_switch_early_ctor(void)
    {
        static const char m[] = "[ggc] early ctor reached (tls set up)\n";
        svcOutputDebugString(m, sizeof(m) - 1);
    }

#if defined(GGC_SWITCH_APP_NSP)
    // TheSuperHackers @build githubawn 01/07/2026 Application-NSP-only libnx overrides.
    // These are REQUIRED to reach main() when this ELF is packaged as an application
    // NSP (launched as a title), but they BREAK the homebrew NRO path: when launched
    // via hbloader, libnx must use the loader-provided applet type and heap override,
    // and forcing AppletType_Application + a direct svcSetHeapSize aborts __appInit
    // before main(). So they are gated behind GGC_SWITCH_APP_NSP and left OFF for the
    // default (NRO/homebrew) build. NOTE: the application-NSP path additionally hits a
    // HIPC buffer-descriptor incompatibility on the emulators (IStorage read faults at
    // va=0xF000); the NRO/homebrew path is the supported route. See the switch memory.

    // Run as a real application, not homebrew: override libnx's weak applet-type symbol
    // so it does the application applet handshake instead of homebrew auto-detection.
    unsigned int __nx_applet_type = 0; // AppletType_Application

    // libnx's default heap init grabs nearly all reported memory (~3.15 GB), which the
    // emulator refuses for this title (svcSetHeapSize -> OutOfMemory) and libnx then
    // aborts before main(). Request a conservative fixed heap, falling back smaller.
    extern char *fake_heap_start;
    extern char *fake_heap_end;
    unsigned int svcSetHeapSize(void **out_addr, unsigned long size);

    void __libnx_initheap(void)
    {
        static const unsigned long candidates[] = {
            0x60000000UL, // 1.5 GB
            0x40000000UL, // 1.0 GB
            0x20000000UL, // 512 MB
            0x10000000UL, // 256 MB
        };
        void *addr = 0;
        unsigned long chosen = 0;
        for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
        {
            if (svcSetHeapSize(&addr, candidates[i]) == 0)
            {
                chosen = candidates[i];
                break;
            }
        }
        fake_heap_start = (char *)addr;
        fake_heap_end = (char *)addr + chosen;
    }
#endif // GGC_SWITCH_APP_NSP

    int posix_memalign(void **memptr, size_t alignment, size_t size)
    {
        void *ptr = memalign(alignment, size);
        if (!ptr) {
            return ENOMEM;
        }
        *memptr = ptr;
        return 0;
    }
}
#endif
