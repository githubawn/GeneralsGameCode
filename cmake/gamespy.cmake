set(GS_OPENSSL FALSE)
set(GAMESPY_SERVER_NAME "server.cnc-online.net")

FetchContent_Declare(
    gamespy
    GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
    GIT_TAG        07e3d15c500415abc281efb74322ab6d9c857eb8
)

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
