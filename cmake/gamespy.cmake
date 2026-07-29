set(GS_OPENSSL FALSE)
set(GAMESPY_SERVER_NAME "server.cnc-online.net")

FetchContent_Declare(
    gamespy
    GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
    GIT_TAG        07e3d15c500415abc281efb74322ab6d9c857eb8
)

FetchContent_MakeAvailable(gamespy)

if(APPLE AND TARGET gsinterface)
    target_compile_definitions(gsinterface INTERFACE _MACOSX)
endif()

# TheSuperHackers @build githubawn 29/07/2026 The GamespySDK picks its platform code
# paths from exactly one platform macro. Emscripten defines none of them, so point it
# at the Linux sources, whose socket/thread/util code compiles against the Emscripten
# sysroot.
if(EMSCRIPTEN AND TARGET gsinterface)
    target_compile_definitions(gsinterface INTERFACE _UNIX __linux__)
endif()
