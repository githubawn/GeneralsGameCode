set(GS_OPENSSL FALSE)
set(GAMESPY_SERVER_NAME "server.cnc-online.net")

# TheSuperHackers @build githubawn 10/07/2026 PS2 needs gssha1.h patched
# (see cmake/patch_gamespy_ps2.cmake) so its redundant uint32_t/uint8_t
# typedefs don't conflict with ps2sdk's real <stdint.h>. Other platforms
# are unaffected (no PATCH_COMMAND for them).
if(PS2)
    FetchContent_Declare(
        gamespy
        GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
        GIT_TAG        07e3d15c500415abc281efb74322ab6d9c857eb8
        PATCH_COMMAND  ${CMAKE_COMMAND} -DGAMESPY_SHA1_H=include/gamespy/gssha1.h -P ${CMAKE_CURRENT_LIST_DIR}/patch_gamespy_ps2.cmake
    )
else()
    FetchContent_Declare(
        gamespy
        GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
        GIT_TAG        07e3d15c500415abc281efb74322ab6d9c857eb8
    )
endif()

FetchContent_MakeAvailable(gamespy)

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

# TheSuperHackers @build githubawn 10/07/2026 PS2 gets the same generic
# Unix/Linux GameSpy code path as Switch/Emscripten (see docs/ps2-port-plan.md
# -- the GameSpy SDK's own PS2 platform detection wants real GameSpy-era PS2
# network headers like eekernel.h that don't exist in ps2sdk; the SDK's
# master-server integration is dead everywhere regardless, so there is no
# reason to chase its native PS2 path).
if((EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch" OR PS2) AND TARGET gsinterface)
    target_compile_definitions(gsinterface INTERFACE _UNIX __linux__)
endif()

# Note: -U__mips64 (needed so gsplatform.h's "#ifdef __mips64" doesn't treat
# this as a real PS2 devkit demanding eekernel.h etc.) is applied globally in
# cmake/toolchains/ps2.cmake instead of scoped to gsinterface here -- our own
# engine code (GameNetwork/GameSpy/PeerDefs.h) also includes GameSpy headers
# directly and hit the same #error outside gsinterface's reach.

# TheSuperHackers @build githubawn 10/07/2026 PS2: force-include the ioctl()
# declaration (see compat/win32_shims/ps2_ioctl_decl.h /
# ps2_ioctl_compat.c) into every GameSpySDK translation unit. GameSpy's own
# socket code calls ioctl(sock, FIONBIO, ...) directly and doesn't see our
# engine's win32_shims headers.
if(PS2 AND TARGET gsinterface)
    target_compile_options(gsinterface INTERFACE
        -include "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/compat/win32_shims/ps2_ioctl_decl.h")
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
