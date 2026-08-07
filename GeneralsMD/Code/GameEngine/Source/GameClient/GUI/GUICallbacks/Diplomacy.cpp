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

// FILE: Diplomacy.cpp ///////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell - August 2002
// Desc: GUI callbacks for the diplomacy menu
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/MultiplayerSettings.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Recorder.h"
#include "GameClient/AnimateWindowManager.h"
#include "GameClient/Diplomacy.h"
#include "GameClient/DisconnectMenu.h"
#include "GameClient/GameWindow.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetRadioButton.h"
#include "Common/SeatManager.h"	// splitscreen: MAX_SEATS
#include "GameClient/ControlBar.h"	// splitscreen: ControlBarInstances (popup goes in the seat bar)
#include "GameClient/GameClient.h"
#include "GameClient/GameText.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/VictoryConditions.h"
#include "GameNetwork/GameInfo.h"
#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/GameSpy/BuddyDefs.h"
#include "GameNetwork/GameSpy/PeerDefs.h"


//-------------------------------------------------------------------------------------------------

static NameKeyType staticTextPlayerID[MAX_SLOTS];
static NameKeyType staticTextSideID[MAX_SLOTS];
static NameKeyType staticTextTeamID[MAX_SLOTS];
static NameKeyType staticTextStatusID[MAX_SLOTS];
static NameKeyType buttonMuteID[MAX_SLOTS];
static NameKeyType buttonUnMuteID[MAX_SLOTS];
// The NameKeyType ids above are derived from layout-name strings and are IDENTICAL for every
// instance - one id set serves all seats, so they stay scalar. Only per-INSTANCE state below
// becomes per seat.
static NameKeyType radioButtonInGameID = NAMEKEY_INVALID;
static NameKeyType radioButtonBuddiesID = NAMEKEY_INVALID;
static NameKeyType winInGameID = NAMEKEY_INVALID;
static NameKeyType winBuddiesID = NAMEKEY_INVALID;
static NameKeyType winSoloID = NAMEKEY_INVALID;

// Splitscreen: this whole file had no seat concept at all - one layout, one window, one set of
// widget pointers - so the communicator always opened at Diplomacy.wnd's authored full-display
// position no matter which seat pressed the button, and a second seat opening it stomped the
// first seat's pointers. Everything per-instance is now indexed by seat.
//
// Functions below alias these into local names of the original spelling, so the bodies that walk
// slots are untouched: fewer edited lines is the point, a scripted rename in this file compiled
// while being wrong once already.
static GameWindow *s_radioButtonInGame[MAX_SEATS] = {nullptr};
static GameWindow *s_radioButtonBuddies[MAX_SEATS] = {nullptr};
static GameWindow *s_winInGame[MAX_SEATS] = {nullptr};
static GameWindow *s_winBuddies[MAX_SEATS] = {nullptr};
static GameWindow *s_winSolo[MAX_SEATS] = {nullptr};
static GameWindow *s_staticTextPlayer[MAX_SEATS][MAX_SLOTS] = {{nullptr}};
static GameWindow *s_staticTextSide[MAX_SEATS][MAX_SLOTS] = {{nullptr}};
static GameWindow *s_staticTextTeam[MAX_SEATS][MAX_SLOTS] = {{nullptr}};
static GameWindow *s_staticTextStatus[MAX_SEATS][MAX_SLOTS] = {{nullptr}};
static GameWindow *s_buttonMute[MAX_SEATS][MAX_SLOTS] = {{nullptr}};
static GameWindow *s_buttonUnMute[MAX_SEATS][MAX_SLOTS] = {{nullptr}};
static Int s_slotNumInRow[MAX_SEATS][MAX_SLOTS];

//-------------------------------------------------------------------------------------------------

static WindowLayout *s_theLayout[MAX_SEATS] = {nullptr};
static GameWindow *s_theWindow[MAX_SEATS] = {nullptr};
static AnimateWindowManager *s_theAnimateWindowManager[MAX_SEATS] = {nullptr};

