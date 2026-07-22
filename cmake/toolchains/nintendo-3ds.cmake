# cmake/toolchains/nintendo-3ds.cmake
# Configures devkitARM and libctru for New Nintendo 3DS cross-compilation

set(CMAKE_SYSTEM_NAME "Nintendo3DS")
set(CMAKE_SYSTEM_PROCESSOR "armv6k")

if(NOT DEFINED ENV{DEVKITPRO})
    set(ENV{DEVKITPRO} "C:/code/devkitPro")
endif()

set(DEVKITPRO "$ENV{DEVKITPRO}")
set(DEVKITARM "${DEVKITPRO}/devkitARM")

# Cross-compilers
set(CMAKE_C_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc.exe")
set(CMAKE_CXX_COMPILER "${DEVKITARM}/bin/arm-none-eabi-g++.exe")
set(CMAKE_ASM_COMPILER "${DEVKITARM}/bin/arm-none-eabi-as.exe")
set(CMAKE_AR "${DEVKITARM}/bin/arm-none-eabi-gcc-ar.exe" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${DEVKITARM}/bin/arm-none-eabi-gcc-ranlib.exe" CACHE FILEPATH "Ranlib")

# Target environment paths
set(CMAKE_FIND_ROOT_PATH "${DEVKITPRO}/libctru" "${DEVKITPRO}/portlibs/3ds")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# TheSuperHackers @build githubawn 14/07/2026 New 3DS target only (old 3DS's
# 64-96MB is not viable for this engine, see docs/3ds-port-plan.md). ARM11
# MPCore, hardware VFP, soft thread-pointer model (libctru does not provide
# the TLS register access the Switch's hardware -ftls-model=local-exec relies
# on, so this stays -mtp=soft rather than mirroring the Switch flag choice).
set(ARCH "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations")

# TheSuperHackers @build githubawn 14/07/2026 -fno-strict-aliasing is REQUIRED.
# Same rationale as the Switch toolchain (cmake/toolchains/nintendo-switch.cmake):
# this codebase was written for VC6/MSVC (no type-based alias analysis) and is
# full of DX8/COM type-punning through reinterpret_cast. GCC (devkitARM) at
# -O2 aggressively exploits strict aliasing and miscompiles those accesses.
#
# TheSuperHackers @bugfix githubawn 15/07/2026 -fno-short-enums is REQUIRED.
# ARM EABI's default (which devkitARM's GCC follows unless overridden) packs a
# plain enum into the smallest type that fits its enumerator range -- e.g.
# TerrainClass (~38 values) becomes a 1-byte field here, vs. a full 4-byte int
# on every other platform this codebase targets (MSVC always uses int; Linux/
# macOS/Android/Switch's GCC/Clang all default to int-sized plain enums too).
# INI::parseIndexList (Core/GameEngine/Source/Common/INI/INI.cpp) writes enum
# fields via a hardcoded `*(Int*)store = value` (see its own comment: "it is
# assumed that we are going to store the index into a 4 byte integer") --
# widely used for INI "Class"-style fields across the engine. With a 1-byte
# enum that 4-byte write overflows into whatever follows in memory. Observed
# as TerrainTypeCollection's linked list silently losing bytes off its m_next
# pointer immediately after each entry's "Class" INI field was parsed (root-
# caused via SDL_Log instrumentation: sizeof(TerrainType) was 20 here vs. the
# 24 a 4-byte-enum layout requires, and every corrupted m_next value matched
# its correct address with only the low byte zeroed). Forcing int-sized plain
# enums here matches what every other supported platform already does and
# what parseIndexList's own contract assumes, without touching engine code.
set(CMAKE_C_FLAGS "${ARCH} -O2 -fno-strict-aliasing -fno-short-enums -ffunction-sections -fdata-sections -Wall -D__3DS__ -D_GNU_SOURCE" CACHE STRING "C flags" FORCE)
set(CMAKE_CXX_FLAGS "${ARCH} -O2 -fno-strict-aliasing -fno-short-enums -ffunction-sections -fdata-sections -Wall -D__3DS__ -D_GNU_SOURCE -fpermissive" CACHE STRING "C++ flags" FORCE)

include_directories(SYSTEM "${DEVKITPRO}/libctru/include" "${DEVKITPRO}/portlibs/3ds/include")
link_directories("${DEVKITPRO}/libctru/lib" "${DEVKITPRO}/portlibs/3ds/lib")

# Specifying specs file for libctru linking (3dsx homebrew output, not CIA).
# TheSuperHackers @build githubawn 14/07/2026 -lctru is REQUIRED on the link
# line unconditionally, even for CMake's own compiler-sanity-check trivial
# program: 3dsx_crt0.o's startup/ClrLoop reference initSystem/__system_argc/
# __system_argv, which only libctru provides. Every devkitARM 3DS project
# links libctru for this reason (it is not an engine-specific dependency,
# unlike citro3d/citro2d which corei_ww3d2 links separately per backend).
# TheSuperHackers @feature githubawn 19/07/2026 Match-exit crash groundwork:
# wrap abort() so any abort (assert failure, uncaught std::terminate fallback,
# etc.) logs via SDL_Log before the process dies, instead of vanishing
# silently (libctru has no POSIX signal handlers to catch this the way the
# Android build's SIGABRT handler does -- see GeneralsMD/Code/Main/
# ThreeDSPlatformStubs.cpp's __wrap_abort for the implementation). Scoped to
# this 3DS-only toolchain file so no other platform's link flags change.
set(CMAKE_EXE_LINKER_FLAGS "-specs=3dsx.specs -g ${ARCH} -Wl,--allow-multiple-definition -Wl,--wrap=abort -L${DEVKITPRO}/libctru/lib -lctru -lm" CACHE STRING "Linker flags" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# TheSuperHackers @build githubawn 14/07/2026 Same rationale as the Switch
# toolchain: wrap all libraries (direct and transitive) in
# --start-group/--end-group to resolve circular dependencies between engine
# libraries, SDL3, and libctru.
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")

# Disable response files so that the linker commands are output directly on the command line,
# preserving the start-group/end-group wrapping around the library list.
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES OFF CACHE BOOL "Disable response files for libraries" FORCE)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS OFF CACHE BOOL "Disable response files for objects" FORCE)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES OFF CACHE BOOL "Disable response files for libraries" FORCE)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS OFF CACHE BOOL "Disable response files for objects" FORCE)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE OFF CACHE BOOL "Force response files" FORCE)

# Disable tools requiring MFC/Win32/DirectX
set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Disable tools for 3DS" FORCE)
set(RTS_BUILD_GENERALS_TOOLS OFF CACHE BOOL "Disable tools for 3DS" FORCE)
set(RTS_BUILD_ZEROHOUR_TOOLS OFF CACHE BOOL "Disable tools for 3DS" FORCE)

# TheSuperHackers @build githubawn 14/07/2026 Same corruption risk documented
# on Switch (cmake/toolchains/nintendo-switch.cmake): the custom GameMemory
# pool allocator and its global operator new/delete overrides are unproven
# under devkitARM/GCC + static libstdc++. Use the GameMemoryNull backend
# (plain malloc/free) on 3DS until/unless proven safe. Isolated to this
# toolchain so no other platform's allocator behavior changes.
set(RTS_GAMEMEMORY_ENABLE OFF CACHE BOOL "Use system malloc/free on 3DS" FORCE)
