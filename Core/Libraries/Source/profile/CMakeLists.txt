set(PROFILE_SRC
    "internal_cmd.h"
    "internal_funclevel.h"
    "internal_highlevel.h"
    "internal_result.h"
    "internal.h"
    "profile_cmd.cpp"
    "profile_doc.h"
    "profile_funclevel.cpp"
    "profile_funclevel.h"
    "profile_highlevel.cpp"
    "profile_highlevel.h"
    "profile_result.cpp"
    "profile_result.h"
    "profile.cpp"
    "profile.h"
)

add_library(core_profile_legacy STATIC)

target_include_directories(core_profile_legacy INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# TheSuperHackers @build bobtista 13/06/2026 Legacy EA profiling lib is Windows-
# only and unused by the engine on other platforms; build an empty stub there.
if(WIN32)
    target_sources(core_profile_legacy PRIVATE ${PROFILE_SRC})

    target_precompile_headers(core_profile_legacy PRIVATE
        [["Utility/CppMacros.h"]] # Must be first, to be removed when abandoning VC6
        "profile.h"
        "internal.h"
        <windows.h>
    )
else()
    target_sources(core_profile_legacy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/profile_stub_nonwindows.cpp)
endif()

target_link_libraries(core_profile_legacy PRIVATE
    core_wwcommon
    corei_always
)
