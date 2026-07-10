# TheSuperHackers @build githubawn 10/07/2026 Patch helper invoked as a
# FetchContent PATCH_COMMAND on PS2 only. gssha1.h unconditionally typedefs
# uint32_t/uint8_t for "old compilers without stdint.h", guarded only by
# "#ifndef _PS3" (PS3's Sony libs already provide them). ps2sdk's modern
# newlib provides a real <stdint.h> too, so on PS2 this redundant typedef
# conflicts with the real one ("conflicting types for 'uint32_t'"). Skip it
# on PS2 the same way it is already skipped on PS3. Idempotent.
#
# Invoke with: ${CMAKE_COMMAND} -DGAMESPY_SHA1_H=<path-to-gssha1.h> -P this.cmake
if(NOT DEFINED GAMESPY_SHA1_H)
    message(FATAL_ERROR "GAMESPY_SHA1_H not set")
endif()
file(READ "${GAMESPY_SHA1_H}" _gs_src)
string(REPLACE "#ifndef _PS3\n// these common types are defined in sony libs"
               "#if !defined(_PS3) && !defined(__PS2__)\n// these common types are defined in sony libs / ps2sdk's <stdint.h>"
               _gs_src "${_gs_src}")
file(WRITE "${GAMESPY_SHA1_H}" "${_gs_src}")
message(STATUS "patch_gamespy_ps2: excluded __PS2__ from gssha1.h's redundant uint32_t/uint8_t typedefs")
