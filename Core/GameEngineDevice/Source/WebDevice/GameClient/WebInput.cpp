/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "PreRTS.h"
#include "WebDevice/GameClient/WebInput.h"
#include "Common/Debug.h"
#include <SDL2/SDL.h>
#include "WebShims/dinput.h"

std::deque<KeyboardIO> WebKeyboard::m_eventQueue;
std::deque<MouseIO> WebMouse::m_eventQueue;

static UnsignedByte SDLScancodeToDIK(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_A: return DIK_A;
        case SDL_SCANCODE_B: return DIK_B;
        case SDL_SCANCODE_C: return DIK_C;
        case SDL_SCANCODE_D: return DIK_D;
        case SDL_SCANCODE_E: return DIK_E;
        case SDL_SCANCODE_F: return DIK_F;
        case SDL_SCANCODE_G: return DIK_G;
        case SDL_SCANCODE_H: return DIK_H;
        case SDL_SCANCODE_I: return DIK_I;
        case SDL_SCANCODE_J: return DIK_J;
        case SDL_SCANCODE_K: return DIK_K;
        case SDL_SCANCODE_L: return DIK_L;
        case SDL_SCANCODE_M: return DIK_M;
        case SDL_SCANCODE_N: return DIK_N;
        case SDL_SCANCODE_O: return DIK_O;
        case SDL_SCANCODE_P: return DIK_P;
        case SDL_SCANCODE_Q: return DIK_Q;
        case SDL_SCANCODE_R: return DIK_R;
        case SDL_SCANCODE_S: return DIK_S;
        case SDL_SCANCODE_T: return DIK_T;
        case SDL_SCANCODE_U: return DIK_U;
        case SDL_SCANCODE_V: return DIK_V;
        case SDL_SCANCODE_W: return DIK_W;
        case SDL_SCANCODE_X: return DIK_X;
        case SDL_SCANCODE_Y: return DIK_Y;
        case SDL_SCANCODE_Z: return DIK_Z;
        case SDL_SCANCODE_1: return DIK_1;
        case SDL_SCANCODE_2: return DIK_2;
        case SDL_SCANCODE_3: return DIK_3;
        case SDL_SCANCODE_4: return DIK_4;
        case SDL_SCANCODE_5: return DIK_5;
        case SDL_SCANCODE_6: return DIK_6;
        case SDL_SCANCODE_7: return DIK_7;
        case SDL_SCANCODE_8: return DIK_8;
        case SDL_SCANCODE_9: return DIK_9;
        case SDL_SCANCODE_0: return DIK_0;
        case SDL_SCANCODE_RETURN: return DIK_RETURN;
        case SDL_SCANCODE_ESCAPE: return DIK_ESCAPE;
        case SDL_SCANCODE_BACKSPACE: return DIK_BACK;
        case SDL_SCANCODE_TAB: return DIK_TAB;
        case SDL_SCANCODE_SPACE: return DIK_SPACE;
        case SDL_SCANCODE_LSHIFT: return DIK_LSHIFT;
        case SDL_SCANCODE_RSHIFT: return DIK_RSHIFT;
        case SDL_SCANCODE_LCTRL: return DIK_LCONTROL;
        case SDL_SCANCODE_RCTRL: return DIK_RCONTROL;
        case SDL_SCANCODE_LALT: return DIK_LALT;
        case SDL_SCANCODE_RALT: return DIK_RALT;
        case SDL_SCANCODE_UP: return DIK_UPARROW;
        case SDL_SCANCODE_DOWN: return DIK_DOWNARROW;
        case SDL_SCANCODE_LEFT: return DIK_LEFTARROW;
        case SDL_SCANCODE_RIGHT: return DIK_RIGHTARROW;
        default: return 0;
    }
}

WebKeyboard::WebKeyboard() : Keyboard() {
}

WebKeyboard::~WebKeyboard() {
}

void WebKeyboard::init() {
    Keyboard::init();
}

void WebKeyboard::reset() {
    Keyboard::reset();
    m_eventQueue.clear();
}

void WebKeyboard::update() {
    Keyboard::update();
}

void WebKeyboard::getKey(KeyboardIO *key) {
    if (m_eventQueue.empty()) {
        key->key = KEY_NONE;
        return;
    }
    *key = m_eventQueue.front();
    m_eventQueue.pop_front();
}

Bool WebKeyboard::getCapsState() {
    return (SDL_GetModState() & KMOD_CAPS) != 0;
}

void WebKeyboard::enqueueKeyEvent(UnsignedByte sdlScancode, Bool isDown) {
    UnsignedByte dikCode = SDLScancodeToDIK((SDL_Scancode)sdlScancode);
    if (dikCode == 0) return;

    KeyboardIO event;
    event.key = dikCode;
    event.state = isDown ? KEY_STATE_DOWN : KEY_STATE_UP;
    event.status = KeyboardIO::STATUS_UNUSED;
    event.keyDownTimeMsec = SDL_GetTicks();
    m_eventQueue.push_back(event);
}

// WebMouse Implementation
WebMouse::WebMouse() : Mouse() {
}

WebMouse::~WebMouse() {
}

void WebMouse::init() {
    Mouse::init();
}

void WebMouse::reset() {
    Mouse::reset();
    m_eventQueue.clear();
}

void WebMouse::update() {
    Mouse::update();
}

UnsignedByte WebMouse::getMouseEvent(MouseIO *result, Bool flush) {
    if (m_eventQueue.empty()) {
        return MOUSE_NONE;
    }
    *result = m_eventQueue.front();
    if (!flush) {
        m_eventQueue.pop_front();
    }
    return MOUSE_OK;
}

void WebMouse::setPosition(Int x, Int y) {
    Mouse::setPosition(x, y);
    // On web, we probably can't force the mouse position easily without full lock
}

void WebMouse::setCursor(MouseCursor cursor) {
    // Stub: Mapping internal cursor to browser/SDL cursor
}

void WebMouse::enqueueMouseEvent(const MouseIO& event) {
    m_eventQueue.push_back(event);
}
