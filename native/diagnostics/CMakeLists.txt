add_library(olysf2sampler_diagnostics INTERFACE)

target_include_directories(olysf2sampler_diagnostics
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olysf2sampler_diagnostics INTERFACE olysf2sampler_core)
target_compile_features(olysf2sampler_diagnostics INTERFACE cxx_std_20)