/// Which seat a window/layout belongs to, or -1. Used by the callbacks, which are handed a
/// window rather than a seat.
static Int seatForDiplomacyLayout( const WindowLayout *layout )
{
	for( Int s = 0; s < MAX_SEATS; ++s )
		if( layout != nullptr && s_theLayout[s] == layout )
			return s;
	return -1;
}

static Int seatForDiplomacyWindow( GameWindow *window )
{
	for( GameWindow *w = window; w != nullptr; w = w->winGetParent() )
		for( Int s = 0; s < MAX_SEATS; ++s )
			if( s_theWindow[s] == w )
				return s;
	return -1;
}
WindowMsgHandledType BuddyControlSystem( GameWindow *window, UnsignedInt msg,
														 WindowMsgData mData1, WindowMsgData mData2);
void InitBuddyControls(Int type);
void updateBuddyInfo();
static void grabWindowPointers( Int seat )
{
	if (seat < 0 || seat >= MAX_SEATS)
		return;

	GameWindow *theWindow = s_theWindow[seat];
	GameWindow **staticTextPlayer = s_staticTextPlayer[seat];
	GameWindow **staticTextSide   = s_staticTextSide[seat];
	GameWindow **staticTextTeam   = s_staticTextTeam[seat];
	GameWindow **staticTextStatus = s_staticTextStatus[seat];
	GameWindow **buttonMute       = s_buttonMute[seat];
	GameWindow **buttonUnMute     = s_buttonUnMute[seat];
	Int *slotNumInRow             = s_slotNumInRow[seat];

	for (Int i=0; i<MAX_SLOTS; ++i)
	{
		AsciiString temp;
		temp.format("Diplomacy.wnd:StaticTextPlayer%d", i);
		staticTextPlayerID[i] = NAMEKEY(temp);
		temp.format("Diplomacy.wnd:StaticTextSide%d", i);
		staticTextSideID[i] = NAMEKEY(temp);
		temp.format("Diplomacy.wnd:StaticTextTeam%d", i);
		staticTextTeamID[i] = NAMEKEY(temp);
		temp.format("Diplomacy.wnd:StaticTextStatus%d", i);
		staticTextStatusID[i] = NAMEKEY(temp);
		temp.format("Diplomacy.wnd:ButtonMute%d", i);
		buttonMuteID[i] = NAMEKEY(temp);
		temp.format("Diplomacy.wnd:ButtonUnMute%d", i);
		buttonUnMuteID[i] = NAMEKEY(temp);

		// scoped to THIS seat's tree - winFindChildById is the form the rest of the branch
		// standardised on, and with N identical layouts a global lookup returns an arbitrary
		// seat's widget (handoff2 5.2 bug class 1).
		staticTextPlayer[i] = TheWindowManager->winFindChildById(theWindow, staticTextPlayerID[i]);
		staticTextSide[i] = TheWindowManager->winFindChildById(theWindow, staticTextSideID[i]);
		staticTextTeam[i] = TheWindowManager->winFindChildById(theWindow, staticTextTeamID[i]);
		staticTextStatus[i] = TheWindowManager->winFindChildById(theWindow, staticTextStatusID[i]);
		buttonMute[i] = TheWindowManager->winFindChildById(theWindow, buttonMuteID[i]);
		buttonUnMute[i] = TheWindowManager->winFindChildById(theWindow, buttonUnMuteID[i]);

		slotNumInRow[i] = -1;
	}
}

static void releaseWindowPointers( Int seat )
{
	// only THIS seat's row - clearing the shared set would blank another seat's open popup
	if (seat < 0 || seat >= MAX_SEATS)
		return;

	for (Int i=0; i<MAX_SLOTS; ++i)
	{
		s_staticTextPlayer[seat][i] = nullptr;
		s_staticTextSide[seat][i] = nullptr;
		s_staticTextTeam[seat][i] = nullptr;
		s_staticTextStatus[seat][i] = nullptr;
		s_buttonMute[seat][i] = nullptr;
		s_buttonUnMute[seat][i] = nullptr;

		s_slotNumInRow[seat][i] = -1;
	}
}


