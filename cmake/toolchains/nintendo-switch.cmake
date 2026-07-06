# cmake/toolchains/nintendo-switch.cmake
# Configures devkitA64 and libnx for Nintendo Switch cross-compilation

set(CMAKE_SYSTEM_NAME "NintendoSwitch")
set(CMAKE_SYSTEM_PROCESSOR "aarch64")

if(NOT DEFINED ENV{DEVKITPRO})
    set(ENV{DEVKITPRO} "C:/code/devkitPro")
endif()

set(DEVKITPRO "$ENV{DEVKITPRO}")
set(DEVKITA64 "${DEVKITPRO}/devkitA64")

# Cross-compilers
set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-g++.exe")
set(CMAKE_ASM_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-as.exe")
set(CMAKE_AR "${DEVKITA64}/bin/aarch64-none-elf-gcc-ar.exe" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${DEVKITA64}/bin/aarch64-none-elf-gcc-ranlib.exe" CACHE FILEPATH "Ranlib")

# Target environment paths
set(CMAKE_FIND_ROOT_PATH "${DEVKITPRO}/libnx" "${DEVKITPRO}/portlibs/switch")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Compile flags targeting Nintendo Switch CPU
# TheSuperHackers @build githubawn 03/07/2026 Use the standard libnx hardware TLS
# (thread pointer via TPIDRRO_EL0, -ftls-model=local-exec), NOT -mtp=soft. -mtp=soft
# made every thread_local access route through a hand-rolled __aarch64_read_tp, which
# returned a bad thread pointer once libnx's fsdev touched thread-local state
# (fsdev_open -> crash at va=0x10000). Standard homebrew (e.g. RetroArch) uses hardware
# TLS and reads SD files fine. Dropping -mtp=soft also removes the __aarch64_read_tp
# symbol dependency entirely (the OpenAL link-order issue it was added for disappears).
set(ARCH "-march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIE -ftls-model=local-exec")
# TheSuperHackers @build githubawn 02/07/2026 GGC_SWITCH_SD_DATA: read game data
# from the SD card (sdmc:/generalszh) instead of an embedded application RomFS.
# The embedded-RomFS application NSP path reaches main() but faults on libnx's
# first romfs IStorage read (MapBufferFromClientProcess va=0xF000) under Ryujinx/
# yuzu; embedded-RomFS application NSPs are a known-fragile path. With this flag
# set we skip romfsMountSelf entirely and take the SD path, dodging that read.
# Switch-only define; no other target sees it.
# TheSuperHackers @build githubawn 05/07/2026 -fno-strict-aliasing is REQUIRED.
# This codebase was written for VC6/MSVC (which never does type-based alias
# analysis) and is full of DX8/COM type-punning through reinterpret_cast. GCC
# (devkitA64) at -O2 aggressively exploits strict aliasing and miscompiles those
# accesses, corrupting e.g. window/gadget callback data (submenus stopped taking
# input) and the terrain/water render path. Android's Clang tolerated it, which is
# why this only bit the Switch/GCC build. cmake/mingw.cmake sets the same flag.
set(CMAKE_C_FLAGS "${ARCH} -O2 -fno-strict-aliasing -ffunction-sections -fdata-sections -Wall -D__SWITCH__ -D__NX__ -D_GNU_SOURCE -DGGC_SWITCH_SD_DATA=1" CACHE STRING "C flags" FORCE)
set(CMAKE_CXX_FLAGS "${ARCH} -O2 -fno-strict-aliasing -ffunction-sections -fdata-sections -Wall -D__SWITCH__ -D__NX__ -D_GNU_SOURCE -DGGC_SWITCH_SD_DATA=1 -fpermissive" CACHE STRING "C++ flags" FORCE)

include_directories(SYSTEM "${DEVKITPRO}/libnx/include" "${DEVKITPRO}/portlibs/switch/include")
link_directories("${DEVKITPRO}/libnx/lib" "${DEVKITPRO}/portlibs/switch/lib")

# Specifying specs file for libnx linking
# TheSuperHackers @build githubawn 03/07/2026 -z gcs=never: mark the output as NOT
# using the ARMv9.4 Guarded Control Stack. devkitA64's libgcc C++ exception unwinder
# (_Unwind_RaiseException) otherwise enables GCS handling and reads gcspr_el0, which
# the emulators do not implement -> "Unknown MRS 0xD53B2521" crash on the FIRST thrown
# C++ exception. Marking GCS never keeps the unwinder off that path.
set(CMAKE_EXE_LINKER_FLAGS "-specs=${DEVKITPRO}/libnx/switch.specs -g -march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIE -pie -Wl,--allow-multiple-definition -Wl,-z,gcs=never" CACHE STRING "Linker flags" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Override link rules to automatically wrap all libraries (direct and transitive)
# in --start-group/--end-group. This is required on Nintendo Switch to resolve circular
# dependencies between engine libraries, SDL3, OpenAL, FFmpeg, and libnx.
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
set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Disable tools for Switch" FORCE)
set(RTS_BUILD_GENERALS_TOOLS OFF CACHE BOOL "Disable tools for Switch" FORCE)
set(RTS_BUILD_ZEROHOUR_TOOLS OFF CACHE BOOL "Disable tools for Switch" FORCE)

# TheSuperHackers @bugfix githubawn 01/07/2026 Boot crash: the custom GameMemory
# pool allocator (and its global operator new/delete overrides, incl. the C++14
# sized / C++17 aligned forms) corrupts memory under devkitA64/GCC + static
# libstdc++, unlike clang/libc++ on the other ports. The corruption zeroed the
# TheLocalFileSystem global mid-init, so GameLODManager::init() dispatched a
# virtual call through a null pointer (blr 0 -> "unmapped instruction"). Use the
# GameMemoryNull backend (plain malloc/free) on Switch. Isolated to this
# toolchain so no other platform's allocator behavior changes.
set(RTS_GAMEMEMORY_ENABLE OFF CACHE BOOL "Use system malloc/free on Switch" FORCE)
