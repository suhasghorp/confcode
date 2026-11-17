# CMake replacement for the Bazel targets in hotpathcplusplus/common/shared_memory.
# Configure with: cmake -S bench -B build -DHOTPATH_REPO=/path/to/hotpathcplusplus
cmake_minimum_required(VERSION 3.20)
project(zero_copy_ipc CXX)

set(HOTPATH_REPO "" CACHE PATH "Checkout of github.com/ktgorekore/hotpathcplusplus")
if(NOT HOTPATH_REPO)
  message(FATAL_ERROR "Set -DHOTPATH_REPO=<path to hotpathcplusplus checkout>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# Same flags as the repo's .bazelrc.
add_compile_options(-O3 -march=native -Wall -Wextra -Wpessimizing-move)

list(PREPEND CMAKE_PREFIX_PATH /usr/local)
find_package(absl REQUIRED CONFIG)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED CONFIG)
find_package(Threads REQUIRED)

set(SM ${HOTPATH_REPO}/common/shared_memory)

add_library(memory_map ${SM}/memory_map.cc)
target_include_directories(memory_map PUBLIC ${HOTPATH_REPO})
target_link_libraries(memory_map PUBLIC absl::status absl::statusor absl::strings)

add_library(broadcast_ring_buffer ${SM}/broadcast_ring_buffer.cc)
target_link_libraries(broadcast_ring_buffer PUBLIC memory_map absl::status
                      absl::statusor absl::str_format)

add_executable(memory_map_test ${SM}/memory_map_test.cc)
target_link_libraries(memory_map_test memory_map absl::strings GTest::gtest_main)

add_executable(broadcast_ring_buffer_test ${SM}/broadcast_ring_buffer_test.cc)
target_link_libraries(broadcast_ring_buffer_test broadcast_ring_buffer memory_map
                      absl::strings GTest::gtest_main GTest::gmock)

add_executable(broadcast_ring_buffer_benchmark ${SM}/broadcast_ring_buffer_benchmark.cc)
target_link_libraries(broadcast_ring_buffer_benchmark broadcast_ring_buffer memory_map
                      absl::log absl::check absl::time benchmark::benchmark_main Threads::Threads)

# Our own experiments (not part of the hotpathcplusplus repo).
add_executable(kernel_ipc_latency kernel_ipc_latency.cc)
add_executable(ring_cross_process_latency ring_cross_process_latency.cc)
target_link_libraries(ring_cross_process_latency broadcast_ring_buffer)
add_executable(false_sharing false_sharing.cc)
target_link_libraries(false_sharing Threads::Threads)
add_executable(sync_costs_benchmark sync_costs_benchmark.cc)
target_link_libraries(sync_costs_benchmark absl::time benchmark::benchmark)
