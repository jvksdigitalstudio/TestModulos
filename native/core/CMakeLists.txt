# native/core — tipos base compartidos (Result, Error, versión).
# Sin dependencias hacia otros módulos de OlySf2 Sampler: es la base de
# la que todos los demás módulos pueden depender.

add_library(olysf2sampler_core STATIC
    src/version.cpp
)

target_include_directories(olysf2sampler_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(olysf2sampler_core PUBLIC cxx_std_20)
