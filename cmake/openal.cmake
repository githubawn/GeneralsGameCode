# cmake/openal.cmake
#
# TheSuperHackers @build bobtista 28/04/2026 OpenAL Soft dependency for the
# GeneralsMD macOS-native audio branch.

if(SAGE_USE_OPENAL)
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        # TheSuperHackers @build 27/06/2026 Emscripten ships its own OpenAL
        # implementation (AL/*.h headers in the sysroot, mapped onto Web Audio).
        # Link the built-in -lopenal instead of compiling OpenAL Soft for wasm.
        message(STATUS "Using Emscripten's built-in OpenAL for GeneralsMD audio")
        if(NOT TARGET OpenAL::OpenAL)
            add_library(OpenAL::OpenAL INTERFACE IMPORTED GLOBAL)
            set_target_properties(OpenAL::OpenAL PROPERTIES
                INTERFACE_LINK_LIBRARIES "openal"
            )
        endif()
    else()
        message(STATUS "Configuring OpenAL Soft for GeneralsMD audio")

        include(FetchContent)

        FetchContent_Declare(
            openal_soft
            URL "https://github.com/kcat/openal-soft/archive/refs/tags/1.24.2.tar.gz"
            URL_HASH "SHA256=7efd383d70508587fbc146e4c508771a2235a5fc8ae05bf6fe721c20a348bd7c"
        )

        set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
        set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
        set(ALSOFT_NO_CONFIG_UTIL ON CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(openal_soft)
    endif()
endif()
