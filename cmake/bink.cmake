if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    FetchContent_Declare(
        bink
        GIT_REPOSITORY https://github.com/TheSuperHackers/bink-sdk-stub.git
        GIT_TAG        180fc4620ed72fd700347ab837a5271fd0259901
        PATCH_COMMAND  ${CMAKE_COMMAND} -DBINK_CML=CMakeLists.txt -P ${CMAKE_CURRENT_LIST_DIR}/patch_bink_static.cmake
    )
else()
    FetchContent_Declare(
        bink
        GIT_REPOSITORY https://github.com/TheSuperHackers/bink-sdk-stub.git
        GIT_TAG        180fc4620ed72fd700347ab837a5271fd0259901
    )
endif()

FetchContent_MakeAvailable(bink)
