# Allow FetchContent_Populate for binary-only dependency
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(
    dx9
    GIT_REPOSITORY https://github.com/hrydgard/minidx9.git
    GIT_TAG        master
)

FetchContent_GetProperties(dx9)
if(NOT dx9_POPULATED)
    message(STATUS "Populating DX9 SDK...")
    FetchContent_Populate(dx9)
    # Surgical Cleanup: Remove headers that conflict with the modern Windows SDK.
    # These legacy files in minidx9 shadow the system headers, breaking D2D/MFC in C++20.
    message(STATUS "Cleaning up conflicting DX9 headers...")
    file(REMOVE "${dx9_SOURCE_DIR}/Include/Dcommon.h")
    file(REMOVE "${dx9_SOURCE_DIR}/Include/DWrite.h")
    file(REMOVE "${dx9_SOURCE_DIR}/Include/D2D1.h")
endif()

# Provide targets for the project to consume
if(NOT TARGET d3d9)
    add_library(d3d9 INTERFACE IMPORTED GLOBAL)
    set_target_properties(d3d9 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${dx9_SOURCE_DIR}/Include"
    )
    if(MSVC)
        set_target_properties(d3d9 PROPERTIES
            INTERFACE_LINK_LIBRARIES "${dx9_SOURCE_DIR}/Lib/x86/d3d9.lib"
        )
    else()
        set_target_properties(d3d9 PROPERTIES
            INTERFACE_LINK_LIBRARIES "-L${dx9_SOURCE_DIR}/Lib/x86 -ld3d9"
        )
    endif()
endif()

if(NOT TARGET d3dx9)
    add_library(d3dx9 INTERFACE IMPORTED GLOBAL)
    if(MSVC)
        set_target_properties(d3dx9 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${dx9_SOURCE_DIR}/Include"
            INTERFACE_LINK_LIBRARIES "${dx9_SOURCE_DIR}/Lib/x86/d3dx9.lib"
        )
    else()
        # MinGW usually links to d3dx9 which maps to d3dx9d
        set_target_properties(d3dx9 PROPERTIES
            INTERFACE_LINK_LIBRARIES "d3dx9"
        )
    endif()
endif()

# The main project might expect a 'dx9' target
if(NOT TARGET dx9)
    add_library(dx9 INTERFACE IMPORTED GLOBAL)
    target_link_libraries(dx9 INTERFACE d3d9 d3dx9)
endif()

if(NOT TARGET d3d9lib)
    add_library(d3d9lib INTERFACE IMPORTED GLOBAL)
    target_link_libraries(d3d9lib INTERFACE d3d9)
endif()

# Case-sensitivity alias for legacy CMakeLists.txt files
if(NOT TARGET d3DX9)
    add_library(d3DX9 INTERFACE IMPORTED GLOBAL)
    target_link_libraries(d3DX9 INTERFACE d3dx9)
endif()

if(NOT TARGET dinput8)
    add_library(dinput8 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(dinput8 PROPERTIES
        IMPORTED_LOCATION "${dx9_SOURCE_DIR}/Lib/x86/dinput8.lib"
    )
endif()

if(NOT TARGET dxguid)
    add_library(dxguid UNKNOWN IMPORTED GLOBAL)
    set_target_properties(dxguid PROPERTIES
        IMPORTED_LOCATION "${dx9_SOURCE_DIR}/Lib/x86/dxguid.lib"
    )
endif()
