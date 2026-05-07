include(FetchContent)

# TheSuperHackers @build JohnsterID 13/05/2026 Add BGFX integration
FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(bgfx)

# Ensure BGFX is linked with the correct flags
if (MSVC)
    # BGFX needs some specific defines for Windows/MSVC
    add_compile_definitions(BGFX_CONFIG_RENDERER_DIRECT3D9=1) # Default for now
endif()
