# cmake/render-backend.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 Selects the rendering backend
# for WW3D2 at configure time. See Core/Libraries/Source/WWVegas/WW3D2/RENDER_BACKEND.md
#
# Valid values:
#   dx8    - existing DirectX 8 backend. VC6-compatible. Windows only.
#   dx9ex  - Direct3D 9Ex backend. Modern MSVC/Clang only. Requires cmake/dx9.cmake.
#
# This file must be included from the top-level CMakeLists.txt after
# cmake/config.cmake (which sets IS_VS6_BUILD) and before Core is added.

if(IS_VS6_BUILD)
    set(GGC_RENDER_BACKEND "dx8" CACHE STRING
        "Rendering backend for WW3D2: dx8 (VC6) or dx9ex (modern only)" FORCE)
else()
    set(GGC_RENDER_BACKEND "dx9ex" CACHE STRING
        "Rendering backend for WW3D2: dx8 or dx9ex (default on modern toolchains)")
endif()
set_property(CACHE GGC_RENDER_BACKEND PROPERTY STRINGS dx8 dx9ex)

if(NOT GGC_RENDER_BACKEND STREQUAL "dx8" AND
   NOT GGC_RENDER_BACKEND STREQUAL "dx9ex")
    message(FATAL_ERROR
        "Invalid GGC_RENDER_BACKEND: '${GGC_RENDER_BACKEND}'. "
        "Must be one of: dx8, dx9ex.")
endif()

message(STATUS "WW3D2 render backend: ${GGC_RENDER_BACKEND}")

if(GGC_RENDER_BACKEND STREQUAL "dx9ex")
    if(IS_VS6_BUILD)
        message(FATAL_ERROR
            "GGC_RENDER_BACKEND=dx9ex requires a modern C++ toolchain "
            "(MSVC 2022 or Clang 11+). VC6 builds must use GGC_RENDER_BACKEND=dx8.")
    endif()
    if(NOT WIN32)
        message(WARNING
            "GGC_RENDER_BACKEND=dx9ex is being configured on a non-Windows host. "
            "Expect compile failures outside Windows.")
    endif()
endif()

# Expose the selection as a compile definition so downstream code can do
#   #if defined(GGC_RENDER_BACKEND_DX9EX)
# without coupling to the raw string variable.
if(GGC_RENDER_BACKEND STREQUAL "dx8")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_DX8=1")
elseif(GGC_RENDER_BACKEND STREQUAL "dx9ex")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_DX9EX=1")
endif()

# Pull in the backend's dependency module. dx8 uses cmake/dx8.cmake, already
# included unconditionally from the top-level CMakeLists.txt for the min-dx8-sdk.
if(GGC_RENDER_BACKEND STREQUAL "dx9ex")
    include(cmake/dx9.cmake)
endif()
