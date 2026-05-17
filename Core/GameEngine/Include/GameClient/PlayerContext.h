/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#include "Lib/BaseType.h"
#include <vector>

// Forward declarations of engine classes
class GameWindowManager;
class ControlBar;
class InGameUI;
class Mouse;
class Keyboard;
class View;

/**
 * ViewportRect: Defines the screen region for a player
 */
struct ViewportRect {
    Int x, y, w, h;
};

/**
 * PlayerContext: Encapsulates all state for a single local player viewport.
 * This allows multiple "TheWindowManager" etc. to exist in one process.
 */
class PlayerContext {
public:
    PlayerContext();
    ~PlayerContext();

    void init(Int playerIndex, ViewportRect rect);
    void update();

    // View-specific subsystems
    GameWindowManager* m_windowManager;
    ControlBar*        m_controlBar;
    InGameUI*          m_inGameUI;
    Mouse*             m_mouse;
    Keyboard*          m_keyboard;
    View*              m_view;
    
    ViewportRect       m_viewport;
    Int                m_playerIndex; 
    
    // Input routing state
    Bool               m_isGamepadControlled;
    Int                m_gamepadID; ///< SDL_JoystickID stored as Int to avoid SDL3 dependency in this header

    // Viewport-aware coordinate translation
    void screenToLocal(Int screenX, Int screenY, Int& localX, Int& localY);
    void localToScreen(Int localX, Int localY, Int& screenX, Int& screenY);
};

/**
 * MultiPlayerManager: Manages the collection of local players and the active context.
 */
class MultiPlayerManager {
public:
    static void init();
    static void shutdown();
    
    static Int addPlayer(); // Returns player index
    static void removePlayer(Int index);
    
    static void setPlayerViewport(Int index, ViewportRect rect);
    static void autoLayoutViewports(Int screenW, Int screenH);
    
    static PlayerContext* getPlayer(Int index);
    static Int getPlayerCount() { return (Int)m_players.size(); }
    
    static void setActivePlayer(Int index);
    static void clearActivePlayer(); ///< Reset active context to null so macros fall back to global singletons.
    static PlayerContext* getActivePlayer() { return m_activePlayer; }
    
    static Int getPlayerAtPoint(Int x, Int y);

private:
    static std::vector<PlayerContext*> m_players;
    static PlayerContext* m_activePlayer;
};

/**
 * The core "Context Swap" macro. 
 * This allows existing code to work by redirecting global pointers 
 * to the currently rendering/updating player.
 */
extern PlayerContext* TheActivePlayerContext;

// These will be defined in a way that doesn't conflict with existing externs
// once we start the migration of GameWindowManager.cpp etc.
