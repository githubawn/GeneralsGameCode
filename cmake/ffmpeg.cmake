# cmake/ffmpeg.cmake
#
# TheSuperHackers @build 27/06/2026 Provide FFmpeg for the OpenAL audio backend
# and the FFmpeg video player on every supported platform.
#
# Desktop and macOS use a system FFmpeg via find_package(FFMPEG) (Homebrew,
# vcpkg, apt, ...). The mobile/web cross targets (Android NDK, iOS, Emscripten)
# have no system FFmpeg, so they SELF-COMPILE a minimal, static FFmpeg from
# source via scripts/ffmpeg/build-ffmpeg.sh, on the matching native host:
#
#   Windows host -> compiles the android / wasm FFmpeg
#   macOS host   -> compiles the ios FFmpeg (macOS itself uses brew)
#
# This happens automatically at configure time (guarded so it only runs once per
# build tree). It needs the host's normal cross toolchain on PATH plus a POSIX
# shell + make:
#   android : ANDROID_NDK_HOME, bash, make
#   wasm    : activated emsdk (emconfigure/emmake), bash, make
#   ios     : Xcode (xcrun), bash, make
# On Windows that means a bash + make (Git Bash + e.g. MSYS2/mingw32-make).
#
# To skip the self-compile (CI cache, prebuilt artifact, an existing tree) pass:
#   -DGGC_FFMPEG_PREBUILT_DIR=/path/to/ffmpeg-<plat>        (local tree), or
#   -DGGC_FFMPEG_PREBUILT_URL=https://.../ffmpeg-<plat>.tar.xz [-DGGC_FFMPEG_PREBUILT_SHA256=<hash>]
#
# The tree layout is the usual FFmpeg install prefix:
#   include/libavcodec/avcodec.h, ...
#   lib/libavformat.a, lib/libavcodec.a, lib/libavutil.a [, lib/libswscale.a]
#
# This module sets the same surface FindFFMPEG.cmake produces:
#   FFMPEG_FOUND, FFMPEG_INCLUDE_DIRS, FFMPEG_LIBRARIES, FFMPEG_LIBRARY_DIRS
#   FFMPEG::FFMPEG

if(FFMPEG_FOUND)
    return()
endif()

# ---------------------------------------------------------------------------
# Desktop / macOS: system FFmpeg. (Mac builds macOS natively; brew/vcpkg/apt.)
# ---------------------------------------------------------------------------
if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Android"
        OR CMAKE_SYSTEM_NAME STREQUAL "iOS"
        OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten"))
    find_package(FFMPEG REQUIRED)
    return()
endif()

