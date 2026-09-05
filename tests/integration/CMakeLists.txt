# tests/integration — smoke test cruzado del núcleo C++ de OlySf2
# Sampler (confirma que todos los módulos enlazan juntos sin
# dependencias circulares).
#
# En vez de introducir GoogleTest como dependencia externa todavía sin
# evaluarla formalmente (ver third_party/README.md: toda tercera parte
# se evalúa antes de integrarse), este smoke test usa únicamente
# assert()/<cassert> de la librería estándar. GoogleTest se integrará
# vía FetchContent cuando se escriban tests unitarios reales por
# subsistema (tests/core, tests/soundfont, ...) en la fase de
# implementación de cada módulo.

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler_integration_smoke_tests LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../native ${CMAKE_CURRENT_BINARY_DIR}/olysf2sampler_native)

add_executable(olysf2sampler_smoke_test smoke_test.cpp)
target_link_libraries(olysf2sampler_smoke_test PRIVATE olysf2sampler_all)

enable_testing()
add_test(NAME olysf2sampler_smoke_test COMMAND olysf2sampler_smoke_test)
