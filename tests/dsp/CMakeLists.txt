# tests/dsp — tests reales de DSP (Fase C): forma de envolvente ADSR,
# atenuación medida de un filtro biquad, valores conocidos de LFO,
# longitud de salida de un resampler.

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler_dsp_tests LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../native ${CMAKE_CURRENT_BINARY_DIR}/olysf2sampler_native)

add_executable(olysf2sampler_dsp_test real_dsp_test.cpp)
target_link_libraries(olysf2sampler_dsp_test PRIVATE olysf2sampler_dsp)

enable_testing()
add_test(NAME olysf2sampler_dsp_test COMMAND olysf2sampler_dsp_test)
