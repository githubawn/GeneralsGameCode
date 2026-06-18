set(UTILITY_SRC
    Utility/compat.h
    Utility/CppMacros.h
    Utility/endian_compat.h
    Utility/fstream_adapter.h
    Utility/hash_map_adapter.h
    Utility/intrin_compat.h
    Utility/iostream_adapter.h
    Utility/mem_compat.h
    Utility/sstream_adapter.h
    Utility/stdint_adapter.h
    Utility/stdio_adapter.h
    Utility/string_compat.h
    Utility/tchar_compat.h
    Utility/thread_compat.h
    Utility/time_compat.h
    Utility/wchar_compat.h
)

## core_utility

add_library(core_utility INTERFACE)
target_include_directories(core_utility INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(core_utility INTERFACE
    [["Utility/CppMacros.h"]] # Must be first, to be removed when abandoning VC6
)
# TheSuperHackers @build githubawn 18/06/2026 On non-Windows, put the win32_shims
# <windows.h> into the (widely propagated) core_utility precompiled header so the
# Win32 types/macros (LONG/HWND/_stricmp/wcs*) are available to every consumer via
# the PCH. This lets the Xcode generator drop the separate forced -include windows.h
# (which it places before the PCH and thereby disables it). The include dir is added
# so <windows.h> resolves to the shim during PCH compilation.
if(UNIX OR ANDROID)
    target_include_directories(core_utility INTERFACE
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/compat/win32_shims
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib) # for bittype.h used by the shim windows.h
    target_precompile_headers(core_utility INTERFACE <windows.h>)
endif()
target_sources(core_utility PRIVATE ${UTILITY_SRC})

## core_utility_no_pch

add_library(core_utility_no_pch INTERFACE)
target_include_directories(core_utility_no_pch INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(core_utility_no_pch PRIVATE ${UTILITY_SRC})
