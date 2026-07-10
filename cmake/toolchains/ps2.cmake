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
# TheSuperHackers @build githubawn 10/07/2026 -D_DEFAULT_SOURCE: the main
# CMakeLists sets -std=c++20 (strict ISO, not gnu++20), which defines
# __STRICT_ANSI__. newlib's sys/cdefs.h uses that to gate __BSD_VISIBLE/
# __XSI_VISIBLE to 0 by default, hiding usleep(), strcasecmp(), and other
# BSD/XSI extensions the engine's compat headers rely on. _DEFAULT_SOURCE
# restores newlib's normal (non-strict) header visibility without having
# to touch -std= itself. PS2-only; no other toolchain sets -std=c++20
# against newlib.
# TheSuperHackers @build githubawn 10/07/2026 -D_UNIX: several engine
# compat files (e.g. WWLib/thread.cpp) already have a correct pthread-based
# implementation gated behind "#ifdef _UNIX", with the #else branch calling
# raw Win32 threading APIs (_beginthread, SetThreadPriority, ...) that only
# exist on real Windows. No CMake file in this repo currently defines
# _UNIX globally for any platform (grepped), so this is PS2's own choice,
# not a change to another platform's flags. ps2sdk provides a real pthread
# implementation, so this is both safe and correct here.
# TheSuperHackers @build githubawn 10/07/2026 -U__mips64: the compiler
# predefines this (real GCC target macro for this triple, not ours) and the
# vendored GameSpySDK's own "#ifdef __mips64" platform detection treats it
# as "this is a real PS2 devkit", demanding ancient EENET/INSOCK/eekernel.h
# support ps2sdk doesn't have. Originally scoped this to just the
# gsinterface CMake target, but our own engine code (e.g.
# GameNetwork/GameSpy/PeerDefs.h) also includes GameSpy headers directly
# and hit the same #error outside that target's reach. Nothing in this
# codebase uses __mips64 itself (grepped), so undefining it for the whole
# PS2 build is safe.
# TheSuperHackers @build githubawn 10/07/2026 __declspec(x)= : the Miles/
# Bink SDK stubs use bare "__declspec(dllimport/dllexport)" with no
# non-Windows guard (unlike their adjacent __stdcall handling, which does
# guard). Clang (used by Android/Linux/macOS/iOS) understands __declspec
# unconditionally as a compatibility extension; GCC does not (confirmed:
# -fms-extensions does NOT add __declspec parsing -- that's a mingw/PE-
# target-only GCC feature, not a general frontend flag), and fails outright
# ("expected declaration specifiers before '__declspec'"), with confusing
# cascading errors afterward since it derails the parser. Defining it away
# as a no-op function-like macro is the standard portable fix. PS2-only: no
# other toolchain in this repo is GCC-targeting-non-Windows.
set(EE_FLAGS "-D_EE -DPS2 -D__PS2__ -D_UNIX -D_DEFAULT_SOURCE -U__mips64 -D__declspec(x)= -O2 -G0 -Wall -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS "${EE_FLAGS} -I${PS2SDK}/ee/include -I${PS2SDK}/common/include -I${GSKIT}/include -I${PS2SDK}/ports/include" CACHE STRING "C flags" FORCE)
# TheSuperHackers @build githubawn 10/07/2026 Do NOT add -fno-exceptions/
# -fno-rtti here: the engine genuinely uses C++ exceptions (e.g. FileSystem
# INI parsing throws on error) and no other platform toolchain in this repo
# disables them. An earlier version of this file did, which broke on
# Core/Libraries/Source/WWVegas/compat/win32_shims/file_compat.h's
# try/catch with "exception handling disabled, use '-fexceptions'".
set(CMAKE_CXX_FLAGS "${EE_FLAGS} -I${PS2SDK}/ee/include -I${PS2SDK}/common/include -I${GSKIT}/include -I${PS2SDK}/ports/include" CACHE STRING "C++ flags" FORCE)

# TheSuperHackers @build githubawn 10/07/2026 No -Wl,--gc-sections: it
# corrupts .eh_frame on this toolchain/target combination. Confirmed with a
# minimal standalone repro (ps2-port/bringup-exceptions): a plain
# throw-across-function-boundary crashed inside libgcc's get_cie_encoding
# (DWARF CFI parser) reading garbage as a CIE augmentation string, then
# inside __deregister_frame_info_bases walking a corrupted frame list --
# both symptoms disappeared immediately when --gc-sections was removed from
# an otherwise-identical link. GC'ing code sections without fully
# preserving CIE/FDE linkage is a known class of bug on less-common
# ld/target combinations. Costs some code size; revisit only with a
# demonstrated need and a real fix (e.g. --gc-sections plus verified
# eh_frame integrity), not by re-adding this blind.
set(CMAKE_EXE_LINKER_FLAGS "-L${PS2SDK}/ee/lib -L${GSKIT}/lib -L${PS2SDK}/ports/lib -Wl,-zmax-page-size=128 -T${PS2SDK}/ee/startup/linkfile" CACHE STRING "Linker flags" FORCE)

# TheSuperHackers @build githubawn 10/07/2026 -latomic: GCC on this MIPS
# target emits calls to __atomic_fetch_add_4/__atomic_test_and_set/etc. for
# std::atomic<T> operations it can't inline as single instructions (the
# R5900 has no hardware CAS); the toolchain does ship a real libatomic.a
# with these, just not linked by default. Set via CMAKE_*_STANDARD_LIBRARIES
# (appended after all objects/libraries on the link line) rather than
# CMAKE_EXE_LINKER_FLAGS (placed before them) -- static library symbol
# resolution is left-to-right, so -latomic before the .a files that
# reference __atomic_* never actually resolves anything.
set(CMAKE_C_STANDARD_LIBRARIES "-latomic" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "-latomic" CACHE STRING "" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Matches the ps2dev-provided ps2dev.cmake convention: downstream CMake
# projects that already ship real PS2 support (e.g. upstream SDL3, which has
# an unpatched PS2 video/audio/joystick backend) key off this bare PS2
# variable, not CMAKE_SYSTEM_NAME.
set(PS2 TRUE CACHE BOOL "Building for PS2" FORCE)
set(PLATFORM_PS2 TRUE CACHE BOOL "Building for PS2" FORCE)
set(EE TRUE CACHE BOOL "Building for the PS2 Emotion Engine" FORCE)

# Disable tools requiring MFC/Win32/DirectX -- none of this builds for a
# freestanding MIPS EE target.
set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Disable tools for PS2" FORCE)
set(RTS_BUILD_GENERALS_TOOLS OFF CACHE BOOL "Disable tools for PS2" FORCE)
set(RTS_BUILD_ZEROHOUR_TOOLS OFF CACHE BOOL "Disable tools for PS2" FORCE)

# Note: tried disabling RTS_GAMEMEMORY_ENABLE here (matching Switch's fix
# for a similar-looking symptom) to explain GlobalData::
# BuildUserDataPathFromRegistry() silently returning an empty AsciiString.
# It didn't fix that symptom, and the real cause turned out to be
# --gc-sections corrupting .eh_frame (removed above) -- reverted this to
# test that fix in isolation rather than stacking two speculative changes.
# Revisit with real evidence if GameMemory pool corruption shows up later.
