#pragma once
#include "WebWin32.h"

typedef struct joycaps_tag {
    WORD wMid;
    WORD wPid;
    CHAR szPname[32];
    UINT wXmin;
    UINT wXmax;
    UINT wYmin;
    UINT wYmax;
    UINT wZmin;
    UINT wZmax;
    UINT wNumButtons;
    UINT wPeriodMin;
    UINT wPeriodMax;
    UINT wRmin;
    UINT wRmax;
    UINT wUmin;
    UINT wUmax;
    UINT wVmin;
    UINT wVmax;
    UINT wCaps;
    UINT wMaxAxes;
    UINT wNumAxes;
    UINT wMaxButtons;
    CHAR szRegKey[32];
    CHAR szOEMVxD[260];
} JOYCAPSA, *PJOYCAPSA, *LPJOYCAPSA;

typedef struct joyinfoex_tag {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwXpos;
    DWORD dwYpos;
    DWORD dwZpos;
    DWORD dwRpos;
    DWORD dwUpos;
    DWORD dwVpos;
    DWORD dwButtons;
    DWORD dwButtonNumber;
    DWORD dwPOV;
    DWORD dwReserved1;
    DWORD dwReserved2;
} JOYINFOEX, *PJOYINFOEX, *LPJOYINFOEX;

#define JOY_RETURNALL        0x000000FF
#define JOY_RETURNCENTERED   0x00000100
#define JOYSTICKID1          0
#define JOYSTICKID2          1

#define MMSYSERR_NOERROR     0

inline UINT joyGetNumDevs() { return 0; }
inline UINT joyGetDevCapsA(UINT_PTR uJoyID, LPJOYCAPSA pjc, UINT cbjc) { return 1; }
inline UINT joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji) { return 1; }

inline DWORD timeGetTime() { return (DWORD)SDL_GetTicks(); }
