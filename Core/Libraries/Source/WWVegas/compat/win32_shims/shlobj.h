// TheSuperHackers @build bobtista 13/06/2026 Minimal <shlobj.h> shim. Shell
// folder lookups are Windows-only; callers that need real paths on Android use
// SDL_GetPrefPath instead. Only the common CSIDL constants are provided so
// headers that reference them still parse.
#pragma once

#include <windows.h>

#ifndef CSIDL_PERSONAL
#define CSIDL_PERSONAL      0x0005
#define CSIDL_APPDATA       0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define CSIDL_COMMON_APPDATA 0x0023
#define CSIDL_FLAG_CREATE   0x8000
#endif
