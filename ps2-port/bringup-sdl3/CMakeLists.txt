# Standalone SDL3-for-PS2 smoke test (see docs/ps2-port-plan.md, Phase 1/4).
# Not part of the main engine build. Reuses the repo's own cmake/sdl3.cmake
# fetch/build logic to prove upstream SDL3's PS2 backend (video/audio/
# joystick -- unpatched, unlike the Switch fork) compiles and links against
# our ps2dev toolchain, before wiring the real GeneralsMD SDL3 backend to it.
cmake_minimum_required(VERSION 3.20)
project(ps2_sdl3_bringup C CXX)

set(SAGE_USE_SDL3 ON CACHE BOOL "" FORCE)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/sdl3.cmake)

add_executable(ps2_sdl3_bringup main.c)
target_link_libraries(ps2_sdl3_bringup sdl3lib -ldebug -lc)
set_target_properties(ps2_sdl3_bringup PROPERTIES SUFFIX ".elf")
