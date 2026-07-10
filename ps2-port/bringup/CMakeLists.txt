# Standalone toolchain smoke test for the PS2 port (see docs/ps2-port-plan.md,
# Phase 1). Not part of the main engine build -- configure/build this
# directory directly against cmake/toolchains/ps2.cmake to prove the
# mips64r5900el-ps2-elf toolchain + ps2sdk + gsKit produce a bootable ELF
# before any engine code is touched.
cmake_minimum_required(VERSION 3.20)
project(ps2_bringup C CXX)

add_executable(ps2_bringup main.c)
target_link_libraries(ps2_bringup -ldebug -lc)

set_target_properties(ps2_bringup PROPERTIES OUTPUT_NAME "ps2_bringup")
