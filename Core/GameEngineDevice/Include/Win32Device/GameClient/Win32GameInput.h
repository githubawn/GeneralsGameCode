/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#pragma once

#include "Lib/BaseType.h"

#if (_MSC_VER >= 1930) // VS 2022 and later
#include <windows.h>
#include <GameInput.h>

/**
 * GameInputManager singleton.
 * Manages the IGameInput interface and shared resources.
 */
class GameInputManager
{
public:
    static GameInputManager* getInstance();
    
    Bool init();
    void shutdown();
    
    GameInput::v3::IGameInput* getGameInput() { return m_gameInput; }
    
private:
    GameInputManager();
    ~GameInputManager();
    
    GameInput::v3::IGameInput* m_gameInput;
    static GameInputManager* m_instance;
};

#define TheGameInputManager (GameInputManager::getInstance())

#else

// Dummy or disabled for VC6
class GameInputManager
{
public:
    static GameInputManager* getInstance() { return nullptr; }
    Bool init() { return FALSE; }
    void shutdown() {}
};

#define TheGameInputManager ((GameInputManager*)nullptr)

#endif
