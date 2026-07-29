if(WIN32)
    add_executable(z_generals WIN32)
else()
    add_executable(z_generals)
endif()

# Use a binary name that doesn't conflict with original game.
if(EMSCRIPTEN)
    # TheSuperHackers @build githubawn 29/07/2026 Emit index.html/.js/.wasm so the
    # root URL of the directory the web build is served from boots the game.
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "index")
elseif("${CMAKE_SYSTEM}" MATCHES "Windows")
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "generalszh${RTS_BUILD_OUTPUT_SUFFIX}")
else()
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "generalszh${RTS_BUILD_OUTPUT_SUFFIX}")
endif()

target_link_libraries(z_generals PRIVATE
    core_profile_legacy
    z_gameengine
    z_gameenginedevice
    zi_always
)

if(WIN32)
    target_link_libraries(z_generals PRIVATE
        binkstub
        comctl32
        core_debug
        dinput8
        dxguid
        imm32
        milesstub
        vfw32
        winmm
    )
endif()

if(SAGE_USE_SDL3)
    target_link_libraries(z_generals PRIVATE sdl3lib)
    # TheSuperHackers @build bobtista 07/06/2026 SDL3Main.cpp uses a cross-platform int main(),
    # but on Windows the exe is built with the GUI subsystem whose default entry is WinMainCRTStartup
    # (expects WinMain). Point the entry at mainCRTStartup so the CRT calls main() directly while
    # staying in the WINDOWS subsystem (no console window), avoiding an SDL_main/WinMain shim.
    if(WIN32 AND MSVC)
        target_link_options(z_generals PRIVATE "/ENTRY:mainCRTStartup")
    endif()
endif()

# TheSuperHackers @build bobtista 22/04/2026
# Standalone bgfx owns the device through bgfx and supplies the remaining
# D3DX helper symbols in-tree, so neither d3d8.lib nor d3dx8.lib is needed.
# Ref-popup keeps both for the real DX8 reference path.
if(NOT GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(z_generals PRIVATE d3d8 d3dx8)
endif()

# TODO Originally referred to build host and user, replace with git info perhaps?
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GeneratedVersion.h
"#pragma once

#define VERSION_LOCALBUILDNUM 0
#define VERSION_BUILDUSER \"\"
#define VERSION_BUILDLOC \"\"
"
)

# Based on original binary values for these variables.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.h
"#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_BUILDNUM 601
"
)

if(MSVC)
    target_link_options(z_generals PRIVATE "/NODEFAULTLIB:libci.lib")
endif()

target_include_directories(z_generals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# TheSuperHackers @build bobtista 07/06/2026 SDL3 is the entry point whenever SAGE_USE_SDL3 is
# set, including on Windows (win32-generalsmd-sdl3-bgfx). The Win32 WinMain entry is only used for
# the non-SDL3 Windows build. Previously SDL3Main was gated on NOT WIN32, so the Windows SDL3 build
# linked WinMain and left TheSDL3Window (defined in SDL3Main.cpp) unresolved.
target_sources(z_generals PRIVATE
    $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<BOOL:${SAGE_USE_SDL3}>>>:WinMain.cpp>
    $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<BOOL:${SAGE_USE_SDL3}>>>:WinMain.h>
    $<$<BOOL:${SAGE_USE_SDL3}>:SDL3Main.cpp>
)

# RC files optional for MinGW builds
# Icon: LoadIcon() handles missing resource gracefully (uses default system icon)
# Manifest: DPI awareness metadata (nice to have but not essential)
if(MSVC)
    # VS2005 and later adds default manifest, we need to turn it off to prevent conflict with custom manifest
    if(NOT IS_VS6_BUILD)
        target_link_options(z_generals PRIVATE
            "/MANIFEST:NO"
        )
    endif()

    target_sources(z_generals PRIVATE
        RTS.RC
    )
endif()

# Strip debug symbols to separate file for MinGW Release builds
# This creates generalszh${RTS_BUILD_OUTPUT_SUFFIX}.exe.debug (similar to MSVC .pdb files)
if(MINGW AND COMMAND add_debug_strip_target)
    add_debug_strip_target(z_generals)
endif()

# TheSuperHackers @feature githubawn 29/07/2026 Emscripten (WebAssembly) link wiring.
# Produces an .html shell plus the .js/.wasm next to it, in one predictable directory
# a web server can be pointed at. Memory growth, threads and exceptions are set
# globally in cmake/compilers.cmake because the dependencies need them too; only the
# executable-specific settings live here.
if(EMSCRIPTEN)
    set_target_properties(z_generals PROPERTIES
        SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web")

    target_link_options(z_generals PRIVATE
        # Drop DWARF from the shipped module. The dependencies compile with debug
        # info even in Release, which is most of the module's size, and it is dead
        # weight for anything but local debugging.
        $<$<NOT:$<CONFIG:Debug>>:-g0>
        # Asyncify lets the engine's blocking wait loops (e.g. the "all players
        # loaded" barrier in GameLogic::startNewGame) yield to the browser event
        # loop through emscripten_sleep() instead of deadlocking, since network
        # messages are only delivered when control returns to the event loop.
        "-sASYNCIFY"
        # Pre-spawn the Web Worker pool the engine's threads run on. Pre-spawning
        # avoids a deadlock when a thread is created while the main thread waits;
        # POOL_SIZE_STRICT=0 lets the pool grow instead of erroring.
        "-sPTHREAD_POOL_SIZE=8"
        "-sPTHREAD_POOL_SIZE_STRICT=0"
        "-sINITIAL_MEMORY=536870912"
        "-sSTACK_SIZE=4MB"
        "-sFORCE_FILESYSTEM=1"
        # bgfx renders through WebGL 2 only.
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
        "-sEXIT_RUNTIME=0"
        "-sEXPORTED_RUNTIME_METHODS=['FS','IDBFS','ccall','cwrap','callMain','stringToUTF8']"
        # IDBFS backs the options/save directory with IndexedDB so settings survive a
        # reload; the game mounts it in SDL3Main.
        "-lidbfs.js"
        # TheSuperHackers @build githubawn 30/07/2026 LAN multiplayer over WebSockets:
        # udp.cpp reimplements the UDP class on the emscripten_websocket_* API to tunnel the
        # game's datagrams to relay.py, and that API lives in the websocket.js JS library.
        # stringToUTF8 (exported above) is used from EM_ASM to build the relay URL from
        # location.hostname.
        "-lwebsocket.js"
        # The browser has no system fonts and no Fontconfig, so the font the text
        # renderer uses is embedded in the module (see cmake/webfont.cmake).
        # SHELL: keeps each flag with its argument: CMake de-duplicates repeated link
        # options, which would drop the second --embed-file and leave its path parsed
        # as a stray input file.
        "SHELL:--embed-file ${GGC_WEB_FONT_REGULAR}@/fonts/DejaVuSans.ttf"
        "SHELL:--embed-file ${GGC_WEB_FONT_BOLD}@/fonts/DejaVuSans-Bold.ttf"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )

    # Relink when the shell changes.
    set_property(TARGET z_generals APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
endif()
