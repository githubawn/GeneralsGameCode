set(GS_OPENSSL FALSE)
set(GAMESPY_SERVER_NAME "server.cnc-online.net")

if(NOT RTS_PLATFORM_WEB)
    FetchContent_Declare(
        gamespy
        GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
        GIT_TAG        07e3d15c500415abc281efb74322ab6d9c857eb8
    )

    FetchContent_MakeAvailable(gamespy)
else()
    # Stub target for web to avoid link errors
    if(NOT TARGET gamespy)
        add_library(gamespy INTERFACE)
        add_library(gamespy::gamespy ALIAS gamespy)
    endif()
endif()
