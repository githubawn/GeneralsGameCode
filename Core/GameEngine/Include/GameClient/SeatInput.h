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

// SeatInput.h
//
// Device-independent logical input for a local seat (splitscreen WP1). A device
// backend (SDL3 gamepad today, keyboard/mouse later) translates its physical
// inputs into a SeatInputState each frame and hands it to TheSeatManager. Nothing
// here references SDL or any device type, so both the core engine and the client
// can consume it.

#pragma once

#include "Lib/BaseType.h"

// Engine-side logical buttons. Device backends map their physical controls onto
// this set so consumers never see raw gamepad/keyboard codes.
enum SeatButton CPP_11(: Int)
{
	SEAT_BUTTON_CONFIRM = 0,   // A / South   - primary action (select / left click)
	SEAT_BUTTON_CANCEL,        // B / East    - cancel / right click
	SEAT_BUTTON_ACTION,        // X / West
	SEAT_BUTTON_ALT_ACTION,    // Y / North
	// These two names predate the binding table and no longer describe what the buttons do:
	// the RIGHT shoulder is the shift-equivalent, matching the legacy pad. What any of these
	// mean is decided in exactly one place, getSeatButtonBinding() in SeatManager.h.
	SEAT_BUTTON_MODIFIER,      // left shoulder
	SEAT_BUTTON_COMMAND_BAR,   // right shoulder
	SEAT_BUTTON_JOIN,          // Start       - claim a seat / pause
	SEAT_BUTTON_LEAVE,         // Back        - release a seat
	SEAT_BUTTON_CURSOR_CLICK,  // left stick click
	SEAT_BUTTON_CAMERA_RESET,  // right stick click
	SEAT_BUTTON_DPAD_UP,
	SEAT_BUTTON_DPAD_DOWN,
	SEAT_BUTTON_DPAD_LEFT,
	SEAT_BUTTON_DPAD_RIGHT,

	SEAT_BUTTON_COUNT
};

// One frame of logical input for a single seat. Sticks are post-deadzone in
// [-1,1]; triggers in [0,1]. Button edges (pressed/released) are computed by the
// device backend against its own previous frame.
struct SeatInputState
{
	Real leftX, leftY;      // left stick, post-deadzone
	Real rightX, rightY;    // right stick, post-deadzone
	Real leftTrigger;       // 0..1
	Real rightTrigger;      // 0..1
	Bool buttonDown[SEAT_BUTTON_COUNT];
	Bool buttonPressed[SEAT_BUTTON_COUNT];   // edge: up->down this frame
	Bool buttonReleased[SEAT_BUTTON_COUNT];  // edge: down->up this frame

	SeatInputState()
	{
		clear();
	}

	void clear()
	{
		leftX = leftY = rightX = rightY = 0.0f;
		leftTrigger = rightTrigger = 0.0f;
		for (Int i = 0; i < SEAT_BUTTON_COUNT; ++i)
		{
			buttonDown[i] = FALSE;
			buttonPressed[i] = FALSE;
			buttonReleased[i] = FALSE;
		}
	}
};
