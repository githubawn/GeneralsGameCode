/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "Win32Device/GameClient/Win32GameInput.h"
#include "Common/Debug.h"

#if (_MSC_VER >= 1930) // VS 2022

GameInputManager* GameInputManager::m_instance = nullptr;

GameInputManager* GameInputManager::getInstance()
{
    if (!m_instance)
    {
        m_instance = new GameInputManager();
    }
    return m_instance;
}

GameInputManager::GameInputManager()
    : m_gameInput(nullptr)
{
}

GameInputManager::~GameInputManager()
{
    shutdown();
}

Bool GameInputManager::init()
{
    if (m_gameInput) return TRUE;
    
    HRESULT hr = GameInput::v3::GameInputCreate(&m_gameInput);
    if (FAILED(hr))
    {
        DEBUG_LOG(("ERROR - GameInputManager: GameInputCreate failed with 0x%08x", hr));
        m_gameInput = nullptr;
        return FALSE;
    }
    
    DEBUG_LOG(("OK - GameInputManager initialized successfully."));
    return TRUE;
}

void GameInputManager::shutdown()
{
    if (m_gameInput)
    {
        m_gameInput->Release();
        m_gameInput = nullptr;
        DEBUG_LOG(("OK - GameInputManager shutdown."));
    }
}

#endif
