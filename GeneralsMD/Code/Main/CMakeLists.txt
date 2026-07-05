# TheSuperHackers @build bobtista 13/06/2026 On Android the game is loaded by the
# Java SDLActivity via System.loadLibrary("z_generals"), which then calls the
# SDL_main entry point. So it must be a shared library (libz_generals.so), not an
# executable (there is no crt main()).
if(ANDROID)
    add_library(z_generals SHARED)
elseif(WIN32)
    add_executable(z_generals WIN32)
else()
    add_executable(z_generals)
endif()

# Use a binary name that doesn't conflict with original game.
if(ANDROID)
    # SDLActivity.getLibraries() returns "z_generals" -> libz_generals.so
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "z_generals")
elseif(EMSCRIPTEN)
    # Emit index.html/.js/.wasm so the root URL served by serve.py boots the game.
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "index")
else()
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "generalszh${RTS_BUILD_OUTPUT_SUFFIX}")
endif()

# TheSuperHackers @build githubawn 18/06/2026 iOS app bundle + code signing.
# iOS executables must be .app bundles with a valid Info.plist; the Xcode
# generator drives automatic signing from these attributes. Pass the signing
# team via -DGGC_IOS_DEVELOPMENT_TEAM=XXXXXXXXXX at configure time.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(z_generals PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/iOSInfo.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.generalszh"
        MACOSX_BUNDLE_BUNDLE_NAME "Zero Hour"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.example.generalszh"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${GGC_IOS_DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        # TheSuperHackers @build githubawn 18/06/2026 Generate a runnable Xcode
        # scheme for the app target so it can be selected and Run/installed onto
        # a device from the GUI (the default project only exposes the aggregate
        # "genzh"/ALL_BUILD scheme, which is build-only and cannot be run).
        XCODE_GENERATE_SCHEME ON
    )
endif()

target_link_libraries(z_generals PRIVATE
    core_debug
    core_profile_legacy
    z_gameengine
    z_gameenginedevice
    zi_always
)

if(SAGE_USE_SDL3)
    target_link_libraries(z_generals PRIVATE sdl3lib)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(_dkp "$ENV{DEVKITPRO}")
    target_link_libraries(z_generals PRIVATE
        "${_dkp}/portlibs/switch/lib/libz.a"
        "${_dkp}/portlibs/switch/lib/libbz2.a"
        "${_dkp}/portlibs/switch/lib/libEGL.a"
        "${_dkp}/portlibs/switch/lib/libGLESv2.a"
        "${_dkp}/portlibs/switch/lib/libglapi.a"
        "${_dkp}/portlibs/switch/lib/libdrm_nouveau.a"
        "${_dkp}/portlibs/switch/lib/libdav1d.a"
        "${_dkp}/portlibs/switch/lib/libswresample.a"
        "${_dkp}/libnx/lib/libnx.a"
    )
    unset(_dkp)
endif()

if(WIN32)
    target_link_libraries(z_generals PRIVATE
        binkstub
        comctl32
        dinput8
        dxguid
        imm32
        milesstub
        vfw32
        winmm
    )
endif()

# TheSuperHackers @build bobtista 13/06/2026 The bgfx GLES renderer references
# GL/EGL entry points directly; link the Android system GLES/EGL libraries
# (and libandroid for ANativeWindow).
if(ANDROID)
    target_link_libraries(z_generals PRIVATE GLESv2 EGL android log)
endif()

# TheSuperHackers @build bobtista 22/04/2026 Phase 5.2 Stage 5 —
# In standalone bgfx mode all D3D8/D3DX8 symbol references resolve to
# in-tree implementations: StubD3D8Device.cpp replaces Direct3DCreate8
# and the IDirect3D* vtables, D3DXStandaloneStubs.cpp replaces the
# D3DX matrix/texture helpers. Neither d3d8.lib nor d3dx8.lib is
# needed in the link. Ref-popup keeps both.
if(NOT GGC_BGFX_STANDALONE)
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

