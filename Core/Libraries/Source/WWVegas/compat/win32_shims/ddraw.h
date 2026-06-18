// TheSuperHackers @build bobtista 13/06/2026 Minimal <ddraw.h> shim. ddsfile.cpp
// parses DDS files using its own Legacy* structs (see ddsfile.h) and only needs
// these DirectDraw surface-capability flag constants from the real header.
#pragma once

#ifndef DDSCAPS2_CUBEMAP
#define DDSCAPS2_CUBEMAP            0x00000200L
#define DDSCAPS2_CUBEMAP_POSITIVEX 0x00000400L
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x00000800L
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x00001000L
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x00002000L
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x00004000L
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x00008000L
#define DDSCAPS2_VOLUME            0x00200000L
#endif
