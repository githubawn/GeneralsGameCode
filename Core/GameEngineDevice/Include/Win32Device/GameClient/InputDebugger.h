/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#pragma once

#include "always.h"
#include "Win32Device/GameClient/Win32DIKeyboard.h"
#include "Win32Device/GameClient/Win32Mouse.h"
#include "W3DDevice/GameClient/W3DMouse.h"
#include "WW3D2/render2d.h"

#if (_MSC_VER >= 1930) // VS 2022
#include "Win32Device/GameClient/Win32GameInput.h"
#include "Win32Device/GameClient/Win32GameInputKeyboard.h"
#include "Win32Device/GameClient/Win32GameInputMouse.h"
#endif

class Font3DInstanceClass;

/**
 * Integrated Input Debugger that compares Legacy and Modern input systems side-by-side.
 * Self-contained to minimize dependency on engine versions.
 */
class InputDebugger
{
public:
    static InputDebugger* getInstance();
    
    void init();
    void update();
    void render();
    
    void toggle() { m_enabled = !m_enabled; }
    Bool isEnabled() const { return m_enabled; }

private:
    InputDebugger();
    ~InputDebugger();

    static InputDebugger* m_instance;
    Bool m_enabled;
    
    // Low-level rendering
    Render2DTextClass* m_renderer;
    Font3DInstanceClass* m_font;
    
    // Legacy Monitors
    DirectInputKeyboard* m_legacyKeyboard;
    W3DMouse* m_legacyMouse;
    
    struct InputState {
        UnsignedInt lastKey;
        Int lastMouseX;
        Int lastMouseY;
        UnsignedInt lastMouseButtons;
        Int queueCount;
    };
    
    InputState m_legacyState;
    InputState m_modernState;

    void updateStats();
};

extern InputDebugger* TheInputDebugger;
