/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "Win32Device/GameClient/Win32GameInputKeyboard.h"

#if (_MSC_VER >= 1930) // VS 2022
#include "Common/Debug.h"
#include <windows.h>

GameInputKeyboard::GameInputKeyboard()
    : m_queueHead(0)
    , m_queueTail(0)
    , m_lastScanCode(0)
{
    memset(m_lastKeyState, 0, sizeof(m_lastKeyState));
}

GameInputKeyboard::~GameInputKeyboard()
{
}

void GameInputKeyboard::init()
{
    Keyboard::init();
    if (TheGameInputManager)
    {
        TheGameInputManager->init();
    }
}

void GameInputKeyboard::reset()
{
    Keyboard::reset();
    m_queueHead = m_queueTail = 0;
    memset(m_lastKeyState, 0, sizeof(m_lastKeyState));
}

void GameInputKeyboard::update()
{
    Keyboard::update();
    processReadings();
}

Bool GameInputKeyboard::getCapsState()
{
    return (GetKeyState(VK_CAPITAL) & 0x01) != 0;
}

void GameInputKeyboard::getKey(KeyboardIO *key)
{
    if (!dequeueKey(key))
    {
        key->key = KEY_NONE;
    }
}

void GameInputKeyboard::processReadings()
{
    if (!TheGameInputManager || !TheGameInputManager->getGameInput()) return;

    GameInput::v3::IGameInput* gi = TheGameInputManager->getGameInput();
    GameInput::v3::IGameInputReading* reading = nullptr;

    // Get the most recent reading
    if (SUCCEEDED(gi->GetCurrentReading(GameInput::v3::GameInputKindKeyboard, nullptr, &reading)))
    {
        uint32_t keyCount = reading->GetKeyState(0, nullptr);
        if (keyCount > 0)
        {
            GameInput::v3::GameInputKeyState* states = (GameInput::v3::GameInputKeyState*)_alloca(sizeof(GameInput::v3::GameInputKeyState) * keyCount);
            reading->GetKeyState(keyCount, states);

            // This is just current state. For Generals, we should ideally use GetNextReading
            // to catch all pressed/released transitions between frames.
            // But for a simple implementation, let's just track state changes here.
            
            UnsignedByte currentKeyState[NUM_KEYS];
            memset(currentKeyState, 0, sizeof(currentKeyState));

            for (uint32_t i = 0; i < keyCount; ++i)
            {
                UnsignedByte scanCode = (UnsignedByte)(states[i].scanCode & 0xFF);
                if (scanCode < NUM_KEYS)
                {
                    currentKeyState[scanCode] = 1;
                    if (!m_lastKeyState[scanCode])
                    {
                        // Key Pressed
                        KeyboardIO kio;
                        kio.key = scanCode;
                        kio.state = KEY_STATE_DOWN;
                        kio.status = KeyboardIO::STATUS_UNUSED;
                        kio.keyDownTimeMsec = (UnsignedInt)reading->GetTimestamp();
                        enqueueKey(kio);
                    }
                }
            }

            for (Int i = 0; i < NUM_KEYS; ++i)
            {
                if (m_lastKeyState[i] && !currentKeyState[i])
                {
                    // Key Released
                    KeyboardIO kio;
                    kio.key = (UnsignedByte)i;
                    kio.state = KEY_STATE_UP;
                    kio.status = KeyboardIO::STATUS_UNUSED;
                    kio.keyDownTimeMsec = (UnsignedInt)reading->GetTimestamp();
                    enqueueKey(kio);
                }
            }

            memcpy(m_lastKeyState, currentKeyState, sizeof(m_lastKeyState));
        }
        else
        {
            // No keys down, check if we need to release anything
            for (Int i = 0; i < NUM_KEYS; ++i)
            {
                if (m_lastKeyState[i])
                {
                    KeyboardIO kio;
                    kio.key = (UnsignedByte)i;
                    kio.state = KEY_STATE_UP;
                    kio.status = KeyboardIO::STATUS_UNUSED;
                    kio.keyDownTimeMsec = (UnsignedInt)reading->GetTimestamp();
                    enqueueKey(kio);
                    m_lastKeyState[i] = 0;
                }
            }
        }
        reading->Release();
    }
}

void GameInputKeyboard::enqueueKey(const KeyboardIO& key)
{
    m_lastScanCode = key.key;
    Int nextTail = (m_queueTail + 1) % KEY_QUEUE_SIZE;
    if (nextTail != m_queueHead)
    {
        m_keyQueue[m_queueTail] = key;
        m_queueTail = nextTail;
    }
}

Bool GameInputKeyboard::dequeueKey(KeyboardIO* key)
{
    if (m_queueHead == m_queueTail) return FALSE;
    *key = m_keyQueue[m_queueHead];
    m_queueHead = (m_queueHead + 1) % KEY_QUEUE_SIZE;
    return TRUE;
}

Int GameInputKeyboard::getQueueDepth() const
{
    if (m_queueTail >= m_queueHead) {
        return m_queueTail - m_queueHead;
    } else {
        return (KEY_QUEUE_SIZE - m_queueHead) + m_queueTail;
    }
}

#endif
