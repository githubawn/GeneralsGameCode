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

///////////////////////////////////////////////////////////////////////////////////////
// FILE: NetworkDirectConnect.cpp
// Author: Bryan Cleveland, November 2001
// Description: Lan Lobby Menu
///////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "gamespy/peer/peer.h"

#include "Common/QuotedPrintable.h"
#include "Common/OptionPreferences.h"
#include "GameClient/AnimateWindowManager.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/Shell.h"
#include "GameClient/GameWindowTransitions.h"

#include "GameNetwork/LANAPI.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/IPEnumeration.h"


// window ids ------------------------------------------------------------------------------

// Window Pointers ------------------------------------------------------------------------

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////

extern Bool LANbuttonPushed;
extern Bool LANisShuttingDown;

static Bool isShuttingDown = false;
static Bool buttonPushed = false;
static Bool staticLocalIPRevealed = false;

static NameKeyType buttonBackID = NAMEKEY_INVALID;
static NameKeyType buttonHostID = NAMEKEY_INVALID;
static NameKeyType buttonJoinID = NAMEKEY_INVALID;
static NameKeyType editPlayerNameID = NAMEKEY_INVALID;
static NameKeyType comboboxRemoteIPID = NAMEKEY_INVALID;
static NameKeyType staticLocalIPID = NAMEKEY_INVALID;

static GameWindow *buttonBack = nullptr;
static GameWindow *buttonHost = nullptr;
static GameWindow *buttonJoin = nullptr;
static GameWindow *editPlayerName = nullptr;
static GameWindow *comboboxRemoteIP = nullptr;
static GameWindow *staticLocalIP = nullptr;

static void UpdateLocalIPDisplay();

