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

// FILE: W3DInGameUI.h ////////////////////////////////////////////////////////
//
// W3D implementation of the in game user interface, the in game ui is
// responsible for
//
// Author: Colin Day, April 2001
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Common/SeatManager.h"
#include "GameClient/DrawableInfo.h"	// splitscreen: owner tag on the UI's own scene markers
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"
#include "W3DDevice/GameClient/W3DView.h"
#include "WW3D2/render2d.h"
#include "WW3D2/rendobj.h"
#include "WW3D2/line3d.h"

class HAnimClass;

// W3DInGameUI ----------------------------------------------------------------
/** Implementation for the W3D game user interface.  This singleton is
  * responsible for all user interaction and feedback with as part of the
	* user interface */
//-----------------------------------------------------------------------------
class W3DInGameUI : public InGameUI
{

public:

	W3DInGameUI();
	virtual ~W3DInGameUI() override;

	// Inherited from subsystem interface -----------------------------------------------------------
	virtual	void init() override;		///< Initialize the in-game user interface
	virtual void update() override;	//< Update the UI by calling preDraw(), draw(), and postDraw()
	virtual void reset() override;		///< Reset
	//-----------------------------------------------------------------------------------------------

	virtual void draw() override; ///< Render the in-game user interface

protected:

	/// factory for views
	virtual View *createView(bool dummy) override
	{
		if (dummy)
			return NEW ViewDummy;
		return NEW W3DView;
	}

	virtual void drawSelectionRegion( Int seat );			///< draw one seat's selection region on screen
	virtual void drawMoveHints( View *view );			///< draw move hint visual feedback
	virtual void drawAttackHints( View *view );		///< draw attack hint visual feedback
	virtual void drawPlaceAngle( View *view ); 		///< draw place building angle if needed

	// Splitscreen: one set of hint objects PER SEAT. These are world-space render objects living in
	// the shared 3D scene, so a single shared set meant one seat's "move here" marker was built
	// from seat 0's hint list and shown to everyone.
	RenderObjClass *m_moveHintRenderObj[ MAX_SEATS ][ MAX_MOVE_HINTS ];
	/// Splitscreen: one per seat, attached to that seat's hint render objects so the per-view
	/// owner filter knows whose marker it is. Bare render objects carry no owner otherwise.
	DrawableInfo m_moveHintInfo[ MAX_SEATS ];
	HAnimClass		 *m_moveHintAnim[ MAX_SEATS ][ MAX_MOVE_HINTS ];
	RenderObjClass *m_buildingPlacementAnchor;
	RenderObjClass *m_buildingPlacementArrow;

};
