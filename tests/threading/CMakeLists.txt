# tests/threading — tests reales de primitivas de concurrencia
# (cierre de pendiente documentado: cola SPSC lock-free entre control
# thread y audio thread).

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler_threading_tests LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../native ${CMAKE_CURRENT_BINARY_DIR}/olysf2sampler_native)

find_package(Threads REQUIRED)

add_executable(olysf2sampler_spsc_queue_test spsc_queue_test.cpp)
target_link_libraries(olysf2sampler_spsc_queue_test PRIVATE olysf2sampler_threading Threads::Threads)

enable_testing()
add_test(NAME olysf2sampler_spsc_queue_test COMMAND olysf2sampler_spsc_queue_test)