# ---------------------------------------------------------------------------
# Cross targets: consume a prebuilt FFmpeg.
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(_ffmpeg_plat "android-arm64")
    set(_ffmpeg_target "android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_ffmpeg_plat "ios-arm64")
    set(_ffmpeg_target "ios")
else()
    set(_ffmpeg_plat "wasm32")
    set(_ffmpeg_target "wasm")
endif()

# Video support (swscale + Bink) is built when the FFmpeg video player is on, so
# the prebuilt archive is expected to carry libswscale in that case.
if(RTS_BUILD_OPTION_FFMPEG)
    set(_ffmpeg_libnames avformat avcodec swscale avutil)
else()
    set(_ffmpeg_libnames avformat avcodec avutil)
endif()

set(GGC_FFMPEG_PREBUILT_DIR "" CACHE PATH
    "Path to a prebuilt FFmpeg install tree (include/ + lib/) for ${_ffmpeg_plat}")
set(GGC_FFMPEG_PREBUILT_URL "" CACHE STRING
    "URL of a prebuilt FFmpeg archive (include/ + lib/) for ${_ffmpeg_plat}")
set(GGC_FFMPEG_PREBUILT_SHA256 "" CACHE STRING
    "Expected SHA256 of the prebuilt FFmpeg archive (optional)")

# --- resolve the prebuilt install tree -------------------------------------
set(_ffmpeg_prefix "")

if(GGC_FFMPEG_PREBUILT_DIR)
    set(_ffmpeg_prefix "${GGC_FFMPEG_PREBUILT_DIR}")
elseif(GGC_FFMPEG_PREBUILT_URL)
    set(_ffmpeg_dl_root "${CMAKE_BINARY_DIR}/_deps/ffmpeg-prebuilt-${_ffmpeg_plat}")
    set(_ffmpeg_extract "${_ffmpeg_dl_root}/tree")
    if(NOT EXISTS "${_ffmpeg_extract}/include/libavcodec/avcodec.h")
        # Derive an archive filename from the URL.
        string(REGEX REPLACE "^.*/" "" _ffmpeg_archive_name "${GGC_FFMPEG_PREBUILT_URL}")
        if(NOT _ffmpeg_archive_name)
            set(_ffmpeg_archive_name "ffmpeg-${_ffmpeg_plat}-archive")
        endif()
        set(_ffmpeg_archive "${_ffmpeg_dl_root}/${_ffmpeg_archive_name}")
        file(MAKE_DIRECTORY "${_ffmpeg_dl_root}")
        if(NOT EXISTS "${_ffmpeg_archive}")
            message(STATUS "FFmpeg(${_ffmpeg_plat}): downloading prebuilt ${GGC_FFMPEG_PREBUILT_URL}")
            if(GGC_FFMPEG_PREBUILT_SHA256)
                file(DOWNLOAD "${GGC_FFMPEG_PREBUILT_URL}" "${_ffmpeg_archive}"
                    SHOW_PROGRESS EXPECTED_HASH "SHA256=${GGC_FFMPEG_PREBUILT_SHA256}"
                    STATUS _ffmpeg_dl_status)
            else()
                file(DOWNLOAD "${GGC_FFMPEG_PREBUILT_URL}" "${_ffmpeg_archive}"
                    SHOW_PROGRESS STATUS _ffmpeg_dl_status)
            endif()
            list(GET _ffmpeg_dl_status 0 _ffmpeg_dl_code)
            if(NOT _ffmpeg_dl_code EQUAL 0)
                list(GET _ffmpeg_dl_status 1 _ffmpeg_dl_msg)
                file(REMOVE "${_ffmpeg_archive}")
                message(FATAL_ERROR "FFmpeg prebuilt download failed: ${_ffmpeg_dl_msg}")
            endif()
        endif()
        file(MAKE_DIRECTORY "${_ffmpeg_extract}")
        file(ARCHIVE_EXTRACT INPUT "${_ffmpeg_archive}" DESTINATION "${_ffmpeg_extract}")
    endif()

    # Tolerate a single wrapping directory inside the archive.
    if(EXISTS "${_ffmpeg_extract}/include/libavcodec/avcodec.h")
        set(_ffmpeg_prefix "${_ffmpeg_extract}")
    else()
        file(GLOB_RECURSE _ffmpeg_hdr "${_ffmpeg_extract}/*/include/libavcodec/avcodec.h")
        if(_ffmpeg_hdr)
            list(GET _ffmpeg_hdr 0 _ffmpeg_hdr0)
            # .../<prefix>/include/libavcodec/avcodec.h -> <prefix>
            get_filename_component(_ffmpeg_prefix "${_ffmpeg_hdr0}" DIRECTORY) # libavcodec
            get_filename_component(_ffmpeg_prefix "${_ffmpeg_prefix}" DIRECTORY) # include
            get_filename_component(_ffmpeg_prefix "${_ffmpeg_prefix}" DIRECTORY) # prefix
        endif()
    endif()
else()
    # --- self-compile via scripts/ffmpeg/build-ffmpeg.sh -------------------
    set(_ffmpeg_sc_root "${CMAKE_BINARY_DIR}/_deps/ffmpeg-selfbuild-${_ffmpeg_target}")
    set(_ffmpeg_prefix "${_ffmpeg_sc_root}/ffmpeg-${_ffmpeg_target}")

    if(NOT EXISTS "${_ffmpeg_prefix}/include/libavcodec/avcodec.h")
        find_program(GGC_BASH_EXE NAMES bash)
        find_program(GGC_MAKE_EXE NAMES make gmake mingw32-make)
        if(NOT GGC_BASH_EXE OR NOT GGC_MAKE_EXE)
            message(FATAL_ERROR
                "Self-compiling FFmpeg for ${_ffmpeg_plat} needs 'bash' and 'make' on PATH "
                "(Windows: Git Bash + make/mingw32-make). Install them, or skip the build with "
                "-DGGC_FFMPEG_PREBUILT_DIR=<tree> / -DGGC_FFMPEG_PREBUILT_URL=<archive>.")
        endif()
        message(STATUS "FFmpeg(${_ffmpeg_plat}): self-compiling via scripts/ffmpeg/build-ffmpeg.sh")
        execute_process(
            COMMAND "${GGC_BASH_EXE}" "${CMAKE_SOURCE_DIR}/scripts/ffmpeg/build-ffmpeg.sh"
                    "${_ffmpeg_target}" "${_ffmpeg_sc_root}"
            RESULT_VARIABLE _ffmpeg_sc_res
        )
        if(NOT _ffmpeg_sc_res EQUAL 0)
            message(FATAL_ERROR "FFmpeg self-compile for ${_ffmpeg_plat} failed (exit ${_ffmpeg_sc_res}).")
        endif()
    endif()
endif()

if(NOT _ffmpeg_prefix OR NOT EXISTS "${_ffmpeg_prefix}/include/libavcodec/avcodec.h")
    message(FATAL_ERROR
        "FFmpeg for ${_ffmpeg_plat} is unavailable. Provide a tree via "
        "-DGGC_FFMPEG_PREBUILT_DIR / -DGGC_FFMPEG_PREBUILT_URL, or ensure the "
        "self-compile toolchain is present (see scripts/ffmpeg/build-ffmpeg.sh). "
        "The tree must contain include/libavcodec/avcodec.h and lib/lib*.a.")
endif()

# --- assemble the variables + target ---------------------------------------
set(FFMPEG_INCLUDE_DIRS "${_ffmpeg_prefix}/include")
set(FFMPEG_LIBRARY_DIRS "${_ffmpeg_prefix}/lib")
set(FFMPEG_LIBRARIES "")
foreach(_lib IN LISTS _ffmpeg_libnames)
    set(_libpath "${_ffmpeg_prefix}/lib/lib${_lib}.a")
    if(NOT EXISTS "${_libpath}")
        message(FATAL_ERROR
            "Prebuilt FFmpeg for ${_ffmpeg_plat} is missing lib${_lib}.a at ${_libpath}. "
            "Rebuild the archive with the required components "
            "(video build also needs libswscale).")
    endif()
    list(APPEND FFMPEG_LIBRARIES "${_libpath}")
endforeach()

set(FFMPEG_FOUND TRUE)

if(NOT TARGET FFMPEG::FFMPEG)
    add_library(FFMPEG::FFMPEG INTERFACE IMPORTED GLOBAL)
    set_target_properties(FFMPEG::FFMPEG PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${FFMPEG_LIBRARIES}"
        INTERFACE_LINK_DIRECTORIES "${FFMPEG_LIBRARY_DIRS}"
    )
endif()

message(STATUS "FFmpeg(${_ffmpeg_plat}): using prebuilt ${_ffmpeg_prefix}")
