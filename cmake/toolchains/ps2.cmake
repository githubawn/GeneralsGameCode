# cmake/toolchains/ps2.cmake
# Configures the ps2dev/ps2sdk (mips64r5900el-ps2-elf) toolchain for
# Sony PlayStation 2 (Emotion Engine) cross-compilation.

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_PROCESSOR "mips")

if(NOT DEFINED ENV{PS2DEV})
    set(ENV{PS2DEV} "C:/code/ps2dev")
endif()
if(NOT DEFINED ENV{PS2SDK})
    set(ENV{PS2SDK} "$ENV{PS2DEV}/ps2sdk")
endif()
if(NOT DEFINED ENV{GSKIT})
    set(ENV{GSKIT} "$ENV{PS2DEV}/gsKit")
endif()

set(PS2DEV "$ENV{PS2DEV}")
set(PS2SDK "$ENV{PS2SDK}")
set(GSKIT "$ENV{GSKIT}")

# The ps2dev Windows release's mips64r5900el-ps2-elf-* binaries (gcc, cc1,
# cc1plus, as, ld, ...) are 32-bit (x86) and need their own MinGW32 runtime
# DLLs (libwinpthread, libiconv, libzstd, libgmp, libisl, libmpc, libmpfr,
# libgcc_s_dw2, libstdc++, ...) which the release tarball does not bundle.
# cc1.exe/cc1plus.exe live in a separate libexec/ directory from gcc.exe, so
# placing DLLs next to gcc.exe alone is not enough -- put them on PATH instead
# so every EE/IOP toolchain binary finds them regardless of its own directory.
set(ENV{PATH} "${PS2DEV}/runtime-dlls;$ENV{PATH}")

# Cross-compilers (EE = Emotion Engine, the PS2 main CPU)
set(CMAKE_C_COMPILER "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-g++.exe")
set(CMAKE_ASM_COMPILER "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-as.exe")
set(CMAKE_AR "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-ar.exe" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-ranlib.exe" CACHE FILEPATH "Ranlib")

set(CMAKE_FIND_ROOT_PATH "${PS2SDK}" "${PS2SDK}/ports" "${GSKIT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# -D__PS2__ is the guard macro for all PS2-only code in this codebase (see
# docs/ps2-port-plan.md). -D_EE/-DPS2 are the ps2sdk/gsKit convention macros.
# -G0 disables MIPS small-data-section GP-relative addressing (required once
# static data exceeds the 8-byte-aligned small-data threshold; ps2sdk samples
# always build with -G0).
set(EE_FLAGS "-D_EE -DPS2 -D__PS2__ -O2 -G0 -Wall -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS "${EE_FLAGS} -I${PS2SDK}/ee/include -I${PS2SDK}/common/include -I${GSKIT}/include -I${PS2SDK}/ports/include" CACHE STRING "C flags" FORCE)
set(CMAKE_CXX_FLAGS "${EE_FLAGS} -I${PS2SDK}/ee/include -I${PS2SDK}/common/include -I${GSKIT}/include -I${PS2SDK}/ports/include -fno-exceptions -fno-rtti" CACHE STRING "C++ flags" FORCE)

set(CMAKE_EXE_LINKER_FLAGS "-L${PS2SDK}/ee/lib -L${GSKIT}/lib -L${PS2SDK}/ports/lib -Wl,-zmax-page-size=128 -T${PS2SDK}/ee/startup/linkfile -Wl,--gc-sections" CACHE STRING "Linker flags" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Disable tools requiring MFC/Win32/DirectX -- none of this builds for a
# freestanding MIPS EE target.
set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Disable tools for PS2" FORCE)
set(RTS_BUILD_GENERALS_TOOLS OFF CACHE BOOL "Disable tools for PS2" FORCE)
set(RTS_BUILD_ZEROHOUR_TOOLS OFF CACHE BOOL "Disable tools for PS2" FORCE)
