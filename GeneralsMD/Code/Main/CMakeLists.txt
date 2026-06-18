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

if(SAGE_USE_SDL3)
    target_link_libraries(z_generals PRIVATE sdl3lib)
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
