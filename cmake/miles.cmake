# TheSuperHackers @bugfix githubawn 18/06/2026 On iOS build the Miles stub as a
# STATIC library. Upstream hardcodes it as SHARED (libmss32.dylib); an iOS app
# that links it as an @rpath dylib aborts at launch (dyld __abort_with_payload,
# before main()) unless the dylib is embedded and signed in the bundle. A
# PATCH_COMMAND flips SHARED -> STATIC at populate time. (Other platforms keep
# the upstream SHARED build, so this is scoped to iOS only.)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    FetchContent_Declare(
        miles
        GIT_REPOSITORY https://github.com/TheSuperHackers/miles-sdk-stub.git
        GIT_TAG        6e32700d7ba4b4713a03bf1f5ffc3b0ac8d17264
        PATCH_COMMAND  ${CMAKE_COMMAND} -DMILES_CML=CMakeLists.txt -P ${CMAKE_CURRENT_LIST_DIR}/patch_miles_static.cmake
    )
else()
    FetchContent_Declare(
        miles
        GIT_REPOSITORY https://github.com/TheSuperHackers/miles-sdk-stub.git
        GIT_TAG        6e32700d7ba4b4713a03bf1f5ffc3b0ac8d17264
    )
endif()

FetchContent_MakeAvailable(miles)
