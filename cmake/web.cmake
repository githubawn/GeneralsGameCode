if(EMSCRIPTEN)
    set(RTS_PLATFORM_WEB TRUE)
    add_compile_definitions(RTS_PLATFORM_WEB=1)
    
    # Emscripten specific flags
    add_compile_options("-sUSE_SDL=2")
    add_link_options("-sWASM=1")
    add_link_options("-sALLOW_MEMORY_GROWTH=1")
    add_link_options("-sUSE_SDL=2")
    add_link_options("-lidbfs.js")
    
    # Use SDL2 for windowing/input abstraction
    add_compile_definitions(USE_SDL2=1)
    
    include_directories(BEFORE "${CMAKE_SOURCE_DIR}/Core/Libraries/Include/WebShims")
    add_compile_options("-include" "WebWin32.h")
    add_compile_options("-ferror-limit=0")
    
    message(STATUS "Emscripten build detected. Enabling Web Platform Layer.")
endif()
