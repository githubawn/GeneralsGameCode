# cmake/bgfx.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 bgfx dependency for the
# GGC_RENDER_BACKEND=bgfx build. Included from cmake/render-backend.cmake.
#
# Pulls in bgfx via the community bgfx.cmake wrapper, which internally
# fetches bgfx/bx/bimg as git submodules. We pin a specific bgfx.cmake
# SHA for reproducibility.
#
# This file is NOT included when GGC_RENDER_BACKEND is dx8.

# Disable bgfx features we don't need. These must be set BEFORE
# FetchContent_MakeAvailable so bgfx.cmake picks them up at configure time.
set(BGFX_BUILD_EXAMPLES       OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TESTS          OFF CACHE BOOL "" FORCE)
if(ANDROID)
    set(BGFX_BUILD_TOOLS          OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS_BIN2C    OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS_SHADER   OFF CACHE BOOL "" FORCE)
else()
    set(BGFX_BUILD_TOOLS          ON  CACHE BOOL "" FORCE)  # shaderc is mandatory
    set(BGFX_BUILD_TOOLS_BIN2C    ON  CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS_SHADER   ON  CACHE BOOL "" FORCE)
endif()
set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_TEXTURE  OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL              OFF CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS       OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_WEBGPU OFF CACHE BOOL "" FORCE)


FetchContent_Declare(
    bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        668550dc7c47c71860a39c5ef4c162e79294c93f
    # Nested submodules (bgfx, bx, bimg) are cloned recursively by FetchContent.
    GIT_SUBMODULES_RECURSE FALSE
)

FetchContent_GetProperties(bgfx_cmake)
if(NOT bgfx_cmake_POPULATED)
    FetchContent_Populate(bgfx_cmake)

    if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
        # Patch bx/include/bx/platform.h for GCC 15 compatibility
        set(PLATFORM_H "${bgfx_cmake_SOURCE_DIR}/bx/include/bx/platform.h")
        if(EXISTS "${PLATFORM_H}")
            message(STATUS "Patching bx/include/bx/platform.h for GCC 15 compatibility...")
            file(READ "${PLATFORM_H}" PLATFORM_CONTENT)
            string(REPLACE "#elif defined(__has_builtin) && __has_builtin(__is_target_os) && __is_target_os(xros)" "#elif defined(__clang__) && defined(__has_builtin)\n#\tif __has_builtin(__is_target_os)\n#\t\tif __is_target_os(xros)\n#\t\t\tundef  BX_PLATFORM_VISIONOS\n#\t\t\tdefine BX_PLATFORM_VISIONOS 1\n#\t\tendif\n#\tendif" PLATFORM_CONTENT "${PLATFORM_CONTENT}")
            file(WRITE "${PLATFORM_H}" "${PLATFORM_CONTENT}")
        endif()

        # Patch bx/src/os.cpp to not include dlfcn.h on Nintendo Switch
        set(OS_CPP "${bgfx_cmake_SOURCE_DIR}/bx/src/os.cpp")
        if(EXISTS "${OS_CPP}")
            message(STATUS "Patching bx/src/os.cpp for Nintendo Switch compatibility...")
            file(READ "${OS_CPP}" OS_CONTENT)
            string(REPLACE "!BX_PLATFORM_PS4\r\n#\t\tinclude <dlfcn.h>" "!BX_PLATFORM_PS4 && !BX_PLATFORM_NX\r\n#\t\tinclude <dlfcn.h>" OS_CONTENT "${OS_CONTENT}")
            string(REPLACE "!BX_PLATFORM_PS4\n#\t\tinclude <dlfcn.h>" "!BX_PLATFORM_PS4 && !BX_PLATFORM_NX\n#\t\tinclude <dlfcn.h>" OS_CONTENT "${OS_CONTENT}")
            file(WRITE "${OS_CPP}" "${OS_CONTENT}")
        endif()

        # TheSuperHackers @build githubawn 03/07/2026 bgfx bundles its own khronos EGL
        # headers whose eglplatform.h has no Nintendo Switch case (-> #error "Platform
        # not recognized", EGLNativeWindowType undefined). switch-mesa uses opaque void*
        # native types; add an NX case so bgfx's OpenGLES/EGL backend compiles. We pass
        # libnx's NWindow* (as void*) via GetNativeWindowHandle.
        set(EGLPLAT_H "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/khronos/EGL/eglplatform.h")
        if(EXISTS "${EGLPLAT_H}")
            file(READ "${EGLPLAT_H}" EGLPLAT_CONTENT)
            # Idempotent: only patch if the NX case is not already present. The
            # replacement below re-emits the "#else / #error" search pattern, so
            # without this guard every reconfigure would splice in another block.
            string(FIND "${EGLPLAT_CONTENT}" "__SWITCH__" _nx_already)
            if(_nx_already EQUAL -1)
                message(STATUS "Patching bgfx khronos eglplatform.h for Nintendo Switch...")
                # NOTE: use plain ';' here, not '\;'. Inside a quoted set() the
                # semicolon is already literal; escaping it leaves a stray '\' in
                # the generated header ("stray '\' in program" compile error).
                set(_nx_egl "#elif defined(__SWITCH__) || defined(__NX__)\n\ntypedef void *EGLNativeDisplayType;\ntypedef void *EGLNativePixmapType;\ntypedef void *EGLNativeWindowType;\n\n#else\n#error \"Platform not recognized\"")
                string(REPLACE "#else\r\n#error \"Platform not recognized\"" "${_nx_egl}" EGLPLAT_CONTENT "${EGLPLAT_CONTENT}")
                string(REPLACE "#else\n#error \"Platform not recognized\"" "${_nx_egl}" EGLPLAT_CONTENT "${EGLPLAT_CONTENT}")
                file(WRITE "${EGLPLAT_H}" "${EGLPLAT_CONTENT}")
            endif()
        endif()
    endif()

    add_subdirectory(${bgfx_cmake_SOURCE_DIR} ${bgfx_cmake_BINARY_DIR})
