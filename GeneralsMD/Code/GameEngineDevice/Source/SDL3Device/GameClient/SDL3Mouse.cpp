/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "SDL3Device/GameClient/SDL3Mouse.h"

#if defined(SAGE_USE_SDL3)

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include "SDL3GameEngine.h"

SDL3Mouse::SDL3Mouse() :
	m_nextGetIndex(0),
	m_nextFreeIndex(0)
{
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
	for (UnsignedInt i = 0; i < NUM_MOUSE_CURSORS; ++i)
	{
		m_sdlCursors[i] = nullptr;
	}
#endif
	reset();
}

SDL3Mouse::~SDL3Mouse()
{
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
	for (UnsignedInt i = 0; i < NUM_MOUSE_CURSORS; ++i)
	{
		if (m_sdlCursors[i] != nullptr)
		{
			SDL_DestroyCursor(m_sdlCursors[i]);
			m_sdlCursors[i] = nullptr;
		}
	}
#endif
}

void SDL3Mouse::init()
{
	Mouse::init();
	// TheSuperHackers @bugfix bobtista 15/06/2026 SDL delivers absolute window
	// coordinates for both mouse motion and touch, so the cursor must be driven
	// in absolute mode. The base class defaults to relative, which made SDL/touch
	// positions get accumulated and fly the cursor off-screen (menu unclickable).
	m_inputMovesAbsolute = TRUE;
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
	SDL_ShowCursor();
#endif
}

void SDL3Mouse::reset()
{
	Mouse::reset();
	// TheSuperHackers @bugfix bobtista 15/06/2026 Mouse::reset() forces relative
	// mode; re-assert absolute mode here (SDL/touch deliver absolute coords) so
	// menu navigation works after any reset.
	m_inputMovesAbsolute = TRUE;
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
	SDL_ShowCursor();
#endif
	for (UnsignedInt i = 0; i < NUM_MOUSE_EVENTS; ++i)
	{
		m_buffer[i].leftState = MBS_None;
		m_buffer[i].rightState = MBS_None;
		m_buffer[i].middleState = MBS_None;
		m_buffer[i].leftEvent = MOUSE_EVENT_NONE;
		m_buffer[i].rightEvent = MOUSE_EVENT_NONE;
		m_buffer[i].middleEvent = MOUSE_EVENT_NONE;
		m_buffer[i].pos.x = 0;
		m_buffer[i].pos.y = 0;
		m_buffer[i].deltaPos.x = 0;
		m_buffer[i].deltaPos.y = 0;
		m_buffer[i].wheelPos = 0;
		m_buffer[i].time = 0;
	}
	m_nextGetIndex = 0;
	m_nextFreeIndex = 0;
}

void SDL3Mouse::initCursorResources()
{
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
	// Initialize standard SDL system cursors mapping to the game's MouseCursor enum.
	for (int i = 0; i < NUM_MOUSE_CURSORS; ++i)
	{
		SDL_SystemCursor id = SDL_SYSTEM_CURSOR_DEFAULT;
		switch (i)
		{
			case NONE:
				continue; // Will handle hiding cursor separately
			case SCROLL:
				id = SDL_SYSTEM_CURSOR_MOVE;
				break;
			case CROSS:
			case ATTACK_OBJECT:
			case FORCE_ATTACK_OBJECT:
			case FORCE_ATTACK_GROUND:
				id = SDL_SYSTEM_CURSOR_CROSSHAIR;
				break;
			case GENERIC_INVALID:
			case INVALID_BUILD_PLACEMENT:
				id = SDL_SYSTEM_CURSOR_NOT_ALLOWED;
				break;
			case SELECTING:
			case MOVETO:
			case ATTACKMOVETO:
				id = SDL_SYSTEM_CURSOR_POINTER;
				break;
			default:
				id = SDL_SYSTEM_CURSOR_DEFAULT;
				break;
		}
		m_sdlCursors[i] = SDL_CreateSystemCursor(id);
	}
#endif
}

void SDL3Mouse::setCursor(MouseCursor cursor)
{
	m_currentCursor = cursor;
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
	if (cursor == NONE)
	{
		SDL_HideCursor();
	}
	else
	{
		SDL_ShowCursor();
		if (m_sdlCursors[cursor] != nullptr)
		{
			SDL_SetCursor(m_sdlCursors[cursor]);
		}
	}
#endif
}

void SDL3Mouse::setPosition(Int x, Int y)
{
	Mouse::setPosition(x, y);
	if (TheSDL3Window != NULL)
	{
		SDL_WarpMouseInWindow(TheSDL3Window, static_cast<float>(x), static_cast<float>(y));
	}
}

