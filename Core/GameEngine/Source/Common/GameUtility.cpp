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

#include "PreRTS.h"

#include "Common/GameUtility.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/Radar.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ParticleSys.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/GhostObject.h"
#include "GameLogic/PartitionManager.h"

namespace rts
{

namespace detail
{
static void changePlayerCommon(Player* player)
{
	TheParticleSystemManager->setLocalPlayerIndex(player->getPlayerIndex());
	ThePartitionManager->refreshShroudForLocalPlayer();
	TheGhostObjectManager->setLocalPlayerIndex(player->getPlayerIndex());
	TheGameClient->updateFakeDrawables();
	TheRadar->refreshObjects();
	TheInGameUI->deselectAllDrawables();
}

} // namespace detail

bool localPlayerHasRadar()
{
	// Using "local" instead of "observed or local" player because as an observer we prefer
	// the radar to be turned on when observing a player that has no radar.
	const Player* player = ThePlayerList->getLocalPlayer();
	const PlayerIndex index = player->getPlayerIndex();

	if (TheRadar->isRadarForced(index))
		return true;

	if (!TheRadar->isRadarHidden(index) && player->hasRadar())
		return true;

	return false;
}

// Splitscreen (WP7): scoped render-player override (see header).
static Int TheRenderPlayerIndexOverride = -1;

Player* getObservedOrLocalPlayer()
{
	if (TheRenderPlayerIndexOverride >= 0 && ThePlayerList != nullptr)
		return ThePlayerList->getNthPlayer(TheRenderPlayerIndexOverride);

	DEBUG_ASSERTCRASH(TheControlBar != nullptr, ("TheControlBar is null"));
	Player* player = TheControlBar->getObservedPlayer();
	if (player == nullptr)
	{
		DEBUG_ASSERTCRASH(ThePlayerList != nullptr, ("ThePlayerList is null"));
		player = ThePlayerList->getLocalPlayer();
	}
	return player;
}

Player* getObservedOrLocalPlayer_Safe()
{
	// The scoped render-player override means "draw as this player". It has to apply to the
	// Player accessor as well as the index one, or render-side systems that ask for the player
	// object - the radar is the notable one - keep drawing seat 0's world in every viewport.
	if (TheRenderPlayerIndexOverride >= 0 && ThePlayerList != nullptr)
		return ThePlayerList->getNthPlayer(TheRenderPlayerIndexOverride);

	Player* player = nullptr;

	if (TheControlBar != nullptr)
		player = TheControlBar->getObservedPlayer();

	if (player == nullptr)
		if (ThePlayerList != nullptr)
			player = ThePlayerList->getLocalPlayer();

	return player;
}


void setRenderPlayerIndexOverride(Int playerIndex) { TheRenderPlayerIndexOverride = playerIndex; }
void clearRenderPlayerIndexOverride() { TheRenderPlayerIndexOverride = -1; }

// Splitscreen: rect of the view being drawn (see header). -1 width = unset.
static Int TheRenderViewX = 0, TheRenderViewY = 0, TheRenderViewW = -1, TheRenderViewH = -1;

void setRenderViewRect(Int x, Int y, Int width, Int height)
{
	TheRenderViewX = x;
	TheRenderViewY = y;
	TheRenderViewW = width;
	TheRenderViewH = height;
}

void clearRenderViewRect()
{
	TheRenderViewW = -1;
	TheRenderViewH = -1;
}

void getRenderViewRect(Int* x, Int* y, Int* width, Int* height)
{
	if (TheRenderViewW >= 0 && TheRenderViewH >= 0)
	{
		*x = TheRenderViewX;
		*y = TheRenderViewY;
		*width = TheRenderViewW;
		*height = TheRenderViewH;
		return;
	}

	// Unset: the whole display, which is what a single full-screen view covers.
	*x = 0;
	*y = 0;
	*width  = TheDisplay ? TheDisplay->getWidth() : 0;
	*height = TheDisplay ? TheDisplay->getHeight() : 0;
}

PlayerIndex getObservedOrLocalPlayerIndex_Safe()
{
	if (TheRenderPlayerIndexOverride >= 0)
		return TheRenderPlayerIndexOverride;

	if (Player* player = getObservedOrLocalPlayer_Safe())
		return player->getPlayerIndex();

	return 0;
}

void changeLocalPlayer(Player* player)
{
	DEBUG_ASSERTCRASH(player != nullptr, ("Player is null"));

	ThePlayerList->setLocalPlayer(player);
	TheControlBar->setObserverLookAtPlayer(nullptr);
	TheControlBar->setObservedPlayer(nullptr);
	TheControlBar->setControlBarSchemeByPlayer(player);
	TheControlBar->initSpecialPowershortcutBar(player);

	detail::changePlayerCommon(player);
}

void changeObservedPlayer(Player* player)
{
	TheControlBar->setObserverLookAtPlayer(player);

	const Bool canBeginObservePlayer = TheGlobalData->m_enablePlayerObserver && TheGhostObjectManager->trackAllPlayers();
	const Bool canEndObservePlayer = TheControlBar->getObservedPlayer() != nullptr && TheControlBar->getObserverLookAtPlayer() == nullptr;

	if (canBeginObservePlayer || canEndObservePlayer)
	{
		TheControlBar->setObservedPlayer(player);

		Player *becomePlayer = player;
		if (becomePlayer == nullptr)
			becomePlayer = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey("ReplayObserver"));
		detail::changePlayerCommon(becomePlayer);
	}
}

} // namespace rts