target_sources(z_generals PRIVATE
    $<$<BOOL:${WIN32}>:WinMain.cpp>
    $<$<BOOL:${WIN32}>:WinMain.h>
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<BOOL:${SAGE_USE_SDL3}>>:SDL3Main.cpp>
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},NintendoSwitch>:SwitchPlatformStubs.cpp>
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

# TheSuperHackers @feature githubawn 23/06/2026 Emscripten (WebAssembly) link wiring.
# Produce a .html shell + .js + .wasm, run a non-blocking main loop via Asyncify,
# grow memory up to the wasm32 4GB ceiling, and expose the runtime FS + IDBFS so
# SDL3Main can mount IndexedDB for options/saves persistence. WebGL2 (GLES3) is the
# bgfx essl target. All scoped under EMSCRIPTEN so no other platform is affected.
if(EMSCRIPTEN)
    # Emit all web artifacts (index.html/.js/.wasm/.data) into a single predictable
    # directory that serve.py serves from (build/<preset>/web).
    set_target_properties(z_generals PROPERTIES
        SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web")
    # NOTE: -sALLOW_MEMORY_GROWTH / -sMAXIMUM_MEMORY are applied globally to all
    # Emscripten targets in cmake/compilers.cmake; only exe-specific flags go here.
    target_link_options(z_generals PRIVATE
        # TheSuperHackers @feature githubawn 28/06/2026 Asyncify is required for LAN
        # multiplayer: the game-start path (GameLogic::startNewGame) blocks in
        # while(!isProgressComplete()) Sleep(100) waiting for every player's "load complete"
        # network message. On the web, WebSocket messages are only delivered when control
        # returns to the browser event loop, so that spin-loop would deadlock. Those loops
        # now call emscripten_sleep() (guarded by __EMSCRIPTEN__), which yields cooperatively
        # — but that only works with Asyncify.
        #
        # NOTE: ASYNCIFY_IGNORE_INDIRECT is intentionally NOT used. startNewGame is reached
        # through indirect/virtual dispatch from the main loop, and Asyncify must instrument
        # every frame on the stack at the emscripten_sleep point (including the frames above
        # the indirect call) to unwind it. With IGNORE_INDIRECT those upper frames are deemed
        # unable to reach an async call and are left un-instrumented, so the unwind breaks.
        # Full Asyncify is correct; the render loop never sleeps so the steady-state cost is
        # the (small) per-indirect-call bookkeeping, not repeated stack unwinding.
        "-sASYNCIFY"
        # Pre-spawn a Web Worker pool for the engine's threads (texture loader, audio,
        # file preload, GameSpy). Pre-spawning avoids deadlocks when a thread is created
        # while the main thread is blocked. PTHREAD_POOL_SIZE_STRICT=0 lets it grow if more
        # are needed rather than erroring.
        "-sPTHREAD_POOL_SIZE=8"
        "-sPTHREAD_POOL_SIZE_STRICT=0"
        "-sINITIAL_MEMORY=536870912"
        "-sSTACK_SIZE=4MB"
        "-sFORCE_FILESYSTEM=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
        "-sEXIT_RUNTIME=0"
        "-sEXPORTED_RUNTIME_METHODS=['FS','IDBFS','ccall','cwrap','callMain','stringToUTF8']"
        "-lidbfs.js"
        # TheSuperHackers @feature githubawn 27/06/2026 LAN multiplayer over WebSockets:
        # udp.cpp (__EMSCRIPTEN__) uses the emscripten_websocket_* API to tunnel the game's
        # UDP to relay.py. That API lives in the websocket.js JS library. stringToUTF8 (above)
        # is used from EM_ASM to build the relay URL from location.hostname.
        "-lwebsocket.js"
        # Embed a free TTF (Droid Sans) at /fonts/droidsans.ttf for the non-Windows
        # stb_truetype text path; the browser has no system fonts in MEMFS.
        "--embed-file" "${CMAKE_CURRENT_SOURCE_DIR}/web/droidsans.ttf@/fonts/droidsans.ttf"
        "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
    # Re-run the link step when the shell file changes.
    set_property(TARGET z_generals APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
endif()
