// Spike 1.1 — proof that two engines with IDENTICAL global symbol names can
// coexist in ONE statically-linked exe via namespaces (no DLLs).
//
// This mimics the real collision: both engines define a class GameEngine, a
// global singleton TheGameEngine, and a RunEngine() entry point with the SAME
// names. In the current codebase these are at global scope in both trees, so
// linking them together is a duplicate-symbol error. Wrapping each tree in a
// namespace makes the symbols distinct.

#include <cstdio>   // includes stay at GLOBAL scope (the wrapping discipline)
#include <string>

// ----- "Generals" engine tree, wrapped ---------------------------------------
namespace Gen {
    class GameEngine {
    public:
        std::string name() const { return "Generals"; }
        int run() { std::printf("[%s] engine running\n", name().c_str()); return 0; }
    };
    GameEngine* TheGameEngine = nullptr;              // colliding global name
    int RunEngine() { TheGameEngine = new GameEngine(); return TheGameEngine->run(); }
}

// ----- "Zero Hour" engine tree, wrapped --------------------------------------
namespace ZH {
    class GameEngine {
    public:
        std::string name() const { return "Zero Hour"; }
        int run() { std::printf("[%s] engine running\n", name().c_str()); return 0; }
    };
    GameEngine* TheGameEngine = nullptr;              // SAME global name, different symbol
    int RunEngine() { TheGameEngine = new GameEngine(); return TheGameEngine->run(); }
}

// ----- scaffold: selector "main", one engine active at a time ----------------
int main(int argc, char** argv) {
    const char* which = (argc > 1) ? argv[1] : "gen";
    if (std::string(which) == "zh") return ZH::RunEngine();
    return Gen::RunEngine();
}
