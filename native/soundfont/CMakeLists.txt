# native/soundfont — modelo, parser, writer, validación y
# serialización de SF2. Depende únicamente de olysf2sampler_core.
# NO depende de olysf2sampler_sampler (SoundFont no conoce UI ni se
# acopla al sampler; el sampler consume soundfont, no al revés).
#
# STATIC (no INTERFACE): parser/writer/validator tienen implementación
# real (RIFF/INFO/sdta/pdta) desde la Fase B. sf2_binary_format.hpp
# (en src/, no en include/) es un detalle interno no expuesto a
# consumidores del módulo.

add_library(olysf2sampler_soundfont STATIC
    src/sf2_parser_impl.cpp
    src/sf2_writer_impl.cpp
    src/sf2_validator_impl.cpp
)

target_include_directories(olysf2sampler_soundfont
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(olysf2sampler_soundfont PUBLIC olysf2sampler_core)
target_compile_features(olysf2sampler_soundfont PUBLIC cxx_std_20)
