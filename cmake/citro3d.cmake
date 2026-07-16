# cmake/citro3d.cmake
#
# TheSuperHackers @build githubawn 14/07/2026 citro3d/citro2d dependency for
# the GGC_RENDER_BACKEND=citro3d build (New Nintendo 3DS). Included from
# cmake/render-backend.cmake. Not included when GGC_RENDER_BACKEND is dx8 or
# bgfx.
#
# Unlike bgfx (fetched via FetchContent), citro3d/citro2d/libctru are
# provided by the devkitPro package manager and are already resolvable via
# CMAKE_FIND_ROOT_PATH set up in cmake/toolchains/nintendo-3ds.cmake, so this
# just locates the libraries and wraps them in one INTERFACE target.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Nintendo3DS")
    message(FATAL_ERROR "cmake/citro3d.cmake included on non-3DS target.")
endif()

find_library(GGC_CITRO3D_LIBRARY NAMES citro3d PATHS "${DEVKITPRO}/libctru/lib" REQUIRED)
find_library(GGC_CITRO2D_LIBRARY NAMES citro2d PATHS "${DEVKITPRO}/libctru/lib" REQUIRED)
find_library(GGC_CTRU_LIBRARY NAMES ctru PATHS "${DEVKITPRO}/libctru/lib" REQUIRED)

add_library(citro3dlib INTERFACE)
target_link_libraries(citro3dlib INTERFACE
    "${GGC_CITRO3D_LIBRARY}"
    "${GGC_CITRO2D_LIBRARY}"
    "${GGC_CTRU_LIBRARY}"
)
target_include_directories(citro3dlib INTERFACE "${DEVKITPRO}/libctru/include")

message(STATUS "citro3d backend: found citro3d=${GGC_CITRO3D_LIBRARY} citro2d=${GGC_CITRO2D_LIBRARY} ctru=${GGC_CTRU_LIBRARY}")

# TheSuperHackers @build githubawn 14/07/2026 3DS has no GDI, same as the
# bgfx non-Windows path in render2dsentence.cpp, which rasterizes glyphs with
# stb_truetype.h against a system TrueType font (source expects the header at
# <stb/stb_truetype.h>, unconditionally on !defined(_WIN32) — not gated on
# the bgfx backend). bgfx builds get it for free from bgfx's vendored
# 3rdparty/stb copy; citro3d builds don't fetch bgfx at all, so fetch just
# this one header directly into a matching stb/ subdirectory.
set(GGC_STB_TRUETYPE_URL "https://raw.githubusercontent.com/nothings/stb/f0569113c93ad095470c54bf34a17b36646bbbb/stb_truetype.h")
set(GGC_STB_INCLUDE_DIR "${CMAKE_BINARY_DIR}/ggc_stb_include" CACHE INTERNAL "")
set(GGC_STB_TRUETYPE_HEADER "${GGC_STB_INCLUDE_DIR}/stb/stb_truetype.h")
if(NOT EXISTS "${GGC_STB_TRUETYPE_HEADER}")
    message(STATUS "Fetching stb_truetype.h for citro3d (3DS has no GDI)...")
    # Pinned to an exact upstream commit SHA in the URL (same reproducibility
    # guarantee as this repo's GIT_TAG-pinned FetchContent deps); no separate
    # file hash check.
    file(DOWNLOAD "${GGC_STB_TRUETYPE_URL}" "${GGC_STB_TRUETYPE_HEADER}" TLS_VERIFY ON)
endif()
target_include_directories(citro3dlib INTERFACE "${GGC_STB_INCLUDE_DIR}")

# TheSuperHackers @feature githubawn 15/07/2026 PICA200 shader compile step
# (docs/3ds-port-plan.md Phase 3 Milestone 2). picasso assembles a .v.pica
# source into a .shbin, which Citro3dBackend loads via DVLB_ParseFile. Unlike
# bgfx's shaderc (cmake/bgfx.cmake), picasso has no --bin2c: convert the
# .shbin to a C header ourselves via cmake/scripts/bin2h.cmake so it can be
# #include'd directly, matching how ggc_embedded_font.h is consumed.
find_program(GGC_PICASSO_EXE NAMES picasso PATHS "${DEVKITPRO}/tools/bin" REQUIRED)

