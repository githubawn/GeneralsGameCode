/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

/*
** Derived from the GeneralsX branch by fbraz3
*/

#pragma once

#include "Lib/BaseType.h"

// SYSTEM INCLUDES
#include <SDL3/SDL.h>
#include <array>
#include <functional>

// USER INCLUDES
#include "GameClient/Mouse.h"
#include "GameClient/Keyboard.h"
#include "GameClient/KeyDefs.h"

// FORWARD REFERENCES
class SDL3InputManager;

// GLOBALS ---------------------------------------------------------------------
extern SDL3InputManager* TheSDL3InputManager;

// TYPE DEFINES ----------------------------------------------------------------
typedef KeyDefType KeyVal;

// SDL3Mouse ------------------------------------------------------------------
/** Mouse interface using SDL3 APIs */
//-----------------------------------------------------------------------------
class SDL3Mouse : public Mouse
{
public:
	SDL3Mouse(SDL_Window* window);
	virtual ~SDL3Mouse(void);

	// SubsystemInterface
	virtual void init(void) override;
	virtual void reset(void) override;
	virtual void update(void) override;
	virtual void initCursorResources(void) override;
	static void freeCursorResources(void);

	// Mouse interface
	virtual void setCursor(MouseCursor cursor) override;
	virtual void setVisibility(Bool visible) override;
	virtual void loseFocus() override;
	virtual void regainFocus() override;
	virtual Bool hasSecondLocalInput() const override;

	// SDL3-specific methods
	void addSDLEvent(SDL_Event* event);

protected:
	virtual void capture(void) override;
	virtual void releaseCapture(void) override;
	virtual UnsignedByte getMouseEvent(MouseIO* result, Bool flush) override;

private:
	// Event translation from SDL_Event (Clean Slate implementation)
	void translateEvent(const SDL_Event& event, MouseIO* result);

	// Scale raw SDL window coordinates to game internal resolution
	void scaleMouseCoordinates(int rawX, int rawY, Uint32 windowID, int& scaledX, int& scaledY);

	SDL_Window* m_Window;
	Bool m_IsCaptured;
	Bool m_IsVisible;
	Bool m_LostFocus;

	Int m_directionFrame;

	float m_accumulatedDeltaX;
	float m_accumulatedDeltaY;

	SDL_Cursor* m_activeSDLCursor;
};

// SDL3Keyboard ---------------------------------------------------------------
/** Keyboard interface using SDL3 APIs */
//-----------------------------------------------------------------------------
class SDL3Keyboard : public Keyboard
{
public:
	SDL3Keyboard(void);
	virtual ~SDL3Keyboard(void);

	// SubsystemInterface
	virtual void init(void) override;
	virtual void reset(void) override;
	virtual void update(void) override;

	// Keyboard interface
	virtual Bool getCapsState(void) override;

	// SDL3-specific methods
	void addSDLEvent(SDL_Event* event);

protected:
	virtual void getKey(KeyboardIO* key) override;
	virtual KeyVal translateScanCodeToKeyVal(unsigned char scan);

private:
	void translateKeyEvent(const SDL_KeyboardEvent& event);
};

// SDL3InputManager -----------------------------------------------------------
/** Unified manager for SDL3 input events */
//-----------------------------------------------------------------------------
class SDL3InputManager
{
public:
	SDL3InputManager(SDL_Window* window);
	virtual ~SDL3InputManager();

	void update();

	// Buffer access
	Bool getNextMouseEvent(SDL_Event& outEvent, int playerIndex);
	Bool getNextKeyboardEvent(SDL_Event& outEvent, int playerIndex);

	void addMouseSDLEvent(const SDL_Event& event, int playerIndex);
	void addKeyboardSDLEvent(const SDL_Event& event, int playerIndex);

	Bool isQuitting() const { return m_isQuitting; }
	Bool hasGamepad(int playerIndex) const
	{
		if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return FALSE;
		return m_playerStates[playerIndex].gamepad != nullptr;
	}
	void getVirtualMousePos(int playerIndex, float& outX, float& outY) const
	{
		if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) { outX = 0; outY = 0; return; }
		outX = m_playerStates[playerIndex].virtualMouseX;
		outY = m_playerStates[playerIndex].virtualMouseY;
	}

	// Constants
	static constexpr float AXIS_MAX = 32767.0f;
	static constexpr int TRIGGER_THRESHOLD = 16384;
	static constexpr float DEFAULT_DEADZONE = 0.15f;
	static constexpr float DEFAULT_CURSOR_SPEED = 800.0f;
	static constexpr int MAX_PLAYERS = 8;

private:
	struct GamepadState
	{
		bool buttonState[SDL_GAMEPAD_BUTTON_COUNT];
		bool stickLeft, stickRight, stickUp, stickDown;
		bool ltDown, rtDown;

		GamepadState()
		{
			memset(buttonState, 0, sizeof(buttonState));
			stickLeft = stickRight = stickUp = stickDown = false;
			ltDown = rtDown = false;
		}
	};

private:
	// Gamepad management
	void openGamepads();
	void closeGamepads();

	SDL_Window* m_window;
	void processGamepadInput();
	void handleGamepadButton(SDL_GamepadButton button, bool& currentState, bool isDown, std::function<void(bool)> action);

	// Virtual event injection
	void virtualPulseKey(SDL_Scancode scancode, bool down, int playerIndex);
	void virtualPulseMouse(Uint8 button, bool down, int playerIndex);

	// Event buffers
	static const UnsignedInt MAX_MOUSE_EVENTS = 256;
	static const UnsignedInt MAX_KEY_EVENTS = 256;

	struct PlayerInputState
	{
		SDL_Gamepad* gamepad;
		GamepadState state;
		Bool precisionMode;
		float virtualMouseX;
		float virtualMouseY;

		SDL_Event mouseEvents[MAX_MOUSE_EVENTS];
		UnsignedInt mouseNextFree;
		UnsignedInt mouseNextGet;

		SDL_Event keyEvents[MAX_KEY_EVENTS];
		UnsignedInt keyNextFree;
		UnsignedInt keyNextGet;

		PlayerInputState()
		{
			gamepad = nullptr;
			precisionMode = FALSE;
			virtualMouseX = 400.0f; // Default center
			virtualMouseY = 300.0f;
			mouseNextFree = 0;
			mouseNextGet = 0;
			keyNextFree = 0;
			keyNextGet = 0;
			memset(mouseEvents, 0, sizeof(mouseEvents));
			memset(keyEvents, 0, sizeof(keyEvents));
		}
	};

	PlayerInputState m_playerStates[MAX_PLAYERS];

	Uint64 m_lastUpdateTime;
	Bool m_isQuitting;
};
