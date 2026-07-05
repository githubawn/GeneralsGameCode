# cmake/sdl3.cmake
#
# TheSuperHackers @build bobtista 28/04/2026 SDL3 dependency for the
# GeneralsMD macOS-native platform backend. SDL3 is currently used only for
# windowing, event pumping, and input.

if(SAGE_USE_SDL3)
    message(STATUS "Configuring SDL3 for GeneralsMD platform backend")

    include(FetchContent)

    set(GGC_SDL3_VERSION "3.4.2" CACHE STRING "SDL3 version used by the GeneralsMD SDL3 backend")
    set(GGC_SDL3_URL "https://github.com/libsdl-org/SDL/releases/download/release-${GGC_SDL3_VERSION}/SDL3-${GGC_SDL3_VERSION}.tar.gz")
    set(GGC_SDL3_URL_HASH "SHA256=ef39a2e3f9a8a78296c40da701967dd1b0d0d6e267e483863ce70f8a03b4050c")

    if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch" OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    else()
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    endif()
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    # Disable tests and examples for Switch builds
    if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(
        SDL3
        URL ${GGC_SDL3_URL}
        URL_HASH ${GGC_SDL3_URL_HASH}
    )

    FetchContent_GetProperties(SDL3)
    if(NOT sdl3_POPULATED)
        FetchContent_Populate(SDL3)

        if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
            if(NOT EXISTS "${sdl3_SOURCE_DIR}/src/audio/switch/SDL_switchaudio.c")
                message(STATUS "Patching SDL3 for NintendoSwitch support using neomody77/sdl3-switch...")
                
                # 1. Clone the patch repository
                set(PATCH_REPO_DIR "${CMAKE_BINARY_DIR}/sdl3-patch-repo")
                if(NOT EXISTS "${PATCH_REPO_DIR}")
                    execute_process(
                        COMMAND git clone https://github.com/neomody77/sdl3-switch.git "${PATCH_REPO_DIR}"
                        RESULT_VARIABLE CLONE_RES
                    )
                    if(NOT CLONE_RES EQUAL 0)
                        message(FATAL_ERROR "Failed to clone SDL3 patch repository!")
                    endif()
                endif()

                # 2. Init temporary git repo in the extracted SDL3 source directory to allow git apply to run
                execute_process(
                    COMMAND git init
                    WORKING_DIRECTORY "${sdl3_SOURCE_DIR}"
                    RESULT_VARIABLE INIT_RES
                    OUTPUT_QUIET
                )

                # 3. Apply the patch (strip the a/external/SDL3/ prefix and ignore CRLF/LF line endings)
                execute_process(
                    COMMAND git apply -p3 --ignore-whitespace --ignore-space-change "${PATCH_REPO_DIR}/sdl3-switch.patch"
                    WORKING_DIRECTORY "${sdl3_SOURCE_DIR}"
                    RESULT_VARIABLE PATCH_RES
                )
                if(NOT PATCH_RES EQUAL 0)
                    message(FATAL_ERROR "Failed to apply SDL3 Switch patch!")
                endif()
                
                message(STATUS "Successfully applied SDL3 Switch patch.")
            else()
                message(STATUS "SDL3 is already patched for NintendoSwitch.")
            endif()
        endif()

        add_subdirectory(${sdl3_SOURCE_DIR} ${sdl3_BINARY_DIR})
    endif()

    add_library(sdl3lib INTERFACE)
    target_link_libraries(sdl3lib INTERFACE SDL3::SDL3)
endif()
