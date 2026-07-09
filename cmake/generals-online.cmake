# GeneralsOnline (NGMP) multiplayer backend dependencies, built from source via
# FetchContent (matching this repo's existing convention for third-party code --
# see bink.cmake, dx8.cmake, gamespy.cmake, miles.cmake, stlport.cmake). No prebuilt
# binaries are committed or downloaded; everything here compiles as part of this
# build using our own toolchain, so there is a single, internally consistent CRT
# and ABI across the whole executable.
#
# This intentionally builds everything as STATIC libraries linked directly into the
# executable rather than shipping separate DLLs. An earlier prototype linked GO's
# own prebuilt GameNetworkingSockets.dll/abseil_dll.dll/libprotobuf.dll and hit a
# heap-corruption crash (STATUS_HEAP_CORRUPTION) inside libprotobuf's own static
# initialization, at DLL-load time -- a classic symptom of either duplicate/
# ABI-mismatched protobuf runtimes across separately-built DLLs, or a CRT mismatch.
# Static linking through one toolchain avoids that whole class of problem.

# --- protobuf ---------------------------------------------------------------
# GameNetworkingSockets only requires protobuf 2.6.1+; pinning to the last v21.x
# release deliberately avoids v22+, which made abseil a hard dependency of
# protobuf itself. Not needing abseil at all keeps this dependency chain small.
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
# Must match this project's CMAKE_MSVC_RUNTIME_LIBRARY (MultiThreaded[Debug]DLL,
# i.e. /MD). protobuf defaults this ON (/MT); leaving it on would link a second,
# separate CRT into the same binary -- exactly the kind of mismatch that caused
# the heap corruption mentioned above.
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    Protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG        v21.12
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(Protobuf)

# protobuf_generate()/protobuf_generate_cpp() are normally only defined via
# protobuf's own install step, which we skip (protobuf_INSTALL is OFF above).
# GameNetworkingSockets' build calls protobuf_generate_cpp() directly, so provide
# it ourselves -- see the file for why this is a straight copy, not a shim.
include(${CMAKE_CURRENT_LIST_DIR}/protobuf-generate-compat.cmake)

# --- GameNetworkingSockets ---------------------------------------------------
set(BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ENABLE_ICE ON CACHE BOOL "" FORCE)
# Native ICE (STUN/TURN) only; the WebRTC ICE backend needs abseil, which we're
# deliberately avoiding (see protobuf note above).
set(USE_STEAMWEBRTC OFF CACHE BOOL "" FORCE)
# BCrypt is Windows' built-in crypto API -- no OpenSSL/libsodium dependency needed
# for AES/SHA256. USE_CRYPTO25519 is deliberately left unset, which falls back to
# GNS's own bundled reference ed25519/curve25519 implementation (also no external
# dependency).
set(USE_CRYPTO "BCrypt" CACHE STRING "" FORCE)
set(Protobuf_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    GameNetworkingSockets
    GIT_REPOSITORY https://github.com/ValveSoftware/GameNetworkingSockets.git
    GIT_TAG        v1.6.0
    EXCLUDE_FROM_ALL
)

# GNS unconditionally declares install(EXPORT ...) rules for itself, which CMake
# validates at generate time even though we never run `cmake --install` -- and
# that validation fails because protobuf_INSTALL is OFF (libprotobuf, one of GNS's
# link dependencies, isn't part of any export set). CMAKE_SKIP_INSTALL_RULES makes
# install() a no-op project-wide, sidestepping the validation entirely. Scoped
# tightly around just this fetch so it doesn't suppress this repo's own real
# install rules elsewhere.
set(_go_saved_skip_install_rules ${CMAKE_SKIP_INSTALL_RULES})
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(GameNetworkingSockets)
set(CMAKE_SKIP_INSTALL_RULES ${_go_saved_skip_install_rules})
unset(_go_saved_skip_install_rules)

# --- libcurl ------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
# Schannel is Windows' built-in TLS implementation -- same rationale as GNS's
# BCrypt choice above, no OpenSSL dependency needed.
set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
# Keep this build minimal and deterministic: just HTTP/HTTPS + WebSocket, which
# is all NGMP's HTTPManager/HTTPRequest actually use. Without these explicitly
# off, curl's find_package() calls for each would silently pick up whatever
# happens to be discoverable on a given machine, making the build non-reproducible.
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB "OFF" CACHE STRING "" FORCE)

FetchContent_Declare(
    Curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_11_1
    EXCLUDE_FROM_ALL
)

set(_go_saved_skip_install_rules ${CMAKE_SKIP_INSTALL_RULES})
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(Curl)
set(CMAKE_SKIP_INSTALL_RULES ${_go_saved_skip_install_rules})
unset(_go_saved_skip_install_rules)
