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

// This file contains thread related functions for compatibility with non-windows platforms.
#pragma once
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

inline int GetCurrentThreadId()
{
  // TheSuperHackers @build githubawn 17/06/2026 pthread_t is integral on the
  // Android NDK but an opaque pointer on Apple/Darwin, so it cannot be returned
  // as an int directly. Use pthread_threadid_np for a real integer TID there.
#if defined(__APPLE__)
  uint64_t tid = 0;
  pthread_threadid_np(NULL, &tid);
  return static_cast<int>(tid);
#elif defined(__SWITCH__) || defined(__3DS__)
  // TheSuperHackers @build githubawn 14/07/2026 devkitARM/newlib's pthread_t
  // is an opaque pointer (__pthread_t*), same as devkitA64/libnx's above.
  return static_cast<int>(reinterpret_cast<uintptr_t>(pthread_self()));
#else
  return static_cast<int>(pthread_self());
#endif
}

inline void Sleep(int ms)
{
  usleep(ms * 1000);
}

