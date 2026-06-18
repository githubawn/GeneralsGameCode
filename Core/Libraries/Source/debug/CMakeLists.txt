set(DEBUG_SRC
    "debug_cmd.cpp"
    "debug_cmd.h"
    "debug_debug.cpp"
    "debug_debug.h"
    "debug_doc.h"
    "debug_except.cpp"
    "debug_getdefaultcommands.cpp"
    "debug_internal.cpp"
    "debug_io_con.cpp"
    "debug_io_flat.cpp"
    "debug_io_net.cpp"
    "debug_io_ods.cpp"
    "debug_io.h"
    "debug_macro.h"
    "debug_stack.cpp"
    "debug_stack.h"
    "debug.h"
    "internal_except.h"
    "internal_io.h"
    "internal.h"
)

add_library(core_debug STATIC)

target_include_directories(core_debug INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# TheSuperHackers @build bobtista 13/06/2026 The legacy EA Debug library uses
# Win32 console / named-pipe I/O, DbgHelp stack walking and SEH (debug_debug.cpp
# even #errors on non-MSVC). It is only consumed by the Windows tools; the game's
# runtime logging goes through Common/Debug.cpp. On non-Windows build an empty
# stub so link lines that reference core_debug stay valid.
if(WIN32)
    target_sources(core_debug PRIVATE ${DEBUG_SRC})

    target_precompile_headers(core_debug PRIVATE
        [["Utility/CppMacros.h"]] # Must be first, to be removed when abandoning VC6
        "debug.h"
        "internal.h"
        "internal_except.h"
        "internal_io.h"
        <windows.h>
    )
else()
    target_sources(core_debug PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/debug_stub_nonwindows.cpp)
endif()

target_link_libraries(core_debug PRIVATE
    core_wwcommon
    corei_always
)
