/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "GameClient/PlayerContext.h"
#include <algorithm>
#include "GameClient/GameWindowManager.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/Keyboard.h"
#include "GameClient/View.h"

// Global active context
PlayerContext* TheActivePlayerContext = nullptr;

// Static members for MultiPlayerManager
std::vector<PlayerContext*> MultiPlayerManager::m_players;
PlayerContext* MultiPlayerManager::m_activePlayer = nullptr;

PlayerContext::PlayerContext()
    : m_windowManager(nullptr)
    , m_controlBar(nullptr)
    , m_inGameUI(nullptr)
    , m_mouse(nullptr)
    , m_keyboard(nullptr)
    , m_view(nullptr)
    , m_playerIndex(-1)
    , m_isGamepadControlled(false)
    , m_gamepadID(0)
{
    m_viewport = {0, 0, 800, 600};
}

PlayerContext::~PlayerContext()
{
    // Subsystems should be cleaned up by whoever owns them or via MultiPlayerManager::shutdown
}

void PlayerContext::init(Int playerIndex, ViewportRect rect)
{
    m_playerIndex = playerIndex;
    m_viewport = rect;
}

void PlayerContext::screenToLocal(Int screenX, Int screenY, Int& localX, Int& localY)
{
    localX = screenX - m_viewport.x;
    localY = screenY - m_viewport.y;
}

void PlayerContext::localToScreen(Int localX, Int localY, Int& screenX, Int& screenY)
{
    screenX = localX + m_viewport.x;
    screenY = localY + m_viewport.y;
}

void MultiPlayerManager::init()
{
    // Always start with at least one player.
    // Do NOT call setActivePlayer() here — TheActivePlayerContext must stay null
    // so all context-aware macros (TheControlBar, TheInGameUI, etc.) fall back to
    // the global singletons during normal (non-splitscreen) gameplay.
    // setActivePlayer() is only called temporarily inside the per-player
    // render and input loops, then cleared by the caller.
    addPlayer();
}

void MultiPlayerManager::shutdown()
{
    for (auto player : m_players)
    {
        delete player;
    }
    m_players.clear();
    m_activePlayer = nullptr;
    TheActivePlayerContext = nullptr;
}

void MultiPlayerManager::removePlayer(Int index)
{
    if (index < 0 || index >= (Int)m_players.size())
        return;
    delete m_players[index];
    m_players.erase(m_players.begin() + index);
    // Re-index remaining players
    for (Int i = index; i < (Int)m_players.size(); ++i)
        m_players[i]->m_playerIndex = i;
    // If active player was removed or is now out of bounds, fall back to player 0
    if (m_players.empty())
    {
        m_activePlayer = nullptr;
        TheActivePlayerContext = nullptr;
    }
    else if (!m_activePlayer || std::find(m_players.begin(), m_players.end(), m_activePlayer) == m_players.end())
    {
        setActivePlayer(0);
    }
}

Int MultiPlayerManager::addPlayer()
{
    PlayerContext* newPlayer = new PlayerContext();
    Int index = (Int)m_players.size();
    m_players.push_back(newPlayer);
    
    // Default viewport - will be overwritten by autoLayout
    newPlayer->init(index, {0, 0, 800, 600});
    
    return index;
}

void MultiPlayerManager::setActivePlayer(Int index)
{
    if (index >= 0 && index < (Int)m_players.size())
    {
        m_activePlayer = m_players[index];
        TheActivePlayerContext = m_activePlayer;
    }
}

void MultiPlayerManager::clearActivePlayer()
{
    m_activePlayer = nullptr;
    TheActivePlayerContext = nullptr;
}

Int MultiPlayerManager::getPlayerAtPoint(Int x, Int y)
{
    for (Int i = 0; i < (Int)m_players.size(); ++i)
    {
        const ViewportRect& r = m_players[i]->m_viewport;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
        {
            return i;
        }
    }
    return -1;
}

void MultiPlayerManager::autoLayoutViewports(Int screenW, Int screenH)
{
    Int count = getPlayerCount();
    if (count <= 1)
    {
        if (count == 1) setPlayerViewport(0, {0, 0, screenW, screenH});
        return;
    }

    if (count == 2)
    {
        // Side-by-side or Over-under based on aspect ratio
        if (screenW > screenH)
        {
            setPlayerViewport(0, {0, 0, screenW / 2, screenH});
            setPlayerViewport(1, {screenW / 2, 0, screenW / 2, screenH});
        }
        else
        {
            setPlayerViewport(0, {0, 0, screenW, screenH / 2});
            setPlayerViewport(1, {0, screenH / 2, screenW, screenH / 2});
        }
    }
    else if (count <= 4)
    {
        // 2x2 grid
        Int w = screenW / 2;
        Int h = screenH / 2;
        setPlayerViewport(0, {0, 0, w, h});
        setPlayerViewport(1, {w, 0, w, h});
        if (count > 2) setPlayerViewport(2, {0, h, w, h});
        if (count > 3) setPlayerViewport(3, {w, h, w, h});
    }
    else
    {
        // 3x3 or larger... generic grid
        Int cols = (Int)ceil(sqrt((double)count));
        Int rows = (Int)ceil((double)count / cols);
        Int w = screenW / cols;
        Int h = screenH / rows;
        
        for (Int i = 0; i < count; ++i)
        {
            Int r = i / cols;
            Int c = i % cols;
            setPlayerViewport(i, {c * w, r * h, w, h});
        }
    }
}

void MultiPlayerManager::setPlayerViewport(Int index, ViewportRect rect)
{
    if (index >= 0 && index < (Int)m_players.size())
    {
        m_players[index]->m_viewport = rect;
    }
}

PlayerContext* MultiPlayerManager::getPlayer(Int index)
{
    if (index >= 0 && index < (Int)m_players.size())
        return m_players[index];
    return nullptr;
}
