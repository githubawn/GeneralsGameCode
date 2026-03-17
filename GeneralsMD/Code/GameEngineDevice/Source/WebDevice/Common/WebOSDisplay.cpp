/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "Common/OSDisplay.h"
#include "Common/AsciiString.h"
#include <emscripten.h>

OSDisplayButtonType OSDisplayWarningBox(AsciiString p, AsciiString m, UnsignedInt buttonFlags, UnsignedInt otherFlags)
{
    // Simple browser alert/confirm shim
    EM_ASM({
        alert(UTF8ToString($0) + "\n\n" + UTF8ToString($1));
    }, p.str(), m.str());

    return OSDBT_OK;
}

void OSDisplaySetBusyState(Bool busyDisplay, Bool busySystem) {
    // No-op in browser for now
}
