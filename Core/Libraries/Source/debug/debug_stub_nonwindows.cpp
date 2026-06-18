// TheSuperHackers @build bobtista 13/06/2026 The legacy EA "Debug" library
// (console/named-pipe I/O, DbgHelp stack walking, SEH) is Windows-only and is
// not referenced by the engine on other platforms — runtime logging goes
// through Common/Debug.cpp instead. This stub keeps core_debug a valid (empty)
// static library on non-Windows so existing link lines stay unchanged.
