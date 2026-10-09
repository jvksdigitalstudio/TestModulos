# native/dsp — primitivas de procesamiento de señal. Depende
# únicamente de olysf2sampler_core. NO depende de olysf2sampler_sampler
# (dirección permitida: sampler -> dsp, nunca al revés).
#
# STATIC: filter/envelope/lfo/resampler tienen implementación real
# desde la Fase C (además de interpolation/gain/pan, ya reales desde
# antes).

add_library(olysf2sampler_dsp STATIC
    src/interpolation.cpp
    src/gain.cpp
    src/pan.cpp
    src/envelope_impl.cpp
    src/filter_impl.cpp
    src/lfo_impl.cpp
    src/resampler_impl.cpp
)

target_include_directories(olysf2sampler_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olysf2sampler_dsp PUBLIC olysf2sampler_core)
target_compile_features(olysf2sampler_dsp PUBLIC cxx_std_20)
