
# Print some information
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
if (DEFINED MSVC_VERSION)
    message(STATUS "MSVC_VERSION: ${MSVC_VERSION}")
endif()

# TheSuperHackers @build JohnsterID 05/01/2026 Add MinGW-w64 detection and configure compiler flags
# Detect MinGW-w64
if(MINGW)
    message(STATUS "MinGW-w64 detected")
    set(IS_MINGW_BUILD TRUE)
else()
    set(IS_MINGW_BUILD FALSE)
endif()

# Set variable for VS6 to handle special cases.
if (DEFINED MSVC_VERSION AND MSVC_VERSION LESS 1300)
    set(IS_VS6_BUILD TRUE)
else()
    set(IS_VS6_BUILD FALSE)
endif()

# Make release builds have debug information too.
if(MSVC)
    # Create PDB for Release as long as debug info was generated during compile.
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    
    # /INCREMENTAL:NO prevents PDB size bloat in Debug configuration(s).
    add_link_options("/INCREMENTAL:NO")
else()
    # We go a bit wild here and assume any other compiler we are going to use supports -g for debug info.
    # Add debug symbols to Release builds for crash dump analysis, profiling, and post-mortem debugging.
    # For MinGW, symbols will be stripped to separate .debug files (matching MSVC PDB workflow).
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
    string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Ensures only ISO features are used

if (NOT IS_VS6_BUILD)
    if (MSVC)
        # Multithreaded build.
        add_compile_options(/MP)
        # Enforce strict __cplusplus version
        add_compile_options(/Zc:__cplusplus)
    else()
        add_compile_options(-Wsuggest-override)
        # TheSuperHackers @build githubawn 17/06/2026 Clang (Apple/iOS and the
        # Android NDK) does not parse the __declspec attributes used by the
        # Win32 DX8 and Bink SDK headers unless -fdeclspec is enabled. GCC and
        # MinGW support __declspec natively, so scope this to Clang only.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-fdeclspec)
        endif()
        if(EMSCRIPTEN)
            add_link_options(-sALLOW_MEMORY_GROWTH=1)
            add_link_options(-sMAXIMUM_MEMORY=4194304000)
            # The engine uses wchar_t=4, and we should disable warning suggests to keep build clean
            add_compile_options(-Wno-suggest-override)
            # TheSuperHackers @feature githubawn 24/06/2026 Enable real threads (Web Workers)
            # so the engine's worker threads (texture loader, audio, file preload, ...) run
            # like every other platform instead of the no-op stubs in thread.cpp. Without
            # this the texture-load thread never runs and the synchronous drain misses
            # assets, leaving units/terrain textures (and reloaded font glyph atlases)
            # blank/magenta after the shell map loads. -pthread must be on BOTH compile and
            # link and for every target/dep (added globally here, before SDL3/bgfx). Requires
            # the page to be cross-origin isolated (serve.py already sends COOP/COEP).
            add_compile_options(-pthread)
            add_link_options(-pthread)
            # TheSuperHackers @build githubawn 23/06/2026 The engine uses C++ exceptions
            # for INI/asset error handling and control flow. Emscripten disables exception
            # catching by default (any throw -> "exception catching is not enabled" abort),
            # so enable JS-based exception support on both compile and link. -fexceptions
            # is compatible with -sASYNCIFY (unlike -fwasm-exceptions, which conflicts).
            add_compile_options(-fexceptions)
            add_link_options(-fexceptions)
        endif()
    endif()
else()
    if(RTS_BUILD_OPTION_VC6_FULL_DEBUG)
        set_property(GLOBAL PROPERTY JOB_POOLS compile=1 link=1)
    else()
        # Define two pools: 'compile' with plenty of slots, 'link' with just one
        set_property(GLOBAL PROPERTY JOB_POOLS compile=0 link=1)
    endif()

    # Tell CMake that all compile steps go into 'compile'
    set(CMAKE_JOB_POOL_COMPILE compile)
    # and all link steps go into 'link' (so only one link ever runs since vc6 can't handle multithreaded linking)
    set(CMAKE_JOB_POOL_LINK link)
endif()

if(RTS_BUILD_OPTION_ASAN)
    if(MSVC)
        set(ENV{ASAN_OPTIONS} "shadow_scale=2")
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()
