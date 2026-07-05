#pragma once

#ifdef interface
#define GGC_TEMP_INTERFACE interface
#undef interface
#endif

#if defined(__GNUC__) || defined(__clang__)
#include_next <SDL3/SDL.h>
#else
#include <../include/SDL3/SDL.h>
#endif

#ifdef GGC_TEMP_INTERFACE
#define interface GGC_TEMP_INTERFACE
#undef GGC_TEMP_INTERFACE
#endif
