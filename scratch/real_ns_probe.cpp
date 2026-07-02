// Spike 1.1 Part 2 — real engine headers, wrapped-namespace discipline.
// Structured like a real engine .cpp: PreRTS.h first (at global scope), then
// definitions inside a namespace using REAL engine types (AsciiString), plus a
// colliding-name global singleton + RunEngine entry.
#include "Utility/CppMacros.h"      // force-included first by the real build's PCH
#include "PreRTS.h"                 // the header every engine cpp includes first
#include "Common/AsciiString.h"

namespace ZH {

    struct EngineThing
    {
        AsciiString name;           // real engine type, used from inside namespace
        void init() { name = "Zero Hour engine"; }
    };

    EngineThing* TheGameEngine = nullptr;   // colliding global name

    int RunEngine()
    {
        TheGameEngine = new EngineThing();
        TheGameEngine->init();
        return TheGameEngine->name.getLength();
    }
}
