/* TheSuperHackers @build githubawn 10/07/2026 PS2-only compat shim (see
   docs/ps2-port-plan.md). ps2sdk's own headers assume a variadic POSIX
   ioctl(fd, request, ...) exists (sys/socket.h #defines ioctlsocket/
   lwip_ioctl in terms of it), but no ps2sdk library actually exports a
   symbol named plain "ioctl" -- only the fixed-arity "_ps2sdk_ioctl" that
   the real newlib device-dispatch layer is built on. Bridges the two so
   both our own win32_shims/winsock.h and the vendored GameSpySDK (which
   calls ioctl(sock, FIONBIO, ...) directly for non-blocking sockets)
   link against a real symbol instead of failing to compile/link.
   A .cpp file (not .c): adding a plain C source to this C++-only static
   library target made CMake generate a C-language precompiled header for
   the whole target, which then tried to compile the library's C++-only
   headers (class/template) as C and failed outright. extern "C" gives the
   same unmangled symbol GameSpySDK's C code links against. */

#if defined(__PS2__)

#include <stdarg.h>

extern "C" int _ps2sdk_ioctl(int fd, int request, void *data);

extern "C" int ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    void *data;

    va_start(ap, request);
    data = va_arg(ap, void *);
    va_end(ap);

    return _ps2sdk_ioctl(fd, (int)request, data);
}

#include <string.h>

// TheSuperHackers @build githubawn 10/07/2026 gethostname() has no ps2sdk
// implementation (no DNS-style hostname concept on this platform -- it's
// IP-configured, not named). Tier 0 stub matching this port's pattern
// elsewhere (see PS2Backend.h): satisfies the link for GameNetwork's
// IPEnumeration, WWDownload's FTP client, and GameSpySDK's socket code,
// all of which fall back gracefully on a short fixed name today. Revisit
// if/when Phase 5 networking needs something more meaningful here.
extern "C" int gethostname(char *name, size_t namelen)
{
    static const char kName[] = "ps2";

    if (name == nullptr || namelen == 0)
    {
        return -1;
    }

    size_t copyLen = sizeof(kName) < namelen ? sizeof(kName) : namelen;
    memcpy(name, kName, copyLen);
    if (copyLen < namelen)
    {
        name[copyLen] = '\0';
    }
    else
    {
        name[namelen - 1] = '\0';
    }

    return 0;
}

// TheSuperHackers @build githubawn 10/07/2026 GCC's atomic_flag::test_and_set
// (libstdc++'s bits/atomic_base.h) emits a call to the generic, unsized
// "__atomic_test_and_set" libcall on this MIPS target instead of expanding
// it inline or calling the sized variant directly. This toolchain's
// libatomic.a only defines the size-suffixed forms (_1/_2/_4/_8/_16), not
// the plain name -- confirmed via nm, not a linking/ordering issue.
// std::atomic_flag is always exactly 1 byte, so forwarding to
// __atomic_test_and_set_1 (which does exist) is the correct fix, not a
// stub: this restores real atomic test-and-set semantics, just via ps2sdk's
// actual 1-byte implementation instead of the generic name GCC expected.
// TheSuperHackers @build githubawn 10/07/2026 __atomic_test_and_set is
// itself a GCC builtin with a fixed signature (volatile void*, int); a
// mismatched declaration ("ambiguates built-in declaration") fails to
// compile even though we're trying to *provide* the definition GCC's own
// codegen is calling.
extern "C" bool __atomic_test_and_set_1(volatile void *ptr, int memorder);

extern "C" bool __atomic_test_and_set(volatile void *ptr, int memorder)
{
    return __atomic_test_and_set_1(ptr, memorder);
}

#endif /* __PS2__ */