//-------------------------------------------------------------------------------------------------

static void updateFunc( WindowLayout *layout, void *param )
{
	// Splitscreen: tick only the seat that owns THIS layout, and hide only its window. The
	// layout pointer is already unique per seat, so no userdata is needed.
	const Int seat = seatForDiplomacyLayout( layout );
	if (seat < 0)
		return;

	AnimateWindowManager *theAnimateWindowManager = s_theAnimateWindowManager[seat];
	GameWindow *theWindow = s_theWindow[seat];

	if (theAnimateWindowManager && TheGlobalData->m_animateWindows)
	{
		Bool wasFinished = theAnimateWindowManager->isFinished();
		theAnimateWindowManager->update();
		if (theAnimateWindowManager->isFinished() && !wasFinished && theAnimateWindowManager->isReversed() && theWindow)
			theWindow->winHide( TRUE );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static BriefingList theBriefingList;

//-------------------------------------------------------------------------------------------------
BriefingList* GetBriefingTextList()
{
	return &theBriefingList;
}

//-------------------------------------------------------------------------------------------------
void UpdateDiplomacyBriefingText(AsciiString newText, Bool clear)
{
	// Solo briefing text is a singleplayer feature - seat 0 only, by construction.
	GameWindow *listboxSolo = TheWindowManager->winFindChildById(s_theWindow[0], NAMEKEY("Diplomacy.wnd:ListboxSolo"));

	if (clear)
	{
		theBriefingList.clear();
		if (listboxSolo)
			GadgetListBoxReset(listboxSolo);
	}

	if (newText.isEmpty())
		return;

	if (std::find(theBriefingList.begin(), theBriefingList.end(), newText) != theBriefingList.end())
		return;

	theBriefingList.push_back(newText);
	if (!listboxSolo)
		return;

	UnicodeString translated = TheGameText->fetch(newText);

	Int numEntries = GadgetListBoxGetNumEntries(listboxSolo);
	GadgetListBoxAddEntryText(listboxSolo, translated, TheInGameUI->getMessageColor(numEntries%2), -1);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void ShowDiplomacy( Bool immediate, Int seat )
{
	// seat < 0 on show means seat 0 - the classic single-view meaning
	if (seat < 0 || seat >= MAX_SEATS)
		seat = 0;

	WindowLayout *&theLayout = s_theLayout[seat];
	GameWindow *&theWindow = s_theWindow[seat];
	AnimateWindowManager *&theAnimateWindowManager = s_theAnimateWindowManager[seat];
	GameWindow *&radioButtonInGame = s_radioButtonInGame[seat];
	GameWindow *&radioButtonBuddies = s_radioButtonBuddies[seat];
	GameWindow *&winInGame = s_winInGame[seat];
	GameWindow *&winBuddies = s_winBuddies[seat];
	GameWindow *&winSolo = s_winSolo[seat];

	if (!TheInGameUI->getInputEnabled() || TheGameLogic->isIntroMoviePlaying() ||
			TheGameLogic->isLoadingMap())
		return;


	if (TheInGameUI->isQuitMenuVisible())
		return;

	if (TheDisconnectMenu && TheDisconnectMenu->isScreenVisible())
		return;

	if (theWindow)
	{
		theWindow->winHide(FALSE);
		theWindow->winEnable(TRUE);
	}
	else
	{
		theLayout = TheWindowManager->winCreateLayout( "Diplomacy.wnd" );
		theWindow = theLayout->getFirstWindow();
		theLayout->setUpdate(updateFunc);
		theAnimateWindowManager = NEW AnimateWindowManager;
		radioButtonInGameID = TheNameKeyGenerator->nameToKey("Diplomacy.wnd:RadioButtonInGame");
		radioButtonBuddiesID = TheNameKeyGenerator->nameToKey("Diplomacy.wnd:RadioButtonBuddies");
		// scoped to this seat's own tree, not a global walk that returns an arbitrary instance
		radioButtonInGame = TheWindowManager->winFindChildById(theWindow, radioButtonInGameID);
		radioButtonBuddies = TheWindowManager->winFindChildById(theWindow, radioButtonBuddiesID);
		winInGameID = TheNameKeyGenerator->nameToKey("Diplomacy.wnd:InGameParent");
		winBuddiesID = TheNameKeyGenerator->nameToKey("Diplomacy.wnd:BuddiesParent");
		winSoloID = TheNameKeyGenerator->nameToKey("Diplomacy.wnd:SoloParent");
		winInGame = TheWindowManager->winFindChildById(theWindow, winInGameID);
		winBuddies = TheWindowManager->winFindChildById(theWindow, winBuddiesID);
		winSolo = TheWindowManager->winFindChildById(theWindow, winSoloID);

		if (!TheRecorder->isMultiplayer())
		{
			GameWindow *listboxSolo = TheWindowManager->winFindChildById(theWindow, NAMEKEY("Diplomacy.wnd:ListboxSolo"));
			if (listboxSolo)
			{
				for (BriefingList::iterator it = theBriefingList.begin(); it != theBriefingList.end(); ++it)
				{
					UnicodeString translated = TheGameText->fetch(*it);
					Int numEntries = GadgetListBoxGetNumEntries(listboxSolo);
					GadgetListBoxAddEntryText(listboxSolo, translated, TheInGameUI->getMessageColor(numEntries%2), -1);
				}
			}
		}
	}
	theLayout->hide(FALSE);

	radioButtonInGame->winHide(TRUE);
	radioButtonBuddies->winHide(TRUE);
	GadgetRadioSetSelection(radioButtonInGame, FALSE);
	if (TheRecorder->isMultiplayer())
	{
		winInGame->winHide(FALSE);
		winBuddies->winHide(TRUE);
		winSolo->winHide(TRUE);
	}
	else
	{
		winInGame->winHide(TRUE);
		winBuddies->winHide(TRUE);
		winSolo->winHide(FALSE);
	}

	theAnimateWindowManager->reset();
	if (!immediate && TheGlobalData->m_animateWindows)
		theAnimateWindowManager->registerGameWindow( theWindow, WIN_ANIMATION_SLIDE_TOP, TRUE, 200 );

	// Splitscreen: hand this popup to the seat's own ControlBar. There is no general-purpose
	// "put a layout in a seat's viewport" helper - registering with the bar IS the mechanism,
	// and it is what the generals screen and the special-power shortcut bar already use. It
	// buys four things at once: position, per-frame re-dock, paint clipping, AND click
	// ownership - winSeatOwnsWindow resolves through ControlBar::ownsLayoutWindow, and without
	// this a seat>0 could see the popup but not press a single button in it, because that
	// function otherwise keeps diplomacy with seat 0 by design.
	if (seat > 0)
	{
		ControlBar *bar = ControlBarInstances::get( seat );
		if (bar != nullptr)
		{
			bar->adoptPopupLayout( theLayout );

			// keep the slide-in inside this seat's viewport instead of sweeping across others'
			const IRegion2D &d = bar->getBarDockRect();
			if (d.hi.x > d.lo.x)
				theAnimateWindowManager->setAnimationBounds( d.hi.x - d.lo.x, d.hi.y - d.lo.y );
		}
	}

	TheInGameUI->registerWindowLayout(theLayout);
	grabWindowPointers(seat);
	PopulateInGameDiplomacyPopup(seat);

	if(TheGameSpyInfo && TheGameSpyInfo->getLocalProfileID() != 0)
	{
		radioButtonInGame->winHide(FALSE);
		radioButtonBuddies->winHide(FALSE);
		InitBuddyControls(1);
		PopulateOldBuddyMessages();
		updateBuddyInfo();
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void ResetDiplomacy()
{
	// no-arg by design: this is reached from GameLogic (GameLogicDispatch closeWindows) and
	// always means "every seat".
	for (Int seat = 0; seat < MAX_SEATS; ++seat)
	{
		if(s_theLayout[seat])
		{
			// MANDATORY before destroyWindows(): a bar-registered layout torn down without this
			// leaves ControlBar::dockToRect writing through freed GameWindows every frame, and
			// the crash surfaces later inside winSetFont with an unrelated call stack. Same
			// defect ControlBar.cpp documents for the superweapon strip. ResetDiplomacy runs on
			// EVERY match teardown, so without this the second match crashes.
			ControlBar *bar = ControlBarInstances::get( seat );
			if (bar != nullptr)
				bar->forgetBarLayout( s_theLayout[seat] );

			TheInGameUI->unregisterWindowLayout(s_theLayout[seat]);
			s_theLayout[seat]->destroyWindows();
			deleteInstance(s_theLayout[seat]);
			if (seat == 0)
				InitBuddyControls(-1);
			s_theLayout[seat] = nullptr;
		}
		s_theWindow[seat] = nullptr;
		s_radioButtonInGame[seat] = nullptr;
		s_radioButtonBuddies[seat] = nullptr;
		s_winInGame[seat] = nullptr;
		s_winBuddies[seat] = nullptr;
		s_winSolo[seat] = nullptr;
		releaseWindowPointers(seat);

		delete s_theAnimateWindowManager[seat];
		s_theAnimateWindowManager[seat] = nullptr;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void HideDiplomacy( Bool immediate, Int seat )
{
	// seat < 0 on hide means EVERY seat - it is called from GameLogic teardown
	if (seat < 0)
	{
		for (Int s = 0; s < MAX_SEATS; ++s)
			if (s_theWindow[s])
				HideDiplomacy( immediate, s );
		return;
	}

	if (seat >= MAX_SEATS)
		return;

	releaseWindowPointers(seat);

	GameWindow *theWindow = s_theWindow[seat];
	AnimateWindowManager *theAnimateWindowManager = s_theAnimateWindowManager[seat];

	if (theWindow)
	{
		if (immediate || !TheGlobalData->m_animateWindows)
		{
			theWindow->winHide(TRUE);
			theWindow->winEnable(FALSE);
		}
		else
		{
			if (theAnimateWindowManager && theAnimateWindowManager->isFinished())
				theAnimateWindowManager->reverseAnimateWindow();
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void ToggleDiplomacy( Bool immediate, Int seat )
{
	// seat < 0 on toggle means seat 0 - the classic single-view meaning
	if (seat < 0 || seat >= MAX_SEATS)
		seat = 0;

	// If we bring this up, let's hide the quit menu
	HideQuitMenu();

	GameWindow *theWindow = s_theWindow[seat];

	if (theWindow)
	{
		Bool show = theWindow->winIsHidden();
		if (show)
			ShowDiplomacy( immediate, seat );
		else
			HideDiplomacy( immediate, seat );
	}
	else
	{
		ShowDiplomacy( immediate, seat );
	}
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType DiplomacyInput( GameWindow *window, UnsignedInt msg,
																			WindowMsgData mData1, WindowMsgData mData2 )
{

	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
//			UnsignedByte state = mData2;

			switch( key )
			{

				// ----------------------------------------------------------------------------------------
				case KEY_ESC:
				{
					HideDiplomacy();
					return MSG_HANDLED;
					//return MSG_IGNORED;
				}

			}

			return MSG_HANDLED;

		}

	}

	return MSG_IGNORED;

}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType DiplomacySystem( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	if(BuddyControlSystem(window, msg, mData1, mData2) == MSG_HANDLED)
	{
		return MSG_HANDLED;
	}
	switch( msg )
	{
		//---------------------------------------------------------------------------------------------
		case GGM_FOCUS_CHANGE:
		{
//			Bool focus = (Bool) mData1;
			//if (focus)
				//TheWindowManager->winSetGrabWindow( chatTextEntry );
			break;
		}

		//---------------------------------------------------------------------------------------------
		case GWM_INPUT_FOCUS:
		{
			// if we're given the opportunity to take the keyboard focus we must say we don't want it
			if( mData1 == TRUE )
				*(Bool *)mData2 = FALSE;

			return MSG_HANDLED;
		}

		//---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			NameKeyType controlID = (NameKeyType)control->winGetWindowId();

			// Splitscreen: the callback is handed a window, not a seat - resolve which seat's
			// popup this control belongs to by walking up to a known root. Falls back to 0.
			Int seat = seatForDiplomacyWindow( control );
			if (seat < 0)
				seat = 0;

			GameWindow *winInGame  = s_winInGame[seat];
			GameWindow *winBuddies = s_winBuddies[seat];
			Int *slotNumInRow      = s_slotNumInRow[seat];

			static NameKeyType buttonHideID = NAMEKEY( "Diplomacy.wnd:ButtonHide" );
			if (controlID == buttonHideID)
			{
				HideDiplomacy( FALSE, seat );
			}
			else if( controlID == radioButtonInGameID)
			{
				if (winInGame)  winInGame->winHide(FALSE);
				if (winBuddies) winBuddies->winHide(TRUE);
			}
			else if( controlID == radioButtonBuddiesID)
			{
				if (winInGame)  winInGame->winHide(TRUE);
				if (winBuddies) winBuddies->winHide(FALSE);
			}

			for (Int i=0; i<MAX_SLOTS; ++i)
			{
				if (controlID == buttonMuteID[i] && slotNumInRow[i] >= 0)
				{
					TheGameInfo->getSlot(slotNumInRow[i])->mute(TRUE);
					PopulateInGameDiplomacyPopup(seat);
					break;
				}
				if (controlID == buttonUnMuteID[i] && slotNumInRow[i] >= 0)
				{
					TheGameInfo->getSlot(slotNumInRow[i])->mute(FALSE);
					PopulateInGameDiplomacyPopup(seat);
					break;
				}
			}
			break;

		}

		//---------------------------------------------------------------------------------------------
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;

}

void PopulateInGameDiplomacyPopup( Int seat )
{
	if (!TheGameInfo)
		return;

	// seat < 0 => every seat with a live popup. Keeps the GameLogic-side caller seat-free.
	if (seat < 0)
	{
		for (Int s = 0; s < MAX_SEATS; ++s)
			if (s_theWindow[s])
				PopulateInGameDiplomacyPopup( s );
		return;
	}

	if (seat >= MAX_SEATS)
		return;

	GameWindow **staticTextPlayer = s_staticTextPlayer[seat];
	GameWindow **staticTextSide   = s_staticTextSide[seat];
	GameWindow **staticTextTeam   = s_staticTextTeam[seat];
	GameWindow **staticTextStatus = s_staticTextStatus[seat];
	GameWindow **buttonMute       = s_buttonMute[seat];
	GameWindow **buttonUnMute     = s_buttonUnMute[seat];
	Int *slotNumInRow             = s_slotNumInRow[seat];

	Int rowNum = 0;
	for (Int slotNum=0; slotNum<MAX_SLOTS; ++slotNum)
	{
		const GameSlot *slot = TheGameInfo->getConstSlot(slotNum);
		if (slot && slot->isOccupied())
		{
			Bool isInGame = false;
			// Note - for skirmish, TheNetwork == nullptr.  jba.
			if (TheNetwork &&	TheNetwork->isPlayerConnected(slotNum)) {
				isInGame = true;
			} else if ((TheNetwork == nullptr) && slot->isHuman()) {
				// this is a skirmish game and it is the human player.
				isInGame = true;
			}
			if (slot->isAI())
				isInGame = true;
			AsciiString playerName;
			playerName.format("player%d", slotNum);
			Player *player = ThePlayerList->findPlayerWithNameKey(NAMEKEY(playerName));
			Bool isAlive = !TheVictoryConditions->hasSinglePlayerBeenDefeated(player);
			Bool isObserver = player->isPlayerObserver();

			if (slot->isHuman() && TheGameInfo->getLocalSlotNum() != slotNum && isInGame)
			{
				// show mute button
				if (buttonMute[rowNum])
				{
					buttonMute[rowNum]->winHide(slot->isMuted());
				}
				if (buttonUnMute[rowNum])
				{
					buttonUnMute[rowNum]->winHide(!slot->isMuted());
				}
			}
			else
			{
				// can't mute self, AI players, or MIA humans
				if (buttonMute[rowNum])
					buttonMute[rowNum]->winHide(TRUE);
				if (buttonUnMute[rowNum])
					buttonUnMute[rowNum]->winHide(TRUE);
			}

			Color playerColor = TheMultiplayerSettings->getColor(slot->getApparentColor())->getColor();
			Color backColor = GameMakeColor(0, 0, 0, 255);
			Color aliveColor = GameMakeColor(0, 255, 0, 255);
			Color deadColor = GameMakeColor(255, 0, 0, 255);
			Color observerInGameColor = GameMakeColor(255, 255, 255, 255);
			Color goneColor = GameMakeColor(196, 0, 0, 255);
			Color observerGoneColor = GameMakeColor(196, 196, 196, 255);

			if (staticTextPlayer[rowNum])
			{
				staticTextPlayer[rowNum]->winSetEnabledTextColors( playerColor, backColor );
				GadgetStaticTextSetText(staticTextPlayer[rowNum], slot->getName());
			}
			if (staticTextSide[rowNum])
			{
				staticTextSide[rowNum]->winSetEnabledTextColors( playerColor, backColor );
				GadgetStaticTextSetText(staticTextSide[rowNum], slot->getApparentPlayerTemplateDisplayName() );
			}
			if (staticTextTeam[rowNum])
			{
				staticTextTeam[rowNum]->winSetEnabledTextColors( playerColor, backColor );
				AsciiString teamStr;
				teamStr.format("Team:%d", slot->getTeamNumber() + 1);
				if (slot->isAI() && slot->getTeamNumber() == -1)
					teamStr = "Team:AI";
				GadgetStaticTextSetText(staticTextTeam[rowNum], TheGameText->fetch(teamStr) );
			}
			if (staticTextStatus[rowNum])
			{
				staticTextStatus[rowNum]->winHide(FALSE);
				if (isInGame)
				{
					if (isAlive)
					{
						staticTextStatus[rowNum]->winSetEnabledTextColors( aliveColor, backColor );
						GadgetStaticTextSetText(staticTextStatus[rowNum], TheGameText->fetch("GUI:PlayerAlive"));
					}
					else
					{
						if (isObserver)
						{
							staticTextStatus[rowNum]->winSetEnabledTextColors( observerInGameColor, backColor );
							GadgetStaticTextSetText(staticTextStatus[rowNum], TheGameText->fetch("GUI:PlayerObserver"));
						}
						else
						{
							staticTextStatus[rowNum]->winSetEnabledTextColors( deadColor, backColor );
							GadgetStaticTextSetText(staticTextStatus[rowNum], TheGameText->fetch("GUI:PlayerDead"));
						}
					}
				}
				else
				{
					// not in game
					if (isObserver)
					{
						staticTextStatus[rowNum]->winSetEnabledTextColors( observerGoneColor, backColor );
						GadgetStaticTextSetText(staticTextStatus[rowNum], TheGameText->fetch("GUI:PlayerObserverGone"));
					}
					else
					{
						staticTextStatus[rowNum]->winSetEnabledTextColors( goneColor, backColor );
						GadgetStaticTextSetText(staticTextStatus[rowNum], TheGameText->fetch("GUI:PlayerGone"));
					}
				}
			}

			slotNumInRow[rowNum++] = slotNum;
		}
	}

	while (rowNum < MAX_SLOTS)
	{
		slotNumInRow[rowNum] = -1;
		if (staticTextPlayer[rowNum])
			staticTextPlayer[rowNum]->winHide(TRUE);
		if (staticTextSide[rowNum])
			staticTextSide[rowNum]->winHide(TRUE);
		if (staticTextTeam[rowNum])
			staticTextTeam[rowNum]->winHide(TRUE);
		if (staticTextStatus[rowNum])
			staticTextStatus[rowNum]->winHide(TRUE);
		if (buttonMute[rowNum])
			buttonMute[rowNum]->winHide(TRUE);
		if (buttonUnMute[rowNum])
			buttonUnMute[rowNum]->winHide(TRUE);

		++rowNum;
	}
}