endif()

# TheSuperHackers @build githubawn 28/06/2026 bgfx defaults BGFX_CONFIG_RENDERER_VULKAN
# to OFF on Apple (it targets Android/Linux/Windows). When we ask for the Vulkan
# renderer on macOS (via MoltenVK), the backend is compiled but never registered, so
# bgfx::init silently falls back to Metal and then chokes feeding it SPIR-V. Force the
# config on so renderer_vk registers and Vulkan can actually be selected. config.h uses
# #ifndef, so this define is respected without redefinition conflicts.
if(GGC_RENDER_BACKEND STREQUAL "bgfx" AND GGC_BGFX_RENDERER STREQUAL "vulkan" AND TARGET bgfx)
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_VULKAN=1)
    message(STATUS "Forced BGFX_CONFIG_RENDERER_VULKAN=1 (Apple/MoltenVK)")
endif()

# TheSuperHackers @build githubawn 28/06/2026 Same story for desktop OpenGL: bgfx
# defaults BGFX_CONFIG_RENDERER_OPENGL to 0 on Apple (Metal is preferred there), so
# renderer_gl is compiled but never registered and bgfx::init falls back to Metal.
# Force GL 4.1 (the macOS core-profile ceiling) so the GL backend is selectable.
if(GGC_RENDER_BACKEND STREQUAL "bgfx" AND GGC_BGFX_RENDERER STREQUAL "glsl" AND TARGET bgfx)
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_OPENGL=41)
    message(STATUS "Forced BGFX_CONFIG_RENDERER_OPENGL=41 (Apple desktop GL)")
endif()

