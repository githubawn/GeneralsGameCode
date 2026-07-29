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

// This file contains the time functions for compatibility with non-windows platforms.
#pragma once
#include <time.h>

#define TIMERR_NOERROR 0
typedef int MMRESULT;
static inline MMRESULT timeBeginPeriod(int) { return TIMERR_NOERROR; }
static inline MMRESULT timeEndPeriod(int) { return TIMERR_NOERROR; }

inline unsigned int timeGetTime()
{
  struct timespec ts;
#if defined(__EMSCRIPTEN__)
  // TheSuperHackers @bugfix githubawn 29/07/2026 Emscripten's musl declares
  // CLOCK_BOOTTIME but does not implement it, so clock_gettime() fails and leaves ts
  // zeroed and timeGetTime() always returns 0. Every timeGetTime-gated loop then stops
  // making progress - Shell::update's 30Hz gate never opens, so menu update callbacks
  // never run and menus draw but never navigate. CLOCK_MONOTONIC is supported, and is
  // what GetTickCount() below already uses.
  clock_gettime(CLOCK_MONOTONIC, &ts);
#elif defined(CLOCK_BOOTTIME)
  clock_gettime(CLOCK_BOOTTIME, &ts);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
inline unsigned int GetTickCount()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  // Return ms since boot
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
