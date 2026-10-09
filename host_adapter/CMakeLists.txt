# host_adapter/ — adaptador ABI C real para OlyzeAudioModuleHost.
# Produce libolysf2sampler_module.so, cargable con dlopen() por
# cualquier host que hable AudioModuleContract.h (no solo el de
# YeiViKas — el contrato es genérico). Independiente de jni/ y de
# Oboe: este target NO necesita NDK ni Android para compilar y
# probarse (host_simulation_harness.cpp corre en Linux normal), solo
# lo necesita para producir la .so final en arm64-v8a que el Host
# real puede cargar en un dispositivo Android.

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler_host_adapter LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../native ${CMAKE_CURRENT_BINARY_DIR}/olysf2sampler_native)

add_library(olysf2sampler_module SHARED
    src/olyze_module_adapter.cpp
)

target_include_directories(olysf2sampler_module PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/contracts)
target_link_libraries(olysf2sampler_module PRIVATE
    olysf2sampler_core
    olysf2sampler_dsp
    olysf2sampler_sampler
)
target_compile_features(olysf2sampler_module PRIVATE cxx_std_20)
set_target_properties(olysf2sampler_module PROPERTIES OUTPUT_NAME "olysf2sampler_module")

# Arnés de prueba: dlopen() real de la .so de arriba, simulando
# exactamente lo que hace ModuleLoader.cpp del Host — ver
# tests/host_simulation_harness.cpp.
find_library(DL_LIB dl)
add_executable(olysf2sampler_host_harness tests/host_simulation_harness.cpp)
target_include_directories(olysf2sampler_host_harness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/contracts)
if(DL_LIB)
    target_link_libraries(olysf2sampler_host_harness PRIVATE ${DL_LIB})
endif()

enable_testing()
add_test(
    NAME olysf2sampler_host_adapter_test
    COMMAND olysf2sampler_host_harness $<TARGET_FILE:olysf2sampler_module>
)
