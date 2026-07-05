#pragma once

#ifdef interface
#define GGC_TEMP_INTERFACE interface
#undef interface
#endif

#if defined(__GNUC__) || defined(__clang__)
#include_next <switch.h>
#else
#include <../include/switch.h>
#endif

#ifdef GGC_TEMP_INTERFACE
#define interface GGC_TEMP_INTERFACE
#undef GGC_TEMP_INTERFACE
#endif
