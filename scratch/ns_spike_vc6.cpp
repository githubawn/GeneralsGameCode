// Spike 1.1 — VC6 (C++98) variant of the concept proof.
// Two engines with IDENTICAL global names, separated by namespaces, one exe.
// C++98-safe: no nullptr, <stdio.h> instead of <cstdio>.
#include <stdio.h>
#include <string>

namespace Gen {
    class GameEngine {
    public:
        std::string name() const { return "Generals"; }
        int run() { printf("[%s] engine running\n", name().c_str()); return 0; }
    };
    GameEngine* TheGameEngine = 0;                    // colliding global name
    int RunEngine() { TheGameEngine = new GameEngine(); return TheGameEngine->run(); }
}

namespace ZH {
    class GameEngine {
    public:
        std::string name() const { return "Zero Hour"; }
        int run() { printf("[%s] engine running\n", name().c_str()); return 0; }
    };
    GameEngine* TheGameEngine = 0;                    // SAME global name, different symbol
    int RunEngine() { TheGameEngine = new GameEngine(); return TheGameEngine->run(); }
}

int main(int argc, char** argv) {
    const char* which = (argc > 1) ? argv[1] : "gen";
    if (std::string(which) == "zh") return ZH::RunEngine();
    return Gen::RunEngine();
}
