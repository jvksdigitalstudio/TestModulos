# native/sampler — motor de reproducción (voces, polifonía, mapeo).
# Depende de olysf2sampler_core, olysf2sampler_dsp (dirección
# permitida: sampler -> dsp, nunca al revés) y olysf2sampler_threading
# (cola lock-free control-thread -> audio-thread, ver
# docs/architecture/THREADING.md).
#
# STATIC: implementación real desde la Fase C/cierre-de-pendientes
# (Voice con filtro por voz, VoiceManager, KeyVelocityMapping,
# SamplerEngine realtime-safe vía cola lock-free).

add_library(olysf2sampler_sampler STATIC
    src/voice_impl.cpp
    src/voice_manager_impl.cpp
    src/mapping_impl.cpp
    src/sampler_engine_impl.cpp
)

target_include_directories(olysf2sampler_sampler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olysf2sampler_sampler PUBLIC olysf2sampler_core olysf2sampler_dsp olysf2sampler_threading)
target_compile_features(olysf2sampler_sampler PUBLIC cxx_std_20)
