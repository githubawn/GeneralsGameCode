/*
**	Command & Conquer Generals(tm)
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

// FILE: W3DGameEngine.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2001
// Description:
//   Implementation of the Win32 game engine, this is the highest level of
//   the game application, it creates all the devices we will use for the game
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

#include "Win32Device/Common/Win32GameEngine.h"
#include "Common/PerfTimer.h"
#include "Common/GlobalData.h"

#include "GameNetwork/LANAPICallbacks.h"

DWORD TheMessageTime = 0;
Int gPendingWidth = 0;
Int gPendingHeight = 0;
DWORD gLastResizeTime = 0;
Int gProcessingResolutionChange = 0;
Bool gResolutionChangeFromOptions = FALSE;

#include "GameClient/Display.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/Mouse.h"
#include "GameClient/Shell.h"
#include "GameClient/InGameUI.h"

//-------------------------------------------------------------------------------------------------
/** Constructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::Win32GameEngine()
{
	// Stop blue screen
	m_previousErrorMode = SetErrorMode( SEM_FAILCRITICALERRORS );
}

//-------------------------------------------------------------------------------------------------
/** Destructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::~Win32GameEngine()
{
	// restore it (this isn't really necessary, but feels good.)
	SetErrorMode( m_previousErrorMode );
}


//-------------------------------------------------------------------------------------------------
/** Initialize the game engine */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::init()
{

	// extending functionality
	GameEngine::init();

}

//-------------------------------------------------------------------------------------------------
/** Reset the system */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::reset()
{

	// extending functionality
	GameEngine::reset();

}

//-------------------------------------------------------------------------------------------------
/** Update the game engine by updating the GameClient and
	* GameLogic singletons. */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::update()
{
	// TheSuperHackers @fix Antigravity 21/05/2026 Defer & debounce resolution changes to main game loop to avoid re-entry corruption
	if (gPendingWidth > 0 && gPendingHeight > 0)
	{
		DWORD now = GetTickCount();
		if (now - gLastResizeTime >= 300) // Debounce for 300ms of inactivity/stabilization
		{
			Int newWidth = gPendingWidth;
			Int newHeight = gPendingHeight;

			if (TheDisplay && TheGlobalData && (gResolutionChangeFromOptions || TheGlobalData->m_windowed))
			{
				if (newWidth != TheDisplay->getWidth() || newHeight != TheDisplay->getHeight())
				{
					// TheSuperHackers @fix Antigravity 21/05/2026 Defer resizes if the shell is active but in an unstable transition state.
					// NOTE: Do NOT return early from update() as that starves the message loop and game updates, causing a deadlock!
					// Instead, we just bypass the resize processing in this frame.
					if (!(TheShell && TheShell->isShellActive() && !TheShell->isStable()))
					{
						gPendingWidth = 0;
						gPendingHeight = 0;

						gProcessingResolutionChange++; // Increment the resolution change guard

						Bool targetWindowed = gResolutionChangeFromOptions ? TheDisplay->getWindowed() : TRUE;

						if (TheDisplay->setDisplayMode(newWidth, newHeight, TheDisplay->getBitDepth(), targetWindowed))
						{
							TheWritableGlobalData->m_xResolution = newWidth;
							TheWritableGlobalData->m_yResolution = newHeight;

							if (TheHeaderTemplateManager)
								TheHeaderTemplateManager->onResolutionChanged();
							if (TheMouse)
								TheMouse->onResolutionChanged();

							if (TheShell)
								TheShell->recreateWindowLayouts();

							if (TheInGameUI)
							{
								TheInGameUI->recreateControlBar();
								TheInGameUI->refreshCustomUiResources();
							}
						}

						// TheSuperHackers @fix Antigravity 21/05/2026 Pump and silently discard any pending WM_SIZE / WM_MOVE messages 
						// generated synchronously or asynchronously by the resolution change while gProcessingResolutionChange is still active.
						// This completely eliminates resolution feedback loops.
						// Note: We do NOT translate/dispatch these messages because we are handling the layout changes directly ourselves.
						extern HWND ApplicationHWnd;
						MSG msg;
						while (PeekMessage(&msg, ApplicationHWnd, WM_SIZE, WM_SIZE, PM_REMOVE))
						{
							// Silently discarded
						}
						while (PeekMessage(&msg, ApplicationHWnd, WM_MOVE, WM_MOVE, PM_REMOVE))
						{
							// Silently discarded
						}

						if (gProcessingResolutionChange > 0)
							gProcessingResolutionChange--; // Decrement the guard

						// If the change came from options, launch the confirmation dialog now
						if (gResolutionChangeFromOptions)
						{
							gResolutionChangeFromOptions = FALSE;
							extern void DoResolutionDialog();
							DoResolutionDialog();
						}
					}
				}
				else
				{
					// If the resolution didn't change (e.g. reverted or set to same), we still reset the flags
					gPendingWidth = 0;
					gPendingHeight = 0;
					if (gResolutionChangeFromOptions)
					{
						gResolutionChangeFromOptions = FALSE;
						extern void DoResolutionDialog();
						DoResolutionDialog();
					}
				}
			}
		}
	}

	// call the engine normal update
	GameEngine::update();

	extern HWND ApplicationHWnd;
	if (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) {
		while (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) {
			// We are alt-tabbed out here.  Sleep a bit, & process windows
			// so that we can become un-alt-tabbed out.
			Sleep(5);
			serviceWindowsOS();

			if (TheLAN != nullptr) {
				// BGC - need to update TheLAN so we can process and respond to other
				// people's messages who may not be alt-tabbed out like we are.
				TheLAN->setIsActive(isActive());
				TheLAN->update();
			}

			// If we are running a multiplayer game, keep running the logic.
			// There is code in the client to skip client redraw if we are
			// iconic.  jba.
			if (TheGameEngine->getQuitting() || TheGameLogic->isInInternetGame() || TheGameLogic->isInLanGame()) {
				break; // keep running.
			}
		}
	}

	// allow windows to perform regular windows maintenance stuff like msgs
	serviceWindowsOS();

}

//-------------------------------------------------------------------------------------------------
/** This function may be called from within this application to let
  * Microsoft Windows do its message processing and dispatching.  Presumeably
	* we would call this at least once each time around the game loop to keep
	* Windows services from backing up */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::serviceWindowsOS()
{
	MSG msg;
  Int returnValue;

	//
	// see if we have any messages to process, a nullptr window handle tells the
	// OS to look at the main window associated with the calling thread, us!
	//
	while( PeekMessage( &msg, nullptr, 0, 0, PM_NOREMOVE ) )
	{

		// get the message
		returnValue = GetMessage( &msg, nullptr, 0, 0 );

		// this is one possible way to check for quitting conditions as a message
		// of WM_QUIT will cause GetMessage() to return 0
/*
		if( returnValue == 0 )
		{

			setQuitting( true );
			break;

		}
*/

		TheMessageTime = msg.time;
		// translate and dispatch the message
		TranslateMessage( &msg );
		DispatchMessage( &msg );
		TheMessageTime = 0;

	}

}