# TheSuperHackers @bugfix githubawn 15/07/2026 corei_ww3d2 (the target that
# eventually needs the generated shader header) is an INTERFACE library
# (add_library(corei_ww3d2 INTERFACE), see WW3D2's CMakeLists.txt). A GENERATED
# custom-command OUTPUT added via target_sources(interface_lib INTERFACE ...)
# is silently never scheduled by Ninja -- INTERFACE libraries have no object
# files of their own, and CMake's generated-source machinery needs a real
# (STATIC/OBJECT) target to actually own the build edge. Plain (non-generated)
# sources like Citro3dBackend.cpp propagate through an INTERFACE library fine;
# GENERATED ones silently vanish (no CMake error -- just a later #include
# "file not found" once the consumer tries to compile). ggc_compile_bgfx_shader
# (cmake/bgfx.cmake) hits the exact same requirement and solves it the same
# way: a small dummy-sourced STATIC library owns the generated headers, and
# corei_ww3d2 links against that real target instead of holding the generated
# source itself.
function(ggc_pica_shaders_init)
    if(TARGET ggc_pica_shaders)
        return()
    endif()
    set(_dummy "${CMAKE_BINARY_DIR}/ggc_pica_shaders_dummy.c")
    if(NOT EXISTS "${_dummy}")
        file(WRITE "${_dummy}" "/* dummy TU so ggc_pica_shaders is a real, buildable target */\nint ggc_pica_shaders_dummy_symbol = 0;\n")
    endif()
    add_library(ggc_pica_shaders STATIC "${_dummy}")
endfunction()

function(ggc_compile_pica_shader source_pica out_header_var)
    ggc_pica_shaders_init()

    get_filename_component(_src_abs "${source_pica}" ABSOLUTE)
    get_filename_component(_name "${_src_abs}" NAME)
    # NAME_WE only strips the LAST extension, leaving "vs_2d.v" for a
    # "vs_2d.v.pica" input (two dots). Strip the whole ".v.pica" suffix
    # explicitly so the generated symbol/header names are "vs_2d", matching
    # the #include the backend actually uses.
    string(REGEX REPLACE "\\.v\\.pica$" "" _name "${_name}")

    set(_out_dir "${CMAKE_BINARY_DIR}/ggc_pica_shaders")
    set(_shbin "${_out_dir}/${_name}.shbin")
    set(_header "${_out_dir}/${_name}_shbin.h")
    set(_varname "${_name}_shbin")

    add_custom_command(
        OUTPUT "${_shbin}"
        COMMAND "${GGC_PICASSO_EXE}" -o "${_shbin}" "${_src_abs}"
        MAIN_DEPENDENCY "${_src_abs}"
        COMMENT "Compiling PICA200 shader ${_name}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${_header}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${_shbin}" "-DOUTPUT=${_header}" "-DVARNAME=${_varname}"
            -P "${CMAKE_SOURCE_DIR}/cmake/scripts/bin2h.cmake"
        DEPENDS "${_shbin}" "${CMAKE_SOURCE_DIR}/cmake/scripts/bin2h.cmake"
        COMMENT "Embedding ${_name}.shbin as a C header"
        VERBATIM
    )
    target_sources(ggc_pica_shaders PRIVATE "${_header}")
    set_source_files_properties("${_header}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    target_include_directories(citro3dlib INTERFACE "${_out_dir}")
    # Force build ordering: anything linking citro3dlib now depends on
    # ggc_pica_shaders, which is what actually owns the custom-command edge
    # that builds this header before any consumer tries to #include it.
    target_link_libraries(citro3dlib INTERFACE ggc_pica_shaders)

    set(${out_header_var} "${_header}" PARENT_SCOPE)
endfunction()