# TheSuperHackers @feature githubawn 03/07/2026 Nintendo Switch renders through the
# OpenGL ES backend on switch-mesa (nouveau), NOT Vulkan (there is no Vulkan ICD in the
# Switch homebrew environment; bgfx's native NX path is NVN/licensed which we do not
# have). bgfx defaults BGFX_CONFIG_RENDERER_OPENGLES off for BX_PLATFORM_NX, so force
# the GLES backend on and force the NX glcontext to use EGL (like Android) so renderer_gl
# registers and can create a context on the SDL Switch driver's default NWindow.
if(GGC_RENDER_BACKEND STREQUAL "bgfx" AND CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch" AND TARGET bgfx)
    target_compile_definitions(bgfx PRIVATE
        BGFX_CONFIG_RENDERER_OPENGLES=30
        BGFX_CONFIG_RENDERER_VULKAN=0
        BGFX_USE_EGL=1)
    # switch-mesa GLES/EGL stack (nouveau). PUBLIC so it propagates into the game link.
    target_link_libraries(bgfx PUBLIC EGL GLESv2 glapi drm_nouveau)
    message(STATUS "Forced BGFX_CONFIG_RENDERER_OPENGLES=30 + EGL (Nintendo Switch / switch-mesa)")
endif()

# IDE organization.
foreach(_t bgfx bx bimg shaderc bimg_decode bimg_encode)
    if(TARGET ${_t})
        set_target_properties(${_t} PROPERTIES FOLDER "Dependencies/bgfx")
    endif()
endforeach()

# TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.1 shader compilation.
# bgfx shaders are authored in ".sc" files (GLSL-ish with bgfx pragmas) and
# compiled by the shaderc tool (built as part of bgfx.cmake) into per-platform
# bytecode. The --bin2c option emits a C header with the compiled bytecode as
# a uint8_t array, which we then #include from BgfxBackend.cpp and hand to
# bgfx::createShader via bgfx::makeRef.
#
# Shader output follows GGC_BGFX_RENDERER. DX11 remains the Windows default;
# Metal is the macOS default.
#
# Usage in callers:
#   ggc_compile_bgfx_shader(<source.sc>)   # once per shader file
# Then link the ggc_bgfx_shaders target into whatever library consumes the
# generated headers (from either the corei_ww3d2 INTERFACE chain or directly).
#
# The generated header ends up at ${CMAKE_BINARY_DIR}/ggc_bgfx_shaders/
# with the C array named from the basename, e.g. vs_passthrough_dx11.
#
# The varying.def.sc file is resolved relative to the .sc file's directory
# and must exist there.
#
# The ggc_bgfx_shaders target is a STATIC library with a single dummy .cpp so
# the generated header files have a concrete library to live on. Making it a
# real library (not INTERFACE) is deliberate: CMake's INCLUDE_DIRECTORIES
# propagation through INTERFACE libraries was flaky for our use case and the
# dependency from consumers to the custom_command outputs wasn't firing
# reliably. STATIC + PUBLIC include dirs + link chain is the robust path.

# Shaderc include path (where bgfx_shader.sh lives in the fetched bgfx tree).
set(GGC_BGFX_SHADER_INCLUDE_DIR "${bgfx_cmake_SOURCE_DIR}/bgfx/src" CACHE INTERNAL "")

# TheSuperHackers @build bobtista 14/06/2026 bgfx vendors stb_truetype.h here.
# WW3D2 uses it to rasterize fonts on non-Windows builds (no GDI).
set(GGC_BGFX_STB_INCLUDE_DIR "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty" CACHE INTERNAL "")

# Shared output directory for every compiled shader header.
set(GGC_BGFX_SHADERS_OUT_DIR "${CMAKE_BINARY_DIR}/ggc_bgfx_shaders" CACHE INTERNAL "")

# One-time setup: create the ggc_bgfx_shaders target with a dummy source so
# it compiles as a real STATIC library.
function(ggc_bgfx_shaders_init)
    if(TARGET ggc_bgfx_shaders)
        return()
    endif()

    file(MAKE_DIRECTORY "${GGC_BGFX_SHADERS_OUT_DIR}")
    set(_dummy "${GGC_BGFX_SHADERS_OUT_DIR}/_ggc_bgfx_shaders_dummy.cpp")
    if(NOT EXISTS "${_dummy}")
        file(WRITE "${_dummy}"
             "// Auto-generated. Exists only so ggc_bgfx_shaders has a compilable source.\n"
             "namespace { char ggc_bgfx_shaders_anchor = 0; }\n")
    endif()

    add_library(ggc_bgfx_shaders STATIC "${_dummy}")
    set_target_properties(ggc_bgfx_shaders PROPERTIES FOLDER "Dependencies/bgfx")
    target_include_directories(ggc_bgfx_shaders PUBLIC "${GGC_BGFX_SHADERS_OUT_DIR}")
endfunction()

function(ggc_compile_bgfx_shader source_sc)
    # TheSuperHackers @build githubawn 17/06/2026 Cross builds (Android, iOS)
    # cannot execute the target-built shaderc on the host (an iOS/arm64 shaderc
    # is killed by macOS, an Android one cannot run on the build host). Use a
    # host-compiled shaderc via GGC_SHADERC_EXE instead of the cross target.
    if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten" OR CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
        if(NOT DEFINED GGC_SHADERC_EXE)
            if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" OR WIN32)
                set(GGC_SHADERC_EXE "${CMAKE_SOURCE_DIR}/build/win32-bgfx-standalone/_deps/bgfx_cmake-build/cmake/bgfx/Release/shaderc.exe")
                if(NOT EXISTS "${GGC_SHADERC_EXE}")
                    if(EXISTS "${CMAKE_SOURCE_DIR}/build/win32-bgfx-standalone/_deps/bgfx_cmake-build/cmake/bgfx/Debug/shaderc.exe")
                        set(GGC_SHADERC_EXE "${CMAKE_SOURCE_DIR}/build/win32-bgfx-standalone/_deps/bgfx_cmake-build/cmake/bgfx/Debug/shaderc.exe")
                    elseif(EXISTS "${CMAKE_SOURCE_DIR}/build/win32-bgfx-standalone/Release/shaderc.exe")
                        set(GGC_SHADERC_EXE "${CMAKE_SOURCE_DIR}/build/win32-bgfx-standalone/Release/shaderc.exe")
                    endif()
                endif()
            else()
                # Non-Windows host (macOS / Linux): reuse the macOS/Unix host build's shaderc.
                set(GGC_SHADERC_EXE "${CMAKE_SOURCE_DIR}/build/macos-generalsmd-sdl3-bgfx/_deps/bgfx_cmake-build/cmake/bgfx/Release/shaderc")
            endif()
        endif()
        if(NOT EXISTS "${GGC_SHADERC_EXE}")
            message(FATAL_ERROR "Cross build requires a host-compiled shaderc. "
                                "Build the host bgfx standalone preset first (win32 for Android, "
                                "macos-generalsmd-sdl3-bgfx for iOS), or set -DGGC_SHADERC_EXE=/path/to/shaderc. "
                                "(looked for: ${GGC_SHADERC_EXE})")
        endif()
        set(_shaderc_command "${GGC_SHADERC_EXE}")
        set(_shaderc_dependency "")
    else()
        if(NOT TARGET shaderc)
            message(FATAL_ERROR "ggc_compile_bgfx_shader: shaderc target not available. "
                                "Ensure BGFX_BUILD_TOOLS_SHADER=ON and bgfx.cmake is included.")
        endif()
        set(_shaderc_command "$<TARGET_FILE:shaderc>")
        set(_shaderc_dependency shaderc)
    endif()

    ggc_bgfx_shaders_init()

    get_filename_component(_sc_abs "${source_sc}" ABSOLUTE)
    get_filename_component(_sc_dir "${_sc_abs}" DIRECTORY)
    get_filename_component(_sc_name "${_sc_abs}" NAME_WE)

    if(_sc_name MATCHES "^vs_")
        set(_shader_type "vertex")
    elseif(_sc_name MATCHES "^fs_")
        set(_shader_type "fragment")
    else()
        message(FATAL_ERROR "ggc_compile_bgfx_shader: '${_sc_name}' must start with vs_ or fs_.")
    endif()

    # TheSuperHackers @feature githubawn 22/06/2026 Build a list of shader variants to
    # compile. Normally one per GGC_BGFX_RENDERER, but on Android we additionally build the
    # Vulkan (spirv) variant alongside GLES (essl) so BgfxBackend can pick at runtime
    # (Vulkan preferred, GLES fallback). Each entry is "suffix|platform|profile".
    set(_shader_variants "")
    if(GGC_BGFX_RENDERER STREQUAL "metal")
        list(APPEND _shader_variants "metal|osx|metal")
    elseif(GGC_BGFX_RENDERER STREQUAL "vulkan")
        list(APPEND _shader_variants "spirv|linux|spirv")
    elseif(GGC_BGFX_RENDERER STREQUAL "glsl")
        # Desktop OpenGL. macOS core profile caps at GL 4.1 -> glsl 410.
        list(APPEND _shader_variants "glsl|osx|410")
    elseif(GGC_BGFX_RENDERER STREQUAL "essl")
        list(APPEND _shader_variants "essl|android|300_es")
        if(ANDROID)
            list(APPEND _shader_variants "spirv|linux|spirv")
        endif()
    else()
        list(APPEND _shader_variants "dx11|windows|s_5_0")
    endif()

    set(_varying_def "${_sc_dir}/varying.def.sc")

    foreach(_variant IN LISTS _shader_variants)
        string(REPLACE "|" ";" _variant_parts "${_variant}")
        list(GET _variant_parts 0 _shader_suffix)
        list(GET _variant_parts 1 _shader_platform)
        list(GET _variant_parts 2 _shader_profile)

        set(_out_header "${GGC_BGFX_SHADERS_OUT_DIR}/${_sc_name}_${_shader_suffix}.bin.h")
        set(_varname "${_sc_name}_${_shader_suffix}")

        add_custom_command(
            OUTPUT "${_out_header}"
            COMMAND "${_shaderc_command}"
                -f "${_sc_abs}"
                -o "${_out_header}"
                --bin2c "${_varname}"
                -i "${GGC_BGFX_SHADER_INCLUDE_DIR}"
                --platform "${_shader_platform}"
                --profile "${_shader_profile}"
                --type "${_shader_type}"
                --varyingdef "${_varying_def}"
                -O 3
            MAIN_DEPENDENCY "${_sc_abs}"
            DEPENDS "${_varying_def}" ${_shaderc_dependency}
            COMMENT "Compiling bgfx shader ${_sc_name} (${_shader_suffix})"
            VERBATIM
        )

        target_sources(ggc_bgfx_shaders PRIVATE "${_out_header}")
        set_source_files_properties("${_out_header}" PROPERTIES
            GENERATED TRUE
            HEADER_FILE_ONLY TRUE
        )
    endforeach()
endfunction()
