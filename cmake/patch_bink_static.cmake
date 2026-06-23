# TheSuperHackers @bugfix static bink library patch
if(NOT DEFINED BINK_CML)
    set(BINK_CML "CMakeLists.txt")
endif()
file(READ "${BINK_CML}" _bink_src)
string(REPLACE "add_library(binkstub SHARED bink.c)"
               "add_library(binkstub STATIC bink.c)"
               _bink_src "${_bink_src}")
file(WRITE "${BINK_CML}" "${_bink_src}")
message(STATUS "patch_bink_static: ensured binkstub is STATIC in ${BINK_CML}")
