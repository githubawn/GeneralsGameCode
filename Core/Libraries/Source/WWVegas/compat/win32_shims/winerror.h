// TheSuperHackers @build bobtista 13/06/2026 Minimal <winerror.h> shim. HRESULT
// and S_OK are already provided by the windows.h shim; this adds the Win32
// error codes the engine references directly.
#pragma once

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS           0L
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND    2L
#endif
#ifndef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED     5L
#endif
#ifndef ERROR_MORE_DATA
#define ERROR_MORE_DATA         234L
#endif
#ifndef ERROR_NO_MORE_ITEMS
#define ERROR_NO_MORE_ITEMS     259L
#endif
