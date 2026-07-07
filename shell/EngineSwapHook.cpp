#include "PreRTS.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindow.h"
#include "Common/GameEngine.h"

// Global flag inside namespaced engine indicating engine swap is requested
bool g_isEngineSwapPending = false;

// Custom input callback for the logo window
WindowMsgHandledType LogoClickInputCallback(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
    if (msg == GWM_LEFT_UP)
    {
        // Trigger engine swap
        g_isEngineSwapPending = true;
        
        // Signal the game engine to quit cleanly
        if (TheGameEngine)
        {
            TheGameEngine->setQuitting(true);
        }
        return MSG_HANDLED;
    }
    return MSG_IGNORED;
}

// Function called every frame in GameWindowManager::update()
void checkForAndCreateEngineSwapButton()
{
    static GameWindow *s_hookedLogoWin = nullptr;
    
    if (!TheWindowManager || !TheNameKeyGenerator)
        return;
        
    static NameKeyType s_logoKey = TheNameKeyGenerator->nameToKey("MainMenu.wnd:Logo");
    GameWindow *logoWin = TheWindowManager->winGetWindowFromId(nullptr, s_logoKey);
    
    if (logoWin)
    {
        if (s_hookedLogoWin != logoWin)
        {
            // Bind our input callback to intercept clicks on the logo
            logoWin->winSetInputFunc(LogoClickInputCallback);
            s_hookedLogoWin = logoWin;
        }
    }
    else
    {
        s_hookedLogoWin = nullptr;
    }
}
