#include <windows.h>
#include <string.h>

// Forward declarations of namespaced RunEngine functions
namespace Gen {
    int RunEngine(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
    extern bool g_isEngineSwapPending;
}
namespace ZH {
    int RunEngine(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
    extern bool g_isEngineSwapPending;
}

// Target engine choice: 1 = Generals, 2 = Zero Hour
int g_targetEngine = 2; // Default to Zero Hour

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Parse command line to choose starting engine
    if (strstr(lpCmdLine, "-generals") || strstr(lpCmdLine, "/generals"))
    {
        g_targetEngine = 1;
    }
    else if (strstr(lpCmdLine, "-zerohour") || strstr(lpCmdLine, "/zerohour"))
    {
        g_targetEngine = 2;
    }

    int exitCode = 0;
    bool swapPending = false;

    do
    {
        // Reset swap pending flags
        Gen::g_isEngineSwapPending = false;
        ZH::g_isEngineSwapPending = false;
        swapPending = false;

        if (g_targetEngine == 1)
        {
            exitCode = Gen::RunEngine(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
            if (Gen::g_isEngineSwapPending)
            {
                swapPending = true;
                g_targetEngine = 2;
            }
        }
        else
        {
            exitCode = ZH::RunEngine(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
            if (ZH::g_isEngineSwapPending)
            {
                swapPending = true;
                g_targetEngine = 1;
            }
        }
    } while (swapPending);

    return exitCode;
}
