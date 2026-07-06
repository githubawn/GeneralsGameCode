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

// TheSuperHackers @build githubawn 17/06/2026 CLOCK_BOOTTIME is Linux/Android
// only; Apple/Darwin has no such clock id. Fall back to CLOCK_MONOTONIC there.
#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME CLOCK_MONOTONIC
#endif

#define TIMERR_NOERROR 0
typedef int MMRESULT;
static inline MMRESULT timeBeginPeriod(int) { return TIMERR_NOERROR; }
static inline MMRESULT timeEndPeriod(int) { return TIMERR_NOERROR; }

inline unsigned int timeGetTime()
{
  struct timespec ts;
#if defined(__EMSCRIPTEN__) || defined(__SWITCH__)
  // TheSuperHackers @bugfix githubawn 26/06/2026 Emscripten/musl does not support
  // CLOCK_BOOTTIME: clock_gettime() fails and leaves ts zeroed, so timeGetTime() always
  // returns 0. That permanently fails every timeGetTime-gated loop (e.g. Shell::update's
  // 30Hz gate -> menu update callbacks never run -> the skirmish menu never reveals its
  // controls). CLOCK_MONOTONIC is supported and is what GetTickCount() already uses.
  // TheSuperHackers @bugfix githubawn 06/07/2026 Same on Nintendo Switch: devkitA64/newlib
  // defines CLOCK_BOOTTIME but libnx does not implement it, so clock_gettime() fails and
  // timeGetTime() returns 0. This froze Shell::update's runUpdate() calls -> MainMenuUpdate
  // never ran -> menus drew and took clicks but never navigated (Skirmish/Singleplayer dead).
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_BOOTTIME, &ts);
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

