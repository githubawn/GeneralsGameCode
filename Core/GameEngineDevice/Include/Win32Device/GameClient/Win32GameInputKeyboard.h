/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#pragma once

#include "GameClient/Keyboard.h"

#if (_MSC_VER >= 1930) // VS 2022
#include "Win32Device/GameClient/Win32GameInput.h"

/**
 * Keyboard implementation using modern GameInput API.
 */
class GameInputKeyboard : public Keyboard
{
public:
    GameInputKeyboard();
    virtual ~GameInputKeyboard() override;

    virtual void init() override;
    virtual void reset() override;
    virtual void update() override;

    // Diagnostic methods for Debugger
    Int getQueueDepth() const;
    UnsignedInt getLastScanCode() const { return m_lastScanCode; }
    virtual Bool getCapsState() override;

protected:
    virtual void getKey(KeyboardIO *key) override;

private:
    void processReadings();
    
    // Internal queue for KeyboardIO events since GameInput is reading-based
    enum { KEY_QUEUE_SIZE = 128 };
    KeyboardIO m_keyQueue[KEY_QUEUE_SIZE];
    Int m_queueHead;
    Int m_queueTail;
    
    void enqueueKey(const KeyboardIO& key);
    Bool dequeueKey(KeyboardIO* key);
    
    UnsignedByte m_lastKeyState[NUM_KEYS];
    UnsignedInt m_lastScanCode;
};

#endif
