// TheSuperHackers @build bobtista 13/06/2026 The legacy EA profiling library is
// Windows-only (GlobalAlloc-based allocator, Win32 timing, thread IDs) and is
// not referenced by the engine on other platforms. This stub keeps
// core_profile_legacy a valid (empty) static library on non-Windows.
