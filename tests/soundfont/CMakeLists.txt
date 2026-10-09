# tests/soundfont — tests reales del núcleo SoundFont (Fase B):
# round-trip write->parse, validación de referencias cruzadas,
# fixtures reales en disco, y fuzz smoke test bajo sanitizers en CI.

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler_soundfont_tests LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../native ${CMAKE_CURRENT_BINARY_DIR}/olysf2sampler_native)

add_executable(olysf2sampler_soundfont_roundtrip_test roundtrip_test.cpp)
target_link_libraries(olysf2sampler_soundfont_roundtrip_test PRIVATE olysf2sampler_soundfont)

add_executable(olysf2sampler_soundfont_fixture_test fixture_test.cpp)
target_link_libraries(olysf2sampler_soundfont_fixture_test PRIVATE olysf2sampler_soundfont)

add_executable(olysf2sampler_soundfont_fuzz_smoke_test fuzz_smoke_test.cpp)
target_link_libraries(olysf2sampler_soundfont_fuzz_smoke_test PRIVATE olysf2sampler_soundfont)

add_executable(olysf2sampler_soundfont_hardening_test hardening_test.cpp)
target_link_libraries(olysf2sampler_soundfont_hardening_test PRIVATE olysf2sampler_soundfont)

enable_testing()
add_test(NAME olysf2sampler_soundfont_roundtrip_test
         COMMAND olysf2sampler_soundfont_roundtrip_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_test(NAME olysf2sampler_soundfont_fixture_test
         COMMAND olysf2sampler_soundfont_fixture_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_test(NAME olysf2sampler_soundfont_fuzz_smoke_test
         COMMAND olysf2sampler_soundfont_fuzz_smoke_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_test(NAME olysf2sampler_soundfont_hardening_test
         COMMAND olysf2sampler_soundfont_hardening_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
