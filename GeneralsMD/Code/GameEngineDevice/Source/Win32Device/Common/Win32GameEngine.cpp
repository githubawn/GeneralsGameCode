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
//  (c) 2001-2003 Electronic Arts Inc.
//  //
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: W3DGameEngine.cpp
// ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2001
// Description:
//   Implementation of the Win32 game engine, this is the highest level of
//   the game application, it creates all the devices we will use for the game
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

#include "Common/PerfTimer.h"
#include "Win32Device/Common/Win32GameEngine.h"


#include "GameNetwork/LANAPICallbacks.h"

extern DWORD TheMessageTime;

//-------------------------------------------------------------------------------------------------
/** Constructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::Win32GameEngine() {
  // Stop blue screen
  m_previousErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
}

//-------------------------------------------------------------------------------------------------
/** Destructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::~Win32GameEngine() {
  // restore it (this isn't really necessary, but feels good.)
  SetErrorMode(m_previousErrorMode);
}

//-------------------------------------------------------------------------------------------------
/** Initialize the game engine */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::init() {

  // extending functionality
  GameEngine::init();
}

//-------------------------------------------------------------------------------------------------
/** Reset the system */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::reset() {

  // extending functionality
  GameEngine::reset();
}

//-------------------------------------------------------------------------------------------------
/** Update the game engine by updating the GameClient and
 * GameLogic singletons. */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::update() {
  static bool firstFrame = true;
  if (firstFrame) {
    firstFrame = false;
    FILE *_f = fopen("trace.txt", "a");
    if (_f) {
      fprintf(_f, "TRACE: Win32GameEngine::update - FIRST FRAME\n");
      fclose(_f);
    }
  }

  // call the engine normal update
  GameEngine::update();

  // Auto-test logic
  static int testState = 0;
  static int frameCount = 0;

  extern Bool g_isAutoTest;
  if (g_isAutoTest) {
    frameCount++;

    // State 0: We are on the main menu. Wait for the game to be fully loaded.
    if (testState == 0) {
      if (frameCount > 150) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: State 0 -> Pushing Skirmish Menu\n");
            fclose(_f);
          }
        }
        extern Shell *TheShell;
        if (TheShell) {
          TheShell->push("Menus/SkirmishGameOptionsMenu.wnd");
          testState = 1;
          frameCount = 0;
        }
      }
    }
    // State 1: In Skirmish setup. Wait to stabilize, trigger screenshot, then resize.
    else if (testState == 1) {
      if (frameCount > 90) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: SCREENSHOT: skirmish_setup\n");
            fclose(_f);
          }
        }
        extern HWND ApplicationHWnd;
        if (ApplicationHWnd) {
          RECT rect;
          GetWindowRect(ApplicationHWnd, &rect);
          SetWindowPos(ApplicationHWnd, nullptr, rect.left, rect.top, 1024, 768, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        testState = 2;
        frameCount = 0;
      }
    }
    // State 2: Post-resize in Skirmish setup. Wait to stabilize, trigger screenshot, then start match.
    else if (testState == 2) {
      if (frameCount > 90) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: SCREENSHOT: skirmish_setup_resized\n");
            fprintf(_f, "AUTOTEST: State 2 -> Starting Skirmish Match\n");
            fclose(_f);
          }
        }
        extern void AutoStartSkirmish();
        AutoStartSkirmish();
        testState = 3;
        frameCount = 0;
      }
    }
    // State 3: Loading match. Wait until in-game.
    else if (testState == 3) {
      extern GameLogic *TheGameLogic;
      if (TheGameLogic && TheGameLogic->isInGame()) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: State 3 -> In-game detected. Waiting to stabilize.\n");
            fclose(_f);
          }
        }
        testState = 4;
        frameCount = 0;
      }
    }
    // State 4: In match. Wait to stabilize, trigger screenshot, then resize mid-match.
    else if (testState == 4) {
      if (frameCount > 200) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: SCREENSHOT: skirmish_match\n");
            fclose(_f);
          }
        }
        extern HWND ApplicationHWnd;
        if (ApplicationHWnd) {
          RECT rect;
          GetWindowRect(ApplicationHWnd, &rect);
          SetWindowPos(ApplicationHWnd, nullptr, rect.left, rect.top, 800, 600, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        testState = 5;
        frameCount = 0;
      }
    }
    // State 5: Resized mid-match. Wait to stabilize, trigger screenshot, then exit match to main menu.
    else if (testState == 5) {
      if (frameCount > 120) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: SCREENSHOT: skirmish_match_resized\n");
            fprintf(_f, "AUTOTEST: State 5 -> Exiting Skirmish Match to Main Menu\n");
            fclose(_f);
          }
        }
        extern GameLogic *TheGameLogic;
        if (TheGameLogic) {
          TheGameLogic->exitGame();
        }
        testState = 6;
        frameCount = 0;
      }
    }
    // State 6: Exiting match. Wait until shell is active.
    else if (testState == 6) {
      extern Shell *TheShell;
      if (TheShell && TheShell->isShellActive()) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: State 6 -> Back in shell. Waiting to stabilize.\n");
            fclose(_f);
          }
        }
        testState = 7;
        frameCount = 0;
      }
    }
    // State 7: Main menu stabilized. Wait to stabilize, trigger screenshot, then quit game.
    else if (testState == 7) {
      if (frameCount > 90) {
        {
          FILE *_f = fopen("trace.txt", "a");
          if (_f) {
            fprintf(_f, "AUTOTEST: SCREENSHOT: back_to_main_menu\n");
            fprintf(_f, "AUTOTEST: State 7 -> Auto-test complete. Quitting game.\n");
            fclose(_f);
          }
        }
        if (TheGameEngine) {
          TheGameEngine->setQuitting(TRUE);
        }
        testState = 8;
      }
    }
  }

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
      if (TheGameEngine->getQuitting() || TheGameLogic->isInInternetGame() ||
          TheGameLogic->isInLanGame()) {
        break; // keep running.
      }
    }

    // When we are alt-tabbed out... the MilesAudioManager seems to go into a
    // coma sometimes and not regain focus properly when we come back. This
    // seems to wake it up nicely.
    AudioAffect aa = (AudioAffect)0x10;
    TheAudio->setVolume(TheAudio->getVolume(aa), aa);
  }

  // allow windows to perform regular windows maintenance stuff like msgs
  serviceWindowsOS();
}

//-------------------------------------------------------------------------------------------------
/** This function may be called from within this application to let
 * Microsoft Windows do its message processing and dispatching.  Presumably
 * we would call this at least once each time around the game loop to keep
 * Windows services from backing up */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::serviceWindowsOS() {
  MSG msg;
  Int returnValue;

  //
  // see if we have any messages to process, a nullptr window handle tells the
  // OS to look at the main window associated with the calling thread, us!
  //
  while (PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE)) {

    // get the message
    returnValue = GetMessage(&msg, nullptr, 0, 0);

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
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    TheMessageTime = 0;
  }
}
