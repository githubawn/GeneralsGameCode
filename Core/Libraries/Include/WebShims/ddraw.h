#pragma once
#include "WebWin32.h"

// Basic DirectDraw types for DDS loading
typedef struct _DDSCAPS2 {
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCaps3;
    DWORD dwCaps4;
} DDSCAPS2;

typedef struct _DDPIXELFORMAT {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwABitMask;
} DDPIXELFORMAT;

typedef struct _DDSURFACEDESC2 {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    union {
        LONG lPitch;
        DWORD dwLinearSize;
    };
    DWORD dwBackBufferCount;
    DWORD dwMipMapCount;
    DWORD dwAlphaBitDepth;
    DWORD dwReserved;
    LPVOID lpSurface;
    DDPIXELFORMAT ddpfPixelFormat;
    DDSCAPS2 ddsCaps;
} DDSURFACEDESC2;

#ifndef DDSD_CAPS
#define DDSD_CAPS               0x00000001l
#define DDSD_HEIGHT             0x00000002l
#define DDSD_WIDTH              0x00000004l
#define DDSD_PITCH              0x00000008l
#define DDSD_PIXELFORMAT        0x00001000l
#define DDSD_MIPMAPCOUNT        0x00020000l
#define DDSD_LINEARSIZE         0x00080000l
#define DDSD_DEPTH              0x00800000l
#endif

#ifndef DDSCAPS2_CUBEMAP
#define DDSCAPS2_CUBEMAP        0x00000200l
#define DDSCAPS2_VOLUME         0x00200000l
#endif
