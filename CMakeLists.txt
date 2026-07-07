cmake_minimum_required(VERSION 3.25)

# Use packagename_ROOT for FindPackage.
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

# Disable default MSVC setting CRT type so we can set it ourselves.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Disable default MSVC warning level so we can set it ourselves.
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif()

# Allow specifying MSVC debug configurations.
#if(POLICY CMP0141)
#    cmake_policy(SET CMP0141 NEW)
#endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

include(FeatureSummary)
include(CMakeDependentOption)

# We don't support in tree builds, so help people make the right choice.
if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

# Top level project, doesn't really affect anything.
project(genzh LANGUAGES C CXX)

# This file handles extra settings wanted/needed for different compilers.
include(cmake/compilers.cmake)

# Debug symbol stripping for Release builds (MinGW)
include(cmake/debug_strip.cmake)

include(FetchContent)

# MinGW-w64 specific configuration
if(MINGW)
    include(cmake/mingw.cmake)
    include(cmake/reactos-atl.cmake)
    include(cmake/widl.cmake)
endif()

# Find/Add build dependencies and stubs shared by all projects
if((WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows") AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    include(cmake/miles.cmake)
    include(cmake/bink.cmake)
    include(cmake/dx8.cmake)
endif()

# Define a dummy stlport target when not on VC6.
if (IS_VS6_BUILD)
    include(cmake/stlport.cmake)
else()
    add_library(stlport INTERFACE)
endif()

include(cmake/config.cmake)
include(cmake/gamespy.cmake)
include(cmake/lzhl.cmake)

if (IS_VS6_BUILD)
    # The original max sdk does not compile against a modern compiler.
    # If there is a desire to make this work, then a fixed max sdk needs to be created.
    add_subdirectory(Dependencies/MaxSDK)
endif()
add_subdirectory(Dependencies/Utility)

# To be removed when abandoning VC6
if (IS_VS6_BUILD)
    include_directories(Dependencies/Utility)
    add_compile_options(/FIUtility/CppMacros.h)
endif()

add_subdirectory(resources)

add_subdirectory(Core)

# Add main build targets
if(RTS_BUILD_ZEROHOUR)
    add_subdirectory(GeneralsMD)
endif()

if(RTS_BUILD_GENERALS)
    add_subdirectory(Generals)
endif()

if (RTS_BUILD_MERGED)
    # Find python interpreter
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    
    # Generate namespaced source directories at configure time
    message(STATUS "Generating namespaced tree for Gen...")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_namespaced_tree.py
            --src-dir ${CMAKE_SOURCE_DIR}/Core
            --dest-dir ${CMAKE_BINARY_DIR}/ns_gen/Gen/Core
            --namespace Gen
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_namespaced_tree.py
            --src-dir ${CMAKE_SOURCE_DIR}/Generals
            --dest-dir ${CMAKE_BINARY_DIR}/ns_gen/Gen/Generals
            --namespace Gen
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_namespaced_tree.py
            --src-dir ${CMAKE_SOURCE_DIR}/shell
            --dest-dir ${CMAKE_BINARY_DIR}/ns_gen/Gen/shell
            --namespace Gen
        COMMAND_ERROR_IS_FATAL ANY
    )
    
    message(STATUS "Generating namespaced tree for ZH...")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_namespaced_tree.py
            --src-dir ${CMAKE_SOURCE_DIR}/Core
            --dest-dir ${CMAKE_BINARY_DIR}/ns_gen/ZH/Core
            --namespace ZH
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_namespaced_tree.py
            --src-dir ${CMAKE_SOURCE_DIR}/GeneralsMD
            --dest-dir ${CMAKE_BINARY_DIR}/ns_gen/ZH/GeneralsMD
            --namespace ZH
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_namespaced_tree.py
            --src-dir ${CMAKE_SOURCE_DIR}/shell
            --dest-dir ${CMAKE_BINARY_DIR}/ns_gen/ZH/shell
            --namespace ZH
        COMMAND_ERROR_IS_FATAL ANY
    )

    # Function to map sources from existing targets to generated tree
    function(gather_sources_merged targets_list namespace output_var)
        set(result "")
        foreach(target IN LISTS targets_list)
            if (TARGET ${target})
                message(STATUS "[Merged] Target ${target} exists.")
                get_target_property(srcs ${target} SOURCES)
                if (NOT srcs)
                    set(srcs "")
                endif()
                get_target_property(if_srcs ${target} INTERFACE_SOURCES)
                if (if_srcs)
                    list(APPEND srcs ${if_srcs})
                endif()
                if (srcs)
                    message(STATUS "[Merged] Target ${target} has sources.")
                    get_target_property(src_dir ${target} SOURCE_DIR)
                    foreach(src IN LISTS srcs)
                        # Ignore headers, we only compile source files
                        if (src MATCHES "\\.cpp$" OR src MATCHES "\\.c$" OR src MATCHES "\\.rc$")
                            get_filename_component(abs_src "${src}" ABSOLUTE BASE_DIR "${src_dir}")
                            string(REPLACE "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/ns_gen/${namespace}" gen_src "${abs_src}")
                            list(APPEND result "${gen_src}")
                        endif()
                    endforeach()
                else()
                    message(STATUS "[Merged] Target ${target} has NO sources.")
                endif()
            else()
                message(STATUS "[Merged] Target ${target} does NOT exist.")
            endif()
        endforeach()
        set(${output_var} ${result} PARENT_SCOPE)
    endfunction()

    set(GEN_TARGETS
        g_gameengine
        g_gameenginedevice
        g_ww3d2
        corei_ww3d2
        corei_wwaudio
        core_debug
        core_profile_legacy
        core_wwlib
        core_utility
        core_wwmath
        core_wwsaveload
        core_wwdebug
        corei_gameenginedevice_private
        corei_gameengine_private
        g_generals
        corei_wwdownload
    )
    # Gather and map sources
    gather_sources_merged("${GEN_TARGETS}" "Gen" GEN_MAPPED_SOURCES)
    # Include namespaced EngineSwapHook
    list(APPEND GEN_MAPPED_SOURCES "${CMAKE_BINARY_DIR}/ns_gen/Gen/shell/EngineSwapHook.cpp")

    # Targets containing source files for Zero Hour
    set(ZH_TARGETS
        z_gameengine
        z_gameenginedevice
        z_ww3d2
        corei_ww3d2
        corei_wwaudio
        core_debug
        core_profile_legacy
        core_wwlib
        core_utility
        core_wwmath
        core_wwsaveload
        core_wwdebug
        corei_gameenginedevice_private
        corei_gameengine_private
        z_generals
        corei_wwdownload
    )
    # Gather and map sources
    gather_sources_merged("${ZH_TARGETS}" "ZH" ZH_MAPPED_SOURCES)
    # Include namespaced EngineSwapHook
    list(APPEND ZH_MAPPED_SOURCES "${CMAKE_BINARY_DIR}/ns_gen/ZH/shell/EngineSwapHook.cpp")

    # Add the merged executable
    add_executable(merged_generals WIN32 shell/WinMain.cpp ${GEN_MAPPED_SOURCES} ${ZH_MAPPED_SOURCES})
    set_target_properties(merged_generals PROPERTIES OUTPUT_NAME "generals_merged")
    target_include_directories(merged_generals PRIVATE "${CMAKE_SOURCE_DIR}/Dependencies/Utility")

    # Set include directories on Gen source files
    set(GEN_INCLUDES
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/GameEngine/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/GameEngineDevice/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Source/WWVegas"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Source/WWVegas/WWAudio"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Source/WWVegas/WWDebug"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Source/WWVegas/WWLib"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Source/WWVegas/WWMath"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Libraries/Source/WWVegas/WWSaveLoad"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Core/Main"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Generals/Code/GameEngine/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Generals/Code/GameEngine/Include/Precompiled"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Generals/Code/GameEngineDevice/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Generals/Code/Libraries/Source/WWVegas"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Generals/Code/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_BINARY_DIR}/ns_gen/Gen/Generals/Code/Main"
        "${CMAKE_BINARY_DIR}/Generals/Code/Main"
        "${CMAKE_BINARY_DIR}/Core/Libraries/Source"
        "${CMAKE_BINARY_DIR}/_deps/dx8-src/include"
        "${CMAKE_SOURCE_DIR}/Core/GameEngine/Include"
        "${CMAKE_SOURCE_DIR}/Core/GameEngineDevice/Include"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Include"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWAudio"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWDebug"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWMath"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWSaveLoad"
        "${CMAKE_SOURCE_DIR}/Core/Main"
        "${CMAKE_SOURCE_DIR}/Generals/Code/GameEngine/Include"
        "${CMAKE_SOURCE_DIR}/Generals/Code/GameEngine/Include/Precompiled"
        "${CMAKE_SOURCE_DIR}/Generals/Code/GameEngineDevice/Include"
        "${CMAKE_SOURCE_DIR}/Generals/Code/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_SOURCE_DIR}/Dependencies/Utility"
    )
    set_source_files_properties(${GEN_MAPPED_SOURCES} PROPERTIES
        INCLUDE_DIRECTORIES "${GEN_INCLUDES}"
        COMPILE_DEFINITIONS "RTS_GENERALS=1;RTS_BUILD_MERGED=1"
    )

    # Set include directories on ZH source files
    set(ZH_INCLUDES
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/GameEngine/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/GameEngineDevice/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Source/WWVegas"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Source/WWVegas/WWAudio"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Source/WWVegas/WWDebug"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Source/WWVegas/WWLib"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Source/WWVegas/WWMath"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Libraries/Source/WWVegas/WWSaveLoad"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/Core/Main"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/GeneralsMD/Code/GameEngine/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/GeneralsMD/Code/GameEngine/Include/Precompiled"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/GeneralsMD/Code/GameEngineDevice/Include"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/GeneralsMD/Code/Libraries/Source/WWVegas"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_BINARY_DIR}/ns_gen/ZH/GeneralsMD/Code/Main"
        "${CMAKE_BINARY_DIR}/GeneralsMD/Code/Main"
        "${CMAKE_BINARY_DIR}/Core/Libraries/Source"
        "${CMAKE_BINARY_DIR}/_deps/dx8-src/include"
        "${CMAKE_SOURCE_DIR}/Core/GameEngine/Include"
        "${CMAKE_SOURCE_DIR}/Core/GameEngineDevice/Include"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Include"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWAudio"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWDebug"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWMath"
        "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWSaveLoad"
        "${CMAKE_SOURCE_DIR}/Core/Main"
        "${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Include"
        "${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Include/Precompiled"
        "${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngineDevice/Include"
        "${CMAKE_SOURCE_DIR}/GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2"
        "${CMAKE_SOURCE_DIR}/Dependencies/Utility"
    )
    set_source_files_properties(${ZH_MAPPED_SOURCES} PROPERTIES
        INCLUDE_DIRECTORIES "${ZH_INCLUDES}"
        COMPILE_DEFINITIONS "RTS_ZEROHOUR=1;RTS_BUILD_MERGED=1"
    )

    # Link libraries: same static dependencies as original, linked globally
    target_link_libraries(merged_generals PRIVATE
        core_config
        core_browserdispatch
        gamespy
        binkstub
        comctl32
        d3d8lib
        d3dx8
        dinput8
        dxguid
        imm32
        milesstub
        vfw32
        winmm
        liblzhl
        stlport
        resources
        core_compression
        version
        comsuppw
    )
    
    if (TARGET core_browserenginewin)
        add_dependencies(merged_generals core_browserenginewin)
    endif()
    
    if(MSVC)
        target_compile_options(merged_generals PRIVATE "/FIUtility/CppMacros.h")
        target_link_options(merged_generals PRIVATE "/NODEFAULTLIB:libci.lib")
        # Turn off default manifest on VS2005+ to prevent conflict
        if(NOT IS_VS6_BUILD)
            target_link_options(merged_generals PRIVATE "/MANIFEST:NO")
        endif()
    endif()

endif()

feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
