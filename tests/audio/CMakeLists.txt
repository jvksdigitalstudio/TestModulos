# tests/audio — tests reales de native/audio (Fase D): orquestación
# de AudioEngine verificada con un AudioBackend de prueba inyectado
# (no requiere Oboe ni un dispositivo Android).

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler_audio_tests LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../native ${CMAKE_CURRENT_BINARY_DIR}/olysf2sampler_native)

add_executable(olysf2sampler_audio_engine_test audio_engine_test.cpp)
target_link_libraries(olysf2sampler_audio_engine_test PRIVATE olysf2sampler_audio)

enable_testing()
add_test(NAME olysf2sampler_audio_engine_test COMMAND olysf2sampler_audio_engine_test)