static WindowMsgHandledType LocalIPInput( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{
	if (msg == GWM_LEFT_DOWN || msg == GWM_LEFT_UP || msg == GWM_RIGHT_DOWN || msg == GWM_RIGHT_UP || msg == GBM_SELECTED)
	{
		if (msg == GWM_LEFT_UP || msg == GWM_RIGHT_UP || msg == GBM_SELECTED)
		{
			staticLocalIPRevealed = !staticLocalIPRevealed;
			UpdateLocalIPDisplay();
		}
		return MSG_HANDLED;
	}
	return MSG_IGNORED;
}

static void UpdateLocalIPDisplay()
{
	if (!staticLocalIP)
		return;

	UnsignedShort port = TheLAN ? TheLAN->getBoundPort() : 8086;
	IPEnumeration IPs;
	UnicodeString allIPs;
	UnicodeString hiddenIPs = L"Click to reveal";
	Int count = 0;

	for (EnumeratedIP *ip = IPs.getAddresses(); ip != nullptr; ip = ip->getNext())
	{
		UnsignedInt val = ip->getIP();
		if (val == 0 || val == 0x7F000001 || val == 0x0100007F)
			continue;

		count++;
		UnicodeString ipStr;
		if (port > 0)
		{
			ipStr.format(L"%d.%d.%d.%d:%d", PRINTF_IP_AS_4_INTS(val), port);
		}
		else
		{
			ipStr.format(L"%d.%d.%d.%d", PRINTF_IP_AS_4_INTS(val));
		}

		if (!allIPs.isEmpty())
		{
			allIPs.concat(L"\n");
		}
		allIPs.concat(ipStr);

		hiddenIPs.concat(L"\n***.***.***.***");
	}

	if (count == 0)
	{
		allIPs = L"None";
		hiddenIPs = L"Click to reveal\n***.***.***.***";
	}

	if (!staticLocalIPRevealed)
	{
		GadgetStaticTextSetText(staticLocalIP, hiddenIPs);
	}
	else
	{
		GadgetStaticTextSetText(staticLocalIP, allIPs);
	}
}

void PopulateRemoteIPComboBox()
{
	LANPreferences userprefs;
	GadgetComboBoxReset(comboboxRemoteIP);

	Int numRemoteIPs = userprefs.getNumRemoteIPs();
	Color white = GameMakeColor(255,255,255,255);

	for (Int i = 0; i < numRemoteIPs; ++i)
	{
		UnicodeString entry;
		entry = userprefs.getRemoteIPEntry(i);
		GadgetComboBoxAddEntry(comboboxRemoteIP, entry, white);
	}

	if (numRemoteIPs > 0)
	{
		GadgetComboBoxSetSelectedPos(comboboxRemoteIP, 0, TRUE);
	}
	userprefs.write();
}

void UpdateRemoteIPList()
{
	Int n1[4], n2[4];
	LANPreferences prefs;
	Int numEntries = GadgetComboBoxGetLength(comboboxRemoteIP);
	Int currentSelection = -1;
	GadgetComboBoxGetSelectedPos(comboboxRemoteIP, &currentSelection);
	UnicodeString unisel = GadgetComboBoxGetText(comboboxRemoteIP);
	AsciiString sel;
	sel.translate(unisel);

//	UnicodeString newEntry = prefs.getRemoteIPEntry(0);
	UnicodeString newEntry = unisel;
	UnicodeString newIP;
	newEntry.nextToken(&newIP, L":");
	Int numFields = swscanf(newIP.str(), L"%d.%d.%d.%d", &(n1[0]), &(n1[1]), &(n1[2]), &(n1[3]));

	if (numFields != 4) {
		// this is not a properly formatted IP, don't change a thing.
		return;
	}

	prefs["RemoteIP0"] = sel;

	Int currentINIEntry = 1;

	for (Int i = 0; i < numEntries; ++i)
	{
		if (i != currentSelection)
		{
			GadgetComboBoxSetSelectedPos(comboboxRemoteIP, i, FALSE);
			UnicodeString uni;
			uni = GadgetComboBoxGetText(comboboxRemoteIP);
			AsciiString ascii;
			ascii.translate(uni);

			// prevent more than one copy of an IP address from being put in the list.
			if (currentSelection == -1)
			{
				UnicodeString oldEntry = uni;
				UnicodeString oldIP;
				oldEntry.nextToken(&oldIP, L":");

				swscanf(oldIP.str(), L"%d.%d.%d.%d", &(n2[0]), &(n2[1]), &(n2[2]), &(n2[3]));

				Bool isEqual = TRUE;
				for (Int i = 0; (i < 4) && (isEqual == TRUE); ++i) {
					if (n1[i] != n2[i]) {
						isEqual = FALSE;
					}
				}
				// check to see if this is a duplicate or if this is not a properly formatted IP address.
				if (isEqual == TRUE)
				{
					--numEntries;
					continue;
				}
			}
			AsciiString temp;
			temp.format("RemoteIP%d", currentINIEntry);
			++currentINIEntry;
			prefs[temp.str()] = ascii;
		}
	}

	if (currentSelection == -1)
	{
		++numEntries;
	}

	AsciiString numRemoteIPs;
	numRemoteIPs.format("%d", numEntries);

	prefs["NumRemoteIPs"] = numRemoteIPs;

	prefs.write();
}

void HostDirectConnectGame()
{
	// Init LAN API Singleton
	DEBUG_ASSERTCRASH(TheLAN != nullptr, ("TheLAN is null!"));
	if (!TheLAN)
	{
		TheLAN = NEW LANAPI();
	}

	UnsignedInt localIP = TheLAN->GetLocalIP();
	UnicodeString localIPString;
	localIPString.format(L"%d.%d.%d.%d", PRINTF_IP_AS_4_INTS(localIP));

	UnicodeString name;
	name = GadgetTextEntryGetText(editPlayerName);

	LANPreferences prefs;
	prefs["UserName"] = UnicodeStringToQuotedPrintable(name);
	prefs.write();

	name.truncateTo(g_lanPlayerNameLength);
	TheLAN->RequestSetName(name);
	TheLAN->RequestGameCreate(localIPString, TRUE);
}

void JoinDirectConnectGame()
{
	// Init LAN API Singleton

	if (!TheLAN)
	{
		TheLAN = NEW LANAPI();
	}

	UnsignedInt ipaddress = 0;
	UnicodeString ipunistring = GadgetComboBoxGetText(comboboxRemoteIP);
	AsciiString asciientry;
	asciientry.translate(ipunistring);

	AsciiString ipstring;
	asciientry.nextToken(&ipstring, "(");

	Int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0, port = 0;
	Int numFields = sscanf(ipstring.str(), "%d.%d.%d.%d:%d", &ip1, &ip2, &ip3, &ip4, &port);
	if (numFields < 4)
	{
		numFields = sscanf(ipstring.str(), "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);
	}
	(void)numFields; DEBUG_ASSERTCRASH(numFields >= 4, ("JoinDirectConnectGame - invalid IP address format: %s", ipstring.str()));

	DEBUG_LOG(("JoinDirectConnectGame - joining at %d.%d.%d.%d:%d", ip1, ip2, ip3, ip4, port));

	ipaddress = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;
	if (port > 0)
	{
		UnsignedInt offset = Transport::getInstanceOffsetFromRealPort(8086, (UnsignedShort)port);
		ipaddress = Transport::makeInstanceIP(ipaddress, offset);
	}
//	ipaddress = htonl(ipaddress);

	UnicodeString name;
	name = GadgetTextEntryGetText(editPlayerName);

	LANPreferences prefs;
	prefs["UserName"] = UnicodeStringToQuotedPrintable(name);
	prefs.write();

	UpdateRemoteIPList();
	PopulateRemoteIPComboBox();

	name.truncateTo(g_lanPlayerNameLength);
	TheLAN->RequestSetName(name);

	TheLAN->RequestGameJoinDirectConnect(ipaddress);
}

//-------------------------------------------------------------------------------------------------
/** Initialize the WOL Welcome Menu */
//-------------------------------------------------------------------------------------------------
void NetworkDirectConnectInit( WindowLayout *layout, void *userData )
{
	LANbuttonPushed = false;
	LANisShuttingDown = false;

	if (TheLAN == nullptr)
	{
		TheLAN = NEW LANAPI();
		TheLAN->init();
	}
	TheLAN->reset();

	buttonPushed = false;
	isShuttingDown = false;
	TheShell->showShellMap(TRUE);
	buttonBackID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ButtonBack" );
	buttonHostID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ButtonHost" );
	buttonJoinID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ButtonJoin" );
	editPlayerNameID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:EditPlayerName" );
	comboboxRemoteIPID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ComboboxRemoteIP" );
	staticLocalIPID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:StaticLocalIP" );

	buttonBack = TheWindowManager->winGetWindowFromId( nullptr,  buttonBackID);
	buttonHost = TheWindowManager->winGetWindowFromId( nullptr,	buttonHostID);
	buttonJoin = TheWindowManager->winGetWindowFromId( nullptr,	buttonJoinID);
	editPlayerName = TheWindowManager->winGetWindowFromId( nullptr,	editPlayerNameID);
	comboboxRemoteIP = TheWindowManager->winGetWindowFromId( nullptr,	comboboxRemoteIPID);
	staticLocalIP = TheWindowManager->winGetWindowFromId( nullptr, staticLocalIPID);
	GameWindow *staticLocalIPDesc = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("NetworkDirectConnect.wnd:StaticLocalIPDescription"));

//	// animate controls
//	TheShell->registerWithAnimateManager(buttonBack, WIN_ANIMATION_SLIDE_LEFT, TRUE, 800);
//	TheShell->registerWithAnimateManager(buttonHost, WIN_ANIMATION_SLIDE_LEFT, TRUE, 600);
//	TheShell->registerWithAnimateManager(buttonJoin, WIN_ANIMATION_SLIDE_LEFT, TRUE, 200);
//
	LANPreferences userprefs;
	UnicodeString name;
	name = userprefs.getUserName();

	if (name.isEmpty())
	{
		name = TheGameText->fetch("GUI:Player");
	}

	GadgetTextEntrySetText(editPlayerName, name);

	PopulateRemoteIPComboBox();

	UnicodeString ipstr;

	delete TheLAN;
	TheLAN = nullptr;

	if (TheLAN == nullptr) {
//		DEBUG_ASSERTCRASH(TheLAN != nullptr, ("TheLAN is null initializing the direct connect screen."));
		TheLAN = NEW LANAPI();

		TheLAN->init();
		TheLAN->SetLocalIP(INADDR_ANY);
	}

	staticLocalIPRevealed = false;

	Int rx = 24, ry = 168, rw = 260, rh = 24;
	if (comboboxRemoteIP)
	{
		comboboxRemoteIP->winGetPosition(&rx, &ry);
		comboboxRemoteIP->winGetSize(&rw, &rh);
	}

	if (editPlayerName)
	{
		Int ew = 0, eh = 0;
		editPlayerName->winGetSize(&ew, &eh);
		if (ew > rw)
			rw = ew;
	}

	// Keep Remote IP in its native place (comboboxRemoteIP is not modified)
	// Position Local IP cleanly below Remote IP with 32px separation and 100px text box height
	Int localY_Desc = ry + rh - 18;
	Int localY_Box = ry + rh + 14;

	if (staticLocalIPDesc)
	{
		staticLocalIPDesc->winHide(FALSE);
		staticLocalIPDesc->winSetPosition(rx, localY_Desc);

		UnsignedInt statusDesc = staticLocalIPDesc->winGetStatus();
		statusDesc &= ~(WIN_STATUS_NO_INPUT | WIN_STATUS_NO_FOCUS);
		statusDesc |= WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_TOGGLE;
		staticLocalIPDesc->winSetStatus(statusDesc);
		staticLocalIPDesc->winSetInputFunc(LocalIPInput);
		staticLocalIPDesc->winBringToTop();
	}

	if (staticLocalIP)
	{
		staticLocalIP->winHide(FALSE);
		staticLocalIP->winSetPosition(rx, localY_Box);
		Int newW = (rw < 260) ? 260 : rw;
		staticLocalIP->winSetSize(newW, 100);

		UnsignedInt status = staticLocalIP->winGetStatus();
		status &= ~(WIN_STATUS_NO_INPUT | WIN_STATUS_NO_FOCUS);
		status |= WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_TOGGLE;
		staticLocalIP->winSetStatus(status);
		staticLocalIP->winSetInputFunc(LocalIPInput);
		staticLocalIP->winBringToTop();

		// Extend parent bounds so hit-testing reaches staticLocalIP
		GameWindow* parent = staticLocalIP->winGetParent();
		if (parent)
		{
			Int pw = 0, ph = 0;
			parent->winGetSize(&pw, &ph);
			if (ph < localY_Box + 115)
			{
				parent->winSetSize(pw, localY_Box + 115);
			}
		}

		UpdateLocalIPDisplay();
	}

	TheLAN->RequestLobbyLeave(true);
	layout->hide(FALSE);
	layout->bringForward();

	if (staticLocalIPDesc)
		staticLocalIPDesc->winBringToTop();

	if (staticLocalIP)
		staticLocalIP->winBringToTop();

	TheTransitionHandler->setGroup("NetworkDirectConnectFade");


}

//-------------------------------------------------------------------------------------------------
/** This is called when a shutdown is complete for this menu */
//-------------------------------------------------------------------------------------------------
static void shutdownComplete( WindowLayout *layout )
{

	isShuttingDown = false;

	// hide the layout
	layout->hide( TRUE );

	// our shutdown is complete
	TheShell->shutdownComplete( layout );

}

//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu shutdown method */
//-------------------------------------------------------------------------------------------------
void NetworkDirectConnectShutdown( WindowLayout *layout, void *userData )
{
	isShuttingDown = true;

	// if we are shutting down for an immediate pop, skip the animations
	Bool popImmediate = *(Bool *)userData;
	if( popImmediate )
	{

		shutdownComplete( layout );
		return;

	}

	TheShell->reverseAnimatewindow();

	TheTransitionHandler->reverse("NetworkDirectConnectFade");
}


//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu update method */
//-------------------------------------------------------------------------------------------------
void NetworkDirectConnectUpdate( WindowLayout * layout, void *userData)
{
	// We'll only be successful if we've requested to
	if(isShuttingDown && TheShell->isAnimFinished() && TheTransitionHandler->isFinished())
		shutdownComplete(layout);
}

//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu input callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType NetworkDirectConnectInput( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;
			if (buttonPushed)
				break;

			switch( key )
			{

				// ----------------------------------------------------------------------------------------
				case KEY_ESC:
				{

					//
					// send a simulated selected event to the parent window of the
					// back/exit button
					//
					if( BitIsSet( state, KEY_STATE_UP ) )
					{
						TheWindowManager->winSendSystemMsg( window, GBM_SELECTED,
																							(WindowMsgData)buttonBack, buttonBackID );

					}

					// don't let key fall through anywhere else
					return MSG_HANDLED;

				}

			}

		}

	}

	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu window system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType NetworkDirectConnectSystem( GameWindow *window, UnsignedInt msg,
														 WindowMsgData mData1, WindowMsgData mData2 )
{
	UnicodeString txtInput;

	switch( msg )
	{


		case GWM_CREATE:
			{

				break;
			}

		case GWM_DESTROY:
			{
				break;
			}

		case GWM_INPUT_FOCUS:
			{
				// if we're givin the opportunity to take the keyboard focus we must say we want it
				if( mData1 == TRUE )
					*(Bool *)mData2 = TRUE;

				return MSG_HANDLED;
			}

		case GBM_SELECTED:
			{
				if (buttonPushed)
					break;

				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				if ( controlID == buttonBackID )
				{
					UnicodeString name;
					name = GadgetTextEntryGetText(editPlayerName);

					LANPreferences prefs;
					prefs["UserName"] = UnicodeStringToQuotedPrintable(name);
					prefs.write();

					name.truncateTo(g_lanPlayerNameLength);
					TheLAN->RequestSetName(name);

					buttonPushed = true;
					LANbuttonPushed = true;
					TheShell->pop();
				}
				else if (controlID == buttonHostID)
				{
					HostDirectConnectGame();
				}
				else if (controlID == buttonJoinID)
				{
					JoinDirectConnectGame();
				}
				else if (controlID == staticLocalIPID)
				{
					staticLocalIPRevealed = !staticLocalIPRevealed;
					UpdateLocalIPDisplay();
				}
				break;
			}

		case GEM_EDIT_DONE:
			{
				break;
			}
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;
}
