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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// GameMain.cpp
// The main entry point for the game
// Author: Michael S. Booth, April 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/FramePacer.h"
#include "Common/GameEngine.h"
#include "Common/ReplaySimulation.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <SDL3/SDL.h>

extern SDL_Window *TheSDL3Window;

void emscripten_loop_callback()
{
	if (TheGameEngine != nullptr && !TheGameEngine->getQuitting())
	{
		try {
			TheGameEngine->update();
		}
		catch (...) {
			// Catch exceptions to prevent crashing the browser tab
			SDL_Log("Exception caught in main update loop!");
			TheGameEngine->setQuitting(TRUE);
		}
		
		if (TheFramePacer != nullptr)
		{
			TheFramePacer->update();
		}
	}
	else
	{
		emscripten_cancel_main_loop();
		
		delete TheFramePacer;
		TheFramePacer = nullptr;
		delete TheGameEngine;
		TheGameEngine = nullptr;

		if (TheSDL3Window != nullptr)
		{
			SDL_DestroyWindow(TheSDL3Window);
			TheSDL3Window = nullptr;
		}
		SDL_Quit();
	}
}
#endif


/**
 * This is the entry point for the game system.
 */
Int GameMain()
{
	int exitcode = 0;
	// initialize the game engine using factory function
	TheFramePacer = new FramePacer();
	TheFramePacer->enableFramesPerSecondLimit(TRUE);
	TheGameEngine = CreateGameEngine();
	TheGameEngine->init();

	if (!TheGlobalData->m_simulateReplays.empty())
	{
		exitcode = ReplaySimulation::simulateReplays(TheGlobalData->m_simulateReplays, TheGlobalData->m_simulateReplayJobs);
	}
	else
	{
#if defined(__EMSCRIPTEN__)
		// Start Emscripten non-blocking loop (0 = use browser requestAnimationFrame, 1 = simulate infinite loop via throw)
		emscripten_set_main_loop(emscripten_loop_callback, 0, 1);
#else
		// run it
		TheGameEngine->execute();
#endif
	}

#if !defined(__EMSCRIPTEN__)
	// since execute() returned, we are exiting the game
	delete TheFramePacer;
	TheFramePacer = nullptr;
	delete TheGameEngine;
	TheGameEngine = nullptr;
#endif

	return exitcode;
}

