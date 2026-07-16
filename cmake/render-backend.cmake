# cmake/render-backend.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 Selects the rendering backend
# for WW3D2 at configure time.
#
# Valid values:
#   dx8     - existing DirectX 8 backend. Default. VC6-compatible. Windows only.
#   bgfx    - bgfx abstraction over DX11/Vulkan/Metal/GL. Cross-platform. MSVC 2022+.
#   citro3d - New Nintendo 3DS PICA200 fixed-function backend. See
#             docs/3ds-port-plan.md. bgfx has no PICA200 support (no
#             fragment shaders on that GPU), hence a separate backend.
#
# When set to bgfx the dependency module is included from cmake/bgfx.cmake.
# When set to citro3d the dependency module is included from cmake/citro3d.cmake.
# Neither is fetched when dx8 is selected.
#
# This file must be included from the top-level CMakeLists.txt after the
# project() call but before the WW3D2 source subdirectories are added.

set(GGC_RENDER_BACKEND "dx8" CACHE STRING
    "Rendering backend for WW3D2: dx8 (default), bgfx, or citro3d")
set_property(CACHE GGC_RENDER_BACKEND PROPERTY STRINGS dx8 bgfx citro3d)

if(NOT GGC_RENDER_BACKEND STREQUAL "dx8" AND
   NOT GGC_RENDER_BACKEND STREQUAL "bgfx" AND
   NOT GGC_RENDER_BACKEND STREQUAL "citro3d")
    message(FATAL_ERROR
        "Invalid GGC_RENDER_BACKEND: '${GGC_RENDER_BACKEND}'. "
        "Must be one of: dx8, bgfx, citro3d.")
endif()

message(STATUS "WW3D2 render backend: ${GGC_RENDER_BACKEND}")

# Non-DX8 backends require a modern toolchain. We do NOT want to silently
# pick a broken build configuration, so fail early with a useful message.
if(NOT GGC_RENDER_BACKEND STREQUAL "dx8")
    if(IS_VS6_BUILD)
        message(FATAL_ERROR
            "GGC_RENDER_BACKEND=${GGC_RENDER_BACKEND} requires a modern C++ toolchain "
            "(MSVC 2022 or Clang 11+). VC6 builds must use GGC_RENDER_BACKEND=dx8.")
    endif()
    if(NOT WIN32)
        message(WARNING
            "GGC_RENDER_BACKEND=${GGC_RENDER_BACKEND} is being configured on a non-Windows host. "
            "Cross-platform build targets will land in Phase 4; for now expect compile failures "
            "outside Windows.")
    endif()
endif()

# Expose the selection as a compile definition so downstream code can do
#   #if defined(GGC_RENDER_BACKEND_BGFX)
# without coupling to the raw string variable.
if(GGC_RENDER_BACKEND STREQUAL "dx8")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_DX8=1")
elseif(GGC_RENDER_BACKEND STREQUAL "bgfx")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_BGFX=1")
elseif(GGC_RENDER_BACKEND STREQUAL "citro3d")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_CITRO3D=1")
endif()

# TheSuperHackers @refactor bobtista 21/04/2026 Phase 5 Stage 5 — standalone
# bgfx build. When ON, BgfxBackend inherits from IRenderBackend directly
# instead of DX8Backend, and the DX8 reference popup is disabled. This is
# the path toward removing d3d8.dll / d3dx8.dll from the bgfx build's link
# graph; today it still links against DX8 because the asset loaders haven't
# been migrated yet (see Phase 5.1 follow-up). The define is compiled in so
# code can conditionally exclude DX8-specific mirroring via
# #if defined(GGC_BGFX_STANDALONE).
# TheSuperHackers @build githubawn 14/07/2026 citro3d reuses this same flag:
# it is exactly the "no DX8 reference popup, backend inherits DX8Backend as a
# state-tracking base only" mode that any non-dx8 backend needs. See
# Citro3dBackend.h / docs/3ds-port-plan.md.
option(GGC_BGFX_STANDALONE "bgfx/citro3d without the DX8 reference popup; standalone base class for the active backend" OFF)
if(GGC_BGFX_STANDALONE AND
   NOT GGC_RENDER_BACKEND STREQUAL "bgfx" AND
   NOT GGC_RENDER_BACKEND STREQUAL "citro3d")
    message(FATAL_ERROR
        "GGC_BGFX_STANDALONE=ON requires GGC_RENDER_BACKEND=bgfx or citro3d.")
endif()
if(GGC_BGFX_STANDALONE)
    add_compile_definitions(GGC_BGFX_STANDALONE=1)
    message(STATUS "Bgfx standalone mode enabled — ref popup disabled.")
endif()

if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    if(NOT DEFINED GGC_BGFX_RENDERER)
        if(APPLE)
            set(GGC_BGFX_RENDERER "metal" CACHE STRING "bgfx renderer for GGC_RENDER_BACKEND=bgfx")
        elseif(ANDROID)
            set(GGC_BGFX_RENDERER "essl" CACHE STRING "bgfx renderer for GGC_RENDER_BACKEND=bgfx")
        else()
            set(GGC_BGFX_RENDERER "dx11" CACHE STRING "bgfx renderer for GGC_RENDER_BACKEND=bgfx")
        endif()
    endif()
    set_property(CACHE GGC_BGFX_RENDERER PROPERTY STRINGS dx11 metal vulkan essl glsl)

    if(NOT GGC_BGFX_RENDERER STREQUAL "dx11" AND
       NOT GGC_BGFX_RENDERER STREQUAL "metal" AND
       NOT GGC_BGFX_RENDERER STREQUAL "vulkan" AND
       NOT GGC_BGFX_RENDERER STREQUAL "essl" AND
       NOT GGC_BGFX_RENDERER STREQUAL "glsl")
        message(FATAL_ERROR
            "Invalid GGC_BGFX_RENDERER: '${GGC_BGFX_RENDERER}'. "
            "Must be one of: dx11, metal, vulkan, essl, glsl.")
    endif()

    if(GGC_BGFX_RENDERER STREQUAL "metal")
        add_compile_definitions(GGC_BGFX_RENDERER_METAL=1)
    elseif(GGC_BGFX_RENDERER STREQUAL "vulkan")
        add_compile_definitions(GGC_BGFX_RENDERER_VULKAN=1)
    elseif(GGC_BGFX_RENDERER STREQUAL "essl")
        add_compile_definitions(GGC_BGFX_RENDERER_ESSL=1)
    elseif(GGC_BGFX_RENDERER STREQUAL "glsl")
        # TheSuperHackers @feature githubawn 28/06/2026 Desktop OpenGL (macOS GL 4.1).
        add_compile_definitions(GGC_BGFX_RENDERER_GLSL=1)
    else()
        add_compile_definitions(GGC_BGFX_RENDERER_DX11=1)
    endif()
    message(STATUS "bgfx renderer target: ${GGC_BGFX_RENDERER}")
endif()

# Pull in the backend's dependency module if it has one. dx8 has no module
# here because cmake/dx8.cmake is already included from the top-level
# CMakeLists.txt unconditionally for the min-dx8-sdk.
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    include(cmake/bgfx.cmake)
elseif(GGC_RENDER_BACKEND STREQUAL "citro3d")
    include(cmake/citro3d.cmake)
endif()
