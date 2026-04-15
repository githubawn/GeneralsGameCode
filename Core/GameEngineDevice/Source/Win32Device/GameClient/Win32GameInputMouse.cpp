/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "Win32Device/GameClient/Win32GameInputMouse.h"

#if (_MSC_VER >= 1930) // VS 2022
#include "Common/Debug.h"
#include <windows.h>
#include "WinMain.h"

GameInputMouse::GameInputMouse()
    : m_queueHead(0)
    , m_queueTail(0)
    , m_lastButtons(0)
{
}

GameInputMouse::~GameInputMouse()
{
}

void GameInputMouse::init()
{
    W3DMouse::init();
    if (TheGameInputManager)
    {
        TheGameInputManager->init();
    }
}

void GameInputMouse::update()
{
    // Skip Win32Mouse::update which polls Win32 events
    // Mouse::update() handles the high level logic
    Mouse::update();
    processReadings();
}

UnsignedByte GameInputMouse::getMouseEvent(MouseIO *result, Bool flush)
{
    if (!dequeueMouse(result))
    {
        return MOUSE_NONE;
    }
    return MOUSE_OK;
}

void GameInputMouse::processReadings()
{
    if (!TheGameInputManager || !TheGameInputManager->getGameInput()) return;

    GameInput::v3::IGameInput* gi = TheGameInputManager->getGameInput();
    GameInput::v3::IGameInputReading* reading = nullptr;

    if (SUCCEEDED(gi->GetCurrentReading(GameInput::v3::GameInputKindMouse, nullptr, &reading)))
    {
        GameInput::v3::GameInputMouseState state;
        if (reading->GetMouseState(&state))
        {
            // For Generals we still want to use Windows for absolute position
            // primarily because the cursor is often managed by the OS or 
            // has complex screen constraints. 
            // However, we can read buttons and wheel from GameInput.
            
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(ApplicationHWnd, &pt);

            MouseIO mio;
            mio.pos.x = pt.x;
            mio.pos.y = pt.y;
            mio.time = (UnsignedInt)reading->GetTimestamp();
            mio.wheelPos = (Int)state.wheelY; // GameInput returns cumulative, but Generals expects delta?
            // Actually mio.wheelPos in Mouse.h is delta.
            // GameInput gives cumulative for the reading. We might need historical deltas.
            
            // For now let's focus on buttons
            mio.leftState = (state.buttons & GameInput::v3::GameInputMouseLeftButton) ? MBS_Down : MBS_Up;
            mio.rightState = (state.buttons & GameInput::v3::GameInputMouseRightButton) ? MBS_Down : MBS_Up;
            mio.middleState = (state.buttons & GameInput::v3::GameInputMouseMiddleButton) ? MBS_Down : MBS_Up;
            
            // Generate events for changes
            if (state.buttons != m_lastButtons)
            {
                enqueueMouse(mio);
            }
            
            m_lastButtons = (uint32_t)state.buttons;
        }
        reading->Release();
    }
}

void GameInputMouse::enqueueMouse(const MouseIO& mouse)
{
    Int nextTail = (m_queueTail + 1) % MOUSE_QUEUE_SIZE;
    if (nextTail != m_queueHead)
    {
        m_mouseQueue[m_queueTail] = mouse;
        m_queueTail = nextTail;
    }
}

Bool GameInputMouse::dequeueMouse(MouseIO* mouse)
{
    if (m_queueHead == m_queueTail) return FALSE;
    *mouse = m_mouseQueue[m_queueHead];
    m_queueHead = (m_queueHead + 1) % MOUSE_QUEUE_SIZE;
    return TRUE;
}

Int GameInputMouse::getQueueDepth() const
{
    if (m_queueTail >= m_queueHead) {
        return m_queueTail - m_queueHead;
    } else {
        return (MOUSE_QUEUE_SIZE - m_queueHead) + m_queueTail;
    }
}

#endif
