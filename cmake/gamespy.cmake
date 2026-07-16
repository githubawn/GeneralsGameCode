set(GS_OPENSSL FALSE)
set(GAMESPY_SERVER_NAME "server.cnc-online.net")

FetchContent_Declare(
    gamespy
    GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
    GIT_TAG        07e3d15c500415abc281efb74322ab6d9c857eb8
)

FetchContent_MakeAvailable(gamespy)

# TheSuperHackers @build githubawn 14/07/2026 devkitARM/newlib's uint32_t is
# `long unsigned int`, not `unsigned int` (unlike glibc/Bionic/MSVC, where it
# matches GamespySDK's own gsi_u32). gssha1.h's fallback typedef (written for
# pre-C99 toolchains lacking <stdint.h>) collides as a genuine type conflict,
# not just a redefinition, once <stdint.h> is pulled in transitively ahead of
# it. Patch it out for 3DS the same way cmake/bgfx.cmake patches fetched
# third-party sources for Switch. Idempotent: only patches if not already done.
if(CMAKE_SYSTEM_NAME STREQUAL "Nintendo3DS")
    set(GSSHA1_H "${gamespy_SOURCE_DIR}/include/gamespy/gssha1.h")
    if(EXISTS "${GSSHA1_H}")
        file(READ "${GSSHA1_H}" GSSHA1_CONTENT)
        string(FIND "${GSSHA1_CONTENT}" "__3DS__" _3ds_already)
        if(_3ds_already EQUAL -1)
            message(STATUS "Patching gssha1.h for Nintendo3DS uint32_t/uint8_t conflict...")
            string(REPLACE "#ifndef _PS3\n// these common types are defined in sony libs"
                            "#if !defined(_PS3) && !defined(__3DS__)\n// these common types are defined in sony libs"
                            GSSHA1_CONTENT "${GSSHA1_CONTENT}")
            file(WRITE "${GSSHA1_H}" "${GSSHA1_CONTENT}")
        endif()
    endif()

    # TheSuperHackers @build githubawn 14/07/2026 The 3DS's soc:u sockets
    # service (libctru) does not implement SO_KEEPALIVE. The SDK already
    # excludes other constrained network stacks (PSP/INSOCK, Nintendo DS/
    # _NITRO, Wii/_REVOLUTION) from this setsockopt call for the same reason
    # (see the #if guards in these files) — add 3DS to that existing
    # exclusion list rather than inventing a new mechanism.
    foreach(_gs_sokeepalive_file
        "${gamespy_SOURCE_DIR}/src/chat/chatsocket.c"
        "${gamespy_SOURCE_DIR}/src/common/gsplatformsocket.c"
        "${gamespy_SOURCE_DIR}/src/serverbrowsing/sb_serverlist.c")
        if(EXISTS "${_gs_sokeepalive_file}")
            file(READ "${_gs_sokeepalive_file}" _gs_sokeepalive_content)
            string(FIND "${_gs_sokeepalive_content}" "__3DS__" _3ds_already)
            if(_3ds_already EQUAL -1)
                message(STATUS "Patching ${_gs_sokeepalive_file} for Nintendo3DS SO_KEEPALIVE...")
                string(REPLACE "!defined(INSOCK) && !defined(_NITRO) && !defined(_REVOLUTION)"
                                "!defined(INSOCK) && !defined(_NITRO) && !defined(_REVOLUTION) && !defined(__3DS__)"
                                _gs_sokeepalive_content "${_gs_sokeepalive_content}")
                file(WRITE "${_gs_sokeepalive_file}" "${_gs_sokeepalive_content}")
            endif()
        endif()
    endforeach()

    # TheSuperHackers @bugfix githubawn 15/07/2026 natneg.c's GSI_COMMON_DEBUG
    # block (enabled by our Debug-config gamespy build, see the iOS _MACOSX
    # comment below re: GSI_COMMON_DEBUG) calls getsockname(..., int*) where
    # POSIX/the SDK's own prototype wants socklen_t*. Most platforms this SDK
    # targets have socklen_t==int, so the mismatch is silent there; devkitARM's
    # sys/socket.h (sys/socket.h:49) defines socklen_t as uint32_t, which GCC's
    # strict prototype checking rejects outright. Fix the local's type to match
    # the real prototype -- correct on every platform, just previously
    # unobserved elsewhere because it either doesn't compile this block or
    # socklen_t coincidentally aliases int there.
    set(NATNEG_C "${gamespy_SOURCE_DIR}/src/natneg/natneg.c")
    if(EXISTS "${NATNEG_C}")
        file(READ "${NATNEG_C}" NATNEG_CONTENT)
        string(FIND "${NATNEG_CONTENT}" "__3DS__" _3ds_already)
        if(_3ds_already EQUAL -1)
            message(STATUS "Patching natneg.c for Nintendo3DS getsockname() socklen_t...")
            string(REPLACE "struct sockaddr_in saddr;\n        int namelen = sizeof(saddr);"
                            "struct sockaddr_in saddr;\n        // TheSuperHackers @bugfix githubawn 15/07/2026 socklen_t, not int -- see __3DS__ note in cmake/gamespy.cmake\n        socklen_t namelen = sizeof(saddr);"
                            NATNEG_CONTENT "${NATNEG_CONTENT}")
            file(WRITE "${NATNEG_C}" "${NATNEG_CONTENT}")
        endif()
    endif()
