cmake_minimum_required(VERSION 3.25)
project(sage_msaa_benchmark LANGUAGES C CXX)

include(FetchContent)

# We expect this to be run from the root or with the root as CMAKE_SOURCE_DIR/..
set(CMAKE_CXX_STANDARD 17)

# Include the DX8 setup from the main project
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/dx8.cmake")

add_executable(benchmark_msaa benchmark_main.cpp)

# Link against the DX8 target defined by dx8.cmake
target_link_libraries(benchmark_msaa PRIVATE d3d8lib)

# Set output directory to root/build_benchmark
set_target_properties(benchmark_msaa PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
