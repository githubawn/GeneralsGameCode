/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#pragma once

#include "W3DDevice/GameClient/W3DMouse.h"

#if (_MSC_VER >= 1930) // VS 2022
#include "Win32Device/GameClient/Win32GameInput.h"

/**
 * Mouse implementation using modern GameInput API.
 * Inherits from W3DMouse to reuse cursor rendering logic.
 */
class GameInputMouse : public W3DMouse
{
public:
    GameInputMouse();
    virtual ~GameInputMouse() override;

    virtual void init() override;
    virtual void update() override;
    
    // Diagnostic methods for Debugger
    Int getQueueDepth() const;
    uint32_t getLastButtons() const { return m_lastButtons; }

protected:
    virtual UnsignedByte getMouseEvent(MouseIO *result, Bool flush) override;

private:
    void processReadings();
    
    // Internal queue for MouseIO events
    enum { MOUSE_QUEUE_SIZE = 128 };
    MouseIO m_mouseQueue[MOUSE_QUEUE_SIZE];
    Int m_queueHead;
    Int m_queueTail;
    
    void enqueueMouse(const MouseIO& mouse);
    Bool dequeueMouse(MouseIO* mouse);
    
    uint32_t m_lastButtons;
};

#endif