endif()

# TheSuperHackers @bugfix githubawn 18/06/2026 On iOS define _MACOSX for the
# GamespySDK. The SDK expects exactly one platform macro to be defined and
# selects its Darwin code paths (sockets/threads/util, which already branch on
# TARGET_OS_IPHONE internally) from _MACOSX. Our build never set it: harmless in
# Release (the debug-print code is stripped) but in Debug GSI_COMMON_DEBUG=1
# enables gsdebug.c, which then falls into the #else branch calling the
# nonexistent gsDebugTTyPrint -> compile error. All gs* modules link gsinterface
# PUBLIC, so defining it on that INTERFACE target propagates everywhere. Scoped
# to iOS to leave the (working, Release-only) macOS build untouched.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND TARGET gsinterface)
    target_compile_definitions(gsinterface INTERFACE _MACOSX)
endif()

if((EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch" OR CMAKE_SYSTEM_NAME STREQUAL "Nintendo3DS") AND TARGET gsinterface)
    target_compile_definitions(gsinterface INTERFACE _UNIX __linux__)
endif()

# TheSuperHackers @build bobtista 13/06/2026 On Android the engine links into a
# shared library (libz_generals.so), so every static archive it pulls in must be
# position-independent. The GamespySDK fork doesn't honour the global
# CMAKE_POSITION_INDEPENDENT_CODE, so force it on the target explicitly.
if(ANDROID AND TARGET gamespy)
    set_property(TARGET gamespy PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# TheSuperHackers @build githubawn 18/06/2026 GamespySDK forces
# CMAKE_ARCHIVE_OUTPUT_DIRECTORY=CMAKE_BINARY_DIR, which places its archives in
# build/<dir>/Release while the Xcode generator (for device) writes and links
# from Release-iphoneos (the EFFECTIVE_PLATFORM_NAME suffix). That mismatch makes
# z_generals link a nonexistent path. Reset these targets to CMake's default
# placement under Xcode so the actual output and the link reference agree.
# (Ninja Multi-Config uses plain Release for everything, so it is unaffected.)
if(XCODE)
    foreach(_gs_target gamespy gsgp gsgt2 gsnatneg gssake gsserverbrowsing)
        if(TARGET ${_gs_target})
            set_target_properties(${_gs_target} PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY ""
                LIBRARY_OUTPUT_DIRECTORY ""
                RUNTIME_OUTPUT_DIRECTORY "")
        endif()
    endforeach()
    # The gamespy target has no sources of its own (it only bundles the gs*
    # module libraries). The Xcode generator does not emit an archive for a
    # sourceless static library, so z_generals links a libgamespy.a that never
    # exists. Give it a tiny dummy translation unit so the archive is produced.
    if(TARGET gamespy)
        set(_gs_dummy "${CMAKE_BINARY_DIR}/gamespy_dummy.c")
        if(NOT EXISTS "${_gs_dummy}")
            file(WRITE "${_gs_dummy}" "/* TheSuperHackers @build githubawn 18/06/2026 dummy TU so Xcode emits libgamespy.a */\nint ggc_gamespy_dummy_symbol = 0;\n")
        endif()
        target_sources(gamespy PRIVATE "${_gs_dummy}")
    endif()
endif()
