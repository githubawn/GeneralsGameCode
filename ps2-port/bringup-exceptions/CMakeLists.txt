# PS2 C++ exception-handling smoke test (see docs/ps2-port-plan.md,
# and main.cpp for why). Standalone, not part of the main engine build.
cmake_minimum_required(VERSION 3.20)
project(ps2_exceptions_bringup CXX)

add_executable(ps2_exceptions_bringup main.cpp)
target_link_libraries(ps2_exceptions_bringup -ldebug -lc)
set_target_properties(ps2_exceptions_bringup PROPERTIES SUFFIX ".elf")
