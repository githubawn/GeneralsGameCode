// TheSuperHackers @build bobtista 13/06/2026 Minimal <excpt.h> shim for
// non-Windows builds. Structured Exception Handling (__try/__except) is
// Windows-only; the engine guards its actual SEH usage behind _WIN32, so this
// header only needs to exist for headers (e.g. PreRTS.h) that include it.
#pragma once
