FetchContent_Declare(
    dx8
    GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
)

FetchContent_MakeAvailable(dx8)

# TheSuperHackers @build bobtista 22/04/2026 Phase 5.2 Stage 5 —
# In standalone bgfx mode the game supplies d3d8/d3dx8 functions from
# in-tree stubs (Core/Libraries/Source/WWVegas/WW3D2/StubD3D8Device.cpp
# + D3DXStandaloneStubs.cpp). Pulling the real d3d8.lib / d3dx8.lib
# would cause duplicate-symbol link errors, so strip them off the
# shared d3d8lib INTERFACE target. Headers stay (include dir is still
# on d3d8lib), and import libs that can't be stubbed (dinput8, dxguid)
# stay as well. Ref-popup builds are unaffected.
if(GGC_BGFX_STANDALONE OR NOT WIN32)
    set_property(TARGET d3d8lib PROPERTY INTERFACE_LINK_LIBRARIES "")
    set_property(TARGET d3d8lib PROPERTY INTERFACE_COMPILE_DEFINITIONS "")
    if(WIN32)
        target_link_libraries(d3d8lib INTERFACE dinput8 dxguid)
    endif()
endif()

# TheSuperHackers @build bobtista 13/06/2026 — On non-Windows the min-dx8-sdk
# fork ships a full DirectInput dinput.h that needs Win32 COM types the shims
# don't provide. The win32_shims dir has a lightweight dinput.h (DIK_* enums
# only). Because d3d8lib carries the dx8-src include dir, it would otherwise
# shadow the shim. Prepend win32_shims to d3d8lib's interface includes so the
# shim wins for headers it provides (dinput.h) while dx8-src still supplies the
# real D3D8 headers (d3d8.h, d3d8types.h, ...).
if(NOT WIN32)
    target_include_directories(d3d8lib BEFORE INTERFACE
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/compat/win32_shims)
endif()
