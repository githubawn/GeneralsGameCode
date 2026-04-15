/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "Win32Device/GameClient/InputDebugger.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "WW3D2/font3d.h"
#include "Common/GlobalData.h"
#include "GameLogic/GameLogic.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "Win32Device/GameClient/Win32GameInput.h"
#include "Win32Device/GameClient/Win32DIKeyboard.h"
#include "Win32Device/GameClient/Win32GameInputKeyboard.h"
#include "Win32Device/GameClient/Win32GameInputMouse.h"

InputDebugger* InputDebugger::m_instance = nullptr;
InputDebugger* TheInputDebugger = nullptr;

InputDebugger::InputDebugger() :
    m_enabled(FALSE),
    m_renderer(nullptr),
    m_font(nullptr),
    m_legacyKeyboard(nullptr),
    m_legacyMouse(nullptr)
{
    memset(&m_legacyState, 0, sizeof(m_legacyState));
    memset(&m_modernState, 0, sizeof(m_modernState));
}

InputDebugger::~InputDebugger()
{
    delete m_renderer;
    delete m_legacyKeyboard;
    delete m_legacyMouse;
}

InputDebugger* InputDebugger::getInstance()
{
    if (!m_instance) {
        m_instance = new InputDebugger();
        TheInputDebugger = m_instance;
    }
    return m_instance;
}

void InputDebugger::init()
{
    if (!m_renderer) {
        m_renderer = new Render2DTextClass();
    }

    // Instantiate legacy monitors
    if (!m_legacyKeyboard) {
        m_legacyKeyboard = new DirectInputKeyboard();
        m_legacyKeyboard->init();
    }
    if (!m_legacyMouse) {
        m_legacyMouse = new W3DMouse();
        m_legacyMouse->init();
    }
}

void InputDebugger::updateStats()
{
#if (_MSC_VER >= 1930)
    // Sample modern state from the active client instance
    GameInputKeyboard* modernKb = dynamic_cast<GameInputKeyboard*>(TheKeyboard);
    GameInputMouse* modernMouse = dynamic_cast<GameInputMouse*>(TheMouse);

    if (modernKb) {
        m_modernState.queueCount = modernKb->getQueueDepth();
        m_modernState.lastKey = modernKb->getLastScanCode();
    }
    if (modernMouse) {
        m_modernState.queueCount = modernMouse->getQueueDepth();
        m_modernState.lastMouseButtons = modernMouse->getLastButtons();
    }
#endif

    // Sample legacy state
    if (m_legacyKeyboard) {
        m_legacyKeyboard->update();
        KeyboardIO kbIO;
        m_legacyKeyboard->getKey(&kbIO);
        while (kbIO.key != KEY_NONE) {
            if (kbIO.state == KEY_STATE_DOWN) {
                m_legacyState.lastKey = kbIO.key;
            }
            m_legacyState.queueCount++; 
            m_legacyKeyboard->getKey(&kbIO);
        }
        // Manual decay for visual comparison
        if (m_legacyState.queueCount > 0 && !(TheGameLogic->getFrame() % 10)) m_legacyState.queueCount--;
    }

    if (m_legacyMouse) {
        m_legacyMouse->update();
        const MouseIO* mouseIO = m_legacyMouse->getMouseStatus();
        m_legacyState.lastMouseX = mouseIO->pos.x;
        m_legacyState.lastMouseY = mouseIO->pos.y;
        m_legacyState.lastMouseButtons = (mouseIO->leftState ? 1 : 0) | (mouseIO->rightState ? 2 : 0) | (mouseIO->middleState ? 4 : 0);
    }
}

void InputDebugger::render()
{
    // Enable by default for testing
    m_enabled = TRUE; 

    if (!m_enabled || !m_renderer) return;

    W3DDisplay* display = (W3DDisplay*)TheDisplay;
    if (display && !m_font) {
        // Find a safe font
        if (TheFontLibrary) {
            GameFont* gfont = TheFontLibrary->getFont("FixedSys", 8, FALSE);
            if (gfont) {
                m_font = (Font3DInstanceClass*)gfont->fontData;
                m_renderer->Set_Font(m_font);
            }
        }
    }

    if (!m_font) return; // Need a font to draw

    updateStats();

    m_renderer->Reset();
    m_renderer->Set_Location(Vector2(25.0f, 150.0f)); // Move it down a bit to clear other HUDs
    
    char buffer[1024];
    sprintf(buffer, "=== [ INPUT DEBUGGER - SIDE BY SIDE ] ===\n"
                    "FEATURE          | LEGACY (DInput/Win32) | MODERN (GameInput)\n"
                    "-----------------|-----------------------|-------------------\n"
                    "Last Key         | 0x%02X                  | 0x%02X\n"
                    "Queue Depth      | %-2d                    | %-2d\n"
                    "Mouse Buttons    | %-3d                   | %-3d\n"
                    "Mouse Pos        | (%4d, %4d)         | (OS Bound)\n"
                    "Status           | MONITORING            | %s\n",
                    m_legacyState.lastKey, m_modernState.lastKey,
                    m_legacyState.queueCount, m_modernState.queueCount,
                    m_legacyState.lastMouseButtons, m_modernState.lastMouseButtons,
                    m_legacyState.lastMouseX, m_legacyState.lastMouseY,
                    (TheGlobalData->m_useGameInput ? "ACTIVE" : "STANDBY"));

    // Draw background box for "floating" effect
    m_renderer->Draw_Block(RectClass(20, 145, 520, 280), 0x88000000);

    // Draw shadow
    m_renderer->Set_Location(Vector2(26.0f, 151.0f));
    m_renderer->Draw_Text(buffer, 0xFF000000); 
    
    // Draw text (Lime Green for visibility)
    m_renderer->Set_Location(Vector2(25.0f, 150.0f));
    m_renderer->Draw_Text(buffer, 0xFF00FF00); 

    m_renderer->Render();
}
