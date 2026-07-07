/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// This file contains macros to help upgrade the code for newer cpp standards.
// Must be C compliant

#pragma once

#if __cplusplus >= 201103L
#include <utility>
#endif

#if __cplusplus >= 201103L
#define CPP_11(code) code
#else
#define CPP_11(code)
#define static_assert(expr, msg)
#define constexpr
#define noexcept
#define nullptr 0
#define override
#define final
#endif

#if __cplusplus >= 201703L
#define REGISTER
#define FALLTHROUGH [[fallthrough]]
#define MAYBE_UNUSED [[maybe_unused]]
#else
#define REGISTER register
#define FALLTHROUGH
#define MAYBE_UNUSED
#endif

// noexcept for methods of IUNKNOWN interface
#if defined(_MSC_VER)
#define IUNKNOWN_NOEXCEPT noexcept
#else
#define IUNKNOWN_NOEXCEPT
#endif

#ifdef __cplusplus
namespace stl
{

// Helper to move-assign from reference: uses std::move in C++11, swap in C++98
template<typename T>
inline void move_or_swap(T& dest, T& src)
{
#if __cplusplus >= 201103L
	dest = std::move(src);
#else
	// C++03 fallback: mimic move semantics
	// dest gets src's value, src becomes empty
	T empty;
	dest.swap(src);
	src.swap(empty);
#endif
}

} // namespace stl

#if defined(RTS_GENERALS)
#define RTS_NAMESPACE Gen
#else
#define RTS_NAMESPACE ZH
#endif

namespace RTS_NAMESPACE {
    namespace stl = ::stl;
}

#include <stdint.h>
typedef float Real;
typedef int32_t Int;
typedef uint32_t UnsignedInt;
typedef uint16_t UnsignedShort;
typedef int16_t Short;
typedef unsigned char UnsignedByte;
typedef char Byte;
typedef char Char;
typedef bool Bool;
typedef int64_t Int64;
typedef uint64_t UnsignedInt64;

struct _D3DMATRIX;
struct D3DXMATRIX;
struct IDirect3D8;
struct IDirect3DDevice8;
struct IDirect3DTexture8;
struct IDirect3DBaseTexture8;
struct IDirect3DCubeTexture8;
struct IDirect3DVolumeTexture8;
struct IDirect3DVertexBuffer8;
struct IDirect3DIndexBuffer8;
struct IDirect3DSurface8;
struct IDirect3DVolume8;

#endif
