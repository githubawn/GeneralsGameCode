// TheSuperHackers @build githubawn 17/06/2026 <malloc.h> compatibility shim.
//
// Windows and glibc/Android all provide a top-level <malloc.h>, but Apple /
// Darwin does not — there the allocator declarations live in <malloc/malloc.h>
// (and the standard alloc/free in <stdlib.h>). This shim sits on the
// win32_shims include path (active for UNIX/ANDROID builds) so the many engine
// sources that do `#include <malloc.h>` compile unchanged on macOS/iOS, while
// non-Apple platforms continue to pull their real <malloc.h> via include_next.
#pragma once

#if defined(__APPLE__)
#  include <malloc/malloc.h>
#  include <stdlib.h>
#else
#  include_next <malloc.h>
#endif
