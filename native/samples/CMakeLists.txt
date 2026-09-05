# native/samples — I/O, procesamiento offline y loop points de
# samples. Depende de olysf2sampler_core.

add_library(olysf2sampler_samples INTERFACE)

target_include_directories(olysf2sampler_samples
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olysf2sampler_samples INTERFACE olysf2sampler_core)
target_compile_features(olysf2sampler_samples INTERFACE cxx_std_20)
