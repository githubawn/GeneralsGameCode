/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#ifndef _WEB_INPUT_H_
#define _WEB_INPUT_H_

#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include <deque>

class WebKeyboard : public Keyboard {
public:
    WebKeyboard();
    virtual ~WebKeyboard();

    virtual void init() override;
    virtual void reset() override;
    virtual void update() override;

    virtual void getKey(KeyboardIO *key) override;
    virtual Bool getCapsState() override;

    static void enqueueKeyEvent(UnsignedByte scanCode, Bool isDown);

private:
    static std::deque<KeyboardIO> m_eventQueue;
};

class WebMouse : public Mouse {
public:
    WebMouse();
    virtual ~WebMouse();

    virtual void init() override;
    virtual void reset() override;
    virtual void update() override;

    virtual UnsignedByte getMouseEvent(MouseIO *result, Bool flush) override;
    
    virtual void setMouseLimits() override {}
    virtual void setPosition(Int x, Int y) override;
    virtual void setCursor(MouseCursor cursor) override;
    virtual void capture() override {}
    virtual void releaseCapture() override {}

    static void enqueueMouseEvent(const MouseIO& event);

private:
    static std::deque<MouseIO> m_eventQueue;
};

#endif
