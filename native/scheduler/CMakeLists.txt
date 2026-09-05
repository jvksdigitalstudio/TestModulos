add_library(olysf2sampler_scheduler INTERFACE)

target_include_directories(olysf2sampler_scheduler
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olysf2sampler_scheduler INTERFACE olysf2sampler_core olysf2sampler_threading)
target_compile_features(olysf2sampler_scheduler INTERFACE cxx_std_20)
