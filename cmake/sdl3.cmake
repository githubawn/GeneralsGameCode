# cmake/sdl3.cmake
#
# TheSuperHackers @build bobtista 28/04/2026 SDL3 dependency for the
# GeneralsMD macOS-native platform branch. SDL3 is currently used only for
# windowing, event pumping, and input.

if(SAGE_USE_SDL3)
    message(STATUS "Configuring SDL3 for GeneralsMD platform backend")

    include(FetchContent)

    set(GGC_SDL3_VERSION "3.4.2" CACHE STRING "SDL3 version used by the GeneralsMD SDL3 backend")
    set(GGC_SDL3_URL "https://github.com/libsdl-org/SDL/releases/download/release-${GGC_SDL3_VERSION}/SDL3-${GGC_SDL3_VERSION}.tar.gz")
    set(GGC_SDL3_URL_HASH "SHA256=ef39a2e3f9a8a78296c40da701967dd1b0d0d6e267e483863ce70f8a03b4050c")

    # TheSuperHackers @bugfix githubawn 18/06/2026 On iOS build SDL3 statically.
    # An app that links SDL3 as an @rpath dylib needs the dylib embedded and
    # signed inside the .app bundle; without that, dyld aborts at launch
    # (__abort_with_payload SIGABRT, before main()). Static linking avoids the
    # embed/rpath/codesign dance entirely. When only the static lib is built,
    # SDL's CMake aliases SDL3::SDL3 to SDL3-static, so consumers are unchanged.
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    else()
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    endif()
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        URL ${GGC_SDL3_URL}
        URL_HASH ${GGC_SDL3_URL_HASH}
    )
    FetchContent_MakeAvailable(SDL3)

    add_library(sdl3lib INTERFACE)
    target_link_libraries(sdl3lib INTERFACE SDL3::SDL3)
endif()
