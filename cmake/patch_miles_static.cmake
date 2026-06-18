# TheSuperHackers @bugfix githubawn 18/06/2026 Patch helper invoked as a
# FetchContent PATCH_COMMAND on iOS. The miles-sdk-stub upstream hardcodes the
# stub as a SHARED library (libmss32.dylib). Linking it as an @rpath dylib makes
# an iOS app crash at launch (dyld __abort_with_payload, before main()) unless
# the dylib is embedded and signed in the bundle. Building it STATIC sidesteps
# that entirely. Idempotent: a no-op once already STATIC.
#
# Invoke with: ${CMAKE_COMMAND} -DMILES_CML=<path-to-CMakeLists.txt> -P this.cmake
if(NOT DEFINED MILES_CML)
    set(MILES_CML "CMakeLists.txt")
endif()
file(READ "${MILES_CML}" _miles_src)
string(REPLACE "add_library(milesstub SHARED miles.c)"
               "add_library(milesstub STATIC miles.c)"
               _miles_src "${_miles_src}")
file(WRITE "${MILES_CML}" "${_miles_src}")
message(STATUS "patch_miles_static: ensured milesstub is STATIC in ${MILES_CML}")
