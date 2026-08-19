# native/audio — orquestación del stream de audio (depende de
# olysf2sampler_platform para el backend real, y de olysf2sampler_core).
# STATIC: AudioEngine tiene implementación real desde la Fase D,
# testeable en host inyectando un AudioBackend de prueba (no requiere
# Oboe ni Android — ver tests/audio/).

add_library(olysf2sampler_audio STATIC
    src/audio_engine_impl.cpp
)

target_include_directories(olysf2sampler_audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olysf2sampler_audio PUBLIC olysf2sampler_core olysf2sampler_platform)
target_compile_features(olysf2sampler_audio PUBLIC cxx_std_20)