void SDL3Mouse::capture()
{
#if defined(__ANDROID__)
	// TheSuperHackers @bugfix bobtista 15/06/2026 Cursor capture / relative mouse
	// mode is meaningless for touch input (always absolute). On fullscreen Android
	// the menu would otherwise capture the cursor and switch to relative mode,
	// which breaks touch navigation. Keep absolute mode.
	m_inputMovesAbsolute = TRUE;
#elif defined(__APPLE__) || defined(__EMSCRIPTEN__)
	// TheSuperHackers @bugfix On macOS and WASM, bgfx does not draw a hardware cursor,
	// so we rely on the OS/browser cursor. Relative mouse mode hides the OS cursor,
	// resulting in no mouse. Use Grab mode instead to confine the cursor while keeping it visible.
	m_inputMovesAbsolute = TRUE;
	if (TheSDL3Window != NULL)
	{
		SDL_SetWindowMouseGrab(TheSDL3Window, true);
	}
#else
	// Relative mouse mode (e.g. in-game camera drag) supplies motion deltas.
	m_inputMovesAbsolute = FALSE;
	if (TheSDL3Window != NULL)
	{
		SDL_SetWindowRelativeMouseMode(TheSDL3Window, true);
	}
#endif
}

void SDL3Mouse::releaseCapture()
{
	m_inputMovesAbsolute = TRUE;
	if (TheSDL3Window != NULL)
	{
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
		SDL_SetWindowMouseGrab(TheSDL3Window, false);
#else
		SDL_SetWindowRelativeMouseMode(TheSDL3Window, false);
#endif
	}
}

UnsignedByte SDL3Mouse::getMouseEvent(MouseIO *result, Bool flush)
{
	if (m_nextGetIndex == m_nextFreeIndex)
	{
		return MOUSE_NONE;
	}

	*result = m_buffer[m_nextGetIndex];
	if (flush)
	{
		m_nextGetIndex = (m_nextGetIndex + 1) % NUM_MOUSE_EVENTS;
	}
	return MOUSE_OK;
}

void SDL3Mouse::addSDL3MotionEvent(const SDL_MouseMotionEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = static_cast<Int>(event.x);
	io.pos.y = static_cast<Int>(event.y);
	io.deltaPos.x = static_cast<Int>(event.xrel);
	io.deltaPos.y = static_cast<Int>(event.yrel);
	io.wheelPos = 0;
	io.time = SDL_GetTicks();
	pushEvent(io);
}

void SDL3Mouse::addSDL3ButtonEvent(const SDL_MouseButtonEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = static_cast<Int>(event.x);
	io.pos.y = static_cast<Int>(event.y);
	io.deltaPos.x = 0;
	io.deltaPos.y = 0;
	io.wheelPos = 0;
	io.time = SDL_GetTicks();

	MouseButtonState state = (event.down != 0) ? MBS_Down : MBS_Up;
	if (event.button == SDL_BUTTON_LEFT)
	{
		io.leftState = state;
	}
	else if (event.button == SDL_BUTTON_RIGHT)
	{
		io.rightState = state;
	}
	else if (event.button == SDL_BUTTON_MIDDLE)
	{
		io.middleState = state;
	}

	pushEvent(io);
}

void SDL3Mouse::addSDL3WheelEvent(const SDL_MouseWheelEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = m_currMouse.pos.x;
	io.pos.y = m_currMouse.pos.y;
	io.deltaPos.x = 0;
	io.deltaPos.y = 0;
	io.wheelPos = static_cast<Int>(event.y * MOUSE_WHEEL_DELTA);
	io.time = SDL_GetTicks();
	pushEvent(io);
}

void SDL3Mouse::addSDL3FingerEvent(const SDL_TouchFingerEvent &event, int phase)
{
	// SDL touch coordinates are normalized 0..1 relative to the window. The
	// game's mouse coordinates are in render-resolution (== device pixel) space,
	// which we force to the window pixel size on boot, so scale by window pixels.
	int winW = 0, winH = 0;
	if (TheSDL3Window != NULL)
	{
		SDL_GetWindowSizeInPixels(TheSDL3Window, &winW, &winH);
	}
	if (winW <= 0) winW = 1;
	if (winH <= 0) winH = 1;

	const Int px = static_cast<Int>(event.x * static_cast<float>(winW) + 0.5f);
	const Int py = static_cast<Int>(event.y * static_cast<float>(winH) + 0.5f);

#if defined(__ANDROID__)
	__android_log_print(4, "ggc-touch",
		"finger phase=%d norm=(%.3f,%.3f) win=%dx%d -> px=(%d,%d)",
		phase, event.x, event.y, winW, winH, px, py);
#endif

	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = px;
	io.pos.y = py;
	io.deltaPos.x = 0;
	io.deltaPos.y = 0;
	io.wheelPos = 0;
	io.time = SDL_GetTicks();

	// Always move the cursor to the touch pixel first; on down/up also drive the
	// left button. (phase: 0=down, 1=up, 2=motion)
	if (phase == 0)
	{
		io.leftState = MBS_Down;
	}
	else if (phase == 1)
	{
		io.leftState = MBS_Up;
	}
	pushEvent(io);
}

void SDL3Mouse::pushEvent(const MouseIO &event)
{
	m_buffer[m_nextFreeIndex] = event;
	m_nextFreeIndex = (m_nextFreeIndex + 1) % NUM_MOUSE_EVENTS;
	if (m_nextFreeIndex == m_nextGetIndex)
	{
		m_nextGetIndex = (m_nextGetIndex + 1) % NUM_MOUSE_EVENTS;
	}
}

#endif
