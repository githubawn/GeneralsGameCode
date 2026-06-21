/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header: /G/wwlib/bittype.h 4     4/02/99 1:37p Eric_c $ */
/***************************************************************************
 ***                  Confidential - Westwood Studios                    ***
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Voxel Technology                         *
 *                                                                         *
 *                    File Name : BITTYPE.h                                *
 *                                                                         *
 *                   Programmer : Greg Hjelstrom                           *
 *                                                                         *
 *                   Start Date : 02/24/97                                 *
 *                                                                         *
 *                  Last Update : February 24, 1997 [GH]                   *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

typedef unsigned char	uint8;
typedef unsigned short	uint16;
// TheSuperHackers @bugfix githubawn 19/06/2026 uint32/sint32 must be exactly 4
// bytes (they back every 32-bit field in the on-disk .w3d/W3D file-format structs,
// e.g. W3dMeshHeader3Struct). On Windows (LLP64) 'long' is 4 bytes, but on LP64
// targets (Android/iOS/macOS) 'long' is 8 bytes, which made sizeof(W3dMeshHeader3Struct)
// 160 instead of 116 -> every cload.Read(&hdr,sizeof(hdr)) size check failed ->
// Create_Render_Obj returned null for every model -> no units/structures/trees
// rendered. Same root cause as the DWORD fix below; these two were missed. Use a
// real 4-byte type off Windows; keep 'long' on Windows to preserve retail/replay layout.
#if defined(_WIN32)
typedef unsigned long	uint32;
#else
typedef unsigned int	uint32;
#endif
typedef unsigned int    uint;

typedef signed char		sint8;
typedef signed short		sint16;
#if defined(_WIN32)
typedef signed long		sint32;
#else
typedef signed int		sint32;
#endif
typedef signed int      sint;

typedef float				float32;
typedef double				float64;

// TheSuperHackers @bugfix bobtista 15/06/2026 DWORD is a 32-bit Windows type.
// On Windows (LLP64) 'long' is 4 bytes, but on LP64 targets (Android/Linux/
// macOS) 'long' is 8 bytes. Defining DWORD as 'unsigned long' there made
// sizeof(DWORD)==8, which (via D3DXGetFVFVertexSize counting sizeof(DWORD))
// inflated every FVF vertex size by 4 bytes per DIFFUSE/SPECULAR -> the engine
// wrote 4-byte 'unsigned' diffuse while the VB stride assumed 8 -> all 3D
// geometry (terrain/units/water) misaligned and rendered garbled. Use a real
// 4-byte type off Windows; keep 'unsigned long' on Windows to match <windows.h>.
#if defined(_WIN32)
typedef unsigned long   DWORD;
#else
typedef unsigned int    DWORD;
#endif
typedef unsigned short	WORD;
typedef unsigned char   BYTE;
typedef int             BOOL;
typedef unsigned short	USHORT;
typedef const char *		LPCSTR;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;

#if defined(_MSC_VER) && _MSC_VER < 1300
#ifndef _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif
#endif
