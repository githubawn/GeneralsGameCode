# Interface libraries to set common defines and includes, avoid duplication later
add_library(core_wwcommon INTERFACE)

target_compile_definitions(core_wwcommon INTERFACE
    #NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(core_wwcommon INTERFACE
    d3d8lib
    milesstub
    stlport
)

target_include_directories(core_wwcommon INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    WW3D2
    WWAudio
    WWDebug
    # WWDownload # is not added because headers are included as "WWDownload/file.h"
    WWLib
    WWMath
    WWSaveLoad
)
if(UNIX OR ANDROID)
    target_include_directories(core_wwcommon INTERFACE compat/win32_shims)
    # TheSuperHackers @build bobtista 13/06/2026 — Force-include the windows.h
    # shim so that DX8 SDK headers (d3d8types.h etc.) which use LONG/INT/HWND
    # without including windows.h themselves compile cleanly on NDK Clang.
    # TheSuperHackers @build githubawn 18/06/2026 Skip under the Xcode generator:
    # Xcode emits this -include before its precompiled prefix-header include,
    # which makes clang ignore the PCH. On Xcode, windows.h is instead supplied
    # via the core_utility PCH (see Dependencies/Utility/CMakeLists.txt).
    if(NOT XCODE)
        target_compile_options(core_wwcommon INTERFACE
            -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/win32_shims/windows.h
        )
    endif()
endif()

add_subdirectory(WW3D2)
add_subdirectory(WWAudio)
add_subdirectory(WWDebug)
add_subdirectory(WWDownload)
add_subdirectory(WWLib)
add_subdirectory(WWMath)
add_subdirectory(WWSaveLoad)
add_subdirectory(WWStub)

# Helpful interface to bundle the ww modules together.
add_library(core_wwvegas INTERFACE)

target_include_directories(core_wwvegas INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    WW3D2
    WWAudio
    WWDebug
    # WWDownload # is not added because headers are included as "WWDownload/file.h"
    WWLib
    WWMath
    WWSaveLoad
)
if(UNIX OR ANDROID)
    target_include_directories(core_wwvegas INTERFACE compat/win32_shims)
    if(NOT XCODE)
        target_compile_options(core_wwvegas INTERFACE
            -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/win32_shims/windows.h
        )
    endif()
endif()

target_link_libraries(core_wwvegas INTERFACE
    # core_ww3d2
    # core_wwaudio
    core_wwdebug
    # core_wwdownload
    core_wwlib
    core_wwmath
    core_wwsaveload
)
